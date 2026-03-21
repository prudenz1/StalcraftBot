#include "MainWindow.h"
#include "ItemManagerWidget.h"
#include "ActiveLotsWidget.h"
#include "PriceTableWidget.h"
#include "PriceChartWidget.h"
#include "LogWidget.h"
#include "SettingsWidget.h"
#include "core/Config.h"
#include "core/Database.h"
#include "core/ApiClient.h"
#include "core/Scheduler.h"
#include "core/PriceAnalyzer.h"
#include "core/DealDetector.h"

MainWindow::MainWindow(Config* config, Database* db, ApiClient* api,
                       Scheduler* scheduler, PriceAnalyzer* analyzer,
                       DealDetector* detector, QWidget* parent)
    : QMainWindow(parent)
    , m_config(config)
    , m_db(db)
    , m_api(api)
    , m_scheduler(scheduler)
    , m_analyzer(analyzer)
    , m_detector(detector)
{
    setupUi();
}

void MainWindow::setupUi() {
    setWindowTitle("Stalcraft Auction Bot");
    resize(1200, 800);

    m_tabs = new QTabWidget(this);
    setCentralWidget(m_tabs);

    m_itemManager = new ItemManagerWidget(m_db, m_api, this);
    m_activeLots = new ActiveLotsWidget(m_db, m_api, this);
    m_priceTable = new PriceTableWidget(m_db, m_analyzer, this);
    m_priceChart = new PriceChartWidget(m_db, this);
    m_logWidget = new LogWidget(m_db, this);
    m_settingsWidget = new SettingsWidget(m_config, this);

    m_tabs->addTab(m_itemManager, QString::fromUtf8("Предметы"));
    m_tabs->addTab(m_activeLots, QString::fromUtf8("Активные лоты"));
    m_tabs->addTab(m_priceTable, QString::fromUtf8("Цены и статистика"));
    m_tabs->addTab(m_priceChart, QString::fromUtf8("История цен"));
    m_tabs->addTab(m_logWidget, QString::fromUtf8("Логи / Уведомления"));
    m_tabs->addTab(m_settingsWidget, QString::fromUtf8("Настройки"));

    connect(m_detector, &DealDetector::alertGenerated, m_logWidget, &LogWidget::addAlert);
    connect(m_scheduler, &Scheduler::pollFinished, m_priceTable, &PriceTableWidget::refresh);
    connect(m_scheduler, &Scheduler::pollFinished, m_activeLots, &ActiveLotsWidget::refreshItemList);
    connect(m_itemManager, &ItemManagerWidget::itemTrackingChanged, m_activeLots, &ActiveLotsWidget::refreshItemList);
    connect(m_itemManager, &ItemManagerWidget::itemTrackingChanged, m_priceChart, &PriceChartWidget::refreshChart);
}
