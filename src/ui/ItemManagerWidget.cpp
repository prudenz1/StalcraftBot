#include "ItemManagerWidget.h"
#include "core/Database.h"
#include "core/ApiClient.h"
#include "core/ItemCatalogLoader.h"
#include "utils/Logger.h"

#include <QHeaderView>
#include <QSplitter>
#include <QMessageBox>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <numeric>



static const int HISTORY_PAGE_SIZE = 200;   // Кол-во записей истории за один запрос к API
static const int HISTORY_THROTTLE_MS = 302; // Пауза между запросами страниц (~200 запр./мин)

/// Создаёт виджет управления предметами: БД, API, загрузчик каталога, UI и связи сигналов.
ItemManagerWidget::ItemManagerWidget(Database* db, ApiClient* api, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
    , m_api(api)
    , m_catalogLoader(new ItemCatalogLoader(db, this))
{
    setupUi();
    refreshTrackedList();

    connect(m_catalogLoader, &ItemCatalogLoader::loadingStarted, this, [this]() {
        m_downloadBtn->setEnabled(false);
        m_downloadBtn->setText(QString::fromUtf8("Загрузка..."));
        m_progressBar->setVisible(true);
        m_progressBar->setValue(0);
    });
    connect(m_catalogLoader, &ItemCatalogLoader::progressUpdated, this, [this](int current, int total) {
        m_progressBar->setMaximum(total);
        m_progressBar->setValue(current);
    });
    connect(m_catalogLoader, &ItemCatalogLoader::loadingFinished, this, [this](int count) {
        m_downloadBtn->setEnabled(true);
        m_downloadBtn->setText(QString::fromUtf8("Загрузить каталог с GitHub"));
        m_progressBar->setVisible(false);
        QMessageBox::information(this, QString::fromUtf8("Каталог загружен"),
            QString::fromUtf8("Загружено предметов: %1").arg(count));
    });
    connect(m_catalogLoader, &ItemCatalogLoader::loadingError, this, [this](const QString& err) {
        m_downloadBtn->setEnabled(true);
        m_downloadBtn->setText(QString::fromUtf8("Загрузить каталог с GitHub"));
        m_progressBar->setVisible(false);
        QMessageBox::warning(this, QString::fromUtf8("Ошибка"), err);
    });
}

/// Собирает разметку: каталог, поиск, таблицы результатов и отслеживаемых предметов, обработчики кнопок.
void ItemManagerWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    auto* catalogLayout = new QHBoxLayout();
    m_downloadBtn = new QPushButton(QString::fromUtf8("Загрузить каталог с GitHub"), this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_historyStatus = new QLabel(this);
    m_historyStatus->setVisible(false);
    catalogLayout->addWidget(m_downloadBtn);
    catalogLayout->addWidget(m_progressBar);
    catalogLayout->addWidget(m_historyStatus);
    mainLayout->addLayout(catalogLayout);

    auto* searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QString::fromUtf8("Поиск предмета по названию, ID или категории..."));
    m_searchBtn = new QPushButton(QString::fromUtf8("Найти"), this);
    searchLayout->addWidget(m_searchEdit);
    searchLayout->addWidget(m_searchBtn);
    mainLayout->addLayout(searchLayout);

    auto* splitter = new QSplitter(Qt::Vertical, this);

    auto* searchGroup = new QWidget(splitter);
    auto* searchGroupLayout = new QVBoxLayout(searchGroup);
    searchGroupLayout->setContentsMargins(0, 0, 0, 0);
    searchGroupLayout->addWidget(new QLabel(QString::fromUtf8("Результаты поиска:"), searchGroup));

    m_searchTable = new QTableWidget(searchGroup);
    m_searchTable->setColumnCount(4);
    m_searchTable->setHorizontalHeaderLabels({
        QString::fromUtf8("ID"),
        QString::fromUtf8("Название"),
        QString::fromUtf8("Категория"),
        QString::fromUtf8("Действие")
    });
    m_searchTable->horizontalHeader()->setStretchLastSection(true);
    m_searchTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_searchTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_searchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    searchGroupLayout->addWidget(m_searchTable);

    auto* trackedGroup = new QWidget(splitter);
    auto* trackedGroupLayout = new QVBoxLayout(trackedGroup);
    trackedGroupLayout->setContentsMargins(0, 0, 0, 0);
    trackedGroupLayout->addWidget(new QLabel(QString::fromUtf8("Отслеживаемые предметы:"), trackedGroup));

    m_trackedTable = new QTableWidget(trackedGroup);
    m_trackedTable->setColumnCount(4);
    m_trackedTable->setHorizontalHeaderLabels({
        QString::fromUtf8("ID"),
        QString::fromUtf8("Название"),
        QString::fromUtf8("Категория"),
        QString::fromUtf8("Действие")
    });
    m_trackedTable->horizontalHeader()->setStretchLastSection(true);
    m_trackedTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_trackedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trackedTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trackedGroupLayout->addWidget(m_trackedTable);

    splitter->addWidget(searchGroup);
    splitter->addWidget(trackedGroup);
    mainLayout->addWidget(splitter);

    connect(m_downloadBtn, &QPushButton::clicked, this, &ItemManagerWidget::onDownloadCatalog);
    connect(m_searchBtn, &QPushButton::clicked, this, &ItemManagerWidget::onSearch);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &ItemManagerWidget::onSearch);
}

/// Запускает загрузку каталога предметов с GitHub через ItemCatalogLoader.
void ItemManagerWidget::onDownloadCatalog() {
    m_catalogLoader->downloadFromGitHub();
}

/// Обрабатывает поиск: при непустом запросе заполняет таблицу результатов из БД.
void ItemManagerWidget::onSearch() {
    QString query = m_searchEdit->text().trimmed();
    if (query.isEmpty()) return;
    populateSearchResults(query);
}

/// Выводит в таблицу поиска найденные предметы и кнопки «Отслеживать» / «Убрать».
void ItemManagerWidget::populateSearchResults(const QString& query) {
    auto items = m_db->searchItems(query);
    m_searchTable->setRowCount(items.size());

    for (int i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        m_searchTable->setItem(i, 0, new QTableWidgetItem(item.id));
        m_searchTable->setItem(i, 1, new QTableWidgetItem(item.nameRu));
        m_searchTable->setItem(i, 2, new QTableWidgetItem(item.category));

        auto* btn = new QPushButton(
            item.tracked ? QString::fromUtf8("Убрать") : QString::fromUtf8("Отслеживать"),
            m_searchTable);
        btn->setProperty("itemId", item.id);
        btn->setProperty("tracked", item.tracked);

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            onToggleTracking(i);
        });

        m_searchTable->setCellWidget(i, 3, btn);
    }
}

/// Переключает отслеживание предмета по строке таблицы поиска; при включении тянет полную историю цен.
void ItemManagerWidget::onToggleTracking(int row) {
    auto* btn = qobject_cast<QPushButton*>(m_searchTable->cellWidget(row, 3));
    if (!btn) return;

    QString itemId = btn->property("itemId").toString();
    bool tracked = btn->property("tracked").toBool();
    bool newState = !tracked;

    if (m_db->setItemTracked(itemId, newState)) {
        btn->setProperty("tracked", newState);
        btn->setText(newState ? QString::fromUtf8("Убрать") : QString::fromUtf8("Отслеживать"));
        refreshTrackedList();
        emit itemTrackingChanged();

        if (newState) {
            fetchFullPriceHistory(itemId);
        }
    }
}

/// Обновляет таблицу отслеживаемых предметов из БД и кнопки «Убрать».
void ItemManagerWidget::refreshTrackedList() {
    auto items = m_db->trackedItems();
    m_trackedTable->setRowCount(items.size());

    for (int i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        m_trackedTable->setItem(i, 0, new QTableWidgetItem(item.id));
        m_trackedTable->setItem(i, 1, new QTableWidgetItem(item.nameRu));
        m_trackedTable->setItem(i, 2, new QTableWidgetItem(item.category));

        auto* btn = new QPushButton(QString::fromUtf8("Убрать"), m_trackedTable);
        btn->setProperty("itemId", item.id);
        connect(btn, &QPushButton::clicked, this, [this, itemId = item.id]() {
            m_db->setItemTracked(itemId, false);
            refreshTrackedList();
            emit itemTrackingChanged();
        });
        m_trackedTable->setCellWidget(i, 3, btn);
    }
}

// --- Price history import ---

// Начинает загрузку всей доступной истории цен предмета с API.
// Сбрасывает накопитель и запрашивает первую страницу (offset=0).
void ItemManagerWidget::fetchFullPriceHistory(const QString& itemId) {
    m_pendingHistory[itemId].clear();
    m_historyTotal[itemId] = 0;

    m_historyStatus->setVisible(true);
    m_historyStatus->setText(QString::fromUtf8("Загрузка истории цен для %1...").arg(itemId));

    connect(m_api, &ApiClient::priceHistoryFetched, this,
            &ItemManagerWidget::onHistoryPageReceived, Qt::UniqueConnection);

    fetchHistoryPage(itemId, 0);
}

/// Запрашивает у API одну страницу истории цен (размер HISTORY_PAGE_SIZE) с заданным смещением.
void ItemManagerWidget::fetchHistoryPage(const QString& itemId, int offset) {
    LOG_INFO("Fetching price history page for {}, offset={}", itemId.toStdString(), offset);
    m_api->fetchPriceHistory(itemId, offset, HISTORY_PAGE_SIZE);
}

// Планирует запрос следующей страницы через HISTORY_THROTTLE_MS мс (защита от rate limit API).
void ItemManagerWidget::scheduleNextHistoryPage(const QString& itemId, int offset) {
    QTimer::singleShot(HISTORY_THROTTLE_MS, this, [this, itemId, offset]() {
        if (m_pendingHistory.contains(itemId))
            fetchHistoryPage(itemId, offset);
    });
}

// Вызывается при получении очередной страницы истории от API.
// Накапливает записи в m_pendingHistory; если ещё не все — запрашивает следующую страницу
// через таймер. Когда все записи получены — передаёт их в storeHistoryEntries для записи в БД.
void ItemManagerWidget::onHistoryPageReceived(const QString& itemId,
                                              const QVector<PriceHistoryEntry>& entries,
                                              int total) {
    if (!m_pendingHistory.contains(itemId)) return;

    m_historyTotal[itemId] = total;
    m_pendingHistory[itemId].append(entries);

    int fetched = m_pendingHistory[itemId].size();
    m_historyStatus->setText(
        QString::fromUtf8("История %1: %2 / %3 записей...")
            .arg(itemId).arg(fetched).arg(total));

    if (fetched < total && !entries.isEmpty()) {
        scheduleNextHistoryPage(itemId, fetched);
    } else {
        LOG_INFO("Price history complete for {}: {} entries", itemId.toStdString(), fetched);
        storeHistoryEntries(itemId, m_pendingHistory[itemId]);
        m_pendingHistory.remove(itemId);
        m_historyTotal.remove(itemId);
        m_historyStatus->setText(
            QString::fromUtf8("История %1 загружена: %2 записей").arg(itemId).arg(fetched));
    }
}

// Группирует все полученные сделки по часам и для каждого часа считает статистику
// (min, avg, median, max, stddev). Результат сохраняется в таблицу price_snapshots (по строке на час)
// и обновляет hourly_stats (средняя цена по часам суток для коэффициента времени).
void ItemManagerWidget::storeHistoryEntries(const QString& itemId, const QVector<PriceHistoryEntry>& allEntries) {
    if (allEntries.isEmpty()) return;

    QMap<qint64, QVector<qint64>> hourBuckets;

    for (const auto& entry : allEntries) {
        if (entry.price <= 0 || !entry.time.isValid()) continue;
        QDateTime hourStart = entry.time;
        hourStart.setTime(QTime(hourStart.time().hour(), 0, 0));
        qint64 key = hourStart.toSecsSinceEpoch();
        hourBuckets[key].append(entry.price);
    }

    int saved = 0;
    for (auto it = hourBuckets.begin(); it != hourBuckets.end(); ++it) {
        QVector<qint64> prices = it.value();
        if (prices.isEmpty()) continue;

        std::sort(prices.begin(), prices.end());
        int n = prices.size();
        qint64 minP = prices.first();
        qint64 maxP = prices.last();
        double sum = std::accumulate(prices.begin(), prices.end(), 0.0);
        qint64 avgP = static_cast<qint64>(sum / n);
        qint64 medianP = (n % 2 == 0)
            ? (prices[n / 2 - 1] + prices[n / 2]) / 2
            : prices[n / 2];

        double variance = 0.0;
        for (qint64 p : prices) {
            double diff = static_cast<double>(p) - static_cast<double>(avgP);
            variance += diff * diff;
        }
        double stdDev = std::sqrt(variance / n);

        PriceSnapshot snap;
        snap.itemId = itemId;
        snap.timestamp = QDateTime::fromSecsSinceEpoch(it.key(), Qt::UTC);
        snap.minPrice = minP;
        snap.avgPrice = avgP;
        snap.medianPrice = medianP;
        snap.maxPrice = maxP;
        snap.stdDev = stdDev;
        snap.lotCount = n;
        snap.filteredCount = n;
        m_db->insertPriceSnapshot(snap);

        int hour = snap.timestamp.time().hour();
        m_db->upsertHourlyStat(itemId, hour, medianP, 1);
        saved++;
    }

    LOG_INFO("Stored {} aggregated snapshots for {} from {} history entries",
             saved, itemId.toStdString(), allEntries.size());
}
