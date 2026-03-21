#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>

class Database;
class PriceAnalyzer;

class PriceTableWidget : public QWidget {
    Q_OBJECT
public:
    explicit PriceTableWidget(Database* db, PriceAnalyzer* analyzer,
                              QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    void setupUi();

    Database* m_db;
    PriceAnalyzer* m_analyzer;

    QTableWidget* m_table = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QLabel* m_lastUpdate = nullptr;
};
