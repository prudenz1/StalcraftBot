#include "PriceTableWidget.h"
#include "core/Database.h"
#include "core/PriceAnalyzer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDateTime>

PriceTableWidget::PriceTableWidget(Database* db, PriceAnalyzer* analyzer,
                                   QWidget* parent)
    : QWidget(parent)
    , m_db(db)
    , m_analyzer(analyzer)
{
    setupUi();
}

void PriceTableWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* topLayout = new QHBoxLayout();
    m_lastUpdate = new QLabel(QString::fromUtf8("Последнее обновление: --"), this);
    m_refreshBtn = new QPushButton(QString::fromUtf8("Обновить"), this);
    topLayout->addWidget(m_lastUpdate);
    topLayout->addStretch();
    topLayout->addWidget(m_refreshBtn);
    layout->addLayout(topLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(11);
    m_table->setHorizontalHeaderLabels({
        QString::fromUtf8("Предмет"),
        QString::fromUtf8("Мин. цена"),
        QString::fromUtf8("Средняя"),
        QString::fromUtf8("Медиана"),
        QString::fromUtf8("Макс. цена"),
        QString::fromUtf8("Стд. откл."),
        QString::fromUtf8("Лотов"),
        QString::fromUtf8("Z-Score"),
        QString::fromUtf8("Тренд"),
        QString::fromUtf8("Рейтинг"),
        QString::fromUtf8("Сигнал")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    layout->addWidget(m_table);

    connect(m_refreshBtn, &QPushButton::clicked, this, &PriceTableWidget::refresh);
}

void PriceTableWidget::refresh() {
    auto tracked = m_db->trackedItems();
    m_table->setRowCount(tracked.size());

    for (int i = 0; i < tracked.size(); ++i) {
        const auto& item = tracked[i];
        PriceSnapshot snap = m_db->latestPriceSnapshot(item.id);
        AnalysisResult analysis = m_analyzer->lastResult(item.id);

        m_table->setItem(i, 0, new QTableWidgetItem(item.nameRu));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::number(snap.minPrice)));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(snap.avgPrice)));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(snap.medianPrice)));
        m_table->setItem(i, 4, new QTableWidgetItem(QString::number(snap.maxPrice)));
        m_table->setItem(i, 5, new QTableWidgetItem(QString::number(snap.stdDev, 'f', 0)));
        m_table->setItem(i, 6, new QTableWidgetItem(QString::number(snap.lotCount)));
        m_table->setItem(i, 7, new QTableWidgetItem(QString::number(analysis.zScore, 'f', 2)));

        QString trendStr;
        switch (analysis.trend) {
            case Trend::Down: trendStr = "DOWN"; break;
            case Trend::Up:   trendStr = "UP"; break;
            default:          trendStr = "FLAT"; break;
        }
        m_table->setItem(i, 8, new QTableWidgetItem(trendStr));
        m_table->setItem(i, 9, new QTableWidgetItem(QString::number(analysis.rating, 'f', 2)));

        QString signalStr;
        QColor signalColor;
        switch (analysis.signal) {
            case Signal::Buy:
                signalStr = "BUY";
                signalColor = QColor(46, 204, 113);
                break;
            case Signal::Watch:
                signalStr = "WATCH";
                signalColor = QColor(241, 196, 15);
                break;
            default:
                signalStr = "--";
                signalColor = QColor(149, 165, 166);
                break;
        }
        auto* signalItem = new QTableWidgetItem(signalStr);
        signalItem->setForeground(signalColor);
        m_table->setItem(i, 10, signalItem);
    }

    m_lastUpdate->setText(
        QString::fromUtf8("Последнее обновление: %1")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss dd.MM.yyyy")));
}
