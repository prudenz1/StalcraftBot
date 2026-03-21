#include "ApiClient.h"
#include "Config.h"
#include "utils/Logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>

ApiClient::ApiClient(Config* config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_nam(new QNetworkAccessManager(this))
{
}

QNetworkRequest ApiClient::makeRequest(const QString& path, const QUrlQuery& params) const {
    QUrl url(m_config->apiBaseUrl() + path);
    if (!params.isEmpty()) {
        url.setQuery(params);
    }

    QNetworkRequest req(url);
    req.setRawHeader("Client-Id", m_config->clientId().toUtf8());
    req.setRawHeader("Client-Secret", m_config->clientSecret().toUtf8());
    req.setRawHeader("Content-Type", "application/json");
    return req;
}

void ApiClient::fetchLots(const QString& itemId, int offset, int limit,
                          const QString& sort, const QString& order) {
    QString path = QString("/%1/auction/%2/lots")
        .arg(m_config->region(), itemId);

    QUrlQuery params;
    params.addQueryItem("offset", QString::number(offset));
    params.addQueryItem("limit", QString::number(limit));
    params.addQueryItem("sort", sort);
    params.addQueryItem("order", order);

    QNetworkRequest req = makeRequest(path, params);
    QNetworkReply* reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, itemId]() {
        handleLotsReply(reply, itemId);
    });

    LOG_DEBUG("Fetching lots for item {}: {}", itemId.toStdString(),
              req.url().toString().toStdString());
}

void ApiClient::fetchPriceHistory(const QString& itemId, int offset, int limit) {
    QString path = QString("/%1/auction/%2/history")
        .arg(m_config->region(), itemId);

    QUrlQuery params;
    params.addQueryItem("offset", QString::number(offset));
    params.addQueryItem("limit", QString::number(limit));

    QNetworkRequest req = makeRequest(path, params);
    QNetworkReply* reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, itemId]() {
        handlePriceHistoryReply(reply, itemId);
    });

    LOG_DEBUG("Fetching price history for item {}", itemId.toStdString());
}

void ApiClient::handleLotsReply(QNetworkReply* reply, const QString& itemId) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        LOG_ERROR("API error fetching lots for {}: {}", itemId.toStdString(), err.toStdString());
        emit apiError(itemId, err);
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    int total = root["total"].toInt();
    QJsonArray lotsArr = root["lots"].toArray();

    QVector<Lot> lots;
    lots.reserve(lotsArr.size());

    for (const auto& val : lotsArr) {
        QJsonObject obj = val.toObject();
        Lot lot;
        lot.itemId = itemId;
        lot.startPrice = obj["startPrice"].toVariant().toLongLong();
        lot.buyoutPrice = obj["buyoutPrice"].toVariant().toLongLong();

        QString timeStr = obj["startTime"].toString();
        lot.startTime = QDateTime::fromString(timeStr, Qt::ISODate);
        lot.snapshotTime = QDateTime::currentDateTimeUtc();

        lots.append(lot);
    }

    LOG_INFO("Fetched {} lots for item {} (total: {})",
             lots.size(), itemId.toStdString(), total);
    emit lotsFetched(itemId, lots, total);
}

void ApiClient::handlePriceHistoryReply(QNetworkReply* reply, const QString& itemId) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        LOG_ERROR("API error fetching price history for {}: {}",
                  itemId.toStdString(), err.toStdString());
        emit apiError(itemId, err);
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    int total = root["total"].toInt();
    QJsonArray pricesArr = root["prices"].toArray();

    QVector<PriceHistoryEntry> entries;
    entries.reserve(pricesArr.size());

    for (const auto& val : pricesArr) {
        QJsonObject obj = val.toObject();
        PriceHistoryEntry entry;
        entry.amount = obj["amount"].toVariant().toLongLong();
        entry.price = obj["price"].toVariant().toLongLong();
        entry.time = QDateTime::fromString(obj["time"].toString(), Qt::ISODate);
        entry.additional = obj["additional"].toString();
        entries.append(entry);
    }

    LOG_INFO("Fetched {} price history entries for item {}",
             entries.size(), itemId.toStdString());
    emit priceHistoryFetched(itemId, entries, total);
}
