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
    void refreshTrackedList();
    void onDownloadCatalog();

private:
    void setupUi();
    void populateSearchResults(const QString& query);
    void fetchFullPriceHistory(const QString& itemId);
    void fetchHistoryPage(const QString& itemId, int offset);
    void scheduleNextHistoryPage(const QString& itemId, int offset); /// Следующая страница истории после паузы (троттлинг ~199 запр./мин).
    void onHistoryPageReceived(const QString& itemId, const QVector<PriceHistoryEntry>& entries, int total);
    void storeHistoryEntries(const QString& itemId, const QVector<PriceHistoryEntry>& allEntries);

    Database* m_db;
    ApiClient* m_api;
    ItemCatalogLoader* m_catalogLoader = nullptr;

    QMap<QString, QVector<PriceHistoryEntry>> m_pendingHistory;
    QMap<QString, int> m_historyTotal;
    /// Счётчик повторов запроса той же страницы при детекте дубликата (глюки API).
    QMap<QString, int> m_historyDuplicateRetries;
    QMap<QString, QDateTime> m_historyLastPageOldest;

    QPushButton* m_downloadBtn = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_historyStatus = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QTableWidget* m_searchTable = nullptr;
    QTableWidget* m_trackedTable = nullptr;
};
