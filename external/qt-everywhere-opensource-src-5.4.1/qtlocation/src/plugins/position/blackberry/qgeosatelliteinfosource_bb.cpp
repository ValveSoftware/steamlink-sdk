/****************************************************************************
**
** Copyright (C) 2012 - 2013 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <bb/PpsObject>

#include "qgeosatelliteinfosource_bb_p.h"
#include "locationmanagerutil_bb.h"

#include <location_manager.h>

extern "C" {
#include <wmm/wmm.h>
}

#include <QVariantMap>
#include <QByteArray>
#include <QtDebug>

#include <errno.h>

///////////////////////////
//
// local variables/functions
//
///////////////////////////

namespace global
{

// While waiting for a position fix a satellite update period of 1 sec is considered reasonable.
static const double defaultSatelliteUpdatePeriod = 1.0;

} // namespace global

namespace
{
// map the Location Manager reply error codes to the QGeoSatelliteInfoSource::Error enum values.
QMap<int, QGeoSatelliteInfoSource::Error> createIntToErrorCodeMap()
{
    QMap<int, QGeoSatelliteInfoSource::Error> map;

    map.insert(0, QGeoSatelliteInfoSource::NoError);
    map.insert(1, QGeoSatelliteInfoSource::ClosedError);
    map.insert(2, QGeoSatelliteInfoSource::UnknownSourceError);
    map.insert(3, QGeoSatelliteInfoSource::UnknownSourceError);
    map.insert(4, QGeoSatelliteInfoSource::UnknownSourceError);
    map.insert(5, QGeoSatelliteInfoSource::UnknownSourceError);
    map.insert(6, QGeoSatelliteInfoSource::AccessError);
    // the following are not considered errors from QGeoSatelliteInfoSource perspective
    // (they are timeout conditions)
    map.insert(0x10000, QGeoSatelliteInfoSource::NoError);
    map.insert(0x10001, QGeoSatelliteInfoSource::NoError);
    map.insert(0x10002, QGeoSatelliteInfoSource::NoError);

    return map;
}

const QMap<int, QGeoSatelliteInfoSource::Error> intToErrorCodeMap =
        createIntToErrorCodeMap();

QGeoSatelliteInfo::SatelliteSystem determineSatSystem(int satId)
{
    if (satId >= 1 && satId <= 32)
        return QGeoSatelliteInfo::GPS;

    if (satId >= 65 && satId <= 88)
        return QGeoSatelliteInfo::GLONASS;

    return QGeoSatelliteInfo::Undefined;
}

}

///////////////////////////
//
// QGeoSatelliteInfoSourceBbPrivate
//
///////////////////////////

// Create a QVariantMap suitable for writing to a PpsObject specifying a location request to the
// Location Manager. If the request is periodic then the update interval is used. Otherwise 0
// indicates to the Location Manager that it is a request for a single, immediate location response.
// singleRequestMsec applies only to the single, immediate location response. It represents the
// expected location response time, after which it is assumed a timeout response occurs.
QVariantMap QGeoSatelliteInfoSourceBbPrivate::populateLocationRequest(bool periodic,
                                                                      int singleRequestMsec)
{
    Q_Q(const QGeoSatelliteInfoSourceBb);

    QVariantMap map;
    QVariantMap datMap;

    int period;
    int responseTime;
    if (periodic) {
        // rounding is performed here because the Location Manager truncates to nearest integer
        period = (q->updateInterval() + 500) / 1000;
        if (period == 0)
            period = ::global::defaultSatelliteUpdatePeriod;

        responseTime = _responseTime;
    } else {
        period = 0;
        responseTime = (singleRequestMsec + 500) / 1000;
    }

    datMap.insert("period", period);
    datMap.insert("accuracy", 0);
    datMap.insert("response_time", responseTime);
    datMap.insert("background", _backgroundMode);

    datMap.insert("provider", "gnss");
    datMap.insert("fix_type", "gps_autonomous");

    // have the Location Manager return the satellite information even if it does not have a
    // position fix.
    datMap.insert("report_sat", true);

    map.insert("msg", "location");
    map.insert("id", global::libQtLocationId);
    map.insert("dat", datMap);

    return map;
}

// From a QvariantMap representing a location response from the Location Manager fill the lists of
// QGeoSatelliteInfo instances intended to be emitted via satellitesInUseUpdated() and
// satellitesInViewUpdated() signals.
void QGeoSatelliteInfoSourceBbPrivate::populateSatelliteLists(const QVariantMap &map)
{
    // populate _currentSatelliteInfo
    QVariantMap datMap = map.value("dat").toMap();
    QVariantList satelliteList = datMap.value("satellites").toList();

    _satellitesInView.clear();
    _satellitesInUse.clear();

    Q_FOREACH (const QVariant &satelliteData, satelliteList) {
        datMap = satelliteData.toMap();
        QGeoSatelliteInfo satelliteInfo = QGeoSatelliteInfo();

        if (datMap.contains("id"))
            satelliteInfo.setSatelliteIdentifier(static_cast<int>(datMap.value("id").toDouble()));

        satelliteInfo.setSatelliteSystem(determineSatSystem(satelliteInfo.satelliteIdentifier()));

        if (datMap.contains("cno"))
            satelliteInfo.setSignalStrength(static_cast<int>(datMap.value("cno").toDouble()));

        // attributes
        if (datMap.contains("elevation"))
            satelliteInfo.setAttribute(QGeoSatelliteInfo::Elevation,
                                       static_cast<qreal>(datMap.value("elevation").toDouble()));
        else
            satelliteInfo.removeAttribute(QGeoSatelliteInfo::Elevation);

        if (datMap.contains("azimuth"))
            satelliteInfo.setAttribute(QGeoSatelliteInfo::Azimuth,
                                       static_cast<qreal>(datMap.value("azimuth").toDouble()));
        else
            satelliteInfo.removeAttribute(QGeoSatelliteInfo::Azimuth);

        // each satellite in this list is considered "in view"
        _satellitesInView.append(satelliteInfo);

        if (datMap.value("used").toBool() == true)
            _satellitesInUse.append(satelliteInfo);
    }
}

// The satellite data is retrieved from a location request
bool QGeoSatelliteInfoSourceBbPrivate::requestSatelliteInfo(bool periodic,
                                                            int singleRequestMsec)
{
    // build up the request
    QVariantMap request = populateLocationRequest(periodic, singleRequestMsec);

    bb::PpsObject *ppsObject;
    if (periodic)
        ppsObject = _periodicUpdatePpsObject;
    else
        ppsObject = _singleUpdatePpsObject;

    if (sendRequest(*ppsObject, request) == false) {
        stopUpdates();

#ifndef BB_TEST_BUILD
        // test for pps file error
        switch (ppsObject->error()) {
        case EACCES:
            _sourceError = QGeoSatelliteInfoSource::AccessError;
            break;
        default:
            _sourceError = QGeoSatelliteInfoSource::UnknownSourceError;
            break;
        }
#endif // !BB_TEST_BUILD

        return false;
    }

    return true;
}

void QGeoSatelliteInfoSourceBbPrivate::cancelSatelliteInfo(bool periodic)
{
    bb::PpsObject *ppsObject;
    if (periodic)
        ppsObject = _periodicUpdatePpsObject;
    else
        ppsObject = _singleUpdatePpsObject;

    (void)sendRequest(*ppsObject, global::cancelRequest);
}

// Constructor. Note there are two PpsObjects for handling the two different types of requests that
// can be simultaneously made and which must be handled independently (apart from both being emitted
// through the same signal when done-part of Qt Mobility spec.
QGeoSatelliteInfoSourceBbPrivate::QGeoSatelliteInfoSourceBbPrivate(
    QGeoSatelliteInfoSourceBb *parent)
        :   QObject(parent),
            _startUpdatesInvoked(false),
            _requestUpdateInvoked(false),
            q_ptr(parent),
            _periodicUpdatePpsObject(new bb::PpsObject(global::locationManagerPpsFile, this)),
            _singleUpdatePpsObject(new bb::PpsObject(global::locationManagerPpsFile, this)),
            _sourceError(QGeoSatelliteInfoSource::NoError),
            _backgroundMode(false),
            _responseTime(0)
{
    // connect to periodic update PpsObject::readyRead()
    connect(_periodicUpdatePpsObject, SIGNAL(readyRead()), SLOT(receivePeriodicSatelliteReply()));

    // connect to single update PpsObject::readyRead()
    connect(_singleUpdatePpsObject, SIGNAL(readyRead()), SLOT(receiveSingleSatelliteReply()));

    // queued connection to signal requestTimeout()
    connect(this, SIGNAL(queuedRequestTimeout()), SLOT(emitRequestTimeout()), Qt::QueuedConnection);
}

QGeoSatelliteInfoSourceBbPrivate::~QGeoSatelliteInfoSourceBbPrivate()
{
    stopUpdates();
}

// request periodic updates
void QGeoSatelliteInfoSourceBbPrivate::startUpdates()
{
    // do nothing if periodic updates have already been started
    if (_startUpdatesInvoked)
        return;

    // build a request and initiate it
    if (requestSatelliteInfo(true)) {
        _startUpdatesInvoked = true;
    } else {
        Q_Q(QGeoSatelliteInfoSourceBb);
        Q_EMIT ((QGeoSatelliteInfoSource *)q)->error(_sourceError);
    }
}

// stop periodic updates
void QGeoSatelliteInfoSourceBbPrivate::stopUpdates()
{
    // do nothing if periodic updates have not been started
    if (!_startUpdatesInvoked)
        return;

    cancelSatelliteInfo(true);
    _startUpdatesInvoked = false;

    // close the pps file to ensure readyRead() does not spin in the event that we don't read the
    // reply to the cancel request. Note that open() is done lazily in sendRequest().
    _periodicUpdatePpsObject->close();
}

// request single update
void QGeoSatelliteInfoSourceBbPrivate::requestUpdate(int msec)
{
    // do nothing if an immediate update has already been requested
    if (_requestUpdateInvoked)
        return;

    if (msec) {
        // If it is not possible to update in msec timeout immediately.
        if (msec < global::minUpdateInterval) {
            // The connection is queued because it is possible for the user to call requestUpdate()
            // from within the slot handling the timeout. The queued connection avoids potential
            // infinite recursion.
            Q_EMIT queuedRequestTimeout();
            return;
        }
    }

    if (requestSatelliteInfo(false, msec)) {
        _requestUpdateInvoked = true;
    } else {
        // With Qt5 the error() signal was introduced. If there are any receivers emit error() else
        // maintain QtMobility behavior.
        Q_Q(QGeoSatelliteInfoSourceBb);
        if (q->receivers(SIGNAL(error(QGeoPositionInfoSource::Error)))) {
            Q_EMIT ((QGeoSatelliteInfoSource *)q)->error(_sourceError);
        } else {
            // The connection is queued because it is possible for the user to call requestUpdate()
            // from within the slot handling the timeout. The queued connection avoids potential
            // infinite recursion.
            Q_EMIT queuedRequestTimeout();
        }
    }
}

// single update has timed out. This is a slot for the requestUpdate timer
void QGeoSatelliteInfoSourceBbPrivate::singleUpdateTimeout()
{
    _requestUpdateInvoked = false;

    emitRequestTimeout();

    if (!_requestUpdateInvoked) {
        // close the pps file to ensure readyRead() does not spin in the event that there are
        // unexpected replies that we don't read. Note that open() is done lazily in sendRequest().
        _singleUpdatePpsObject->close();
    }
}

// This slot is intended for queued connection to the signal queuedUpdateTimeout(). If an error
// occurs when an update is requested the error is relayed via updateTimeout() but the connection
// is queued to avoid potential infinite recursion.
void QGeoSatelliteInfoSourceBbPrivate::emitRequestTimeout()
{
    Q_Q(QGeoSatelliteInfoSourceBb);
    Q_EMIT q->requestTimeout();
}

bool QGeoSatelliteInfoSourceBbPrivate::receiveSatelliteReply(bool periodic)
{
    bb::PpsObject *ppsObject;
    if (periodic)
        ppsObject = _periodicUpdatePpsObject;
    else
        ppsObject = _singleUpdatePpsObject;

    QVariantMap reply;
    // receiveReply() tests for errors associated with the request being replied to
    if (!receiveReply(&reply, *ppsObject)) {
        QGeoSatelliteInfoSource::Error fatalError = QGeoSatelliteInfoSource::UnknownSourceError;

        // if there is an error from Location Manager translate it to a
        // QGeoSatelliteInfoSource::Error
        if (reply.contains("errCode")) {
            fatalError = intToErrorCodeMap.value(reply.value("errCode").toInt(),
                                                 QGeoSatelliteInfoSource::UnknownSourceError);
        }

        if (fatalError != QGeoSatelliteInfoSource::NoError)
            _sourceError = fatalError;

        if (!periodic || fatalError != QGeoSatelliteInfoSource::NoError)
            return false;
    }

    // check that this is a location reply (could be a reply to another request type, eg. cancel,
    // which is ignored here)
    if (reply.contains("res") && reply.value("res").toString() == "location") {
        // extract the satellite info from the reply into _satellitesInView and _satellitesInUse
        populateSatelliteLists(reply);

        Q_Q(QGeoSatelliteInfoSourceBb);
        Q_EMIT q->satellitesInUseUpdated(_satellitesInUse);
        Q_EMIT q->satellitesInViewUpdated(_satellitesInView);
    }

    return true;
}

void QGeoSatelliteInfoSourceBbPrivate::receivePeriodicSatelliteReply()
{
    // don't try to receive a reply if periodic updates have not been started. This is
    // necessary because this slot is connected to PpsObject::readyRead() and could be
    // invoked any time the pps file is updated by the server. Under error conditions
    // this would otherwise lead to a circular calling sequence: receive, timeout due to
    // error, cancel, receive...
    if (!_startUpdatesInvoked)
        return;

    if (!receiveSatelliteReply(true)) {
        Q_Q(QGeoSatelliteInfoSourceBb);
        Q_EMIT ((QGeoSatelliteInfoSource *)q)->error(_sourceError);
    }
}

void QGeoSatelliteInfoSourceBbPrivate::receiveSingleSatelliteReply()
{
    // don't try to receive a reply if a single update has not been requested. This is
    // necessary because this slot is connected to PpsObject::readyRead() and could be
    // invoked any time the pps file is updated by the server. Under error conditions
    // this would otherwise lead to a circular calling sequence: receive, timeout due to
    // error, cancel, receive...
    if (!_requestUpdateInvoked)
        return;

    // clear this before calling receivePositionReply() which can emit the positionUpdated()
    // signal. It is possible to call requestUpdate() in the slot connected to
    // positionUpdated() so for requestUpdate() to work _requestUpdateInvoked must be false
    _requestUpdateInvoked = false;

    if (!receiveSatelliteReply(false)) {
        Q_Q(QGeoSatelliteInfoSourceBb);
        if (_sourceError != QGeoSatelliteInfoSource::NoError)
            Q_EMIT ((QGeoSatelliteInfoSource *)q)->error(_sourceError);
        else
            singleUpdateTimeout();
    }

    if (!_requestUpdateInvoked) {
        // close the pps file to ensure readyRead() does not spin in the event that there are
        // unexpected replies that we don't read. Note that open() is done lazily in sendRequest().
        _singleUpdatePpsObject->close();
    }
}

///////////////////////////
//
// QGeoSatelliteInfoSourceBb
//
///////////////////////////

/*!
    \class QGeoSatelliteInfoSourceBb
    \brief The QGeoSatelliteInfoSourceBb class is for the distribution of positional updates
           obtained from the underlying Qnx Location Manager.

    QGeoSatelliteInfoSourceBb is a subclass of QGeoSatelliteInfoSource.
    The static function QGeoSatelliteInfoSource::createDefaultSource() creates a default
    satellite data source that is appropriate for the platform, if one is
    available. On BB10 this is a QGeoSatelliteInfoSourceBb instance. Otherwise, available
    QGeoPositionInfoSourceFactory plugins will
    be checked for one that has a satellite data source available.

    Call startUpdates() and stopUpdates() to start and stop regular updates,
    or requestUpdate() to request a single update.
    When an update is available, satellitesInViewUpdated() and/or
    satellitesInUseUpdated() will be emitted.
*/



/*!
    Constructs a QGeoSatelliteInfoSourceBb instance with the given \a parent
    and \a updateMode.
*/
QGeoSatelliteInfoSourceBb::QGeoSatelliteInfoSourceBb(QObject *parent)
        : QGeoSatelliteInfoSource(parent),
        d_ptr(new QGeoSatelliteInfoSourceBbPrivate(this))
{
}

/*!
    Destroys the satellite source.
*/
QGeoSatelliteInfoSourceBb::~QGeoSatelliteInfoSourceBb()
{
}

/*!
    \reimp
*/
int QGeoSatelliteInfoSourceBb::minimumUpdateInterval() const
{
    return global::minUpdateInterval;
}

/*!
    \reimp
*/
void QGeoSatelliteInfoSourceBb::setUpdateInterval(int msec)
{
    int interval = msec;
    if (interval != 0)
        interval = qMax(msec, minimumUpdateInterval());

    if (interval == updateInterval())
        return;

    QGeoSatelliteInfoSource::setUpdateInterval(interval);

    Q_D(QGeoSatelliteInfoSourceBb);
    if (d->_startUpdatesInvoked) {
        d->stopUpdates();
        d->startUpdates();
    }
}

/*!
    \reimp
*/
void QGeoSatelliteInfoSourceBb::startUpdates()
{
    Q_D(QGeoSatelliteInfoSourceBb);
    d->startUpdates();
}

/*!
    \reimp
*/
void QGeoSatelliteInfoSourceBb::stopUpdates()
{
    Q_D(QGeoSatelliteInfoSourceBb);
    d->stopUpdates();
}

/*!
    \reimp
*/
void QGeoSatelliteInfoSourceBb::requestUpdate(int msec)
{
    Q_D(QGeoSatelliteInfoSourceBb);
    d->requestUpdate(msec);
}

/*!
    \reimp
*/
QGeoSatelliteInfoSource::Error QGeoSatelliteInfoSourceBb::error() const
{
    Q_D(const QGeoSatelliteInfoSourceBb);
    return d->_sourceError;
}

// property managers. These properties extend QGeoSatelliteInfoSource by allowing additional
// control provided by the Location Manager

/*!
    \property QGeoSatelliteInfoSourceBb::period
    \brief The period of the location request, in seconds. A value of '0' indicates that this
    would be a one-time location request.
*/
double QGeoSatelliteInfoSourceBb::period() const
{
    // convert from msec to sec
    return updateInterval() / 1000.0;
}

void QGeoSatelliteInfoSourceBb::setPeriod(double period)
{
    // convert from sec to msec, round to nearest msec
    setUpdateInterval(qRound(static_cast<qreal>(period * 1000.0)));
}


/*!
    \property QGeoSatelliteInfoSourceBb::backgroundMode
    \brief This property determines whether or not requests are allowed to run with the device
    in standby (i.e. screen off)
*/
bool QGeoSatelliteInfoSourceBb::backgroundMode() const
{
    Q_D(const QGeoSatelliteInfoSourceBb);
    return d->_backgroundMode;
}

void QGeoSatelliteInfoSourceBb::setBackgroundMode(bool mode)
{
    Q_D(QGeoSatelliteInfoSourceBb);
    d->_backgroundMode = mode;
}

/*!
    \property QGeoSatelliteInfoSourceBb::responseTime
    \brief This property specifies the desired response time of the fix, in seconds. A value
    of '0' disables response time criteria.
*/
int QGeoSatelliteInfoSourceBb::responseTime() const
{
    Q_D(const QGeoSatelliteInfoSourceBb);
    return d->_responseTime;
}

void QGeoSatelliteInfoSourceBb::setResponseTime(int responseTime)
{
    Q_D(QGeoSatelliteInfoSourceBb);
    d->_responseTime = responseTime;
}
