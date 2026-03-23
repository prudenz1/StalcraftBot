#include "DealDetector.h"
#include "Database.h"
#include "Config.h"
#include "PriceAnalyzer.h"
#include "utils/Logger.h"

#include <algorithm>

DealDetector::DealDetector(Database* db, Config* config,
                           PriceAnalyzer* analyzer, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_config(config)
    , m_analyzer(analyzer)
{
}

void DealDetector::evaluate(const QString& itemId, int quality,
                            const QVector<Lot>& lots, const PriceSnapshot& snapshot) {
    m_currentItemName = m_db->itemName(itemId);
    checkCheapLots(itemId, quality, lots, snapshot);
    checkAnalysisSignal(itemId, quality, snapshot);
}

void DealDetector::checkCheapLots(const QString& itemId, int quality,
                                   const QVector<Lot>& lots,
                                   const PriceSnapshot& snapshot) {
    if (snapshot.medianPrice <= 0) return;

    double commission = m_config->auctionCommission();
    qint64 sellPrice = static_cast<qint64>(snapshot.medianPrice * (1.0 - commission));

    for (const auto& lot : lots) {
        if (lot.buyoutPrice <= 0) continue;

        qint64 profit = sellPrice - lot.buyoutPrice;
        if (profit <= 0) continue;

        double profitPct = (static_cast<double>(profit) / lot.buyoutPrice) * 100.0;

        if (profitPct < 5.0) continue;

        Alert alert;
        alert.itemId = itemId;
        alert.quality = quality;
        alert.itemName = m_currentItemName;
        alert.type = AlertType::CheapLot;
        alert.rating = profitPct;
        alert.message = QString::fromUtf8(
            "Дешёвый лот! Цена: %1, медиана: %2, прибыль после комиссии 5%%: %3 (%4%)")
            .arg(lot.buyoutPrice)
            .arg(snapshot.medianPrice)
            .arg(profit)
            .arg(QString::number(profitPct, 'f', 1));

        m_db->insertAlert(alert);
        emit alertGenerated(alert);

        LOG_INFO("CHEAP LOT detected for {} q={}: price={}, median={}, profit={} ({:.1f}%)",
                 itemId.toStdString(), quality, lot.buyoutPrice, snapshot.medianPrice,
                 profit, profitPct);
    }
}

void DealDetector::checkAnalysisSignal(const QString& itemId, int quality,
                                        const PriceSnapshot& snapshot) {
    AnalysisResult result = m_analyzer->lastResult(itemId, quality);
    if (result.signal == Signal::None) return;

    Alert alert;
    alert.itemId = itemId;
    alert.quality = quality;
    alert.itemName = m_currentItemName;
    alert.rating = result.rating;

    if (result.signal == Signal::Buy) {
        alert.type = AlertType::Buy;
        alert.message = QString::fromUtf8(
            "Сигнал КУПИТЬ! Рейтинг: %1, Z-Score: %2, тренд: %3, текущая цена: %4")
            .arg(QString::number(result.rating, 'f', 2),
                 QString::number(result.zScore, 'f', 2),
                 (result.trend == Trend::Down ? "DOWN" : result.trend == Trend::Up ? "UP" : "FLAT"),
                 QString::number(result.currentPrice));
    } else {
        alert.type = AlertType::Watch;
        alert.message = QString::fromUtf8(
            "Сигнал НАБЛЮДАТЬ! Рейтинг: %1, Z-Score: %2, тренд: %3, текущая цена: %4")
            .arg(QString::number(result.rating, 'f', 2),
                 QString::number(result.zScore, 'f', 2),
                 (result.trend == Trend::Down ? "DOWN" : result.trend == Trend::Up ? "UP" : "FLAT"),
                 QString::number(result.currentPrice));
    }

    m_db->insertAlert(alert);
    emit alertGenerated(alert);
}
