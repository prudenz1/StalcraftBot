#include "ActiveLotsWidget.h"
#include "core/Database.h"
#include "core/ApiClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <algorithm>

ActiveLotsWidget::ActiveLotsWidget(Database* db, ApiClient* api, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
    , m_api(api)
{
    setupUi();

    connect(m_api, &ApiClient::lotsFetched, this, &ActiveLotsWidget::onLotsReceived);
}

void ActiveLotsWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel(QString::fromUtf8("Предмет:"), this));

    m_itemCombo = new QComboBox(this);
    m_itemCombo->setMinimumWidth(300);
    topLayout->addWidget(m_itemCombo);

    m_refreshBtn = new QPushButton(QString::fromUtf8("Обновить"), this);
    topLayout->addWidget(m_refreshBtn);

    m_statusLabel = new QLabel(this);
    topLayout->addWidget(m_statusLabel);
    topLayout->addStretch();
    layout->addLayout(topLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        QString::fromUtf8("Цена выкупа"),
        QString::fromUtf8("Стартовая цена"),
        QString::fromUtf8("Качество"),
        QString::fromUtf8("Время выставления"),
        QString::fromUtf8("Отклонение от медианы")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    layout->addWidget(m_table);

    connect(m_itemCombo, &QComboBox::currentIndexChanged, this, &ActiveLotsWidget::onItemSelected);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ActiveLotsWidget::onRefreshClicked);

    refreshItemList();
}

void ActiveLotsWidget::refreshItemList() {
    QString currentKey = m_itemCombo->currentData().toString();
    m_itemCombo->blockSignals(true);
    m_itemCombo->clear();

    auto items = m_db->trackedItems();
    for (const auto& item : items) {
        m_itemCombo->addItem(item.displayName() +
            QStringLiteral("  [") + item.id + QStringLiteral("]"),
            item.trackingKey());
    }

    if (!currentKey.isEmpty()) {
        int idx = m_itemCombo->findData(currentKey);
        if (idx >= 0) m_itemCombo->setCurrentIndex(idx);
    }
    m_itemCombo->blockSignals(false);
}

void ActiveLotsWidget::onItemSelected() {
    QString data = m_itemCombo->currentData().toString();
    if (data.isEmpty()) return;

    QStringList parts = data.split('|');
    QString itemId = parts[0];
    int quality = parts.size() > 1 ? parts[1].toInt() : -1;

    if (m_lotsCache.contains(itemId)) {
        displayLots(itemId, quality);
    } else {
        m_api->fetchLots(itemId);
        m_statusLabel->setText(QString::fromUtf8("Загрузка..."));
    }
}

void ActiveLotsWidget::onRefreshClicked() {
    QString data = m_itemCombo->currentData().toString();
    if (data.isEmpty()) return;

    QStringList parts = data.split('|');
    QString itemId = parts[0];

    m_api->fetchLots(itemId);
    m_statusLabel->setText(QString::fromUtf8("Загрузка..."));
}

void ActiveLotsWidget::onLotsReceived(const QString& itemId, const QVector<Lot>& lots, int total) {
    m_lotsCache[itemId] = lots;
    m_totalsCache[itemId] = total;

    QString data = m_itemCombo->currentData().toString();
    QStringList parts = data.split('|');
    if (!parts.isEmpty() && parts[0] == itemId) {
        int quality = parts.size() > 1 ? parts[1].toInt() : -1;
        displayLots(itemId, quality);
    }
}

void ActiveLotsWidget::displayLots(const QString& itemId, int quality) {
    const auto& allLots = m_lotsCache[itemId];

    // Filter by quality if specified
    QVector<Lot> lots;
    if (quality >= 0) {
        for (const auto& lot : allLots) {
            if (lot.quality == quality) lots.append(lot);
        }
    } else {
        lots = allLots;
    }

    int total = m_totalsCache.value(itemId, allLots.size());

    m_statusLabel->setText(
        QString::fromUtf8("Показано: %1 (из %2) | Всего на аукционе: %3")
            .arg(lots.size()).arg(allLots.size()).arg(total));

    QVector<qint64> prices;
    for (const auto& lot : lots) {
        if (lot.buyoutPrice > 0) prices.append(lot.buyoutPrice);
    }
    std::sort(prices.begin(), prices.end());
    qint64 median = 0;
    if (!prices.isEmpty()) {
        int n = prices.size();
        median = (n % 2 == 0) ? (prices[n/2 - 1] + prices[n/2]) / 2 : prices[n/2];
    }

    m_table->setSortingEnabled(false);
    m_table->setRowCount(lots.size());

    for (int i = 0; i < lots.size(); ++i) {
        const auto& lot = lots[i];

        auto* buyoutItem = new QTableWidgetItem();
        buyoutItem->setData(Qt::DisplayRole, lot.buyoutPrice > 0
            ? QVariant(lot.buyoutPrice) : QVariant(QString::fromUtf8("--")));
        m_table->setItem(i, 0, buyoutItem);

        auto* startItem = new QTableWidgetItem();
        startItem->setData(Qt::DisplayRole, QVariant(lot.startPrice));
        m_table->setItem(i, 1, startItem);

        m_table->setItem(i, 2, new QTableWidgetItem(
            lot.quality >= 0 ? Item::qualityName(lot.quality) : QStringLiteral("--")));

        m_table->setItem(i, 3, new QTableWidgetItem(
            lot.startTime.toLocalTime().toString("dd.MM.yyyy HH:mm")));

        if (lot.buyoutPrice > 0 && median > 0) {
            double devPct = (static_cast<double>(lot.buyoutPrice) - median) / median * 100.0;
            auto* devItem = new QTableWidgetItem(
                QString("%1%2%").arg(devPct >= 0 ? "+" : "").arg(devPct, 0, 'f', 1));
            if (devPct < -5.0) {
                devItem->setForeground(QColor(46, 204, 113));
            } else if (devPct > 20.0) {
                devItem->setForeground(QColor(231, 76, 60));
            }
            m_table->setItem(i, 4, devItem);
        } else {
            m_table->setItem(i, 4, new QTableWidgetItem("--"));
        }
    }

    m_table->setSortingEnabled(true);
}
