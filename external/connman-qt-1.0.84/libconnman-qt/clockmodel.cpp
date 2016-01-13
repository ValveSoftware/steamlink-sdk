/*
 * Copyright © 2010, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include "clockmodel.h"
#include "connman_clock_interface.h"

#define CONNMAN_SERVICE "net.connman"
#define CONNMAN_CLOCK_INTERFACE CONNMAN_SERVICE ".Clock"

#define SET_CONNMAN_PROPERTY(key, val) \
        if (!mClockProxy) { \
            qCritical("ClockModel: SetProperty: not connected to connman"); \
        } else { \
            QDBusPendingReply<> reply = mClockProxy->SetProperty(key, QDBusVariant(val)); \
            QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this); \
            connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), \
                    this, SLOT(setPropertyFinished(QDBusPendingCallWatcher*))); \
        }

ClockModel::ClockModel() :
    mClockProxy(0)
{
    QTimer::singleShot(0,this,SLOT(connectToConnman()));
}

void ClockModel::connectToConnman()
{
    if (mClockProxy && mClockProxy->isValid())
        return;

    mClockProxy = new NetConnmanClockInterface(CONNMAN_SERVICE, "/", QDBusConnection::systemBus(),
        this);

    if (!mClockProxy->isValid()) {
        qCritical("ClockModel: unable to connect to connman");
        delete mClockProxy;
        mClockProxy = NULL;
        return;
    }

    QDBusPendingReply<QVariantMap> reply = mClockProxy->GetProperties();
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher,
            SIGNAL(finished(QDBusPendingCallWatcher*)),
            this,
            SLOT(getPropertiesFinished(QDBusPendingCallWatcher*)));

    connect(mClockProxy,
            SIGNAL(PropertyChanged(const QString&, const QDBusVariant&)),
            this,
            SLOT(propertyChanged(const QString&, const QDBusVariant&)));
}

void ClockModel::getPropertiesFinished(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QVariantMap> reply = *call;
    if (reply.isError()) {
        qCritical() << "ClockModel: getProperties: " << reply.error().name() << reply.error().message();
    } else {
        QVariantMap properties = reply.value();

        if (properties.contains(QLatin1String("Timezone"))) {
            mTimezone = properties.value("Timezone").toString();
            Q_EMIT timezoneChanged();
        }
        if (properties.contains(QLatin1String("TimezoneUpdates"))) {
            mTimezoneUpdates = properties.value("TimezoneUpdates").toString();
            Q_EMIT timezoneUpdatesChanged();
        }
        if (properties.contains(QLatin1String("TimeUpdates"))) {
            mTimeUpdates = properties.value("TimeUpdates").toString();
            Q_EMIT timeUpdatesChanged();
        }
        if (properties.contains(QLatin1String("Timeservers"))) {
            mTimeservers = properties.value("Timeservers").toStringList();
            Q_EMIT timeserversChanged();
        }
    }
    call->deleteLater();
}

void ClockModel::setPropertyFinished(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<> reply = *call;
    if (reply.isError()) {
        qCritical() << "ClockModel: setProperty: " << reply.error().name() << reply.error().message();
    }
    call->deleteLater();
}

void ClockModel::propertyChanged(const QString &name, const QDBusVariant &value)
{
    if (name == "Timezone") {
        mTimezone = value.variant().toString();
        Q_EMIT timezoneChanged();
    } else if (name == "TimezoneUpdates") {
        mTimezoneUpdates = value.variant().toString();
        Q_EMIT timezoneUpdatesChanged();
    } else if (name == "TimeUpdates") {
        mTimeUpdates = value.variant().toString();
        Q_EMIT timeUpdatesChanged();
    } else if (name == "Timeservers") {
        mTimeservers = value.variant().toStringList();
        Q_EMIT timeserversChanged();
    }
}

QString ClockModel::timezone() const
{
    return mTimezone;
}

void ClockModel::setTimezone(const QString &val)
{
    SET_CONNMAN_PROPERTY("Timezone", val);
}

QString ClockModel::timezoneUpdates() const
{
    return mTimezoneUpdates;
}

void ClockModel::setTimezoneUpdates(const QString &val)
{
    SET_CONNMAN_PROPERTY("TimezoneUpdates", val);
}

QString ClockModel::timeUpdates() const
{
    return mTimeUpdates;
}

void ClockModel::setTimeUpdates(const QString &val)
{
    SET_CONNMAN_PROPERTY("TimeUpdates", val);
}

QStringList ClockModel::timeservers() const
{
    return mTimeservers;
}

void ClockModel::setTimeservers(const QStringList &val)
{
    SET_CONNMAN_PROPERTY("Timeservers", val);
}

void ClockModel::setDate(QDate date)
{
    QDateTime toDate(date, QTime::currentTime());
    quint64 secsSinceEpoch = (quint64)toDate.toTime_t();
    SET_CONNMAN_PROPERTY("Time", secsSinceEpoch);
}

void ClockModel::setTime(QTime time)
{
    QDateTime toDate(QDate::currentDate(), time);
    quint64 secsSinceEpoch = (quint64)toDate.toTime_t();
    SET_CONNMAN_PROPERTY("Time", secsSinceEpoch);
}
