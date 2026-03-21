#include "Config.h"

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

QString Config::clientId() const {
    return m_settings.value("api/clientId", "").toString();
}

void Config::setClientId(const QString& id) {
    m_settings.setValue("api/clientId", id);
    emit configChanged();
}

QString Config::clientSecret() const {
    return m_settings.value("api/clientSecret", "").toString();
}

void Config::setClientSecret(const QString& secret) {
    m_settings.setValue("api/clientSecret", secret);
    emit configChanged();
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
