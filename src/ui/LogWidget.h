#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>

#include "models/Alert.h"

class Database;

class LogWidget : public QWidget {
    Q_OBJECT
public:
    explicit LogWidget(Database* db, QWidget* parent = nullptr);

public slots:
    void addAlert(const Alert& alert);
    void refresh();

private:
    void setupUi();

    Database* m_db;

    QComboBox* m_filterCombo = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QTableWidget* m_table = nullptr;
};
