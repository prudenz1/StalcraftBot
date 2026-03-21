#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QVector>
#include "models/Item.h"

class Database;

class ItemCatalogLoader : public QObject {
    Q_OBJECT
public:
    explicit ItemCatalogLoader(Database* db, QObject* parent = nullptr);

    void loadFromLocalDirectory(const QString& dirPath);
    void downloadFromGitHub();

    static QVector<Item> parseItemFiles(const QString& dirPath);
    static Item parseItemJson(const QByteArray& json);

signals:
    void loadingStarted();
    void progressUpdated(int current, int total);
    void loadingFinished(int itemCount);
    void loadingError(const QString& error);

private:
    void fetchGitHubTree();
    void fetchItemFile(const QString& url, int index, int total);

    Database* m_db;
    QNetworkAccessManager* m_nam;
    QVector<Item> m_pendingItems;
    int m_fetchedCount = 0;
    int m_totalToFetch = 0;
};
