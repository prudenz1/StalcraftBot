#pragma once

#include <QObject>
#include <QTimer>
#include <QQueue>
#include <QVector>
#include <QMap>
#include "models/Lot.h"

class Config;
class Database;
class ApiClient;
class PriceAnalyzer;
class DealDetector;

class Scheduler : public QObject {
    Q_OBJECT
public:
    explicit Scheduler(Config* config, Database* db, ApiClient* api,
                       PriceAnalyzer* analyzer, DealDetector* detector,
                       QObject* parent = nullptr);

    void start();
    void stop();
    bool isRunning() const;

signals:
    void pollStarted();
    void pollFinished();
    void pollItemStarted(const QString& itemId);
    void pollItemFinished(const QString& itemId);

private slots:
    void onPollTimer();
    void onLotsFetched(const QString& itemId, const QVector<Lot>& lots, int total);

private:
    struct QueueEntry {
        QString itemId;
        QVector<int> qualities;
    };

    void processNextItem();
    void aggregateAndAnalyze(const QString& itemId, int quality, const QVector<Lot>& lots);

    Config* m_config;
    Database* m_db;
    ApiClient* m_api;
    PriceAnalyzer* m_analyzer;
    DealDetector* m_detector;

    QTimer* m_pollTimer;
    QQueue<QueueEntry> m_itemQueue;
    QueueEntry m_currentEntry;
    bool m_polling = false;
};
