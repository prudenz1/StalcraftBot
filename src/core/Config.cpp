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
}

QString Config::region() const {
    return m_settings.value("api/region", "RU").toString();
}

void Config::setRegion(const QString& region) {
    m_settings.setValue("api/region", region);
    emit configChanged();
}

QString Config::apiBaseUrl() const {
    return m_settings.value("api/baseUrl", "https://eapi.stalcraft.net").toString();
}

QString Config::bearerToken() const {
    const QByteArray env = qgetenv("STALCRAFT_API_BEARER");
    /*if (!env.isEmpty()) {
        return QString::fromUtf8(env); // Из переменной окружения STALCRAFT_API_BEARER. преобразуем в QString.
    }*/
    return loadPersistedBearerToken();
}

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

void Config::clearBearerToken() {
#ifdef Q_OS_WIN
    m_settings.remove("api/bearerTokenDpapi");
#else
    m_settings.remove("api/bearerTokenPlain");
#endif
    emit configChanged();
}

bool Config::hasPersistedBearerToken() const {
#ifdef Q_OS_WIN
    return m_settings.contains("api/bearerTokenDpapi")
        && !m_settings.value("api/bearerTokenDpapi").toByteArray().isEmpty();
#else
    return !m_settings.value("api/bearerTokenPlain", "").toString().isEmpty();
#endif
}

int Config::pollIntervalSec() const {
    return m_settings.value("scheduler/pollIntervalSec", 300).toInt();
}

void Config::setPollIntervalSec(int seconds) {
    m_settings.setValue("scheduler/pollIntervalSec", seconds);
    emit configChanged();
}

int Config::lotsLimit() const {
    return m_settings.value("auction/lotsLimit", 100).toInt();
}

void Config::setLotsLimit(int limit) {
    m_settings.setValue("auction/lotsLimit", limit);
    emit configChanged();
}

int Config::outlierFilterN() const {
    return m_settings.value("auction/outlierFilterN", 20).toInt();
}

void Config::setOutlierFilterN(int n) {
    m_settings.setValue("auction/outlierFilterN", n);
    emit configChanged();
}

double Config::watchThreshold() const {
    return m_settings.value("thresholds/watch", 1.5).toDouble();
}

void Config::setWatchThreshold(double val) {
    m_settings.setValue("thresholds/watch", val);
    emit configChanged();
}

double Config::buyThreshold() const {
    return m_settings.value("thresholds/buy", 2.5).toDouble();
}

void Config::setBuyThreshold(double val) {
    m_settings.setValue("thresholds/buy", val);
    emit configChanged();
}

double Config::auctionCommission() const {
    return 0.05;
}

QString Config::dbHost() const {
    return m_settings.value("db/host", "localhost").toString();
}

void Config::setDbHost(const QString& host) {
    m_settings.setValue("db/host", host);
    emit configChanged();
}

int Config::dbPort() const {
    return m_settings.value("db/port", 5432).toInt();
}

void Config::setDbPort(int port) {
    m_settings.setValue("db/port", port);
    emit configChanged();
}

QString Config::dbName() const {
    return m_settings.value("db/name", "stalcraft_bot").toString();
}

void Config::setDbName(const QString& name) {
    m_settings.setValue("db/name", name);
    emit configChanged();
}

QString Config::dbUser() const {
    return m_settings.value("db/user", "postgres").toString();
}

void Config::setDbUser(const QString& user) {
    m_settings.setValue("db/user", user);
    emit configChanged();
}

QString Config::dbPassword() const {
    return m_settings.value("db/password", "").toString();
}

void Config::setDbPassword(const QString& password) {
    m_settings.setValue("db/password", password);
    emit configChanged();
}
