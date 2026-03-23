#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QVector>

#include "models/Lot.h"

class Config;

struct PriceHistoryEntry {
    qint64 amount = 0;
    qint64 price = 0;
    QDateTime time;
    int quality = -1;
    QString additional;
};

class ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(Config* config, QObject* parent = nullptr);

    void fetchLots(const QString& itemId, int offset = 0, int limit = 100,
                   const QString& sort = "BUYOUT_PRICE", const QString& order = "ASC");
    void fetchPriceHistory(const QString& itemId, int offset = 0, int limit = 200);

signals:
    void lotsFetched(const QString& itemId, const QVector<Lot>& lots, int total);
    void priceHistoryFetched(const QString& itemId, const QVector<PriceHistoryEntry>& entries, int total);
    void apiError(const QString& itemId, const QString& error);

private:
    QNetworkRequest makeRequest(const QString& path, const QUrlQuery& params = {}) const;
    void handleLotsReply(QNetworkReply* reply, const QString& itemId);
    void handlePriceHistoryReply(QNetworkReply* reply, const QString& itemId);

    Config* m_config;
    QNetworkAccessManager* m_nam;
};
