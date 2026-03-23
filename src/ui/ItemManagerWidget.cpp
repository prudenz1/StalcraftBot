#include "ItemManagerWidget.h"
#include "core/Database.h"
#include "core/ApiClient.h"
#include "core/ItemCatalogLoader.h"
#include "utils/Logger.h"

#include <QHeaderView>
#include <QSplitter>
#include <QMessageBox>

#include <algorithm>
#include <cmath>
#include <numeric>

#include <QDateTime>
#include <QTimer>

namespace {

static const int HISTORY_PAGE_SIZE = 200;
static constexpr int kPaginationOverlapWarnSec = 120;
static constexpr int kDuplicatePrefixRows = 5;
static constexpr int kMaxDuplicateRetries = 3;
static constexpr int kDuplicateRetryDelayMs = 400;
/// Минимальная пауза перед следующим запросом страницы истории. Цель — не более 199 запросов в минуту:
/// ceil(60000 / 199) = 302 мс между стартами при равномерной сетке (строго меньше 200 запр./мин).
static constexpr int kHistoryThrottleIntervalMs = 302;

/// Сравнивает две записи истории: совпадают ли цена и момент времени (по секундам UTC).
static bool sameTimePrice(const PriceHistoryEntry& a, const PriceHistoryEntry& b) {
    return a.price == b.price && a.time.isValid() && b.time.isValid()
        && a.time.toSecsSinceEpoch() == b.time.toSecsSinceEpoch();
}

/// Первые k строк совпадают с началом уже загруженной первой страницы — ответ снова «как offset=0».
static bool isDuplicateRepeatOfFirstPage(const QVector<PriceHistoryEntry>& newPage,
                                         const QVector<PriceHistoryEntry>& pending) {
    if (pending.size() < HISTORY_PAGE_SIZE || newPage.size() < kDuplicatePrefixRows)
        return false;
    for (int i = 0; i < kDuplicatePrefixRows; ++i) {
        if (!sameTimePrice(newPage[i], pending[i])) return false;
    }
    return true;
}

/// Находит минимальную и максимальную дату среди валидных времён записей на странице.
static void historyPageMinMax(const QVector<PriceHistoryEntry>& entries,
                              QDateTime* outMin, QDateTime* outMax) {
    *outMin = *outMax = QDateTime();
    bool any = false;
    for (const auto& e : entries) {
        if (!e.time.isValid()) continue;
        if (!any) {
            *outMin = *outMax = e.time;
            any = true;
        } else {
            if (e.time < *outMin) *outMin = e.time;
            if (e.time > *outMax) *outMax = e.time;
        }
    }
}

} // namespace

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

/// Сбрасывает состояние загрузки истории для предмета и начинает постраничную выгрузку с offset 0.
void ItemManagerWidget::fetchFullPriceHistory(const QString& itemId) {
    m_pendingHistory[itemId].clear();
    m_historyTotal[itemId] = 0;
    m_historyDuplicateRetries.remove(itemId);
    m_historyLastPageOldest.remove(itemId);

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

/// Следующая страница истории после паузы kHistoryThrottleIntervalMs; не вызывать fetch, если загрузка уже снята (m_pendingHistory).
void ItemManagerWidget::scheduleNextHistoryPage(const QString& itemId, int offset) {
    QTimer::singleShot(kHistoryThrottleIntervalMs, this, [this, itemId, offset]() {
        if (m_pendingHistory.contains(itemId))
            fetchHistoryPage(itemId, offset);
    });
}

/// Склеивает страницы истории, обрабатывает дубликаты и перекрытия пагинации, по завершении сохраняет в БД.
void ItemManagerWidget::onHistoryPageReceived(const QString& itemId,
                                              const QVector<PriceHistoryEntry>& entries,
                                              int total) {
    if (!m_pendingHistory.contains(itemId)) return;

    const int pageOffset = m_pendingHistory[itemId].size();

    m_historyTotal[itemId] = total;

    QDateTime pageMin, pageMax;
    historyPageMinMax(entries, &pageMin, &pageMax);

    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    for (const auto& e : entries) {
        if (e.time.isValid() && e.time > nowUtc.addDays(1)) {
            LOG_WARN("История {}: сделка с датой в будущем ({}) — возможна ошибка API/часового пояса",
                     itemId.toStdString(), e.time.toString(Qt::ISODate).toStdString());
            break;
        }
    }

    bool rejectDuplicatePage = false;
    if (pageOffset > 0 && !entries.isEmpty()
        && isDuplicateRepeatOfFirstPage(entries, m_pendingHistory[itemId])) {
        int n = m_historyDuplicateRetries.value(itemId, 0);
        if (n < kMaxDuplicateRetries) {
            m_historyDuplicateRetries[itemId] = n + 1;
            LOG_WARN(
                "История {}: offset={} — повтор первой страницы (первые {} сделок); "
                "повтор запроса {}/{} через {} мс.",
                itemId.toStdString(), pageOffset, kDuplicatePrefixRows, n + 1, kMaxDuplicateRetries,
                kDuplicateRetryDelayMs);
            QTimer::singleShot(kDuplicateRetryDelayMs, this, [this, itemId, pageOffset]() {
                if (m_pendingHistory.contains(itemId))
                    fetchHistoryPage(itemId, pageOffset);
            });
            return;
        }
        LOG_WARN(
            "История {}: offset={} — после {} повторов всё ещё дубликат первой страницы; загрузка остановлена.",
            itemId.toStdString(), pageOffset, kMaxDuplicateRetries);
        rejectDuplicatePage = true;
    } else if (pageOffset > 0) {
        m_historyDuplicateRetries[itemId] = 0;
    }
    if (!rejectDuplicatePage && pageOffset > 0 && pageMin.isValid() && pageMax.isValid()
        && m_historyLastPageOldest.contains(itemId)
        && pageMax > m_historyLastPageOldest[itemId].addSecs(kPaginationOverlapWarnSec)) {
        LOG_WARN(
            "История {}: offset={} — «новейшая» сделка на странице ({}) сильно новее самой старой "
            "на предыдущей ({}), пагинация может быть неконсистентной.",
            itemId.toStdString(),
            pageOffset,
            pageMax.toString(Qt::ISODate).toStdString(),
            m_historyLastPageOldest[itemId].toString(Qt::ISODate).toStdString());
    }

    if (!rejectDuplicatePage) {
        m_pendingHistory[itemId].append(entries);
        if (pageMin.isValid()) {
            m_historyLastPageOldest[itemId] = pageMin;
        }
    }

    int fetched = m_pendingHistory[itemId].size();
    QString totalLabel = (total > 0) ? QString::number(total)
                                     : QString::fromUtf8("—");
    m_historyStatus->setText(
        QString::fromUtf8("История %1: %2 / %3 записей...")
            .arg(itemId).arg(fetched).arg(totalLabel));

    // total иногда отражает не всю историю (например только недавние сделки). Старое условие
    // fetched < total обрывало загрузку при полной странице, хотя дальше по offset данные есть.
    // При полной странице всегда запрашиваем следующую; конец — пустой ответ или неполная
    // страница при fetched >= total (лишний пустой запрос при «ровном» total допустим).
    bool fetchMore = false;
    if (!rejectDuplicatePage && !entries.isEmpty()) {
        if (entries.size() >= HISTORY_PAGE_SIZE) {
            fetchMore = true;
        } else if (total > 0 && fetched < total) {
            fetchMore = true;
        }
    }

    if (fetchMore) {
        scheduleNextHistoryPage(itemId, fetched);
    } else {
        LOG_INFO("Price history complete for {}: {} entries", itemId.toStdString(), fetched);
        storeHistoryEntries(itemId, m_pendingHistory[itemId]);
        m_pendingHistory.remove(itemId);
        m_historyTotal.remove(itemId);
        m_historyDuplicateRetries.remove(itemId);
        m_historyLastPageOldest.remove(itemId);
        m_historyStatus->setText(
            QString::fromUtf8("История %1 загружена: %2 записей").arg(itemId).arg(fetched));
    }
}

/// Агрегирует сырые сделки по часам, пишет снимки цен и почасовую статистику в БД.
void ItemManagerWidget::storeHistoryEntries(const QString& itemId, const QVector<PriceHistoryEntry>& allEntries) {
    if (allEntries.isEmpty()) return;

    // Group entries by hour for aggregated snapshots
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
