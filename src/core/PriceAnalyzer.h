#pragma once

#include <QObject>
#include "models/PriceSnapshot.h"

class Database;
class Config;

enum class Trend { Down, Flat, Up };
enum class DataLevel { Level1, Level2, Level3 };
enum class Signal { None, Watch, Buy };

struct AnalysisResult {
    QString itemId;
    double zScore = 0.0;
    Trend trend = Trend::Flat;
    double trendCoeff = 1.0;
    double timeCoeffRaw = 1.0;
    double timeCoeffNorm = 1.0;
    double rating = 0.0;
    DataLevel level = DataLevel::Level1;
    Signal signal = Signal::None;
    qint64 currentPrice = 0;
    qint64 avgPrice = 0;
    double stdDev = 0.0;
};

class PriceAnalyzer : public QObject {
    Q_OBJECT
public:
    explicit PriceAnalyzer(Database* db, Config* config, QObject* parent = nullptr);

    AnalysisResult analyze(const QString& itemId, const PriceSnapshot& current);

    AnalysisResult lastResult(const QString& itemId) const;

signals:
    void analysisCompleted(const QString& itemId, const AnalysisResult& result);

private:
    DataLevel determineLevel(int daysOfData) const;
    Trend determineTrend(const QVector<PriceSnapshot>& history) const;
    double trendCoefficient(Trend t) const;
    double computeTimeCoeffRaw(const QString& itemId, qint64 overallAvg) const;
    double normalizeTimeCoeff(double raw, double minCoeff, double maxCoeff) const;
    Signal determineSignal(double rating, DataLevel level) const;

    Database* m_db;
    Config* m_config;
    QMap<QString, AnalysisResult> m_lastResults;
};
