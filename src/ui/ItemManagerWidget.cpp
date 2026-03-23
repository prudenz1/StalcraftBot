#include "ItemManagerWidget.h"
#include "core/Database.h"
#include "core/ApiClient.h"
#include "core/ItemCatalogLoader.h"
#include "utils/Logger.h"

#include <QHeaderView>
#include <QSplitter>
#include <QMessageBox>
#include <QTimer>
#include <QDialog>
#include <QCheckBox>
#include <QDialogButtonBox>

#include <algorithm>
#include <cmath>
#include <numeric>

static const int HISTORY_PAGE_SIZE = 200;
static const int HISTORY_THROTTLE_MS = 302;

// Инициализирует виджет и подписывает UI на события загрузчика каталога.
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

// Собирает интерфейс: панель загрузки каталога, поиск и список отслеживаемых предметов.
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
    m_trackedTable->setColumnCount(5);
    m_trackedTable->setHorizontalHeaderLabels({
        QString::fromUtf8("ID"),
        QString::fromUtf8("Название"),
        QString::fromUtf8("Категория"),
        QString::fromUtf8("Качество"),
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

// Запускает загрузку каталога предметов из внешнего источника.
void ItemManagerWidget::onDownloadCatalog() {
    m_catalogLoader->downloadFromGitHub();
}

// Обрабатывает ввод поиска и обновляет таблицу результатов.
void ItemManagerWidget::onSearch() {
    QString query = m_searchEdit->text().trimmed();
    if (query.isEmpty()) return;
    populateSearchResults(query);
}

// Заполняет таблицу найденными предметами и кнопками управления отслеживанием.
void ItemManagerWidget::populateSearchResults(const QString& query) {
    auto items = m_db->searchItems(query);
    m_searchTable->setRowCount(items.size());

    for (int i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        m_searchTable->setItem(i, 0, new QTableWidgetItem(item.id));
        m_searchTable->setItem(i, 1, new QTableWidgetItem(item.nameRu));
        m_searchTable->setItem(i, 2, new QTableWidgetItem(item.category));

        QString btnText;
        if (item.hasQualityTier()) {
            btnText = QString::fromUtf8("Отслеживать");
        } else {
            btnText = item.hasTracking
                ? QString::fromUtf8("Убрать")
                : QString::fromUtf8("Отслеживать");
        }

        auto* btn = new QPushButton(btnText, m_searchTable);
        btn->setProperty("itemId", item.id);
        btn->setProperty("category", item.category);
        btn->setProperty("hasTracking", item.hasTracking);

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            onToggleTracking(i);
        });

        m_searchTable->setCellWidget(i, 3, btn);
    }
}

// Переключает отслеживание предмета; для предметов с качеством показывает выбор качества.
void ItemManagerWidget::onToggleTracking(int row) {
    auto* btn = qobject_cast<QPushButton*>(m_searchTable->cellWidget(row, 3));
    if (!btn) return;

    QString itemId = btn->property("itemId").toString();
    QString category = btn->property("category").toString();
    bool hasTrack = btn->property("hasTracking").toBool();

    bool isQualityItem = category.startsWith(QStringLiteral("artefact/"))
        || category == QStringLiteral("weapon_modules/weapon_module")
        || category == QStringLiteral("weapon_modules/weapon_module_core");

    if (isQualityItem) {
        QDialog dlg(this);
        dlg.setWindowTitle(QString::fromUtf8("Качество предмета"));
        auto* dlgLayout = new QVBoxLayout(&dlg);
        dlgLayout->addWidget(new QLabel(
            QString::fromUtf8("Выберите качество для отслеживания (можно несколько):"), &dlg));

        QStringList qualityNames = {
            QString::fromUtf8("Все качества"),
            QString::fromUtf8("Обычный (0)"),
            QString::fromUtf8("Необычный (1)"),
            QString::fromUtf8("Особый (2)"),
            QString::fromUtf8("Редкий (3)"),
            QString::fromUtf8("Исключительный (4)"),
            QString::fromUtf8("Легендарный (5)")
        };

        QVector<QCheckBox*> checkboxes;
        for (const auto& name : qualityNames) {
            auto* cb = new QCheckBox(name, &dlg);
            dlgLayout->addWidget(cb);
            checkboxes.append(cb);
        }

        // "Все качества" снимает остальные и наоборот
        connect(checkboxes[0], &QCheckBox::toggled, &dlg, [&checkboxes](bool checked) {
            if (checked) {
                for (int i = 1; i < checkboxes.size(); ++i)
                    checkboxes[i]->setChecked(false);
            }
        });
        for (int i = 1; i < checkboxes.size(); ++i) {
            connect(checkboxes[i], &QCheckBox::toggled, &dlg, [&checkboxes](bool checked) {
                if (checked)
                    checkboxes[0]->setChecked(false);
            });
        }

        auto* btnBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        dlgLayout->addWidget(btnBox);
        connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        if (dlg.exec() != QDialog::Accepted) return;

        QVector<int> selectedQualities;
        if (checkboxes[0]->isChecked()) {
            selectedQualities.append(-1);
        } else {
            for (int i = 1; i < checkboxes.size(); ++i) {
                if (checkboxes[i]->isChecked())
                    selectedQualities.append(i - 1);
            }
        }
        if (selectedQualities.isEmpty()) return;

        for (int quality : selectedQualities)
            m_db->addTracking(itemId, quality);
        btn->setProperty("hasTracking", true);
        refreshTrackedList();
        emit itemTrackingChanged();
        fetchFullPriceHistory(itemId, selectedQualities);
    } else {
        if (hasTrack) {
            m_db->removeAllTracking(itemId);
            btn->setProperty("hasTracking", false);
            btn->setText(QString::fromUtf8("Отслеживать"));
        } else {
            m_db->addTracking(itemId, -1);
            btn->setProperty("hasTracking", true);
            btn->setText(QString::fromUtf8("Убрать"));
            fetchFullPriceHistory(itemId, {-1});
        }
        refreshTrackedList();
        emit itemTrackingChanged();
    }
}

// Удаляет конкретную запись отслеживания по ее ID.
void ItemManagerWidget::onRemoveTracking(int trackingId) {
    m_db->removeTracking(trackingId);
    refreshTrackedList();
    emit itemTrackingChanged();
}

// Перечитывает отслеживаемые предметы из БД и заново строит таблицу.
void ItemManagerWidget::refreshTrackedList() {
    auto items = m_db->trackedItems();
    m_trackedTable->setRowCount(items.size());

    for (int i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        m_trackedTable->setItem(i, 0, new QTableWidgetItem(item.id));
        m_trackedTable->setItem(i, 1, new QTableWidgetItem(item.nameRu));
        m_trackedTable->setItem(i, 2, new QTableWidgetItem(item.category));
        m_trackedTable->setItem(i, 3, new QTableWidgetItem(Item::qualityName(item.quality)));

        auto* btn = new QPushButton(QString::fromUtf8("Убрать"), m_trackedTable);
        int tid = item.trackingId;
        connect(btn, &QPushButton::clicked, this, [this, tid]() {
            onRemoveTracking(tid);
        });
        m_trackedTable->setCellWidget(i, 4, btn);
    }
}

// --- Price history import ---

// Инициализирует полную загрузку истории цен и запускает первую страницу.
void ItemManagerWidget::fetchFullPriceHistory(const QString& itemId,
                                               const QVector<int>& qualities) {
    if (m_pendingHistory.contains(itemId)) {
        for (int q : qualities) {
            if (!m_importQualities[itemId].contains(q))
                m_importQualities[itemId].append(q);
        }
        return;
    }

    m_pendingHistory[itemId].clear();
    m_historyTotal[itemId] = 0;
    m_importQualities[itemId] = qualities;

    m_historyStatus->setVisible(true);
    m_historyStatus->setText(QString::fromUtf8("Загрузка истории цен для %1...").arg(itemId));

    connect(m_api, &ApiClient::priceHistoryFetched, this,
            &ItemManagerWidget::onHistoryPageReceived, Qt::UniqueConnection);

    fetchHistoryPage(itemId, 0);
}

// Запрашивает страницу истории цен у API с указанным смещением.
void ItemManagerWidget::fetchHistoryPage(const QString& itemId, int offset) {
    LOG_INFO("Fetching price history page for {}, offset={}", itemId.toStdString(), offset);
    m_api->fetchPriceHistory(itemId, offset, HISTORY_PAGE_SIZE);
}

// Планирует следующий запрос истории с небольшим троттлингом.
void ItemManagerWidget::scheduleNextHistoryPage(const QString& itemId, int offset) {
    QTimer::singleShot(HISTORY_THROTTLE_MS, this, 
        [this, itemId, offset]() {
        if (m_pendingHistory.contains(itemId))
            fetchHistoryPage(itemId, offset);
    });
}

// Принимает страницу истории, обновляет прогресс и завершает импорт при достижении конца.
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
        QVector<int> qualities = m_importQualities.value(itemId, {-1});
        const auto& allEntries = m_pendingHistory[itemId];
        for (int quality : qualities)
            storeHistoryEntries(itemId, quality, allEntries);
        m_pendingHistory.remove(itemId);
        m_historyTotal.remove(itemId);
        m_importQualities.remove(itemId);
        m_historyStatus->setText(
            QString::fromUtf8("История %1 загружена: %2 записей").arg(itemId).arg(fetched));
    }
}

// Фильтрует историю по качеству и сохраняет агрегаты по каждому часу в БД.
void ItemManagerWidget::storeHistoryEntries(const QString& itemId, int quality,
                                            const QVector<PriceHistoryEntry>& allEntries) {
    if (allEntries.isEmpty()) return;

    // Filter entries by target quality
    QVector<const PriceHistoryEntry*> filtered;
    for (const auto& entry : allEntries) {
        if (entry.price <= 0 || !entry.time.isValid()) continue;
        if (quality >= 0 && entry.quality != quality) continue;
        filtered.append(&entry);
    }

    if (filtered.isEmpty()) {
        LOG_WARN("No history entries match quality {} for {}", quality, itemId.toStdString());
        return;
    }

    QMap<qint64, QVector<qint64>> hourBuckets;
    for (const auto* entry : filtered) {
        QDateTime hourStart = entry->time;
        hourStart.setTime(QTime(hourStart.time().hour(), 0, 0));
        qint64 key = hourStart.toSecsSinceEpoch();
        hourBuckets[key].append(entry->price);
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
        snap.quality = quality;
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
        m_db->upsertHourlyStat(itemId, quality, hour, medianP, 1);
        saved++;
    }

    LOG_INFO("Stored {} aggregated snapshots for {} q={} from {} history entries",
             saved, itemId.toStdString(), quality, filtered.size());
}
