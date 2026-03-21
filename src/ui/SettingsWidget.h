#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>

class Config;

class SettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWidget(Config* config, QWidget* parent = nullptr);

private slots:
    void loadFromConfig();
    void saveToConfig();

private:
    void setupUi();

    Config* m_config;

    // API
    QComboBox* m_regionCombo = nullptr;
    QLineEdit* m_bearerTokenEdit = nullptr;
    QPushButton* m_clearBearerBtn = nullptr;

    // Scheduler
    QSpinBox* m_pollIntervalSpin = nullptr;

    // Auction
    QSpinBox* m_lotsLimitSpin = nullptr;
    QSpinBox* m_outlierNSpin = nullptr;

    // Thresholds
    QDoubleSpinBox* m_watchSpin = nullptr;
    QDoubleSpinBox* m_buySpin = nullptr;

    // Database
    QLineEdit* m_dbHostEdit = nullptr;
    QSpinBox* m_dbPortSpin = nullptr;
    QLineEdit* m_dbNameEdit = nullptr;
    QLineEdit* m_dbUserEdit = nullptr;
    QLineEdit* m_dbPasswordEdit = nullptr;

    QPushButton* m_saveBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
};
