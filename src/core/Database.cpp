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
    // `Database` хранит настройки БД и предоставляет методы доступа к данным:
    // миграции схемы, CRUD трекинга и выборки истории цен/алертов.
}

// Разрывает соединение и снимает подписки (через `disconnect()`).
Database::~Database() {
    disconnect();
}

// Подключается к PostgreSQL по параметрам из `Config`.
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

// Закрывает активное соединение с БД (если оно было открыто).
void Database::disconnect() {
    if (m_conn) {
        PQfinish(m_conn);
        m_conn = nullptr;
        LOG_INFO("Disconnected from PostgreSQL");
    }
}

// Быстрая проверка: соединение есть и работает (CONNECTION_OK).
bool Database::isConnected() const {
    return m_conn && PQstatus(m_conn) == CONNECTION_OK;
}

// Применяет миграцию схемы: читает `sql/schema.sql` или выполняет inline SQL,
// затем пытается преобразовать старую структуру через `migrateFromV1()`.
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
                name_ru VARCHAR(256) NOT NULL
            );
            CREATE TABLE IF NOT EXISTS tracked_items (
                id SERIAL PRIMARY KEY,
                item_id VARCHAR(16) NOT NULL REFERENCES items(id),
                quality SMALLINT NOT NULL DEFAULT -1,
                UNIQUE(item_id, quality)
            );
            CREATE TABLE IF NOT EXISTS lot_snapshots (
                id BIGSERIAL PRIMARY KEY,
                item_id VARCHAR(16) REFERENCES items(id),
                quality SMALLINT NOT NULL DEFAULT -1,
                buyout_price BIGINT,
                start_price BIGINT,
                start_time TIMESTAMPTZ,
                snapshot_time TIMESTAMPTZ NOT NULL DEFAULT NOW()
            );
            CREATE INDEX IF NOT EXISTS idx_lot_snapshots_item_time ON lot_snapshots(item_id, snapshot_time);
            CREATE TABLE IF NOT EXISTS price_snapshots (
                id BIGSERIAL PRIMARY KEY,
                item_id VARCHAR(16) REFERENCES items(id),
                quality SMALLINT NOT NULL DEFAULT -1,
                timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                min_price BIGINT, avg_price BIGINT, median_price BIGINT, max_price BIGINT,
                std_dev DOUBLE PRECISION, lot_count INT, filtered_count INT
            );
            CREATE INDEX IF NOT EXISTS idx_price_snapshots_item_time ON price_snapshots(item_id, timestamp);
            CREATE TABLE IF NOT EXISTS hourly_stats (
                id BIGSERIAL PRIMARY KEY,
                item_id VARCHAR(16) REFERENCES items(id),
                quality SMALLINT NOT NULL DEFAULT -1,
                hour SMALLINT NOT NULL CHECK (hour BETWEEN 0 AND 23),
                avg_price BIGINT, sample_count INT,
                updated_at TIMESTAMPTZ DEFAULT NOW(),
                UNIQUE(item_id, quality, hour)
            );
            CREATE TABLE IF NOT EXISTS alerts (
                id BIGSERIAL PRIMARY KEY,
                item_id VARCHAR(16) REFERENCES items(id),
                quality SMALLINT NOT NULL DEFAULT -1,
                alert_type VARCHAR(16) NOT NULL,
                rating DOUBLE PRECISION,
                message TEXT,
                created_at TIMESTAMPTZ DEFAULT NOW()
            );
            CREATE INDEX IF NOT EXISTS idx_alerts_item_time ON alerts(item_id, created_at);
        )");
    } else {
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        QString sql = in.readAll();
        file.close();

        if (exec(sql)) {
            LOG_INFO("Database schema applied");
        } else {
            LOG_ERROR("Database schema failed");
        }
    }

    migrateFromV1();
    LOG_INFO("Database migration completed");
}

// Миграция из версии 1 в текущий формат:
// перенос `items.tracked` в `tracked_items`, расширение таблиц колонкой `quality`
// и перестройка уникальных ограничений.
void Database::migrateFromV1() {
    // tracked_items table (idempotent)
    exec("CREATE TABLE IF NOT EXISTS tracked_items ("
         "id SERIAL PRIMARY KEY, "
         "item_id VARCHAR(16) NOT NULL REFERENCES items(id), "
         "quality SMALLINT NOT NULL DEFAULT -1, "
         "UNIQUE(item_id, quality))");

    // Migrate existing tracked items from old items.tracked column
    exec("DO $$ BEGIN "
         "IF EXISTS (SELECT 1 FROM information_schema.columns "
         "WHERE table_name='items' AND column_name='tracked') THEN "
         "INSERT INTO tracked_items (item_id, quality) "
         "SELECT id, -1 FROM items WHERE tracked = TRUE "
         "ON CONFLICT DO NOTHING; "
         "ALTER TABLE items DROP COLUMN tracked; "
         "END IF; END $$");

    // Add quality columns to existing tables (idempotent)
    exec("ALTER TABLE lot_snapshots ADD COLUMN IF NOT EXISTS quality SMALLINT NOT NULL DEFAULT -1");
    exec("ALTER TABLE price_snapshots ADD COLUMN IF NOT EXISTS quality SMALLINT NOT NULL DEFAULT -1");
    exec("ALTER TABLE hourly_stats ADD COLUMN IF NOT EXISTS quality SMALLINT NOT NULL DEFAULT -1");
    exec("ALTER TABLE alerts ADD COLUMN IF NOT EXISTS quality SMALLINT NOT NULL DEFAULT -1");

    // Update hourly_stats unique constraint: (item_id, hour) -> (item_id, quality, hour)
    exec("DO $$ BEGIN "
         "IF EXISTS (SELECT 1 FROM pg_constraint WHERE conname='hourly_stats_item_id_hour_key') THEN "
         "ALTER TABLE hourly_stats DROP CONSTRAINT hourly_stats_item_id_hour_key; "
         "END IF; END $$");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_hourly_stats_item_quality_hour "
         "ON hourly_stats(item_id, quality, hour)");
}

// --- Items catalog ---

// Вставляет или обновляет запись предмета в таблице `items`.
bool Database::upsertItem(const Item& item) {
    QString sql = QString(
        "INSERT INTO items (id, category, name_ru) "
        "VALUES ('%1', '%2', '%3') "
        "ON CONFLICT (id) DO UPDATE SET category=EXCLUDED.category, name_ru=EXCLUDED.name_ru")
        .arg(escape(item.id), escape(item.category), escape(item.nameRu));
    return exec(sql);
}

// Пакетно обновляет список предметов в транзакции:
// если какая-то операция не удалась — откатывает `ROLLBACK`.
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

// Возвращает все предметы отсортированные по русскому названию.
QVector<Item> Database::allItems() {
    QVector<Item> result;
    PGresult* res = query("SELECT id, category, name_ru FROM items ORDER BY name_ru");
    if (!res) return result;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        Item item;
        item.id = QString::fromUtf8(PQgetvalue(res, i, 0));
        item.category = QString::fromUtf8(PQgetvalue(res, i, 1));
        item.nameRu = QString::fromUtf8(PQgetvalue(res, i, 2));
        result.append(item);
    }
    freeResult(res);
    return result;
}

// Ищет предметы по подстроке в названии/ID/категории,
// а также помечает, есть ли у предмета активный трекинг.
QVector<Item> Database::searchItems(const QString& queryStr) {
    QVector<Item> result;
    QString sql = QString(
        "SELECT i.id, i.category, i.name_ru, "
        "EXISTS(SELECT 1 FROM tracked_items ti WHERE ti.item_id = i.id) "
        "FROM items i "
        "WHERE LOWER(i.name_ru) LIKE LOWER('%%%1%%') OR LOWER(i.id) LIKE LOWER('%%%1%%') "
        "OR LOWER(i.category) LIKE LOWER('%%%1%%') "
        "ORDER BY i.name_ru LIMIT 200")
        .arg(escape(queryStr));

    PGresult* res = query(sql);
    if (!res) return result;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        Item item;
        item.id = QString::fromUtf8(PQgetvalue(res, i, 0));
        item.category = QString::fromUtf8(PQgetvalue(res, i, 1));
        item.nameRu = QString::fromUtf8(PQgetvalue(res, i, 2));
        item.hasTracking = QString::fromUtf8(PQgetvalue(res, i, 3)) == "t";
        result.append(item);
    }
    freeResult(res);
    return result;
}

// --- Tracking ---

// Возвращает все трекинги: предмет + trackingId + quality.
QVector<Item> Database::trackedItems() {
    QVector<Item> result;
    PGresult* res = query(
        "SELECT i.id, i.category, i.name_ru, ti.id, ti.quality "
        "FROM tracked_items ti JOIN items i ON ti.item_id = i.id "
        "ORDER BY i.name_ru, ti.quality");
    if (!res) return result;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        Item item;
        item.id = QString::fromUtf8(PQgetvalue(res, i, 0));
        item.category = QString::fromUtf8(PQgetvalue(res, i, 1));
        item.nameRu = QString::fromUtf8(PQgetvalue(res, i, 2));
        item.trackingId = QString::fromUtf8(PQgetvalue(res, i, 3)).toInt();
        item.quality = QString::fromUtf8(PQgetvalue(res, i, 4)).toInt();
        item.hasTracking = true;
        result.append(item);
    }
    freeResult(res);
    return result;
}

// Добавляет трекинг конкретного item_id и quality (уникальность задаётся `(item_id, quality)`).
bool Database::addTracking(const QString& itemId, int quality) {
    QString sql = QString(
        "INSERT INTO tracked_items (item_id, quality) VALUES ('%1', %2) "
        "ON CONFLICT (item_id, quality) DO NOTHING")
        .arg(escape(itemId)).arg(quality);
    return exec(sql);
}

// Удаляет трекинг по его `id` в таблице `tracked_items`.
bool Database::removeTracking(int trackingId) {
    QString sql = QString("DELETE FROM tracked_items WHERE id = %1").arg(trackingId);
    return exec(sql);
}

// Удаляет все трекинги для конкретного item_id.
bool Database::removeAllTracking(const QString& itemId) {
    QString sql = QString("DELETE FROM tracked_items WHERE item_id = '%1'")
        .arg(escape(itemId));
    return exec(sql);
}

// Проверяет, существует ли любой трекинг для item_id (включая quality=-1).
bool Database::hasTracking(const QString& itemId) {
    PGresult* res = query(QString(
        "SELECT 1 FROM tracked_items WHERE item_id = '%1' LIMIT 1")
        .arg(escape(itemId)));
    if (!res) return false;
    bool found = PQntuples(res) > 0;
    freeResult(res);
    return found;
}

// --- Lot Snapshots ---

// Сохраняет историю лотов (raw данные) в `lot_snapshots` в рамках транзакции.
bool Database::insertLotSnapshots(const QString& itemId, const QVector<Lot>& lots) {
    if (lots.isEmpty()) return true;

    exec("BEGIN");
    for (const auto& lot : lots) {
        QString sql = QString(
            "INSERT INTO lot_snapshots (item_id, quality, buyout_price, start_price, start_time, snapshot_time) "
            "VALUES ('%1', %2, %3, %4, '%5', NOW())")
            .arg(escape(itemId))
            .arg(lot.quality)
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

// Сохраняет агрегированный снапшот цены в `price_snapshots`.
bool Database::insertPriceSnapshot(const PriceSnapshot& snap) {
    QString tsValue = snap.timestamp.isValid()
        ? QString("'%1'").arg(snap.timestamp.toString(Qt::ISODate))
        : "NOW()";

    QString sql = QString(
        "INSERT INTO price_snapshots "
        "(item_id, quality, timestamp, min_price, avg_price, median_price, max_price, "
        "std_dev, lot_count, filtered_count) "
        "VALUES ('%1', %2, %3, %4, %5, %6, %7, %8, %9, %10)")
        .arg(escape(snap.itemId))
        .arg(snap.quality)
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

// Возвращает агрегированную историю цены за `days` дней для конкретного `itemId`/`quality`.
QVector<PriceSnapshot> Database::priceHistory(const QString& itemId, int quality, int days) {
    QVector<PriceSnapshot> result;
    QString sql = QString(
        "SELECT id, item_id, quality, timestamp, min_price, avg_price, median_price, max_price, "
        "std_dev, lot_count, filtered_count FROM price_snapshots "
        "WHERE item_id = '%1' AND quality = %2 AND timestamp >= NOW() - INTERVAL '%3 days' "
        "ORDER BY timestamp ASC")
        .arg(escape(itemId))
        .arg(quality)
        .arg(days);

    PGresult* res = query(sql);
    if (!res) return result;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        PriceSnapshot s;
        s.id = QString::fromUtf8(PQgetvalue(res, i, 0)).toLongLong();
        s.itemId = QString::fromUtf8(PQgetvalue(res, i, 1));
        s.quality = QString::fromUtf8(PQgetvalue(res, i, 2)).toInt();
        s.timestamp = QDateTime::fromString(QString::fromUtf8(PQgetvalue(res, i, 3)), Qt::ISODate);
        s.minPrice = QString::fromUtf8(PQgetvalue(res, i, 4)).toLongLong();
        s.avgPrice = QString::fromUtf8(PQgetvalue(res, i, 5)).toLongLong();
        s.medianPrice = QString::fromUtf8(PQgetvalue(res, i, 6)).toLongLong();
        s.maxPrice = QString::fromUtf8(PQgetvalue(res, i, 7)).toLongLong();
        s.stdDev = QString::fromUtf8(PQgetvalue(res, i, 8)).toDouble();
        s.lotCount = QString::fromUtf8(PQgetvalue(res, i, 9)).toInt();
        s.filteredCount = QString::fromUtf8(PQgetvalue(res, i, 10)).toInt();
        result.append(s);
    }
    freeResult(res);
    return result;
}

// Возвращает последний (самый свежий по timestamp) снапшот цены для itemId/quality.
PriceSnapshot Database::latestPriceSnapshot(const QString& itemId, int quality) {
    PriceSnapshot s;
    QString sql = QString(
        "SELECT id, item_id, quality, timestamp, min_price, avg_price, median_price, max_price, "
        "std_dev, lot_count, filtered_count FROM price_snapshots "
        "WHERE item_id = '%1' AND quality = %2 ORDER BY timestamp DESC LIMIT 1")
        .arg(escape(itemId))
        .arg(quality);

    PGresult* res = query(sql);
    if (!res) return s;

    if (PQntuples(res) > 0) {
        s.id = QString::fromUtf8(PQgetvalue(res, 0, 0)).toLongLong();
        s.itemId = QString::fromUtf8(PQgetvalue(res, 0, 1));
        s.quality = QString::fromUtf8(PQgetvalue(res, 0, 2)).toInt();
        s.timestamp = QDateTime::fromString(QString::fromUtf8(PQgetvalue(res, 0, 3)), Qt::ISODate);
        s.minPrice = QString::fromUtf8(PQgetvalue(res, 0, 4)).toLongLong();
        s.avgPrice = QString::fromUtf8(PQgetvalue(res, 0, 5)).toLongLong();
        s.medianPrice = QString::fromUtf8(PQgetvalue(res, 0, 6)).toLongLong();
        s.maxPrice = QString::fromUtf8(PQgetvalue(res, 0, 7)).toLongLong();
        s.stdDev = QString::fromUtf8(PQgetvalue(res, 0, 8)).toDouble();
        s.lotCount = QString::fromUtf8(PQgetvalue(res, 0, 9)).toInt();
        s.filteredCount = QString::fromUtf8(PQgetvalue(res, 0, 10)).toInt();
    }
    freeResult(res);
    return s;
}

// --- Hourly Stats ---

// Обновляет/вставляет агрегат `hourly_stats` для заданного часа.
// При конфликте усредняет старое и новое по количеству семплов.
bool Database::upsertHourlyStat(const QString& itemId, int quality, int hour,
                                qint64 avgPrice, int sampleCount) {
    QString sql = QString(
        "INSERT INTO hourly_stats (item_id, quality, hour, avg_price, sample_count, updated_at) "
        "VALUES ('%1', %2, %3, %4, %5, NOW()) "
        "ON CONFLICT (item_id, quality, hour) DO UPDATE SET "
        "avg_price = (hourly_stats.avg_price * hourly_stats.sample_count + EXCLUDED.avg_price) "
        "/ (hourly_stats.sample_count + 1), "
        "sample_count = hourly_stats.sample_count + EXCLUDED.sample_count, "
        "updated_at = NOW()")
        .arg(escape(itemId))
        .arg(quality)
        .arg(hour)
        .arg(avgPrice)
        .arg(sampleCount);
    return exec(sql);
}

// Возвращает список (hour -> avg_price) для конкретного itemId/quality.
QVector<std::pair<int, qint64>> Database::hourlyStats(const QString& itemId, int quality) {
    QVector<std::pair<int, qint64>> result;
    QString sql = QString(
        "SELECT hour, avg_price FROM hourly_stats "
        "WHERE item_id = '%1' AND quality = %2 ORDER BY hour")
        .arg(escape(itemId))
        .arg(quality);

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

// Создаёт алерт и, если SQL успешен, эмитит `alertInserted`.
bool Database::insertAlert(const Alert& alert) {
    QString sql = QString(
        "INSERT INTO alerts (item_id, quality, alert_type, rating, message) "
        "VALUES ('%1', %2, '%3', %4, '%5')")
        .arg(escape(alert.itemId))
        .arg(alert.quality)
        .arg(escape(Alert::typeToString(alert.type)))
        .arg(alert.rating)
        .arg(escape(alert.message));
    bool ok = exec(sql);
    if (ok) emit alertInserted(alert);
    return ok;
}

// Возвращает последние `limit` алертов с данными предмета (если он известен).
QVector<Alert> Database::recentAlerts(int limit) {
    QVector<Alert> result;
    QString sql = QString(
        "SELECT a.id, a.item_id, a.quality, a.alert_type, a.rating, a.message, a.created_at, "
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
        a.quality = QString::fromUtf8(PQgetvalue(res, i, 2)).toInt();
        a.type = Alert::stringToType(QString::fromUtf8(PQgetvalue(res, i, 3)));
        a.rating = QString::fromUtf8(PQgetvalue(res, i, 4)).toDouble();
        a.message = QString::fromUtf8(PQgetvalue(res, i, 5));
        a.createdAt = QDateTime::fromString(QString::fromUtf8(PQgetvalue(res, i, 6)), Qt::ISODate);
        a.itemName = QString::fromUtf8(PQgetvalue(res, i, 7));
        result.append(a);
    }
    freeResult(res);
    return result;
}

// Возвращает имя предмета из таблицы `items`.
// Если предмета нет в БД — возвращает входной `itemId`.
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

// Сколько дней прошло с даты первого price_snapshots для itemId/quality.
int Database::daysSinceFirstSnapshot(const QString& itemId, int quality) {
    QString sql = QString(
        "SELECT EXTRACT(DAY FROM NOW() - MIN(timestamp))::INT FROM price_snapshots "
        "WHERE item_id = '%1' AND quality = %2")
        .arg(escape(itemId))
        .arg(quality);

    PGresult* res = query(sql);
    if (!res) return 0;

    int days = 0;
    if (PQntuples(res) > 0 && !PQgetisnull(res, 0, 0)) {
        days = QString::fromUtf8(PQgetvalue(res, 0, 0)).toInt();
    }
    freeResult(res);
    return days;
}

// Средняя величина `avg_price` за последние `days` дней.
double Database::overallAvgPrice(const QString& itemId, int quality, int days) {
    QString sql = QString(
        "SELECT AVG(avg_price)::DOUBLE PRECISION FROM price_snapshots "
        "WHERE item_id = '%1' AND quality = %2 AND timestamp >= NOW() - INTERVAL '%3 days'")
        .arg(escape(itemId))
        .arg(quality)
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

// Выполняет SQL, который не возвращает набор строк (команды CREATE/INSERT/DELETE).
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

// Выполняет SQL-запрос, который возвращает таблицу строк (SELECT).
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

// Освобождает PGresult (обёртка над `PQclear`).
void Database::freeResult(PGresult* res) {
    if (res) PQclear(res);
}

// Экранирует строку для вставки в SQL (заменяет `'` на `''`).
QString Database::escape(const QString& str) {
    QString result = str;
    result.replace("'", "''");
    return result;
}
