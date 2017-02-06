/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgeoareamonitor_polling.h"
#include <QtPositioning/qgeocoordinate.h>
#include <QtPositioning/qgeorectangle.h>
#include <QtPositioning/qgeocircle.h>

#include <QtCore/qmetaobject.h>
#include <QtCore/qtimer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qmutex.h>

#define UPDATE_INTERVAL_5S  5000

typedef QHash<QString, QGeoAreaMonitorInfo> MonitorTable;


static QMetaMethod areaEnteredSignal()
{
    static QMetaMethod signal = QMetaMethod::fromSignal(&QGeoAreaMonitorPolling::areaEntered);
    return signal;
}

static QMetaMethod areaExitedSignal()
{
    static QMetaMethod signal = QMetaMethod::fromSignal(&QGeoAreaMonitorPolling::areaExited);
    return signal;
}

static QMetaMethod monitorExpiredSignal()
{
    static QMetaMethod signal = QMetaMethod::fromSignal(&QGeoAreaMonitorPolling::monitorExpired);
    return signal;
}

class QGeoAreaMonitorPollingPrivate : public QObject
{
    Q_OBJECT
public:
    QGeoAreaMonitorPollingPrivate() : source(0), mutex(QMutex::Recursive)
    {
        nextExpiryTimer = new QTimer(this);
        nextExpiryTimer->setSingleShot(true);
        connect(nextExpiryTimer, SIGNAL(timeout()),
                this, SLOT(timeout()));
    }

    void startMonitoring(const QGeoAreaMonitorInfo &monitor)
    {
        QMutexLocker locker(&mutex);

        activeMonitorAreas.insert(monitor.identifier(), monitor);
        singleShotTrigger.remove(monitor.identifier());

        checkStartStop();
        setupNextExpiryTimeout();
    }

    void requestUpdate(const QGeoAreaMonitorInfo &monitor, int signalId)
    {
        QMutexLocker locker(&mutex);

        activeMonitorAreas.insert(monitor.identifier(), monitor);
        singleShotTrigger.insert(monitor.identifier(), signalId);

        checkStartStop();
        setupNextExpiryTimeout();
    }

    QGeoAreaMonitorInfo stopMonitoring(const QGeoAreaMonitorInfo &monitor)
    {
        QMutexLocker locker(&mutex);

        QGeoAreaMonitorInfo mon = activeMonitorAreas.take(monitor.identifier());

        checkStartStop();
        setupNextExpiryTimeout();

        return mon;
    }

    void registerClient(QGeoAreaMonitorPolling *client)
    {
        QMutexLocker locker(&mutex);

        connect(this, SIGNAL(timeout(QGeoAreaMonitorInfo)),
                client, SLOT(timeout(QGeoAreaMonitorInfo)));

        connect(this, SIGNAL(positionError(QGeoPositionInfoSource::Error)),
                client, SLOT(positionError(QGeoPositionInfoSource::Error)));

        connect(this, SIGNAL(areaEventDetected(QGeoAreaMonitorInfo,QGeoPositionInfo,bool)),
                client, SLOT(processAreaEvent(QGeoAreaMonitorInfo,QGeoPositionInfo,bool)));

        registeredClients.append(client);
    }

    void deregisterClient(QGeoAreaMonitorPolling *client)
    {
        QMutexLocker locker(&mutex);

        registeredClients.removeAll(client);
        if (registeredClients.isEmpty())
            checkStartStop();
    }

    void setPositionSource(QGeoPositionInfoSource *newSource)
    {
        QMutexLocker locker(&mutex);

        if (newSource == source)
            return;

        if (source)
            delete source;

        source = newSource;

        if (source) {
            source->setParent(this);
            source->moveToThread(this->thread());
            if (source->updateInterval() == 0)
                source->setUpdateInterval(UPDATE_INTERVAL_5S);
            disconnect(source, 0, 0, 0); //disconnect all
            connect(source, SIGNAL(positionUpdated(QGeoPositionInfo)),
                    this, SLOT(positionUpdated(QGeoPositionInfo)));
            connect(source, SIGNAL(error(QGeoPositionInfoSource::Error)),
                    this, SIGNAL(positionError(QGeoPositionInfoSource::Error)));
            checkStartStop();
        }
    }

    QGeoPositionInfoSource* positionSource() const
    {
        QMutexLocker locker(&mutex);
        return source;
    }

    MonitorTable activeMonitors() const
    {
        QMutexLocker locker(&mutex);

        return activeMonitorAreas;
    }

    void checkStartStop()
    {
        QMutexLocker locker(&mutex);

        bool signalsConnected = false;
        foreach (const QGeoAreaMonitorPolling *client, registeredClients) {
            if (client->signalsAreConnected) {
                signalsConnected = true;
                break;
            }
        }

        if (signalsConnected && !activeMonitorAreas.isEmpty()) {
            if (source)
                source->startUpdates();
            else
                //translated to InsufficientPositionInfo
                emit positionError(QGeoPositionInfoSource::ClosedError);
        } else {
            if (source)
                source->stopUpdates();
        }
    }

private:
    void setupNextExpiryTimeout()
    {
        nextExpiryTimer->stop();
        activeExpiry.first = QDateTime();
        activeExpiry.second = QString();

        foreach (const QGeoAreaMonitorInfo &info, activeMonitors()) {
            if (info.expiration().isValid()) {
                if (!activeExpiry.first.isValid()) {
                    activeExpiry.first = info.expiration();
                    activeExpiry.second = info.identifier();
                    continue;
                }
                if (info.expiration() < activeExpiry.first) {
                    activeExpiry.first = info.expiration();
                    activeExpiry.second = info.identifier();
                }
            }
        }

        if (activeExpiry.first.isValid())
            nextExpiryTimer->start(QDateTime::currentDateTime().msecsTo(activeExpiry.first));
    }


    //returns true if areaEntered should be emitted
    bool processInsideArea(const QString &monitorIdent)
    {
        if (!insideArea.contains(monitorIdent)) {
            if (singleShotTrigger.value(monitorIdent, -1) == areaEnteredSignal().methodIndex()) {
                //this is the finishing singleshot event
                singleShotTrigger.remove(monitorIdent);
                activeMonitorAreas.remove(monitorIdent);
                setupNextExpiryTimeout();
            } else {
                insideArea.insert(monitorIdent);
            }
            return true;
        }

        return false;
    }

    //returns true if areaExited should be emitted
    bool processOutsideArea(const QString &monitorIdent)
    {
        if (insideArea.contains(monitorIdent)) {
            if (singleShotTrigger.value(monitorIdent, -1) == areaExitedSignal().methodIndex()) {
                //this is the finishing singleShot event
                singleShotTrigger.remove(monitorIdent);
                activeMonitorAreas.remove(monitorIdent);
                setupNextExpiryTimeout();
            } else {
                insideArea.remove(monitorIdent);
            }
            return true;
        }
        return false;
    }



Q_SIGNALS:
    void timeout(const QGeoAreaMonitorInfo &info);
    void positionError(const QGeoPositionInfoSource::Error error);
    void areaEventDetected(const QGeoAreaMonitorInfo &minfo,
                           const QGeoPositionInfo &pinfo, bool isEnteredEvent);
private Q_SLOTS:
    void timeout()
    {
        /*
         * Don't block timer firing even if monitorExpiredSignal is not connected.
         * This allows us to continue to remove the existing monitors as they expire.
         **/
        const QGeoAreaMonitorInfo info = activeMonitorAreas.take(activeExpiry.second);
        setupNextExpiryTimeout();
        emit timeout(info);

    }

    void positionUpdated(const QGeoPositionInfo &info)
    {
        foreach (const QGeoAreaMonitorInfo &monInfo, activeMonitors()) {
            const QString identifier = monInfo.identifier();
            if (monInfo.area().contains(info.coordinate())) {
                if (processInsideArea(identifier))
                    emit areaEventDetected(monInfo, info, true);
            } else {
                if (processOutsideArea(identifier))
                    emit areaEventDetected(monInfo, info, false);
            }
        }
    }

private:
    QPair<QDateTime, QString> activeExpiry;
    QHash<QString, int> singleShotTrigger;
    QTimer* nextExpiryTimer;
    QSet<QString> insideArea;

    MonitorTable activeMonitorAreas;

    QGeoPositionInfoSource* source;
    QList<QGeoAreaMonitorPolling*> registeredClients;
    mutable QMutex mutex;
};

Q_GLOBAL_STATIC(QGeoAreaMonitorPollingPrivate, pollingPrivate)


QGeoAreaMonitorPolling::QGeoAreaMonitorPolling(QObject *parent)
    : QGeoAreaMonitorSource(parent), signalsAreConnected(false)
{
    d = pollingPrivate();
    lastError = QGeoAreaMonitorSource::NoError;
    d->registerClient(this);
    //hookup to default source if existing
    if (!positionInfoSource())
        setPositionInfoSource(QGeoPositionInfoSource::createDefaultSource(this));
}

QGeoAreaMonitorPolling::~QGeoAreaMonitorPolling()
{
    d->deregisterClient(this);
}

QGeoPositionInfoSource* QGeoAreaMonitorPolling::positionInfoSource() const
{
    return d->positionSource();
}

void QGeoAreaMonitorPolling::setPositionInfoSource(QGeoPositionInfoSource *source)
{
    d->setPositionSource(source);
}

QGeoAreaMonitorSource::Error QGeoAreaMonitorPolling::error() const
{
    return lastError;
}

bool QGeoAreaMonitorPolling::startMonitoring(const QGeoAreaMonitorInfo &monitor)
{
    if (!monitor.isValid())
        return false;

    //reject an expiry in the past
    if (monitor.expiration().isValid() &&
            (monitor.expiration() < QDateTime::currentDateTime()))
        return false;

    //don't accept persistent monitor since we don't support it
    if (monitor.isPersistent())
        return false;

    //update or insert
    d->startMonitoring(monitor);

    return true;
}

int QGeoAreaMonitorPolling::idForSignal(const char *signal)
{
    const QByteArray sig = QMetaObject::normalizedSignature(signal + 1);
    const QMetaObject * const mo = metaObject();

    return mo->indexOfSignal(sig.constData());
}

bool QGeoAreaMonitorPolling::requestUpdate(const QGeoAreaMonitorInfo &monitor, const char *signal)
{
    if (!monitor.isValid())
        return false;
    //reject an expiry in the past
    if (monitor.expiration().isValid() &&
            (monitor.expiration() < QDateTime::currentDateTime()))
        return false;

    //don't accept persistent monitor since we don't support it
    if (monitor.isPersistent())
        return false;

    if (!signal)
        return false;

    const int signalId = idForSignal(signal);
    if (signalId < 0)
        return false;

    //only accept area entered or exit signal
    if (signalId != areaEnteredSignal().methodIndex() &&
        signalId != areaExitedSignal().methodIndex())
    {
        return false;
    }

    d->requestUpdate(monitor, signalId);

    return true;
}

bool QGeoAreaMonitorPolling::stopMonitoring(const QGeoAreaMonitorInfo &monitor)
{
    QGeoAreaMonitorInfo info = d->stopMonitoring(monitor);

    return info.isValid();
}

QList<QGeoAreaMonitorInfo> QGeoAreaMonitorPolling::activeMonitors() const
{
    return d->activeMonitors().values();
}

QList<QGeoAreaMonitorInfo> QGeoAreaMonitorPolling::activeMonitors(const QGeoShape &region) const
{
    QList<QGeoAreaMonitorInfo> results;
    if (region.isEmpty())
        return results;

    const MonitorTable list = d->activeMonitors();
    foreach (const QGeoAreaMonitorInfo &monitor, list) {
        if (region.contains(monitor.area().center()))
            results.append(monitor);
    }

    return results;
}

QGeoAreaMonitorSource::AreaMonitorFeatures QGeoAreaMonitorPolling::supportedAreaMonitorFeatures() const
{
    return 0;
}

void QGeoAreaMonitorPolling::connectNotify(const QMetaMethod &/*signal*/)
{
    if (!signalsAreConnected &&
        (isSignalConnected(areaEnteredSignal()) ||
         isSignalConnected(areaExitedSignal())) )
    {
        signalsAreConnected = true;
        d->checkStartStop();
    }
}

void QGeoAreaMonitorPolling::disconnectNotify(const QMetaMethod &/*signal*/)
{
    if (!isSignalConnected(areaEnteredSignal()) &&
        !isSignalConnected(areaExitedSignal()))
    {
        signalsAreConnected = false;
        d->checkStartStop();
    }
}

void QGeoAreaMonitorPolling::positionError(const QGeoPositionInfoSource::Error error)
{
    switch (error) {
    case QGeoPositionInfoSource::AccessError:
        lastError = QGeoAreaMonitorSource::AccessError;
        break;
    case QGeoPositionInfoSource::UnknownSourceError:
        lastError = QGeoAreaMonitorSource::UnknownSourceError;
        break;
    case QGeoPositionInfoSource::ClosedError:
        lastError = QGeoAreaMonitorSource::InsufficientPositionInfo;
        break;
    case QGeoPositionInfoSource::NoError:
        return;
    }

    emit QGeoAreaMonitorSource::error(lastError);
}

void QGeoAreaMonitorPolling::timeout(const QGeoAreaMonitorInfo& monitor)
{
    if (isSignalConnected(monitorExpiredSignal()))
        emit monitorExpired(monitor);
}

void QGeoAreaMonitorPolling::processAreaEvent(const QGeoAreaMonitorInfo &minfo,
                                              const QGeoPositionInfo &pinfo, bool isEnteredEvent)
{
    if (isEnteredEvent)
        emit areaEntered(minfo, pinfo);
    else
        emit areaExited(minfo, pinfo);
}

#include "qgeoareamonitor_polling.moc"
#include "moc_qgeoareamonitor_polling.cpp"
