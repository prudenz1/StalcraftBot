#pragma once

#include <QObject>
#include <QVector>
#include "models/Lot.h"
#include "models/PriceSnapshot.h"
#include "models/Alert.h"

class Database;
class Config;
class PriceAnalyzer;

class DealDetector : public QObject {
    Q_OBJECT
public:
    explicit DealDetector(Database* db, Config* config,
                          PriceAnalyzer* analyzer, QObject* parent = nullptr);

    void evaluate(const QString& itemId, int quality,
                  const QVector<Lot>& lots, const PriceSnapshot& snapshot);

signals:
    void alertGenerated(const Alert& alert);

private:
    void checkCheapLots(const QString& itemId, int quality,
                        const QVector<Lot>& lots, const PriceSnapshot& snapshot);
    void checkAnalysisSignal(const QString& itemId, int quality,
                             const PriceSnapshot& snapshot);

    Database* m_db;
    Config* m_config;
    PriceAnalyzer* m_analyzer;
    QString m_currentItemName;
};
