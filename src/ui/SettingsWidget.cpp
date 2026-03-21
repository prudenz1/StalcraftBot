#include "SettingsWidget.h"
#include "core/Config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

SettingsWidget::SettingsWidget(Config* config, QWidget* parent)
    : QWidget(parent)
    , m_config(config)
{
    setupUi();
    loadFromConfig();
}

void SettingsWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    // API Settings
    auto* apiGroup = new QGroupBox(QString::fromUtf8("API Stalcraft"), this);
    auto* apiLayout = new QFormLayout(apiGroup);

    m_regionCombo = new QComboBox(this);
    m_regionCombo->addItems({"RU", "EU", "NA", "SEA"});
    apiLayout->addRow(QString::fromUtf8("Регион:"), m_regionCombo);

    m_clientIdEdit = new QLineEdit(this);
    apiLayout->addRow("Client-Id:", m_clientIdEdit);

    m_clientSecretEdit = new QLineEdit(this);
    m_clientSecretEdit->setEchoMode(QLineEdit::Password);
    apiLayout->addRow("Client-Secret:", m_clientSecretEdit);

    mainLayout->addWidget(apiGroup);

    // Scheduler Settings
    auto* schedGroup = new QGroupBox(QString::fromUtf8("Планировщик"), this);
    auto* schedLayout = new QFormLayout(schedGroup);

    m_pollIntervalSpin = new QSpinBox(this);
    m_pollIntervalSpin->setRange(30, 3600);
    m_pollIntervalSpin->setSuffix(QString::fromUtf8(" сек"));
    schedLayout->addRow(QString::fromUtf8("Интервал опроса:"), m_pollIntervalSpin);

    mainLayout->addWidget(schedGroup);

    // Auction Settings
    auto* auctionGroup = new QGroupBox(QString::fromUtf8("Аукцион"), this);
    auto* auctionLayout = new QFormLayout(auctionGroup);

    m_lotsLimitSpin = new QSpinBox(this);
    m_lotsLimitSpin->setRange(10, 200);
    auctionLayout->addRow(QString::fromUtf8("Лимит лотов за запрос:"), m_lotsLimitSpin);

    m_outlierNSpin = new QSpinBox(this);
    m_outlierNSpin->setRange(5, 100);
    auctionLayout->addRow(QString::fromUtf8("N первых лотов (фильтр выбросов):"), m_outlierNSpin);

    mainLayout->addWidget(auctionGroup);

    // Threshold Settings
    auto* threshGroup = new QGroupBox(QString::fromUtf8("Пороги сигналов"), this);
    auto* threshLayout = new QFormLayout(threshGroup);

    m_watchSpin = new QDoubleSpinBox(this);
    m_watchSpin->setRange(0.0, 10.0);
    m_watchSpin->setSingleStep(0.1);
    threshLayout->addRow(QString::fromUtf8("WATCH (наблюдать):"), m_watchSpin);

    m_buySpin = new QDoubleSpinBox(this);
    m_buySpin->setRange(0.0, 10.0);
    m_buySpin->setSingleStep(0.1);
    threshLayout->addRow(QString::fromUtf8("BUY (покупать):"), m_buySpin);

    mainLayout->addWidget(threshGroup);

    // Database Settings
    auto* dbGroup = new QGroupBox(QString::fromUtf8("База данных PostgreSQL"), this);
    auto* dbLayout = new QFormLayout(dbGroup);

    m_dbHostEdit = new QLineEdit(this);
    dbLayout->addRow(QString::fromUtf8("Хост:"), m_dbHostEdit);

    m_dbPortSpin = new QSpinBox(this);
    m_dbPortSpin->setRange(1, 65535);
    dbLayout->addRow(QString::fromUtf8("Порт:"), m_dbPortSpin);

    m_dbNameEdit = new QLineEdit(this);
    dbLayout->addRow(QString::fromUtf8("Имя БД:"), m_dbNameEdit);

    m_dbUserEdit = new QLineEdit(this);
    dbLayout->addRow(QString::fromUtf8("Пользователь:"), m_dbUserEdit);

    m_dbPasswordEdit = new QLineEdit(this);
    m_dbPasswordEdit->setEchoMode(QLineEdit::Password);
    dbLayout->addRow(QString::fromUtf8("Пароль:"), m_dbPasswordEdit);

    mainLayout->addWidget(dbGroup);

    // Save button
    auto* btnLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(this);
    btnLayout->addWidget(m_statusLabel);
    btnLayout->addStretch();

    m_saveBtn = new QPushButton(QString::fromUtf8("Сохранить"), this);
    m_saveBtn->setMinimumWidth(120);
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);

    mainLayout->addStretch();

    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsWidget::saveToConfig);
}

void SettingsWidget::loadFromConfig() {
    m_regionCombo->setCurrentText(m_config->region());
    m_clientIdEdit->setText(m_config->clientId());
    m_clientSecretEdit->setText(m_config->clientSecret());
    m_pollIntervalSpin->setValue(m_config->pollIntervalSec());
    m_lotsLimitSpin->setValue(m_config->lotsLimit());
    m_outlierNSpin->setValue(m_config->outlierFilterN());
    m_watchSpin->setValue(m_config->watchThreshold());
    m_buySpin->setValue(m_config->buyThreshold());
    m_dbHostEdit->setText(m_config->dbHost());
    m_dbPortSpin->setValue(m_config->dbPort());
    m_dbNameEdit->setText(m_config->dbName());
    m_dbUserEdit->setText(m_config->dbUser());
    m_dbPasswordEdit->setText(m_config->dbPassword());
}

void SettingsWidget::saveToConfig() {
    m_config->setRegion(m_regionCombo->currentText());
    m_config->setClientId(m_clientIdEdit->text());
    m_config->setClientSecret(m_clientSecretEdit->text());
    m_config->setPollIntervalSec(m_pollIntervalSpin->value());
    m_config->setLotsLimit(m_lotsLimitSpin->value());
    m_config->setOutlierFilterN(m_outlierNSpin->value());
    m_config->setWatchThreshold(m_watchSpin->value());
    m_config->setBuyThreshold(m_buySpin->value());
    m_config->setDbHost(m_dbHostEdit->text());
    m_config->setDbPort(m_dbPortSpin->value());
    m_config->setDbName(m_dbNameEdit->text());
    m_config->setDbUser(m_dbUserEdit->text());
    m_config->setDbPassword(m_dbPasswordEdit->text());

    m_statusLabel->setText(QString::fromUtf8("Настройки сохранены!"));
    m_statusLabel->setStyleSheet("color: green;");
}
