#pragma once

#include <QWidget>
#include <QComboBox>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QMap>
#include <QVector>

#include "models/Lot.h"

class Database;
class ApiClient;

class ActiveLotsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ActiveLotsWidget(Database* db, ApiClient* api, QWidget* parent = nullptr);

public slots:
    void onLotsReceived(const QString& itemId, const QVector<Lot>& lots, int total);
    void refreshItemList();

private slots:
    void onItemSelected();
    void onRefreshClicked();

private:
    void setupUi();
    void displayLots(const QString& itemId);

    Database* m_db;
    ApiClient* m_api;

    QMap<QString, QVector<Lot>> m_lotsCache;
    QMap<QString, int> m_totalsCache;

    QComboBox* m_itemCombo = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTableWidget* m_table = nullptr;
};
