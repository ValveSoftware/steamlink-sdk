/*
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include <QtDBus/QDBusConnection>

#include "counter.h"
#include "networkmanager.h"


Counter::Counter(QObject *parent) :
    QObject(parent),
    m_manager(NetworkManagerFactory::createInstance()),
    bytesInHome(0),
    bytesOutHome(0),
    secondsOnlineHome(0),
    bytesInRoaming(0),
    bytesOutRoaming(0),
    secondsOnlineRoaming(0),
    roamingEnabled(false),
    currentInterval(1),
    currentAccuracy(1024),
    shouldBeRunning(false),
    registered(false)
{
    QTime time = QTime::currentTime();
    qsrand((uint)time.msec());
    int randomValue = qrand();
    //this needs to be unique so we can use more than one at a time with different processes
    counterPath = "/ConnectivityCounter" + QString::number(randomValue);

    new CounterAdaptor(this);
    if (!QDBusConnection::systemBus().registerObject(counterPath, this))
        qWarning("Could not register DBus object on %s", qPrintable(counterPath));

    connect(m_manager, SIGNAL(availabilityChanged(bool)), this, SLOT(updateCounterAgent()));
}

Counter::~Counter()
{
    if (registered)
        m_manager->unregisterCounter(counterPath);
}

void Counter::serviceUsage(const QString &servicePath, const QVariantMap &counters,  bool roaming)
{
    Q_EMIT counterChanged(servicePath, counters, roaming);

    if (roaming != roamingEnabled) {
        roamingEnabled = roaming;
        Q_EMIT roamingChanged(roaming);
    }

    quint64 rxbytes = counters["RX.Bytes"].toULongLong();
    quint64 txbytes = counters["TX.Bytes"].toULongLong();
    quint32 time = counters["Time"].toUInt();

    if (roaming) {
        if (rxbytes != 0) {
            bytesInRoaming = rxbytes;
        }
        if (txbytes != 0) {
            bytesOutRoaming = txbytes;
        }
        if (time != 0) {
            secondsOnlineRoaming = time;
        }
    } else {
        if (rxbytes != 0) {
            bytesInHome = rxbytes;
        }
        if (txbytes != 0) {
            bytesOutHome = txbytes;
        }
        if (time != 0) {
            secondsOnlineHome = time;
        }
    }

    if (rxbytes != 0)
        Q_EMIT bytesReceivedChanged(rxbytes);
    if (txbytes != 0)
        Q_EMIT bytesTransmittedChanged(txbytes);
    if (time != 0)
        Q_EMIT secondsOnlineChanged(time);
}

void Counter::release()
{
    registered = false;
    Q_EMIT runningChanged(registered);
}

bool Counter::roaming() const
{
    return roamingEnabled;
}

quint64 Counter::bytesReceived() const
{
    if (roamingEnabled) {
        return bytesInRoaming;
    } else {
        return bytesInHome;
    }
}

quint64 Counter::bytesTransmitted() const
{
    if (roamingEnabled) {
        return bytesOutRoaming;
    } else {
        return bytesOutHome;
    }
}

quint32 Counter::secondsOnline() const
{
    if (roamingEnabled) {
        return secondsOnlineRoaming;
    } else {
        return secondsOnlineHome;
    }
}

/*
 *The accuracy value is in kilo-bytes. It defines
            the update threshold.

Changing the accuracy will reset the counters, as it will
need to be re registered from the manager.
*/
void Counter::setAccuracy(quint32 accuracy)
{
    if (currentAccuracy == accuracy)
        return;

    currentAccuracy = accuracy;
    Q_EMIT accuracyChanged(accuracy);
    updateCounterAgent();
}

quint32 Counter::accuracy() const
{
    return currentAccuracy;
}

/*
 *The interval value is in seconds.
Changing the accuracy will reset the counters, as it will
need to be re registered from the manager.
*/
void Counter::setInterval(quint32 interval)
{
    if (currentInterval == interval)
        return;

    currentInterval = interval;
    Q_EMIT intervalChanged(interval);
    updateCounterAgent();
}

quint32 Counter::interval() const
{
    return currentInterval;
}

void Counter::setRunning(bool on)
{
    if (shouldBeRunning == on)
        return;

    shouldBeRunning = on;
    updateCounterAgent();
}

void Counter::updateCounterAgent()
{
    if (!m_manager->isAvailable()) {
        if (registered) {
            registered = false;
            Q_EMIT runningChanged(registered);
        }
        return;
    }

    if (registered) {
        m_manager->unregisterCounter(counterPath);
        if (!shouldBeRunning) {
            registered = false;
            Q_EMIT runningChanged(registered);
            return;
        }
    }

    if (shouldBeRunning) {
        m_manager->registerCounter(counterPath, currentAccuracy, currentInterval);
        if (!registered) {
            registered = true;
            Q_EMIT runningChanged(registered);
        }
    }
}

bool Counter::running() const
{
    return registered;
}

/*
 *This is the dbus adaptor to the connman interface
 **/
CounterAdaptor::CounterAdaptor(Counter* parent)
  : QDBusAbstractAdaptor(parent),
    m_counter(parent)
{
}

CounterAdaptor::~CounterAdaptor()
{
}

void CounterAdaptor::Release()
{
     m_counter->release();
}

void CounterAdaptor::Usage(const QDBusObjectPath &service_path,
                           const QVariantMap &home,
                           const QVariantMap &roaming)
{
    if (!home.isEmpty()) {
        // home
        m_counter->serviceUsage(service_path.path(), home, false);
    }
    if (!roaming.isEmpty()) {
        //roaming
        m_counter->serviceUsage(service_path.path(), roaming, true);
    }
}

