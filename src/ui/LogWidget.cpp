#include "LogWidget.h"
#include "core/Database.h"
#include "models/Item.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

#include <QCoreApplication>
#include <QFile>

LogWidget::LogWidget(Database* db, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
{
    // Настраиваем таблицу логов и сразу наполняем её.
    setupUi();
    refresh();
}

// Инициализирует UI: фильтр, кнопка обновления и таблица уведомлений.
void LogWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel(QString::fromUtf8("Фильтр:"), this));

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem(QString::fromUtf8("Все"), "ALL");
    m_filterCombo->addItem(QString::fromUtf8("BUY"), "BUY");
    m_filterCombo->addItem(QString::fromUtf8("WATCH"), "WATCH");
    m_filterCombo->addItem(QString::fromUtf8("CHEAP_LOT"), "CHEAP_LOT");
    topLayout->addWidget(m_filterCombo);

    m_refreshBtn = new QPushButton(QString::fromUtf8("Обновить"), this);
    topLayout->addWidget(m_refreshBtn);

    topLayout->addStretch();
    layout->addLayout(topLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        QString::fromUtf8("Время"),
        QString::fromUtf8("Предмет"),
        QString::fromUtf8("Тип"),
        QString::fromUtf8("Рейтинг"),
        QString::fromUtf8("Сообщение")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    connect(m_refreshBtn, &QPushButton::clicked, this, &LogWidget::refresh);
    connect(m_filterCombo, &QComboBox::currentIndexChanged, this, &LogWidget::refresh);
}

// Добавляет новое уведомление в таблицу (используется сигналом `DealDetector`).
void LogWidget::addAlert(const Alert& alert) {
    int row = 0;
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(
        QDateTime::currentDateTime().toString("HH:mm:ss dd.MM")));
    QString displayName = alert.itemName.isEmpty() ? alert.itemId : alert.itemName;
    if (alert.quality >= 0)
        displayName += QStringLiteral(" [") + Item::qualityName(alert.quality) + QStringLiteral("]");
    m_table->setItem(row, 1, new QTableWidgetItem(displayName));

    QString typeStr = Alert::typeToString(alert.type);
    auto* typeItem = new QTableWidgetItem(typeStr);
    QColor typeColor;
    switch (alert.type) {
        case AlertType::Buy:      typeColor = QColor(46, 204, 113); break;
        case AlertType::Watch:    typeColor = QColor(241, 196, 15); break;
        case AlertType::CheapLot: typeColor = QColor(52, 152, 219); break;
    }
    typeItem->setForeground(typeColor);
    m_table->setItem(row, 2, typeItem);

    m_table->setItem(row, 3, new QTableWidgetItem(QString::number(alert.rating, 'f', 2)));
    m_table->setItem(row, 4, new QTableWidgetItem(alert.message));

#ifdef Q_OS_WIN
    if (alert.type == AlertType::Buy) {
        QString soundPath = QCoreApplication::applicationDirPath() + "/buy_alert.wav";
        if (QFile::exists(soundPath)) {
            PlaySound(reinterpret_cast<LPCWSTR>(soundPath.utf16()), nullptr, SND_FILENAME | SND_ASYNC);
        } else {
            MessageBeep(MB_ICONEXCLAMATION);
        }
    }
#endif
}

// Перезагружает список уведомлений из БД с учётом выбранного фильтра.
void LogWidget::refresh() {
    auto alerts = m_db->recentAlerts(500);
    QString filter = m_filterCombo->currentData().toString();

    m_table->setRowCount(0);

    for (const auto& alert : alerts) {
        if (filter != "ALL" && Alert::typeToString(alert.type) != filter) {
            continue;
        }

        int row = m_table->rowCount();
        m_table->insertRow(row);

        m_table->setItem(row, 0, new QTableWidgetItem(
            alert.createdAt.toString("HH:mm:ss dd.MM")));
        QString displayName = alert.itemName.isEmpty() ? alert.itemId : alert.itemName;
        if (alert.quality >= 0)
            displayName += QStringLiteral(" [") + Item::qualityName(alert.quality) + QStringLiteral("]");
        m_table->setItem(row, 1, new QTableWidgetItem(displayName));

        QString typeStr = Alert::typeToString(alert.type);
        auto* typeItem = new QTableWidgetItem(typeStr);
        QColor typeColor;
        switch (alert.type) {
            case AlertType::Buy:      typeColor = QColor(46, 204, 113); break;
            case AlertType::Watch:    typeColor = QColor(241, 196, 15); break;
            case AlertType::CheapLot: typeColor = QColor(52, 152, 219); break;
        }
        typeItem->setForeground(typeColor);
        m_table->setItem(row, 2, typeItem);

        m_table->setItem(row, 3, new QTableWidgetItem(QString::number(alert.rating, 'f', 2)));
        m_table->setItem(row, 4, new QTableWidgetItem(alert.message));
    }
}
