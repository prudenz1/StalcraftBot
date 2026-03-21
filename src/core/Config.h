#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

class Config : public QObject {
    Q_OBJECT
public:
    explicit Config(QObject* parent = nullptr);

    QString region() const;
    void setRegion(const QString& region);

    QString apiBaseUrl() const;

    /** Токен для Authorization: Bearer (или из переменной окружения STALCRAFT_API_BEARER). */
    QString bearerToken() const;
    /** @return false если на Windows не удалось зашифровать (DPAPI). */
    bool setBearerToken(const QString& token);
    void clearBearerToken();
    /** Есть ли сохранённый в настройках токен (без учёта env). */
    bool hasPersistedBearerToken() const;

    int pollIntervalSec() const;
    void setPollIntervalSec(int seconds);

    int lotsLimit() const;
    void setLotsLimit(int limit);

    int outlierFilterN() const;
    void setOutlierFilterN(int n);

    double watchThreshold() const;
    void setWatchThreshold(double val);

    double buyThreshold() const;
    void setBuyThreshold(double val);

    double auctionCommission() const;

    QString dbHost() const;
    void setDbHost(const QString& host);

    int dbPort() const;
    void setDbPort(int port);

    QString dbName() const;
    void setDbName(const QString& name);

    QString dbUser() const;
    void setDbUser(const QString& user);

    QString dbPassword() const;
    void setDbPassword(const QString& password);

signals:
    void configChanged();

private:
    QString loadPersistedBearerToken() const;

    QSettings m_settings;
};
