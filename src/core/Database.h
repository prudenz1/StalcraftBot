#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <libpq-fe.h>

#include "models/Item.h"
#include "models/Lot.h"
#include "models/PriceSnapshot.h"
#include "models/Alert.h"

class Config;

class Database : public QObject {
    Q_OBJECT
public:
    explicit Database(Config* config, QObject* parent = nullptr);
    ~Database();

    bool connect();
    void disconnect();
    bool isConnected() const;
    void migrate();

    // Items
    bool upsertItem(const Item& item);
    bool upsertItems(const QVector<Item>& items);
    QVector<Item> trackedItems();
    QVector<Item> allItems();
    QVector<Item> searchItems(const QString& query);
    bool setItemTracked(const QString& itemId, bool tracked);

    // Lot Snapshots
    bool insertLotSnapshots(const QString& itemId, const QVector<Lot>& lots);

    // Price Snapshots
    bool insertPriceSnapshot(const PriceSnapshot& snap);
    QVector<PriceSnapshot> priceHistory(const QString& itemId, int days);
    PriceSnapshot latestPriceSnapshot(const QString& itemId);

    // Hourly Stats
    bool upsertHourlyStat(const QString& itemId, int hour, qint64 avgPrice, int sampleCount);
    QVector<std::pair<int, qint64>> hourlyStats(const QString& itemId);

    // Alerts
    bool insertAlert(const Alert& alert);
    QVector<Alert> recentAlerts(int limit = 100);

    // Helpers
    QString itemName(const QString& itemId);

    // Aggregation helper
    int daysSinceFirstSnapshot(const QString& itemId);
    double overallAvgPrice(const QString& itemId, int days);

signals:
    void alertInserted(const Alert& alert);

private:
    bool exec(const QString& query);
    PGresult* query(const QString& sql);
    void freeResult(PGresult* res);
    QString escape(const QString& str);

    Config* m_config;
    PGconn* m_conn = nullptr;
};
