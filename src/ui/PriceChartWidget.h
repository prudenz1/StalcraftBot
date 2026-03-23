#pragma once

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

class Database;

class PriceChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit PriceChartWidget(Database* db, QWidget* parent = nullptr);

public slots:
    void refreshChart();
    void onTrackingChanged();

private:
    void setupUi();
    void populateItemCombo();

    Database* m_db;

    QComboBox* m_itemCombo = nullptr;
    QSpinBox* m_daysSpin = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QChartView* m_chartView = nullptr;
    QChart* m_chart = nullptr;
};
