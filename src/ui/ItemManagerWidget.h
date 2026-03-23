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
    // Виджет управления списком отслеживаемых предметов и импортом истории цен.
    explicit ItemManagerWidget(Database* db, ApiClient* api, QWidget* parent = nullptr);

signals:
    // Изменение состояния отслеживания (добавлено/удалено трекинг-подключение).
    void itemTrackingChanged();

private slots:
    // Слот: выполнить поиск по `m_searchEdit` и заполнить таблицу результатов.
    void onSearch();
    // Слот: переключить трекинг выбранной строки таблицы поиска (с диалогом качества при необходимости).
    void onToggleTracking(int row);
    // Слот: удалить запись отслеживания в БД по `trackingId`.
    void onRemoveTracking(int trackingId);
    // Слот: перечитать `tracked_items` из БД и пересобрать таблицу отслеживаемых.
    void refreshTrackedList();
    // Слот: запустить загрузку/импорт каталога предметов.
    void onDownloadCatalog();

private:
    // Строит UI: кнопки загрузки, поиск и таблицы (поиск/отслеживаемые).
    void setupUi();
    // Заполняет таблицу результатов поиска найденными предметами.
    void populateSearchResults(const QString& query);
    // Инициирует пагинированный импорт истории цен для `itemId` и наборов качества.
    void fetchFullPriceHistory(const QString& itemId, const QVector<int>& qualities);
    // Запрашивает одну страницу истории цен по `offset` (для пагинации).
    void fetchHistoryPage(const QString& itemId, int offset);
    // Планирует следующий запрос истории цен с троттлингом, чтобы не спамить API.
    void scheduleNextHistoryPage(const QString& itemId, int offset);
    // Слот: получает страницу истории цен, обновляет прогресс и завершает импорт в конце.
    void onHistoryPageReceived(const QString& itemId,
                               const QVector<PriceHistoryEntry>& entries, int total);
    // Агрегирует страницу истории: фильтрует по качеству, считает статистики по часу и сохраняет в БД.
    void storeHistoryEntries(const QString& itemId, int quality,
                             const QVector<PriceHistoryEntry>& allEntries);

    Database* m_db;
    ApiClient* m_api;
    ItemCatalogLoader* m_catalogLoader = nullptr;

    QMap<QString, QVector<PriceHistoryEntry>> m_pendingHistory;
    QMap<QString, int> m_historyTotal;
    QMap<QString, QVector<int>> m_importQualities;

    QPushButton* m_downloadBtn = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_historyStatus = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QTableWidget* m_searchTable = nullptr;
    QTableWidget* m_trackedTable = nullptr;
};
