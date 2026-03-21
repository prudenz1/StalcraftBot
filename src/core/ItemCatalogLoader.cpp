#include "ItemCatalogLoader.h"
#include "Database.h"
#include "utils/Logger.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>

ItemCatalogLoader::ItemCatalogLoader(Database* db, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_nam(new QNetworkAccessManager(this))
{
}

Item ItemCatalogLoader::parseItemJson(const QByteArray& json) {
    Item item;
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (doc.isNull()) return item;

    QJsonObject root = doc.object();
    item.id = root["id"].toString();
    item.category = root["category"].toString();

    QJsonObject nameObj = root["name"].toObject();
    QJsonObject linesObj = nameObj["lines"].toObject();
    item.nameRu = linesObj["ru"].toString();

    if (item.nameRu.isEmpty()) {
        item.nameRu = nameObj["key"].toString();
    }

    return item;
}

QVector<Item> ItemCatalogLoader::parseItemFiles(const QString& dirPath) {
    QVector<Item> items;
    QDirIterator it(dirPath, {"*.json"}, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString filePath = it.next();
        if (filePath.contains("/_variants/") || filePath.contains("\\_variants\\")) continue;
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) continue;

        QByteArray data = file.readAll();
        file.close();

        Item item = parseItemJson(data);
        if (!item.id.isEmpty() && !item.nameRu.isEmpty()) {
            items.append(item);
        }
    }

    LOG_INFO("Parsed {} items from local directory: {}", items.size(), dirPath.toStdString());
    return items;
}

void ItemCatalogLoader::loadFromLocalDirectory(const QString& dirPath) {
    emit loadingStarted();

    QVector<Item> items = parseItemFiles(dirPath);
    if (items.isEmpty()) {
        emit loadingError("No items found in directory: " + dirPath);
        return;
    }

    int batchSize = 100;
    for (int i = 0; i < items.size(); i += batchSize) {
        int end = qMin(i + batchSize, items.size());
        QVector<Item> batch(items.begin() + i, items.begin() + end);
        m_db->upsertItems(batch);
        emit progressUpdated(end, items.size());
    }

    LOG_INFO("Loaded {} items into database", items.size());
    emit loadingFinished(items.size());
}

void ItemCatalogLoader::downloadFromGitHub() {
    emit loadingStarted();
    fetchGitHubTree();
}

void ItemCatalogLoader::fetchGitHubTree() {
    QUrl url("https://api.github.com/repos/EXBO-Studio/stalcraft-database/git/trees/main?recursive=1");
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/vnd.github.v3+json");
    req.setRawHeader("User-Agent", "StalcraftBot");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit loadingError("GitHub API error: " + reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray tree = doc.object()["tree"].toArray();

        QStringList itemPaths;
        for (const auto& node : tree) {
            QJsonObject obj = node.toObject();
            QString path = obj["path"].toString();
            if (path.startsWith("ru/items/") && path.endsWith(".json")
                && !path.contains("/_variants/")) {
                itemPaths.append(path);
            }
        }

        m_totalToFetch = itemPaths.size();
        m_fetchedCount = 0;
        m_pendingItems.clear();
        m_pendingItems.reserve(m_totalToFetch);

        LOG_INFO("Found {} item files on GitHub, downloading...", m_totalToFetch);

        for (int i = 0; i < itemPaths.size(); ++i) {
            QString rawUrl = QString("https://raw.githubusercontent.com/EXBO-Studio/stalcraft-database/main/%1")
                .arg(itemPaths[i]);
            fetchItemFile(rawUrl, i, m_totalToFetch);
        }
    });
}

void ItemCatalogLoader::fetchItemFile(const QString& url, int index, int total) {
    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("User-Agent", "StalcraftBot");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, total]() {
        reply->deleteLater();
        m_fetchedCount++;

        if (reply->error() == QNetworkReply::NoError) {
            Item item = parseItemJson(reply->readAll());
            if (!item.id.isEmpty() && !item.nameRu.isEmpty()) {
                m_pendingItems.append(item);
            }
        }

        emit progressUpdated(m_fetchedCount, total);

        if (m_fetchedCount >= total) {
            m_db->upsertItems(m_pendingItems);
            LOG_INFO("Downloaded and saved {} items from GitHub", m_pendingItems.size());
            emit loadingFinished(m_pendingItems.size());
        }
    });
}
