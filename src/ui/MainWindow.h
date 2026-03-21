#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <memory>

class Config;
class Database;
class ApiClient;
class Scheduler;
class PriceAnalyzer;
class DealDetector;
class ItemManagerWidget;
class PriceTableWidget;
class PriceChartWidget;
class ActiveLotsWidget;
class LogWidget;
class SettingsWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Config* config, Database* db, ApiClient* api,
                        Scheduler* scheduler, PriceAnalyzer* analyzer,
                        DealDetector* detector, QWidget* parent = nullptr);

private:
    void setupUi();

    Config* m_config;
    Database* m_db;
    ApiClient* m_api;
    Scheduler* m_scheduler;
    PriceAnalyzer* m_analyzer;
    DealDetector* m_detector;

    QTabWidget* m_tabs = nullptr;
    ItemManagerWidget* m_itemManager = nullptr;
    ActiveLotsWidget* m_activeLots = nullptr;
    PriceTableWidget* m_priceTable = nullptr;
    PriceChartWidget* m_priceChart = nullptr;
    LogWidget* m_logWidget = nullptr;
    SettingsWidget* m_settingsWidget = nullptr;
};
