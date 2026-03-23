#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QVector>
#include <QDateTime>
#include <QMap>

#include "core/ApiClient.h"

class Database;
class ItemCatalogLoader;

class ItemManagerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ItemManagerWidget(Database* db, ApiClient* api, QWidget* parent = nullptr);

signals:
    void itemTrackingChanged();

private slots:
    void onSearch();
    void onToggleTracking(int row);
    void onRemoveTracking(int trackingId);
    void refreshTrackedList();
    void onDownloadCatalog();

private:
    void setupUi();
    void populateSearchResults(const QString& query);
    void fetchFullPriceHistory(const QString& itemId, int quality);
    void fetchHistoryPage(const QString& itemId, int offset);
    void scheduleNextHistoryPage(const QString& itemId, int offset);
    void onHistoryPageReceived(const QString& itemId,
                               const QVector<PriceHistoryEntry>& entries, int total);
    void storeHistoryEntries(const QString& itemId, int quality,
                             const QVector<PriceHistoryEntry>& allEntries);

    Database* m_db;
    ApiClient* m_api;
    ItemCatalogLoader* m_catalogLoader = nullptr;

    QMap<QString, QVector<PriceHistoryEntry>> m_pendingHistory;
    QMap<QString, int> m_historyTotal;
    QMap<QString, int> m_importQuality;

    QPushButton* m_downloadBtn = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_historyStatus = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QTableWidget* m_searchTable = nullptr;
    QTableWidget* m_trackedTable = nullptr;
};
