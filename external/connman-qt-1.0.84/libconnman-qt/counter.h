/*
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef COUNTER_H
#define COUNTER_H

#include <QObject>
#include <QVariantMap>
#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusObjectPath>

class NetworkManager;

/*
 * Proxy class for interface net.connman.Counter
 */
//  static const char counterPath[] = "/ConnectivityCounter";

class Counter : public QObject
{
    Q_OBJECT

    Q_PROPERTY(quint64 bytesReceived READ bytesReceived NOTIFY bytesReceivedChanged)
    Q_PROPERTY(quint64 bytesTransmitted READ bytesTransmitted NOTIFY bytesTransmittedChanged)
    Q_PROPERTY(quint32 secondsOnline READ secondsOnline NOTIFY secondsOnlineChanged)
    Q_PROPERTY(bool roaming READ roaming NOTIFY roamingChanged)
    Q_PROPERTY(quint32 accuracy READ accuracy WRITE setAccuracy NOTIFY accuracyChanged)
    Q_PROPERTY(quint32 interval READ interval WRITE setInterval NOTIFY intervalChanged)

    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)

    Q_DISABLE_COPY(Counter)
public:
      explicit Counter(QObject *parent = 0);

    virtual ~Counter();

    quint64 bytesReceived() const;
    quint64 bytesTransmitted() const;
    quint32 secondsOnline() const;

    bool roaming() const;

    quint32 accuracy() const;
    void setAccuracy(quint32 accuracy);

    quint32 interval() const;
    void setInterval(quint32 interval);

    bool running() const;
    void setRunning(bool on);

Q_SIGNALS:
    void counterChanged(const QString &servicePath, const QVariantMap &counters, bool roaming);
    void bytesReceivedChanged(quint64 bytesRx);
    void bytesTransmittedChanged(quint64 bytesTx);
    void secondsOnlineChanged(quint32 seconds);
    void roamingChanged(bool roaming);
    void accuracyChanged(quint32 accuracy);
    void intervalChanged(quint32 interval);
    void runningChanged(bool running);

private Q_SLOTS:
    void updateCounterAgent();

private:
    NetworkManager* m_manager;

    friend class CounterAdaptor;

    void serviceUsage(const QString &servicePath, const QVariantMap &counters,  bool roaming);
    void release();

    quint64 bytesInHome;
    quint64 bytesOutHome;
    quint32 secondsOnlineHome;

    quint64 bytesInRoaming;
    quint64 bytesOutRoaming;
    quint32 secondsOnlineRoaming;

    bool roamingEnabled;
    quint32 currentInterval;
    quint32 currentAccuracy;

    QString counterPath;
    bool shouldBeRunning;

    bool registered;
};

class CounterAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Counter")

public:
    explicit CounterAdaptor(Counter *parent);
    virtual ~CounterAdaptor();

public Q_SLOTS:
    void Release();
    void Usage(const QDBusObjectPath &service_path,
                                const QVariantMap &home,
                                const QVariantMap &roaming);

private:
    Counter *m_counter;
};
#endif // COUNTER_H
