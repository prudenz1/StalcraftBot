#include "Config.h"

#include <QByteArray>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {

#ifdef Q_OS_WIN
QByteArray protectDpapi(const QByteArray& plain) {
    if (plain.isEmpty()) {
        return {};
    }
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData()));
    in.cbData = static_cast<DWORD>(plain.size());
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"StalcraftBot API", nullptr, nullptr, nullptr, 0, &out)) {
        return {};
    }
    QByteArray result(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return result;
}

QByteArray unprotectDpapi(const QByteArray& blob) {
    if (blob.isEmpty()) {
        return {};
    }
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(blob.constData()));
    in.cbData = static_cast<DWORD>(blob.size());
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        return {};
    }
    QByteArray result(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return result;
}
#endif

} // namespace

Config::Config(QObject* parent)
    : QObject(parent)
    , m_settings("StalcraftBot", "StalcraftBot")
{
    // Хранилище настроек приложения (QSettings):
    // регион/API-параметры, планировщик, пороги, и параметры БД.
}

// Текущий регион API (используется `ApiClient` при построении URL).
QString Config::region() const {
    return m_settings.value("api/region", "RU").toString();
}

// Устанавливает регион API и уведомляет подписчиков о смене конфигурации.
void Config::setRegion(const QString& region) {
    m_settings.setValue("api/region", region);
    emit configChanged();
}

// Базовый URL eAPI Stalcraft (используется при сборке запросов в `ApiClient`).
QString Config::apiBaseUrl() const {
    return m_settings.value("api/baseUrl", "https://eapi.stalcraft.net").toString();
}

// Bearer-токен для API (в win32 хранится зашифрованно через DPAPI, иначе в plain).
QString Config::bearerToken() const {
    const QByteArray env = qgetenv("STALCRAFT_API_BEARER");
    /*if (!env.isEmpty()) {
        return QString::fromUtf8(env); // Из переменной окружения STALCRAFT_API_BEARER. преобразуем в QString.
    }*/
    return loadPersistedBearerToken();
}

// Загружает сохранённый токен из QSettings (дешифровка DPAPI на Windows).
QString Config::loadPersistedBearerToken() const {
#ifdef Q_OS_WIN
    const QByteArray blob = QByteArray::fromBase64(
        m_settings.value("api/bearerTokenDpapi").toByteArray());
    const QByteArray plain = unprotectDpapi(blob);
    return QString::fromUtf8(plain);
#else
    return m_settings.value("api/bearerTokenPlain", "").toString();
#endif
}

// Устанавливает токен: на Windows шифрует DPAPI, иначе сохраняет как есть.
bool Config::setBearerToken(const QString& token) {
    if (token.isEmpty()) {
        clearBearerToken();
        return true;
    }
#ifdef Q_OS_WIN
    const QByteArray enc = protectDpapi(token.toUtf8());
    if (enc.isEmpty()) {
        qWarning() << "CryptProtectData failed; bearer token not saved";
        return false;
    }
    m_settings.setValue("api/bearerTokenDpapi", enc.toBase64());
#else
    m_settings.setValue("api/bearerTokenPlain", token);
#endif
    emit configChanged();
    return true;
}

// Очищает сохранённый токен из QSettings.
void Config::clearBearerToken() {
#ifdef Q_OS_WIN
    m_settings.remove("api/bearerTokenDpapi");
#else
    m_settings.remove("api/bearerTokenPlain");
#endif
    emit configChanged();
}

// Проверяет, есть ли сохранённый токен.
bool Config::hasPersistedBearerToken() const {
#ifdef Q_OS_WIN
    return m_settings.contains("api/bearerTokenDpapi")
        && !m_settings.value("api/bearerTokenDpapi").toByteArray().isEmpty();
#else
    return !m_settings.value("api/bearerTokenPlain", "").toString().isEmpty();
#endif
}

// Интервал опроса в секундах (используется `Scheduler`).
int Config::pollIntervalSec() const {
    return m_settings.value("scheduler/pollIntervalSec", 300).toInt();
}

// Устанавливает интервал опроса и уведомляет подписчиков.
void Config::setPollIntervalSec(int seconds) {
    m_settings.setValue("scheduler/pollIntervalSec", seconds);
    emit configChanged();
}

// Лимит лотов на один запрос (используется `ApiClient::fetchLots` через `Scheduler`).
int Config::lotsLimit() const {
    return m_settings.value("auction/lotsLimit", 100).toInt();
}

// Устанавливает лимит лотов.
void Config::setLotsLimit(int limit) {
    m_settings.setValue("auction/lotsLimit", limit);
    emit configChanged();
}

// N первых лотов для фильтрации выбросов (used в `Scheduler::aggregateAndAnalyze`).
int Config::outlierFilterN() const {
    return m_settings.value("auction/outlierFilterN", 20).toInt();
}

// Устанавливает N для фильтрации выбросов.
void Config::setOutlierFilterN(int n) {
    m_settings.setValue("auction/outlierFilterN", n);
    emit configChanged();
}

// Порог для сигнала WATCH (используется `PriceAnalyzer::determineSignal`).
double Config::watchThreshold() const {
    return m_settings.value("thresholds/watch", 1.5).toDouble();
}

// Устанавливает порог WATCH.
void Config::setWatchThreshold(double val) {
    m_settings.setValue("thresholds/watch", val);
    emit configChanged();
}

// Порог для сигнала BUY (используется `PriceAnalyzer::determineSignal`).
double Config::buyThreshold() const {
    return m_settings.value("thresholds/buy", 2.5).toDouble();
}

// Устанавливает порог BUY.
void Config::setBuyThreshold(double val) {
    m_settings.setValue("thresholds/buy", val);
    emit configChanged();
}

// Комиссия аукциона (влияет на оценку прибыльности в `DealDetector::checkCheapLots`).
double Config::auctionCommission() const {
    return 0.05;
}

// Параметры подключения к PostgreSQL — передаются в `Database`.
QString Config::dbHost() const {
    return m_settings.value("db/host", "localhost").toString();
}

// Устанавливает хост БД.
void Config::setDbHost(const QString& host) {
    m_settings.setValue("db/host", host);
    emit configChanged();
}

// Порт БД.
int Config::dbPort() const {
    return m_settings.value("db/port", 5432).toInt();
}

// Устанавливает порт БД.
void Config::setDbPort(int port) {
    m_settings.setValue("db/port", port);
    emit configChanged();
}

// Имя БД.
QString Config::dbName() const {
    return m_settings.value("db/name", "stalcraft_bot").toString();
}

// Устанавливает имя БД.
void Config::setDbName(const QString& name) {
    m_settings.setValue("db/name", name);
    emit configChanged();
}

// Пользователь БД.
QString Config::dbUser() const {
    return m_settings.value("db/user", "postgres").toString();
}

// Устанавливает пользователя БД.
void Config::setDbUser(const QString& user) {
    m_settings.setValue("db/user", user);
    emit configChanged();
}

// Пароль БД.
QString Config::dbPassword() const {
    return m_settings.value("db/password", "").toString();
}

// Устанавливает пароль БД.
void Config::setDbPassword(const QString& password) {
    m_settings.setValue("db/password", password);
    emit configChanged();
}
