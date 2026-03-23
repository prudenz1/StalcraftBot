#include "Scheduler.h"
#include "Config.h"
#include "Database.h"
#include "ApiClient.h"
#include "PriceAnalyzer.h"
#include "DealDetector.h"
#include "utils/Logger.h"

#include <algorithm>
#include <numeric>
#include <cmath>

Scheduler::Scheduler(Config* config, Database* db, ApiClient* api,
                     PriceAnalyzer* analyzer, DealDetector* detector,
                     QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_db(db)
    , m_api(api)
    , m_analyzer(analyzer)
    , m_detector(detector)
    , m_pollTimer(new QTimer(this))
{
    connect(m_pollTimer, &QTimer::timeout, this, &Scheduler::onPollTimer);
    connect(m_api, &ApiClient::lotsFetched, this, &Scheduler::onLotsFetched);
    connect(m_config, &Config::configChanged, this, [this]() {
        if (m_pollTimer->isActive()) {
            m_pollTimer->setInterval(m_config->pollIntervalSec() * 1000);
        }
    });
}

void Scheduler::start() {
    int intervalMs = m_config->pollIntervalSec() * 1000;
    m_pollTimer->start(intervalMs);
    LOG_INFO("Scheduler started with interval {} sec", m_config->pollIntervalSec());
    onPollTimer();
}

void Scheduler::stop() {
    m_pollTimer->stop();
    m_polling = false;
    LOG_INFO("Scheduler stopped");
}

bool Scheduler::isRunning() const {
    return m_pollTimer->isActive();
}

void Scheduler::onPollTimer() {
    if (m_polling) {
        LOG_WARN("Previous poll still running, skipping");
        return;
    }

    QVector<Item> tracked = m_db->trackedItems();
    if (tracked.isEmpty()) {
        LOG_DEBUG("No tracked items, skipping poll");
        return;
    }

    // Group tracked entries by itemId to fetch lots once per item
    QMap<QString, QVector<int>> qualityMap;
    for (const auto& item : tracked) {
        qualityMap[item.id].append(item.quality);
    }

    m_polling = true;
    m_itemQueue.clear();
    for (auto it = qualityMap.begin(); it != qualityMap.end(); ++it) {
        QueueEntry entry;
        entry.itemId = it.key();
        entry.qualities = it.value();
        m_itemQueue.enqueue(entry);
    }

    emit pollStarted();
    LOG_INFO("Poll started for {} items ({} tracking entries)",
             m_itemQueue.size(), tracked.size());
    processNextItem();
}

void Scheduler::processNextItem() {
    if (m_itemQueue.isEmpty()) {
        m_polling = false;
        emit pollFinished();
        LOG_INFO("Poll cycle completed");
        return;
    }

    m_currentEntry = m_itemQueue.dequeue();
    emit pollItemStarted(m_currentEntry.itemId);
    m_api->fetchLots(m_currentEntry.itemId, 0, m_config->lotsLimit());
}

void Scheduler::onLotsFetched(const QString& itemId, const QVector<Lot>& lots, int total) {
    if (itemId != m_currentEntry.itemId) return;

    LOG_INFO("Processing {} lots for item {} (total on auction: {})",
             lots.size(), itemId.toStdString(), total);

    m_db->insertLotSnapshots(itemId, lots);

    for (int quality : m_currentEntry.qualities) {
        QVector<Lot> filtered;
        if (quality >= 0) {
            for (const auto& lot : lots) {
                if (lot.quality == quality) filtered.append(lot);
            }
        } else {
            filtered = lots;
        }
        aggregateAndAnalyze(itemId, quality, filtered);
    }

    emit pollItemFinished(itemId);
    processNextItem();
}

void Scheduler::aggregateAndAnalyze(const QString& itemId, int quality,
                                     const QVector<Lot>& lots) {
    if (lots.isEmpty()) return;

    QVector<qint64> prices;
    prices.reserve(lots.size());
    for (const auto& lot : lots) {
        if (lot.buyoutPrice > 0) {
            prices.append(lot.buyoutPrice);
        }
    }

    if (prices.isEmpty()) return;

    std::sort(prices.begin(), prices.end());

    int filterN = m_config->outlierFilterN();
    int useCount = qMin(filterN, prices.size());
    QVector<qint64> filtered(prices.begin(), prices.begin() + useCount);

    qint64 minP = filtered.first();
    qint64 maxP = filtered.last();
    double sum = std::accumulate(filtered.begin(), filtered.end(), 0.0);
    qint64 avgP = static_cast<qint64>(sum / useCount);

    qint64 medianP;
    if (useCount % 2 == 0) {
        medianP = (filtered[useCount / 2 - 1] + filtered[useCount / 2]) / 2;
    } else {
        medianP = filtered[useCount / 2];
    }

    double variance = 0.0;
    for (qint64 p : filtered) {
        double diff = static_cast<double>(p) - static_cast<double>(avgP);
        variance += diff * diff;
    }
    double stdDev = std::sqrt(variance / useCount);

    PriceSnapshot snap;
    snap.itemId = itemId;
    snap.quality = quality;
    snap.minPrice = minP;
    snap.avgPrice = avgP;
    snap.medianPrice = medianP;
    snap.maxPrice = maxP;
    snap.stdDev = stdDev;
    snap.lotCount = prices.size();
    snap.filteredCount = useCount;

    m_db->insertPriceSnapshot(snap);

    int currentHour = QDateTime::currentDateTime().time().hour();
    m_db->upsertHourlyStat(itemId, quality, currentHour, medianP, 1);

    m_analyzer->analyze(itemId, quality, snap);
    m_detector->evaluate(itemId, quality, lots, snap);
}
