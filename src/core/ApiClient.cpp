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
    const QString tok = m_config->bearerToken();
    if (!tok.isEmpty()) {
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + tok.toUtf8());
    }
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
    // Как в открытых парсерах (напр. stalcraft-auction-parser): может влиять на выдачу/пагинацию.
    params.addQueryItem("additional", QStringLiteral("true"));

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

// Обработка ответа от API на запрос истории цен (GET /{region}/auction/{itemId}/history).
// Парсит JSON с завершёнными сделками и отправляет результат через сигнал priceHistoryFetched.
void ApiClient::handlePriceHistoryReply(QNetworkReply* reply, const QString& itemId) {
    reply->deleteLater(); // Qt освободит объект reply после выхода из event loop

    // Проверка HTTP-ошибок (таймаут, 404, 500 и т.д.)
    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        LOG_ERROR("API error fetching price history for {}: {}",
                  itemId.toStdString(), err.toStdString());
        emit apiError(itemId, err);
        return;
    }

    // Парсинг JSON-ответа. Формат API:
    // { "total": 43600, "prices": [ { "amount": 1, "price": 50000, "time": "...", "additional": "..." }, ... ] }
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    int total = root["total"].toInt();         // Общее кол-во записей на сервере (для пагинации)
    QJsonArray pricesArr = root["prices"].toArray(); // Массив сделок на текущей странице

    QVector<PriceHistoryEntry> entries;
    entries.reserve(pricesArr.size());

    // Преобразование каждого JSON-объекта в структуру PriceHistoryEntry
    for (const auto& val : pricesArr) {
        QJsonObject obj = val.toObject();
        PriceHistoryEntry entry;
        entry.amount = obj["amount"].toVariant().toLongLong();  // Количество предметов в сделке
        entry.price = obj["price"].toVariant().toLongLong();    // Цена продажи
        QString timeStr = obj["time"].toString();
        entry.time = QDateTime::fromString(timeStr, Qt::ISODate);
        if (!entry.time.isValid()) {
            entry.time = QDateTime::fromString(timeStr, Qt::ISODateWithMs);
        }
        QJsonValue addVal = obj["additional"];
        if (addVal.isObject()) {
            entry.additional = QString::fromUtf8(
                QJsonDocument(addVal.toObject()).toJson(QJsonDocument::Compact));
        } else {
            entry.additional = addVal.toString();
        }
        entries.append(entry);
    }

    LOG_INFO("Fetched {} price history entries for item {}",
             entries.size(), itemId.toStdString());
    // Сигнал подхватывает ItemManagerWidget::onHistoryPageReceived для пагинации и сохранения в БД
    emit priceHistoryFetched(itemId, entries, total);
}
