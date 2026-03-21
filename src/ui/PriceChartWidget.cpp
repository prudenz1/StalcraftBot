#include "PriceChartWidget.h"
#include "core/Database.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTime>

PriceChartWidget::PriceChartWidget(Database* db, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
{
    setupUi();
}

void PriceChartWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* controlLayout = new QHBoxLayout();
    controlLayout->addWidget(new QLabel(QString::fromUtf8("Предмет:"), this));

    m_itemCombo = new QComboBox(this);
    m_itemCombo->setMinimumWidth(250);
    controlLayout->addWidget(m_itemCombo);

    controlLayout->addWidget(new QLabel(QString::fromUtf8("Дней:"), this));
    m_daysSpin = new QSpinBox(this);
    m_daysSpin->setRange(1, 90);
    m_daysSpin->setValue(7);
    controlLayout->addWidget(m_daysSpin);

    m_refreshBtn = new QPushButton(QString::fromUtf8("Показать"), this);
    controlLayout->addWidget(m_refreshBtn);
    controlLayout->addStretch();
    layout->addLayout(controlLayout);

    m_chart = new QChart();
    m_chart->setTitle(QString::fromUtf8("История цен"));
    m_chart->setAnimationOptions(QChart::SeriesAnimations);

    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(m_chartView);

    connect(m_refreshBtn, &QPushButton::clicked, this, &PriceChartWidget::refreshChart);
    connect(m_itemCombo, &QComboBox::currentIndexChanged, this, &PriceChartWidget::refreshChart);

    populateItemCombo();
}

void PriceChartWidget::populateItemCombo() {
    m_itemCombo->clear();
    auto items = m_db->trackedItems();
    for (const auto& item : items) {
        m_itemCombo->addItem(item.nameRu, item.id);
    }
}

void PriceChartWidget::refreshChart() {
    QString itemId = m_itemCombo->currentData().toString();
    if (itemId.isEmpty()) return;

    int days = m_daysSpin->value();
    auto history = m_db->priceHistory(itemId, days);

    m_chart->removeAllSeries();
    for (auto* axis : m_chart->axes()) {
        m_chart->removeAxis(axis);
    }

    if (history.isEmpty()) {
        m_chart->setTitle(QString::fromUtf8("Нет данных"));
        return;
    }

    auto* medianSeries = new QLineSeries();
    medianSeries->setName(QString::fromUtf8("Медиана"));

    auto* avgSeries = new QLineSeries();
    avgSeries->setName(QString::fromUtf8("Средняя"));

    auto* minSeries = new QLineSeries();
    minSeries->setName(QString::fromUtf8("Минимальная"));

    qreal minVal = std::numeric_limits<qreal>::max();
    qreal maxVal = 0;

    for (const auto& snap : history) {
        qreal ms = snap.timestamp.toMSecsSinceEpoch();
        medianSeries->append(ms, snap.medianPrice);
        avgSeries->append(ms, snap.avgPrice);
        minSeries->append(ms, snap.minPrice);

        minVal = qMin(minVal, static_cast<qreal>(snap.minPrice));
        maxVal = qMax(maxVal, static_cast<qreal>(snap.maxPrice));
    }

    m_chart->addSeries(medianSeries);
    m_chart->addSeries(avgSeries);
    m_chart->addSeries(minSeries);

    auto* axisX = new QDateTimeAxis();
    axisX->setFormat("dd.MM HH:mm");
    axisX->setTitleText(QString::fromUtf8("Время"));
    m_chart->addAxis(axisX, Qt::AlignBottom);
    medianSeries->attachAxis(axisX);
    avgSeries->attachAxis(axisX);
    minSeries->attachAxis(axisX);

    auto* axisY = new QValueAxis();
    axisY->setTitleText(QString::fromUtf8("Цена"));
    double margin = (maxVal - minVal) * 0.1;
    axisY->setRange(minVal - margin, maxVal + margin);
    m_chart->addAxis(axisY, Qt::AlignLeft);
    medianSeries->attachAxis(axisY);
    avgSeries->attachAxis(axisY);
    minSeries->attachAxis(axisY);

    m_chart->setTitle(QString::fromUtf8("История цен: %1").arg(m_itemCombo->currentText()));
}
