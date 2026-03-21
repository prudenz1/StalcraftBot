#include "Database.h"
#include "Config.h"
#include "utils/Logger.h"

#include <QFile>
#include <QTextStream>
#include <QCoreApplication>

Database::Database(Config* config, QObject* parent)
    : QObject(parent)
    , m_config(config)
{
}

Database::~Database() {
    disconnect();
}

bool Database::connect() {
    QString connStr = QString("host=%1 port=%2 dbname=%3 user=%4 password=%5")
        .arg(m_config->dbHost())
        .arg(m_config->dbPort())
        .arg(m_config->dbName())
        .arg(m_config->dbUser())
        .arg(m_config->dbPassword());

    m_conn = PQconnectdb(connStr.toUtf8().constData());

    if (PQstatus(m_conn) != CONNECTION_OK) {
        LOG_ERROR("DB connection failed: {}", PQerrorMessage(m_conn));
        PQfinish(m_conn);
        m_conn = nullptr;
        return false;
    }

    PQsetClientEncoding(m_conn, "UTF8");
    LOG_INFO("Connected to PostgreSQL: {}:{}/{}",
             m_config->dbHost().toStdString(),
             m_config->dbPort(),
             m_config->dbName().toStdString());
    return true;
}

void Database::disconnect() {
    if (m_conn) {
        PQfinish(m_conn);
        m_conn = nullptr;
        LOG_INFO("Disconnected from PostgreSQL");
    }
}

bool Database::isConnected() const {
    return m_conn && PQstatus(m_conn) == CONNECTION_OK;
}

void Database::migrate() {
    QString schemaPath = QCoreApplication::applicationDirPath() + "/../sql/schema.sql";
    QFile file(schemaPath);

    if (!file.exists()) {
        schemaPath = QCoreApplication::applicationDirPath() + "/sql/schema.sql";
        file.setFileName(schemaPath);
    }
    if (!file.exists()) {
        schemaPath = "sql/schema.sql";
        file.setFileName(schemaPath);
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_WARN("schema.sql not found at {}, attempting inline migration",
                 schemaPath.toStdString());
        exec(R"(
            CREATE TABLE IF NOT EXISTS items (
                id VARCHAR(16) PRIMARY KEY,
                category VARCHAR(128) NOT NULL,
                name_ru VARCHAR(256) NOT NULL,
                tracked BOOLEAN DEFAULT FALSE
            );
            CREATE TABLE IF NOT EXISTS lot_snapshots (
                id BIGSERIAL PRIMARY KEY,
                item_id VARCHAR(16) REFERENCES items(id),
                buyout_price BIGINT,
                start_price BIGINT,
                start_time TIMESTAMPTZ,
                snapshot_time TIMESTAMPTZ NOT NULL DEFAULT NOW()
            );
            CREATE INDEX IF NOT EXISTS idx_lot_snapshots_item_time ON lot_snapshots(item_id, snapshot_time);
            CREATE TABLE IF NOT EXISTS price_snapshots (
                id BIGSERIAL PRIMARY KEY,
                item_id VARCHAR(16) REFERENCES items(id),
                timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                min_price BIGINT, avg_price BIGINT, median_price BIGINT, max_price BIGINT,
                std_dev DOUBLE PRECISION, lot_count INT, filtered_count INT
            );
            CREATE INDEX IF NOT EXISTS idx_price_snapshots_item_time ON price_snapshots(item_id, timestamp);
            CREATE TABLE IF NOT EXISTS hourly_stats (
                id BIGSERIAL PRIMARY KEY,
                item_id VARCHAR(16) REFERENCES items(id),
                hour SMALLINT NOT NULL CHECK (hour BETWEEN 0 AND 23),
                avg_price BIGINT, sample_count INT,
                updated_at TIMESTAMPTZ DEFAULT NOW(),
                UNIQUE(item_id, hour)
            );
            CREATE TABLE IF NOT EXISTS alerts (
                id BIGSERIAL PRIMARY KEY,
                item_id VARCHAR(16) REFERENCES items(id),
                alert_type VARCHAR(16) NOT NULL,
                rating DOUBLE PRECISION,
                message TEXT,
                created_at TIMESTAMPTZ DEFAULT NOW()
            );
            CREATE INDEX IF NOT EXISTS idx_alerts_item_time ON alerts(item_id, created_at);
        )");
        return;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString sql = in.readAll();
    file.close();

    if (exec(sql)) {
        LOG_INFO("Database migration completed");
    } else {
        LOG_ERROR("Database migration failed");
    }
}

// --- Items ---

bool Database::upsertItem(const Item& item) {
    QString sql = QString(
        "INSERT INTO items (id, category, name_ru, tracked) "
        "VALUES ('%1', '%2', '%3', %4) "
        "ON CONFLICT (id) DO UPDATE SET category=EXCLUDED.category, name_ru=EXCLUDED.name_ru")
        .arg(escape(item.id), escape(item.category), escape(item.nameRu),
             item.tracked ? "TRUE" : "FALSE");
    return exec(sql);
}

bool Database::upsertItems(const QVector<Item>& items) {
    if (items.isEmpty()) return true;

    exec("BEGIN");
    for (const auto& item : items) {
        if (!upsertItem(item)) {
            exec("ROLLBACK");
            return false;
        }
    }
    return exec("COMMIT");
}

QVector<Item> Database::trackedItems() {
    QVector<Item> result;
    PGresult* res = query("SELECT id, category, name_ru, tracked FROM items WHERE tracked = TRUE ORDER BY name_ru");
    if (!res) return result;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        Item item;
        item.id = QString::fromUtf8(PQgetvalue(res, i, 0));
        item.category = QString::fromUtf8(PQgetvalue(res, i, 1));
        item.nameRu = QString::fromUtf8(PQgetvalue(res, i, 2));
        item.tracked = QString::fromUtf8(PQgetvalue(res, i, 3)) == "t";
        result.append(item);
    }
    freeResult(res);
    return result;
}

QVector<Item> Database::allItems() {
    QVector<Item> result;
    PGresult* res = query("SELECT id, category, name_ru, tracked FROM items ORDER BY name_ru");
    if (!res) return result;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        Item item;
        item.id = QString::fromUtf8(PQgetvalue(res, i, 0));
        item.category = QString::fromUtf8(PQgetvalue(res, i, 1));
        item.nameRu = QString::fromUtf8(PQgetvalue(res, i, 2));
        item.tracked = QString::fromUtf8(PQgetvalue(res, i, 3)) == "t";
        result.append(item);
    }
    freeResult(res);
    return result;
}

QVector<Item> Database::searchItems(const QString& queryStr) {
    QVector<Item> result;
    QString sql = QString(
        "SELECT id, category, name_ru, tracked FROM items "
        "WHERE LOWER(name_ru) LIKE LOWER('%%%1%%') OR LOWER(id) LIKE LOWER('%%%1%%') "
        "OR LOWER(category) LIKE LOWER('%%%1%%') "
        "ORDER BY name_ru LIMIT 200")
        .arg(escape(queryStr));

    PGresult* res = query(sql);
    if (!res) return result;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        Item item;
        item.id = QString::fromUtf8(PQgetvalue(res, i, 0));
        item.category = QString::fromUtf8(PQgetvalue(res, i, 1));
        item.nameRu = QString::fromUtf8(PQgetvalue(res, i, 2));
        item.tracked = QString::fromUtf8(PQgetvalue(res, i, 3)) == "t";
        result.append(item);
    }
    freeResult(res);
    return result;
}

bool Database::setItemTracked(const QString& itemId, bool tracked) {
    QString sql = QString("UPDATE items SET tracked = %1 WHERE id = '%2'")
        .arg(tracked ? "TRUE" : "FALSE", escape(itemId));
    return exec(sql);
}

// --- Lot Snapshots ---

bool Database::insertLotSnapshots(const QString& itemId, const QVector<Lot>& lots) {
    if (lots.isEmpty()) return true;

    exec("BEGIN");
    for (const auto& lot : lots) {
        QString sql = QString(
            "INSERT INTO lot_snapshots (item_id, buyout_price, start_price, start_time, snapshot_time) "
            "VALUES ('%1', %2, %3, '%4', NOW())")
            .arg(escape(itemId))
            .arg(lot.buyoutPrice)
            .arg(lot.startPrice)
            .arg(lot.startTime.toString(Qt::ISODate));
        if (!exec(sql)) {
            exec("ROLLBACK");
            return false;
        }
    }
    return exec("COMMIT");
}

// --- Price Snapshots ---

bool Database::insertPriceSnapshot(const PriceSnapshot& snap) {
    QString tsValue = snap.timestamp.isValid()
        ? QString("'%1'").arg(snap.timestamp.toString(Qt::ISODate))
        : "NOW()";

    QString sql = QString(
        "INSERT INTO price_snapshots "
        "(item_id, timestamp, min_price, avg_price, median_price, max_price, std_dev, lot_count, filtered_count) "
        "VALUES ('%1', %2, %3, %4, %5, %6, %7, %8, %9)")
        .arg(escape(snap.itemId))
        .arg(tsValue)
        .arg(snap.minPrice)
        .arg(snap.avgPrice)
        .arg(snap.medianPrice)
        .arg(snap.maxPrice)
        .arg(snap.stdDev)
        .arg(snap.lotCount)
        .arg(snap.filteredCount);
    return exec(sql);
}

QVector<PriceSnapshot> Database::priceHistory(const QString& itemId, int days) {
    QVector<PriceSnapshot> result;
    QString sql = QString(
        "SELECT id, item_id, timestamp, min_price, avg_price, median_price, max_price, "
        "std_dev, lot_count, filtered_count FROM price_snapshots "
        "WHERE item_id = '%1' AND timestamp >= NOW() - INTERVAL '%2 days' "
        "ORDER BY timestamp ASC")
        .arg(escape(itemId))
        .arg(days);

    PGresult* res = query(sql);
    if (!res) return result;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        PriceSnapshot s;
        s.id = QString::fromUtf8(PQgetvalue(res, i, 0)).toLongLong();
        s.itemId = QString::fromUtf8(PQgetvalue(res, i, 1));
        s.timestamp = QDateTime::fromString(QString::fromUtf8(PQgetvalue(res, i, 2)), Qt::ISODate);
        s.minPrice = QString::fromUtf8(PQgetvalue(res, i, 3)).toLongLong();
        s.avgPrice = QString::fromUtf8(PQgetvalue(res, i, 4)).toLongLong();
        s.medianPrice = QString::fromUtf8(PQgetvalue(res, i, 5)).toLongLong();
        s.maxPrice = QString::fromUtf8(PQgetvalue(res, i, 6)).toLongLong();
        s.stdDev = QString::fromUtf8(PQgetvalue(res, i, 7)).toDouble();
        s.lotCount = QString::fromUtf8(PQgetvalue(res, i, 8)).toInt();
        s.filteredCount = QString::fromUtf8(PQgetvalue(res, i, 9)).toInt();
        result.append(s);
    }
    freeResult(res);
    return result;
}

PriceSnapshot Database::latestPriceSnapshot(const QString& itemId) {
    PriceSnapshot s;
    QString sql = QString(
        "SELECT id, item_id, timestamp, min_price, avg_price, median_price, max_price, "
        "std_dev, lot_count, filtered_count FROM price_snapshots "
        "WHERE item_id = '%1' ORDER BY timestamp DESC LIMIT 1")
        .arg(escape(itemId));

    PGresult* res = query(sql);
    if (!res) return s;

    if (PQntuples(res) > 0) {
        s.id = QString::fromUtf8(PQgetvalue(res, 0, 0)).toLongLong();
        s.itemId = QString::fromUtf8(PQgetvalue(res, 0, 1));
        s.timestamp = QDateTime::fromString(QString::fromUtf8(PQgetvalue(res, 0, 2)), Qt::ISODate);
        s.minPrice = QString::fromUtf8(PQgetvalue(res, 0, 3)).toLongLong();
        s.avgPrice = QString::fromUtf8(PQgetvalue(res, 0, 4)).toLongLong();
        s.medianPrice = QString::fromUtf8(PQgetvalue(res, 0, 5)).toLongLong();
        s.maxPrice = QString::fromUtf8(PQgetvalue(res, 0, 6)).toLongLong();
        s.stdDev = QString::fromUtf8(PQgetvalue(res, 0, 7)).toDouble();
        s.lotCount = QString::fromUtf8(PQgetvalue(res, 0, 8)).toInt();
        s.filteredCount = QString::fromUtf8(PQgetvalue(res, 0, 9)).toInt();
    }
    freeResult(res);
    return s;
}

// --- Hourly Stats ---

bool Database::upsertHourlyStat(const QString& itemId, int hour, qint64 avgPrice, int sampleCount) {
    QString sql = QString(
        "INSERT INTO hourly_stats (item_id, hour, avg_price, sample_count, updated_at) "
        "VALUES ('%1', %2, %3, %4, NOW()) "
        "ON CONFLICT (item_id, hour) DO UPDATE SET "
        "avg_price = (hourly_stats.avg_price * hourly_stats.sample_count + EXCLUDED.avg_price) "
        "/ (hourly_stats.sample_count + 1), "
        "sample_count = hourly_stats.sample_count + EXCLUDED.sample_count, "
        "updated_at = NOW()")
        .arg(escape(itemId))
        .arg(hour)
        .arg(avgPrice)
        .arg(sampleCount);
    return exec(sql);
}

QVector<std::pair<int, qint64>> Database::hourlyStats(const QString& itemId) {
    QVector<std::pair<int, qint64>> result;
    QString sql = QString(
        "SELECT hour, avg_price FROM hourly_stats WHERE item_id = '%1' ORDER BY hour")
        .arg(escape(itemId));

    PGresult* res = query(sql);
    if (!res) return result;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        int h = QString::fromUtf8(PQgetvalue(res, i, 0)).toInt();
        qint64 p = QString::fromUtf8(PQgetvalue(res, i, 1)).toLongLong();
        result.append({h, p});
    }
    freeResult(res);
    return result;
}

// --- Alerts ---

bool Database::insertAlert(const Alert& alert) {
    QString sql = QString(
        "INSERT INTO alerts (item_id, alert_type, rating, message) VALUES ('%1', '%2', %3, '%4')")
        .arg(escape(alert.itemId),
             escape(Alert::typeToString(alert.type)))
        .arg(alert.rating)
        .arg(escape(alert.message));
    bool ok = exec(sql);
    if (ok) emit alertInserted(alert);
    return ok;
}

QVector<Alert> Database::recentAlerts(int limit) {
    QVector<Alert> result;
    QString sql = QString(
        "SELECT a.id, a.item_id, a.alert_type, a.rating, a.message, a.created_at, "
        "COALESCE(i.name_ru, a.item_id) "
        "FROM alerts a LEFT JOIN items i ON a.item_id = i.id "
        "ORDER BY a.created_at DESC LIMIT %1")
        .arg(limit);

    PGresult* res = query(sql);
    if (!res) return result;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        Alert a;
        a.id = QString::fromUtf8(PQgetvalue(res, i, 0)).toLongLong();
        a.itemId = QString::fromUtf8(PQgetvalue(res, i, 1));
        a.type = Alert::stringToType(QString::fromUtf8(PQgetvalue(res, i, 2)));
        a.rating = QString::fromUtf8(PQgetvalue(res, i, 3)).toDouble();
        a.message = QString::fromUtf8(PQgetvalue(res, i, 4));
        a.createdAt = QDateTime::fromString(QString::fromUtf8(PQgetvalue(res, i, 5)), Qt::ISODate);
        a.itemName = QString::fromUtf8(PQgetvalue(res, i, 6));
        result.append(a);
    }
    freeResult(res);
    return result;
}

QString Database::itemName(const QString& itemId) {
    QString sql = QString("SELECT name_ru FROM items WHERE id = '%1'").arg(escape(itemId));
    PGresult* res = query(sql);
    if (!res) return itemId;

    QString name = itemId;
    if (PQntuples(res) > 0 && !PQgetisnull(res, 0, 0)) {
        name = QString::fromUtf8(PQgetvalue(res, 0, 0));
    }
    freeResult(res);
    return name;
}

// --- Aggregation helpers ---

int Database::daysSinceFirstSnapshot(const QString& itemId) {
    QString sql = QString(
        "SELECT EXTRACT(DAY FROM NOW() - MIN(timestamp))::INT FROM price_snapshots WHERE item_id = '%1'")
        .arg(escape(itemId));

    PGresult* res = query(sql);
    if (!res) return 0;

    int days = 0;
    if (PQntuples(res) > 0 && !PQgetisnull(res, 0, 0)) {
        days = QString::fromUtf8(PQgetvalue(res, 0, 0)).toInt();
    }
    freeResult(res);
    return days;
}

double Database::overallAvgPrice(const QString& itemId, int days) {
    QString sql = QString(
        "SELECT AVG(avg_price)::DOUBLE PRECISION FROM price_snapshots "
        "WHERE item_id = '%1' AND timestamp >= NOW() - INTERVAL '%2 days'")
        .arg(escape(itemId))
        .arg(days);

    PGresult* res = query(sql);
    if (!res) return 0.0;

    double avg = 0.0;
    if (PQntuples(res) > 0 && !PQgetisnull(res, 0, 0)) {
        avg = QString::fromUtf8(PQgetvalue(res, 0, 0)).toDouble();
    }
    freeResult(res);
    return avg;
}

// --- Private helpers ---

bool Database::exec(const QString& sql) {
    if (!m_conn) return false;
    PGresult* res = PQexec(m_conn, sql.toUtf8().constData());
    ExecStatusType status = PQresultStatus(res);
    bool ok = (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK);
    if (!ok) {
        LOG_ERROR("SQL error: {}\nQuery: {}", PQerrorMessage(m_conn), sql.toStdString());
    }
    PQclear(res);
    return ok;
}

PGresult* Database::query(const QString& sql) {
    if (!m_conn) return nullptr;
    PGresult* res = PQexec(m_conn, sql.toUtf8().constData());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        LOG_ERROR("SQL query error: {}\nQuery: {}", PQerrorMessage(m_conn), sql.toStdString());
        PQclear(res);
        return nullptr;
    }
    return res;
}

void Database::freeResult(PGresult* res) {
    if (res) PQclear(res);
}

QString Database::escape(const QString& str) {
    QString result = str;
    result.replace("'", "''");
    return result;
}
