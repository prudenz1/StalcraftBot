#include "PriceAnalyzer.h"
#include "Database.h"
#include "Config.h"
#include "utils/Logger.h"

#include <cmath>
#include <algorithm>

PriceAnalyzer::PriceAnalyzer(Database* db, Config* config, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_config(config)
{
}

AnalysisResult PriceAnalyzer::lastResult(const QString& itemId, int quality) const {
    return m_lastResults.value({itemId, quality});
}

AnalysisResult PriceAnalyzer::analyze(const QString& itemId, int quality,
                                       const PriceSnapshot& current) {
    AnalysisResult result;
    result.itemId = itemId;
    result.quality = quality;
    result.currentPrice = current.medianPrice;
    result.stdDev = current.stdDev;

    int daysOfData = m_db->daysSinceFirstSnapshot(itemId, quality);
    result.level = determineLevel(daysOfData);

    int lookbackDays = (daysOfData > 30) ? 30 : daysOfData;
    if (lookbackDays < 1) lookbackDays = 1;

    double overallAvg = m_db->overallAvgPrice(itemId, quality, lookbackDays);
    result.avgPrice = static_cast<qint64>(overallAvg);

    if (current.stdDev > 0.0) {
        result.zScore = (overallAvg - static_cast<double>(current.medianPrice)) / current.stdDev;
    }

    QVector<PriceSnapshot> history = m_db->priceHistory(itemId, quality, qMin(lookbackDays, 7));
    result.trend = determineTrend(history);
    result.trendCoeff = trendCoefficient(result.trend);

    result.timeCoeffNorm = 1.0;
    if (result.level != DataLevel::Level1 && overallAvg > 0) {
        auto hourlyData = m_db->hourlyStats(itemId, quality);
        if (!hourlyData.isEmpty()) {
            double minC = 1e9, maxC = -1e9;
            QMap<int, double> coeffMap;

            for (const auto& [hour, avgP] : hourlyData) {
                if (avgP > 0) {
                    double c = overallAvg / static_cast<double>(avgP);
                    coeffMap[hour] = c;
                    minC = std::min(minC, c);
                    maxC = std::max(maxC, c);
                }
            }

            double spread = maxC - minC;
            if (spread >= 0.05) {
                int currentHour = QDateTime::currentDateTime().time().hour();
                double rawCoeff = coeffMap.value(currentHour, 1.0);
                result.timeCoeffRaw = rawCoeff;
                result.timeCoeffNorm = normalizeTimeCoeff(rawCoeff, minC, maxC);
            }
        }
    }

    result.rating = result.zScore * result.trendCoeff * result.timeCoeffNorm;
    result.signal = determineSignal(result.rating, result.level);

    ResultKey key{itemId, quality};
    m_lastResults[key] = result;

    LOG_INFO("Analysis for {} q={}: Z={:.2f}, trend={}, trendC={:.2f}, timeC={:.2f}, "
             "rating={:.2f}, signal={}, level={}",
             itemId.toStdString(), quality, result.zScore,
             (result.trend == Trend::Down ? "DOWN" : result.trend == Trend::Up ? "UP" : "FLAT"),
             result.trendCoeff, result.timeCoeffNorm, result.rating,
             (result.signal == Signal::Buy ? "BUY" : result.signal == Signal::Watch ? "WATCH" : "NONE"),
             static_cast<int>(result.level) + 1);

    emit analysisCompleted(itemId, quality, result);
    return result;
}

DataLevel PriceAnalyzer::determineLevel(int daysOfData) const {
    if (daysOfData < 7) return DataLevel::Level1;
    if (daysOfData <= 30) return DataLevel::Level2;
    return DataLevel::Level3;
}

Trend PriceAnalyzer::determineTrend(const QVector<PriceSnapshot>& history) const {
    if (history.size() < 3) return Trend::Flat;

    int n = history.size();
    int recentCount = qMin(n, 5);

    double recentAvg = 0.0;
    for (int i = n - recentCount; i < n; ++i) {
        recentAvg += static_cast<double>(history[i].avgPrice);
    }
    recentAvg /= recentCount;

    int olderCount = qMin(n - recentCount, 5);
    if (olderCount < 1) olderCount = 1;
    double olderAvg = 0.0;
    int startIdx = qMax(0, n - recentCount - olderCount);
    for (int i = startIdx; i < n - recentCount; ++i) {
        olderAvg += static_cast<double>(history[i].avgPrice);
    }
    olderAvg /= (n - recentCount - startIdx);

    if (olderAvg == 0.0) return Trend::Flat;

    double change = (recentAvg - olderAvg) / olderAvg;
    if (change < -0.03) return Trend::Down;
    if (change > 0.03) return Trend::Up;
    return Trend::Flat;
}

double PriceAnalyzer::trendCoefficient(Trend t) const {
    switch (t) {
        case Trend::Down: return 1.2;
        case Trend::Flat: return 1.0;
        case Trend::Up:   return 0.8;
    }
    return 1.0;
}

double PriceAnalyzer::computeTimeCoeffRaw(const QString& itemId, int quality,
                                           qint64 overallAvg) const {
    if (overallAvg <= 0) return 1.0;
    int hour = QDateTime::currentDateTime().time().hour();

    auto stats = m_db->hourlyStats(itemId, quality);
    for (const auto& [h, avgP] : stats) {
        if (h == hour && avgP > 0) {
            return static_cast<double>(overallAvg) / static_cast<double>(avgP);
        }
    }
    return 1.0;
}

double PriceAnalyzer::normalizeTimeCoeff(double raw, double minCoeff, double maxCoeff) const {
    double spread = maxCoeff - minCoeff;
    if (spread < 0.001) return 1.0;

    double norm = 1.0 + (raw - minCoeff) * 0.3 / spread;
    return std::clamp(norm, 0.85, 1.15);
}

Signal PriceAnalyzer::determineSignal(double rating, DataLevel level) const {
    double watchThreshold, buyThreshold;
    if (level == DataLevel::Level1) {
        watchThreshold = 1.5;
        buyThreshold = 2.5;
    } else {
        watchThreshold = 1.8;
        buyThreshold = 3.0;
    }

    if (rating >= buyThreshold) return Signal::Buy;
    if (rating >= watchThreshold) return Signal::Watch;
    return Signal::None;
}
