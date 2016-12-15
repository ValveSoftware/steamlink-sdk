/*
 * Copyright © 2010, Intel Corporation.
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef NETWORKSERVICE_H
#define NETWORKSERVICE_H

#include <QtDBus>

#define CONNECT_TIMEOUT 300000 // user is supposed to provide input for unconfigured networks
#define CONNECT_TIMEOUT_FAVORITE 60000

class NetConnmanServiceInterface;

class NetworkService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString type READ type NOTIFY typeChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QStringList security READ security NOTIFY securityChanged)
    Q_PROPERTY(uint strength READ strength NOTIFY strengthChanged)
    Q_PROPERTY(bool favorite READ favorite NOTIFY favoriteChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY autoConnectChanged)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(QVariantMap ipv4 READ ipv4 NOTIFY ipv4Changed)
    Q_PROPERTY(QVariantMap ipv4Config READ ipv4Config WRITE setIpv4Config NOTIFY ipv4ConfigChanged)
    Q_PROPERTY(QVariantMap ipv6 READ ipv6 NOTIFY ipv6Changed)
    Q_PROPERTY(QVariantMap ipv6Config READ ipv6Config WRITE setIpv6Config NOTIFY ipv6ConfigChanged)
    Q_PROPERTY(QStringList nameservers READ nameservers NOTIFY nameserversChanged)
    Q_PROPERTY(QStringList nameserversConfig READ nameserversConfig WRITE setNameserversConfig NOTIFY nameserversConfigChanged)
    Q_PROPERTY(QStringList domains READ domains NOTIFY domainsChanged)
    Q_PROPERTY(QStringList domainsConfig READ domainsConfig WRITE setDomainsConfig NOTIFY domainsConfigChanged)
    Q_PROPERTY(QVariantMap proxy READ proxy NOTIFY proxyChanged)
    Q_PROPERTY(QVariantMap proxyConfig READ proxyConfig WRITE setProxyConfig NOTIFY proxyConfigChanged)
    Q_PROPERTY(QVariantMap ethernet READ ethernet NOTIFY ethernetChanged)
    Q_PROPERTY(bool roaming READ roaming NOTIFY roamingChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QStringList timeservers READ timeservers NOTIFY timeserversChanged)
    Q_PROPERTY(QStringList timeserversConfig READ timeserversConfig WRITE setTimeserversConfig NOTIFY timeserversConfigChanged)

    Q_PROPERTY(QString bssid READ bssid NOTIFY bssidChanged)
    Q_PROPERTY(quint32 maxRate READ maxRate NOTIFY maxRateChanged)
    Q_PROPERTY(quint16 frequency READ frequency NOTIFY frequencyChanged)
    Q_PROPERTY(QString encryptionMode READ encryptionMode NOTIFY encryptionModeChanged)
    Q_PROPERTY(bool hidden READ hidden NOTIFY hiddenChanged)

public:
    NetworkService(const QString &path, const QVariantMap &properties, QObject* parent);
    NetworkService(QObject* parent = 0);

    virtual ~NetworkService();

    const QString name() const;
    const QString type() const;
    const QString state() const;
    const QString error() const;
    const QStringList security() const;
    bool autoConnect() const;
    uint strength() const;
    bool favorite() const;
    const QString path() const;
    const QVariantMap ipv4() const;
    const QVariantMap ipv4Config() const;
    const QVariantMap ipv6() const;
    const QVariantMap ipv6Config() const;
    const QStringList nameservers() const;
    const QStringList nameserversConfig() const;
    const QStringList domains() const;
    const QStringList domainsConfig() const;
    const QVariantMap proxy() const;
    const QVariantMap proxyConfig() const;
    const QVariantMap ethernet() const;
    bool roaming() const;

    void setPath(const QString &path);
    void updateProperties(const QVariantMap &properties);
    bool connected();

    QStringList timeservers() const;
    QStringList timeserversConfig() const;
    void setTimeserversConfig(const QStringList &servers);

    const QString bssid();
    quint32 maxRate();
    quint16 frequency();
    const QString encryptionMode();
    bool hidden() const;

Q_SIGNALS:
    void nameChanged(const QString &name);
    void stateChanged(const QString &state);
    void errorChanged(const QString &error);
    void securityChanged(const QStringList &security);
    void strengthChanged(const uint strength);
    void favoriteChanged(const bool &favorite);
    void autoConnectChanged(bool autoconnect);
    void pathChanged(const QString &path);
    void ipv4Changed(const QVariantMap &ipv4);
    void ipv4ConfigChanged(const QVariantMap &ipv4);
    void ipv6Changed(const QVariantMap &ipv6);
    void ipv6ConfigChanged(const QVariantMap &ipv6);
    void nameserversChanged(const QStringList &nameservers);
    void nameserversConfigChanged(const QStringList &nameservers);
    void domainsChanged(const QStringList &domains);
    void domainsConfigChanged(const QStringList &domains);
    void proxyChanged(const QVariantMap &proxy);
    void proxyConfigChanged(const QVariantMap &proxy);
    void ethernetChanged(const QVariantMap &ethernet);
    void connectRequestFailed(const QString &error);
    void typeChanged(const QString &type);
    void roamingChanged(bool roaming);
    void timeserversChanged(const QStringList &timeservers);
    void timeserversConfigChanged(const QStringList &timeservers);

    void serviceConnectionStarted();
    void serviceDisconnectionStarted();
    void connectedChanged(bool connected);

    void propertiesReady();

    void bssidChanged(const QString &bssid);
    void maxRateChanged(quint32 rate);
    void frequencyChanged(quint16 frequency);
    void encryptionModeChanged(const QString &mode);
    void hiddenChanged(bool);

public Q_SLOTS:
    void requestConnect();
    void requestDisconnect();
    void remove();

    void setAutoConnect(const bool autoconnect);
    void setIpv4Config(const QVariantMap &ipv4);
    void setIpv6Config(const QVariantMap &ipv6);
    void setNameserversConfig(const QStringList &nameservers);
    void setDomainsConfig(const QStringList &domains);
    void setProxyConfig(const QVariantMap &proxy);

    void resetCounters();

private:
    NetConnmanServiceInterface *m_service;
    QString m_path;
    QVariantMap m_propertiesCache;

    static const QString Name;
    static const QString State;
    static const QString Type;
    static const QString Security;
    static const QString Strength;
    static const QString Error;
    static const QString Favorite;
    static const QString AutoConnect;
    static const QString IPv4;
    static const QString IPv4Config;
    static const QString IPv6;
    static const QString IPv6Config;
    static const QString Nameservers;
    static const QString NameserversConfig;
    static const QString Domains;
    static const QString DomainsConfig;
    static const QString Proxy;
    static const QString ProxyConfig;
    static const QString Ethernet;
    static const QString Roaming;
    static const QString Timeservers;
    static const QString TimeserversConfig;

    static const QString BSSID;
    static const QString MaxRate;
    static const QString Frequency;
    static const QString EncryptionMode;
    static const QString Hidden;

    bool isConnected;

private Q_SLOTS:
    void updateProperty(const QString &name, const QDBusVariant &value);
    void emitPropertyChange(const QString &name, const QVariant &value);
    void getPropertiesFinished(QDBusPendingCallWatcher *call);

    void handleConnectReply(QDBusPendingCallWatcher *call);
    void handleRemoveReply(QDBusPendingCallWatcher *watcher);
    void handleAutoConnectReply(QDBusPendingCallWatcher*);

private:
    void resetProperties();
    void reconnectServiceInterface();

    Q_DISABLE_COPY(NetworkService)
};

Q_DECLARE_METATYPE(NetworkService*)

#endif // NETWORKSERVICE_H
