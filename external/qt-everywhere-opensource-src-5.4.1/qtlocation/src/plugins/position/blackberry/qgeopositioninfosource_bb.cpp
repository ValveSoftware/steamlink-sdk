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

#include "qgeopositioninfosource_bb_p.h"
#include "locationmanagerutil_bb.h"

#ifndef BB_TEST_BUILD
#   include <bb/PpsObject>
#else
#   include "../tests/include/PpsObjectStuntDouble.hpp"
#endif

extern "C" {
#include <wmm/wmm.h>
}

#include <location_manager.h>

#include <QMap>
#include <QVariantMap>
#include <QByteArray>
#include <QtDebug>
#include <QStringList>

#include <errno.h>

///////////////////////////
//
// local variables/functions
//
///////////////////////////

namespace global
{

// Currently the default behavior for Location Manager is simply to set a constant default
// interval. 5 sec has been chosen as a compromise between timely updates and conserving power.
static const int defaultPositionUpdatePeriod = 5;

} // namespace global

namespace
{

// map the Location Manager reply error codes to the PositionErrorCode enum values.
QMap<int, bb::location::PositionErrorCode::Type> createIntToPositionErrorCodeMap()
{
    QMap<int, bb::location::PositionErrorCode::Type> map;

    map.insert(0, bb::location::PositionErrorCode::None);
    map.insert(1, bb::location::PositionErrorCode::FatalDisabled);
    map.insert(2, bb::location::PositionErrorCode::FatalNoLastKnownPosition);
    map.insert(3, bb::location::PositionErrorCode::FatalInsufficientProviders);
    map.insert(4, bb::location::PositionErrorCode::FatalInvalidRequest);
    map.insert(5, bb::location::PositionErrorCode::FatalUnknown);
    map.insert(6, bb::location::PositionErrorCode::FatalPermission);
    map.insert(0x10000, bb::location::PositionErrorCode::WarnTimeout);
    map.insert(0x10001, bb::location::PositionErrorCode::WarnLostTracking);
    map.insert(0x10002, bb::location::PositionErrorCode::WarnStationary);

    return map;
}

const QMap<int, bb::location::PositionErrorCode::Type> intToPositionErrorCodeMap =
        createIntToPositionErrorCodeMap();

bool fatalError(bb::location::PositionErrorCode::Type code)
{
    if ( code == bb::location::PositionErrorCode::FatalDisabled
         || code == bb::location::PositionErrorCode::FatalNoLastKnownPosition
         || code == bb::location::PositionErrorCode::FatalInsufficientProviders
         || code == bb::location::PositionErrorCode::FatalInvalidRequest
         || code == bb::location::PositionErrorCode::FatalUnknown
         || code == bb::location::PositionErrorCode::FatalPermission ) {
        return true;
    }
    return false;
}

// map the PositionErrorCode enum values to the Qt5 QGeoPositionInfoSource::Error enum values.
QMap<bb::location::PositionErrorCode::Type, QGeoPositionInfoSource::Error>
    createPositionErrorCodeToErrorMap()
{
    QMap<bb::location::PositionErrorCode::Type, QGeoPositionInfoSource::Error> map;

    map.insert(bb::location::PositionErrorCode::FatalDisabled,
               QGeoPositionInfoSource::ClosedError);
    map.insert(bb::location::PositionErrorCode::FatalNoLastKnownPosition,
               QGeoPositionInfoSource::UnknownSourceError);
    map.insert(bb::location::PositionErrorCode::FatalInsufficientProviders,
               QGeoPositionInfoSource::UnknownSourceError);
    map.insert(bb::location::PositionErrorCode::FatalInvalidRequest,
               QGeoPositionInfoSource::UnknownSourceError);
    map.insert(bb::location::PositionErrorCode::FatalUnknown,
               QGeoPositionInfoSource::UnknownSourceError);
    map.insert(bb::location::PositionErrorCode::FatalPermission,
               QGeoPositionInfoSource::AccessError);

    return map;
}

const QMap<bb::location::PositionErrorCode::Type, QGeoPositionInfoSource::Error>
    positionErrorCodeToErrorMap = createPositionErrorCodeToErrorMap();

// map the Location Manager provider names to the QGeoPositionInfoSource positioning methods
QMap<QGeoPositionInfoSource::PositioningMethods, QString> createPositioningMethodsToProviderMap()
{
    QMap<QGeoPositionInfoSource::PositioningMethods, QString> map;

    map.insert(QGeoPositionInfoSource::SatellitePositioningMethods, QString("gnss"));
    map.insert(QGeoPositionInfoSource::NonSatellitePositioningMethods, QString("network"));
    map.insert(QGeoPositionInfoSource::AllPositioningMethods, QString("hybrid"));

    return map;
}

const QMap<QGeoPositionInfoSource::PositioningMethods, QString>
    positioningMethodsToProviderMap = createPositioningMethodsToProviderMap();

// list of valid strings for the Location Manager reset types
QStringList createValidResetTypesList()
{
    QStringList list;
    list.append("cold");
    list.append("warm");
    list.append("hot");
    list.append("factory");
    list.append("ee_data");
    list.append("almanac");
    list.append("ephemeris");

    return list;
}

const QStringList validResetTypes = createValidResetTypesList();

void printGetGeomagneticFieldInputs(const wmm_location_t &location, const struct tm &date)
{
    qWarning() << "location = ("
               << location.latitude_deg
               << ","
               << location.longitude_deg
               << ","
               << location.altitude_meters
               << ")";
    qWarning() << "date = (" <<        date.tm_sec <<
                                "," << date.tm_min <<
                                "," << date.tm_hour <<
                                "," << date.tm_mday <<
                                "," << date.tm_mon <<
                                "," << date.tm_year <<
                                "," << date.tm_wday <<
                                "," << date.tm_yday <<
                                "," << date.tm_isdst <<
#ifndef BB_TEST_BUILD
// the following fields are not present on host (at least Win32)
                                "," << date.tm_gmtoff <<
                                "," << date.tm_zone <<
#endif
                                ")";
}

bool magneticDeclination(double *declination, const QGeoPositionInfo &position)
{
    if (!declination)
        return false;

    wmm_location_t location;
    struct tm date;
    wmm_geomagnetic_field_t field;

    location.latitude_deg = position.coordinate().latitude();
    location.longitude_deg = position.coordinate().longitude();
    if (position.coordinate().type() == QGeoCoordinate::Coordinate3D)
        location.altitude_meters = position.coordinate().altitude();
    else
        location.altitude_meters = 0.0;

    time_t time = (time_t)position.timestamp().toTime_t();
#ifdef BB_TEST_BUILD
    // since gmtime_r() is not defined on host (at least Win32) risk reentrant side effects on the
    // returned data.
    struct tm *pDate = gmtime(&time);
    if (pDate == NULL) {
        qWarning() << "QGeoPositionInfoSourceBbPrivate.cpp:magneticDeclination(): "
                      "gmtime() returned NULL";
        return false;
    }
    date = *pDate;
#else
    if (gmtime_r(&time, &date) == NULL) {
        qWarning() << "QGeoPositionInfoSourceBbPrivate.cpp:magneticDeclination(): "
                      "gmtime_r() returned NULL";
        return false;
    }
#endif

    switch (wmm_get_geomagnetic_field(&location, &date, &field)) {
    case 0:
        break;

    case 1:
        qWarning() << "QGeoPositionInfoSourceBbPrivate.cpp:magneticDeclination(): "
                      "wmm_get_geomagnetic_field() returned: inputs limited to model range";
        printGetGeomagneticFieldInputs(location, date);
        break;

    case -1:
    default:
        qWarning() << "QGeoPositionInfoSourceBbPrivate.cpp:magneticDeclination(): "
                      "wmm_get_geomagnetic_field() returned: error";
        printGetGeomagneticFieldInputs(location, date);
        return false;
    }

    *declination = field.declination_deg;
    return true;
}

QVariantMap populateLastKnownPositionRequest(bool fromSatellitePositioningMethodsOnly)
{
    QVariantMap map;
    QVariantMap datMap;

    if (fromSatellitePositioningMethodsOnly)
        datMap.insert("provider", "gnss");
    else
        datMap.insert("provider", "hybrid");

    datMap.insert("last_known", true);
    datMap.insert("period", 0);

    map.insert("msg", "location");
    map.insert("id", ::global::libQtLocationId);
    map.insert("dat", datMap);

    return map;
}

// From a QvariantMap representing a location response from the Location Manager fill a
// QGeoPositionInfo instance intended to be emitted via positionUpdated() signal. Returns true
// if the position info was successfully populated.
bool populatePositionInfo(QGeoPositionInfo *position, const QVariantMap &map)
{
    // populate position

    // set the reply dat property, which can be accessed by the user in the slot connected to
    //the positionUpdated() signal
    QVariantMap replyDat = map.value("dat").toMap();

    // check for required fields
    if (!replyDat.contains("latitude") || !replyDat.contains("longitude")
        || !replyDat.contains("accuracy")) {
        return false;
    }

    // set the lat/long/alt coordinate
    QGeoCoordinate coord;
    coord.setLatitude(replyDat.value("latitude").toDouble());
    coord.setLongitude(replyDat.value("longitude").toDouble());
    if (replyDat.contains("altitude"))
        coord.setAltitude(replyDat.value("altitude").toDouble());

    if (!coord.isValid())
        return false;

    position->setCoordinate(coord);

    // set the time stamp
    QDateTime dateTime;
    dateTime.setTimeSpec(Qt::UTC);
    if (replyDat.contains("utc") && static_cast<int>(replyDat.value("utc").toDouble()) != 0) {
        // utc is msec since epoch (1970-01-01T00:00:00)
        dateTime.setTime_t(qRound((replyDat.value("utc").toDouble() / 1000.0)));
    } else {
        // this relies on the device's clock being accurate
        dateTime = QDateTime::currentDateTimeUtc();
    }
    position->setTimestamp(dateTime);

    // attributes
    if (replyDat.contains("heading")) {
        position->setAttribute(QGeoPositionInfo::Direction,
                               static_cast<qreal>(replyDat.value("heading").toDouble()));
    } else {
        position->removeAttribute(QGeoPositionInfo::Direction);
    }

    if (replyDat.contains("speed")) {
        position->setAttribute(QGeoPositionInfo::GroundSpeed,
                               static_cast<qreal>(replyDat.value("speed").toDouble()));
    } else {
        position->removeAttribute(QGeoPositionInfo::GroundSpeed);
    }

    if (replyDat.contains("verticalSpeed")) {
        position->setAttribute(QGeoPositionInfo::VerticalSpeed,
                               static_cast<qreal>(replyDat.value("verticalSpeed").toDouble()));
    } else {
        position->removeAttribute(QGeoPositionInfo::VerticalSpeed);
    }

    if (replyDat.contains("declination")) {
        position->setAttribute(QGeoPositionInfo::MagneticVariation,
                               static_cast<qreal>(replyDat.value("declination").toDouble()));
    } else {
        double declination;

        if (magneticDeclination(&declination, *position) == true) {
            position->setAttribute(QGeoPositionInfo::MagneticVariation,
                                   static_cast<qreal>(declination));
        } else {
            position->removeAttribute(QGeoPositionInfo::MagneticVariation);
        }
    }

    // replyDat.contains("accuracy") was confirmed above
    position->setAttribute(QGeoPositionInfo::HorizontalAccuracy,
                           static_cast<qreal>(replyDat.value("accuracy").toDouble()));

    if (replyDat.contains("altitudeAccuracy")) {
        position->setAttribute(QGeoPositionInfo::VerticalAccuracy,
                               static_cast<qreal>(replyDat.value("altitudeAccuracy").toDouble()));
    } else {
        position->removeAttribute(QGeoPositionInfo::VerticalAccuracy);
    }

    return true;
}

} // unnamed namespace

///////////////////////////
//
// QGeoPositionInfoSourceBbPrivate
//
///////////////////////////

// Create a QVariantMap suitable for writing to a PpsObject specifying a location request to the
// Location Manager. If the request is periodic then the update interval is used. Otherwise 0
// indicates to the Location Manager that it is a request for a single, immediate location response.
// singleRequestMsec applies only to the single, immediate location response. It represents the
// expected location response time, after which it is assumed a timeout response occurs.
QVariantMap QGeoPositionInfoSourceBbPrivate::populateLocationRequest(bool periodic,
                                                                     int singleRequestMsec) const
{
    Q_Q(const QGeoPositionInfoSourceBb);

    QVariantMap map;
    QVariantMap datMap;

    int period;
    int responseTime;
    if (periodic) {
        // rounding is performed here because the Location Manager truncates to nearest integer
        period = (q->updateInterval() + 500) / 1000;
        // The Qt MObility API treats a period of 0 as indicating default behavior
        if (period == 0) {
            // specify global::defaultPositionUpdatePeriod as the default behavior for Location
            // Manager
            period = ::global::defaultPositionUpdatePeriod;
        }
        responseTime = qRound(_responseTime);
    } else {
        period = 0;
        responseTime = (singleRequestMsec + 500) / 1000;
    }

    // period is the only mandatory field
    datMap.insert("period", period);

    if (_accuracy > 0.0)
        datMap.insert("accuracy", _accuracy);
    if (responseTime > 0.0)
        datMap.insert("response_time", responseTime);

    // since there is no uninitialized state for bool always specify the background mode
    datMap.insert("background", _canRunInBackground);

    QString provider = positioningMethodsToProviderMap.value(q->preferredPositioningMethods());
    if (!provider.isEmpty())
        datMap.insert("provider", provider);

    if (!_fixType.isEmpty())
        datMap.insert("fix_type", _fixType);

    if (!_appId.isEmpty())
        datMap.insert("app_id", _appId);

    if (!_appPassword.isEmpty())
        datMap.insert("app_password", _appPassword);

    if (!_pdeUrl.isEmpty())
        datMap.insert("pde_url", _pdeUrl.toEncoded().constData());

    if (!_slpUrl.isEmpty())
        datMap.insert("slp_url", _slpUrl.toEncoded().constData());

    map.insert("msg", "location");
    map.insert("id", global::libQtLocationId);
    map.insert("dat", datMap);

    return map;
}

QVariantMap QGeoPositionInfoSourceBbPrivate::populateResetRequest() const
{
    QVariantMap map;
    QVariantMap datMap;

    datMap.insert("reset_type", _resetType);

    map.insert("msg", "reset");
    map.insert("id", ::global::libQtLocationId);
    map.insert("dat", datMap);

    return map;
}

bool QGeoPositionInfoSourceBbPrivate::requestPositionInfo(bool periodic, int singleRequestMsec)
{
    // build up the request
    QVariantMap request = populateLocationRequest(periodic, singleRequestMsec);

    bb::PpsObject *ppsObject;
    if (periodic)
        ppsObject = _periodicUpdatePpsObject;
    else
        ppsObject = _singleUpdatePpsObject;

    bool returnVal = sendRequest(*ppsObject, request);
#ifndef BB_TEST_BUILD
    if (!returnVal) {
        // test for pps file error
        switch (ppsObject->error()) {
        case EACCES:
            _replyErrorCode = bb::location::PositionErrorCode::FatalPermission;
            _replyErr = "failed";
            _replyErrStr = ppsObject->errorString();
            break;

        case EOK:
            _replyErrorCode = bb::location::PositionErrorCode::FatalUnknown;
            _replyErr = "failed";
            _replyErrStr = "Unknown error occurred sending request";
            qWarning() << "QGeoPositionInfoSourceBbPrivate::requestPositionInfo() :"
                       << _replyErrStr;
            break;

        default:
            _replyErrorCode = bb::location::PositionErrorCode::FatalUnknown;
            _replyErr = "failed";
            _replyErrStr = ppsObject->errorString();
            qWarning() << "QGeoPositionInfoSourceBbPrivate::requestPositionInfo() : "
                          "unexpected error, errno ="
                       << ppsObject->error()
                       << " ("
                       << ppsObject->errorString()
                       << ")";
            break;
        }
    }
#endif // !BB_TEST_BUILD

    return returnVal;
}

void QGeoPositionInfoSourceBbPrivate::cancelPositionInfo(bool periodic)
{
    bb::PpsObject *ppsObject;
    if (periodic)
        ppsObject = _periodicUpdatePpsObject;
    else
        ppsObject = _singleUpdatePpsObject;

    (void)sendRequest(*ppsObject, global::cancelRequest);
}

void QGeoPositionInfoSourceBbPrivate::resetLocationProviders()
{
    QVariantMap map = populateResetRequest();
    (void)sendRequest(*_periodicUpdatePpsObject, map);
}

// Get the last known position from the Location Manager. Any error results in the return of an
// invalid position.
QGeoPositionInfo QGeoPositionInfoSourceBbPrivate::lastKnownPosition(
    bool fromSatellitePositioningMethodsOnly) const
{
    QGeoPositionInfo position = QGeoPositionInfo();
    bb::PpsObject ppsObject(global::locationManagerPpsFile);
    QVariantMap lastKnown = populateLastKnownPositionRequest(fromSatellitePositioningMethodsOnly);

    if (!ppsObject.open())
        return position;

    // Location Manager promises to reply immediately with the last known position or an error.
    ppsObject.setBlocking(true);

    if (!sendRequest(ppsObject, lastKnown))
        return position;

    if (!receiveReply(&lastKnown, ppsObject))
        return position;

    if (!lastKnown.contains("res") || lastKnown.value("res").toString() != "location")
        return position;

    // the return value of populatePositionInfo() is ignored since either way position is returned
    // by lastKnownPosition()
    (void)populatePositionInfo(&position, lastKnown);

    return position;
}

// Constructor. Note there are two PpsObjects for handling the two different types of requests that
// can be simultaneously made and which must be handled independently (apart from both being
// emitted through the same signal when done-part of Qt Mobility spec.
QGeoPositionInfoSourceBbPrivate::QGeoPositionInfoSourceBbPrivate(QGeoPositionInfoSourceBb *parent)
        :   QObject(parent),
            _startUpdatesInvoked(false),
            _requestUpdateInvoked(false),
            _canEmitPeriodicUpdatesTimeout(true),
            q_ptr(parent),
            _periodicUpdatePpsObject(new bb::PpsObject(global::locationManagerPpsFile, this)),
            _singleUpdatePpsObject(new bb::PpsObject(global::locationManagerPpsFile, this)),
            _sourceError(QGeoPositionInfoSource::NoError),
            _accuracy(0.0),
            _responseTime(0.0),
            _canRunInBackground(false),
            _fixType(QString()),
            _appId(QString()),
            _appPassword(QString()),
            _pdeUrl(QUrl()),
            _slpUrl(QUrl()),
            _replyDat(QVariantMap()),
            _replyErrorCode(bb::location::PositionErrorCode::None),
            _replyErr(QString()),
            _replyErrStr(QString()),
            _resetType(QString())
{
    // register bb::location::PositionErrorCode::Type so it can be used with QObject::property()
    qRegisterMetaType<bb::location::PositionErrorCode::Type>();

    // connect to periodic update PpsObject::readyRead()
    connect(_periodicUpdatePpsObject, SIGNAL(readyRead()), SLOT(receivePeriodicPositionReply()));

    // connect to single update PpsObject::readyRead()
    connect(_singleUpdatePpsObject, SIGNAL(readyRead()), SLOT(receiveSinglePositionReply()));

    // queued connection to signal updateTimeout()
    connect(this, SIGNAL(queuedUpdateTimeout()), SLOT(emitUpdateTimeout()), Qt::QueuedConnection);
}

QGeoPositionInfoSourceBbPrivate::~QGeoPositionInfoSourceBbPrivate()
{
    stopUpdates();
}

// request periodic updates
void QGeoPositionInfoSourceBbPrivate::startUpdates()
{
    // do nothing if periodic updates have already been started
    if (_startUpdatesInvoked)
        return;

    // This flag is used to limit emitting the timeout signal to once per each interruption in the
    // periodic updates. Since updates are being started here ensure the flag is set to true.
    _canEmitPeriodicUpdatesTimeout = true;

    // build a request and initiate it
    if (requestPositionInfo(true)) {
        _startUpdatesInvoked = true;
        _currentPosition = QGeoPositionInfo();
    } else {
        // With Qt5 the error() signal was introduced. If there are any receivers emit error() else
        // maintain QtMobility behavior.
        _sourceError
            = positionErrorCodeToErrorMap.value(_replyErrorCode,
                                                QGeoPositionInfoSource::UnknownSourceError);
        Q_Q(QGeoPositionInfoSourceBb);
        if (q->receivers(SIGNAL(error(QGeoPositionInfoSource::Error)))) {
            Q_EMIT ((QGeoPositionInfoSource *)q)->error(_sourceError);
        } else {
            // user is expecting a signal to be emitted, cannot emit positionUpdated() because of
            // error so emit timeout signal. The connection is queued because it is possible for the
            // user to call startUpdates() from within the slot handling the timeout. The queued
            // connection avoids potential infinite recursion.
            Q_EMIT queuedUpdateTimeout();
        }
    }
}

// stop periodic updates
void QGeoPositionInfoSourceBbPrivate::stopUpdates()
{
    // do nothing if periodic updates have not been started
    if (!_startUpdatesInvoked)
        return;

    cancelPositionInfo(true);
    _startUpdatesInvoked = false;
    _currentPosition = QGeoPositionInfo();

    // close the pps file to ensure readyRead() does not spin in the event that we don't read the
    // reply to the cancel request. Note that open() is done lazily in sendRequest().
    _periodicUpdatePpsObject->close();
}

// periodic updates have timed out
void QGeoPositionInfoSourceBbPrivate::periodicUpdatesTimeout()
{
    // do nothing if periodic updates have not been started
    if (!_startUpdatesInvoked)
        return;

    // timeout has occurred, but periodic updates are still active. Ensure the timeout signal is
    // emitted only once per interruption of updates. _canEmitPeriodicUpdatesTimeout is set back
    // to true when the next successful periodic update occurs (see emitPositionUpdated()). This
    // behavior is per the Qt Mobility Location API documentation.
    if (_canEmitPeriodicUpdatesTimeout) {
        _canEmitPeriodicUpdatesTimeout = false;
        emitUpdateTimeout();
    }
}

// request single update
void QGeoPositionInfoSourceBbPrivate::requestUpdate(int msec)
{
    // do nothing if an immediate update has already been requested
    if (_requestUpdateInvoked)
        return;

    if (msec) {
        // If it is not possible to update in msec timeout immediately.
        Q_Q(QGeoPositionInfoSourceBb);
        if (msec < q->minimumUpdateInterval()) {
            // The connection is queued because it is possible for the user to call requestUpdate()
            // from within the slot handling the timeout. The queued connection avoids potential
            // infinite recursion.
            Q_EMIT queuedUpdateTimeout();
            return;
        }
    }

    if (requestPositionInfo(false, msec)) {
        _requestUpdateInvoked = true;
    } else {
        // With Qt5 the error() signal was introduced. If there are any receivers emit error() else
        // maintain QtMobility behavior.
        _sourceError
            = positionErrorCodeToErrorMap.value(_replyErrorCode,
                                                QGeoPositionInfoSource::UnknownSourceError);
        Q_Q(QGeoPositionInfoSourceBb);
        if (q->receivers(SIGNAL(error(QGeoPositionInfoSource::Error)))) {
            Q_EMIT ((QGeoPositionInfoSource *)q)->error(_sourceError);
        } else {
            // user is expecting a signal to be emitted, cannot emit positionUpdated() because of
            // error so emit timeout signal. The connection is queued because it is possible for the
            // user to call startUpdates() from within the slot handling the timeout. The queued
            // connection avoids potential infinite recursion.
            Q_EMIT queuedUpdateTimeout();
        }
    }
}

// single update has timed out. This is a slot for the requestUpdate timer
void QGeoPositionInfoSourceBbPrivate::singleUpdateTimeout()
{
    _requestUpdateInvoked = false;

    emitUpdateTimeout();

    if (!_requestUpdateInvoked) {
        // close the pps file to ensure readyRead() does not spin in the event that there are
        // unexpected replies that we don't read. Note that open() is done lazily in sendRequest().
        _singleUpdatePpsObject->close();
    }
}

// This slot is intended for queued connection to the signal queuedUpdateTimeout(). If an error
// occurs when an update is requested the error is relayed via updateTimeout() but the connection
// is queued to avoid potential infinite recursion.
void QGeoPositionInfoSourceBbPrivate::emitUpdateTimeout()
{
    Q_Q(QGeoPositionInfoSourceBb);
    Q_EMIT q->updateTimeout();
}

void QGeoPositionInfoSourceBbPrivate::emitPositionUpdated(const QGeoPositionInfo &update)
{
    // having successfully received a position update, set _canEmitPeriodicUpdatesTimeout to true,
    // which (re)enables a timeout to be emitted upon any subsequent error in periodic updating.
    _canEmitPeriodicUpdatesTimeout = true;

    Q_Q(QGeoPositionInfoSourceBb);
    Q_EMIT q->positionUpdated(update);
}

bool QGeoPositionInfoSourceBbPrivate::receivePositionReply(bb::PpsObject &ppsObject)
{
    QVariantMap reply;
    // receiveReply() tests for errors associated with the request being replied to
    if (!receiveReply(&reply, ppsObject)) {
        _replyErrorCode = bb::location::PositionErrorCode::FatalUnknown;

        // if there is an error from Location Manager report it so user can access it through the
        // properties when responding to the updateTimeout() signal.
        if (reply.contains("errCode")) {
            int errCode = reply.value("errCode").toInt();
            _replyErrorCode
                = intToPositionErrorCodeMap.value(errCode,
                                                  bb::location::PositionErrorCode::FatalUnknown);
            if (fatalError(_replyErrorCode)) {
                _sourceError
                    = positionErrorCodeToErrorMap.value(_replyErrorCode,
                                                        QGeoPositionInfoSource::UnknownSourceError);
            }

            if (reply.contains("err")) {
                _replyErr = reply.value("err").toString();
                if (reply.contains("errstr")) {
                    _replyErrStr = reply.value("errstr").toString();
                }
            }
        } else {
            _sourceError = QGeoPositionInfoSource::UnknownSourceError;
        }
        return false;
    }

    // clear any errors
    _replyErrorCode = bb::location::PositionErrorCode::None;
    _replyErr = QString();
    _replyErrStr = QString();

    // check that this is a location reply (could be a reply to another request type, eg. cancel,
    // which is ignored here)
    if (reply.contains("res") && reply.value("res").toString() == "location") {
        // keep the raw LM reply for access via Qt properties.
        _replyDat = reply.value("dat").toMap();

        // extract the geo position info from the reply into _currentPosition
        if (populatePositionInfo(&_currentPosition, reply)) {
            emitPositionUpdated(_currentPosition);
        }
    }

    return true;
}

void QGeoPositionInfoSourceBbPrivate::receivePeriodicPositionReply()
{
    // don't try to receive a reply if periodic updates have not been started. This is
    // necessary because this slot is connected to PpsObject::readyRead() and could be
    // invoked any time the pps file is updated by the server. Under error conditions
    // this would otherwise lead to a circular calling sequence: receive, timeout due to
    // error, cancel, receive...
    if (!_startUpdatesInvoked)
        return;

    if (!receivePositionReply(*_periodicUpdatePpsObject)) {
        Q_Q(QGeoPositionInfoSourceBb);
        if (fatalError(_replyErrorCode)
            && q->receivers(SIGNAL(error(QGeoPositionInfoSource::Error)))) {
            Q_EMIT ((QGeoPositionInfoSource *)q)->error(_sourceError);
        } else {
            periodicUpdatesTimeout();
        }
    }
}

void QGeoPositionInfoSourceBbPrivate::receiveSinglePositionReply()
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

    if (!receivePositionReply(*_singleUpdatePpsObject)) {
        Q_Q(QGeoPositionInfoSourceBb);
        if (fatalError(_replyErrorCode)
            && q->receivers(SIGNAL(error(QGeoPositionInfoSource::Error)))) {
            Q_EMIT ((QGeoPositionInfoSource *)q)->error(_sourceError);
        } else {
            singleUpdateTimeout();
        }
    }

    if (!_requestUpdateInvoked) {
        // close the pps file to ensure readyRead() does not spin in the event that there are
        // unexpected replies that we don't read. Note that open() is done lazily in sendRequest().
        _singleUpdatePpsObject->close();
    }
}

///////////////////////////
//
// QGeoPositionInfoSourceBb
//
///////////////////////////

/*!
    \class QGeoPositionInfoSourceBb
    \brief The QGeoPositionInfoSourceBb class is for the distribution of positional updates obtained
           from the underlying Qnx Location Manager.

    QGeoPositionInfoSourceBb is a subclass of QGeoPositionInfoSource. The static function
    QGeoPositionInfoSource::createDefaultSource() creates a default
    position source that is appropriate for the platform, if one is available. On BB10 this is
    a QGeoPositionInfoSourceBb instance.

    Users of a QGeoPositionInfoSource subclass can request the current position using
    requestUpdate(), or start and stop regular position updates using
    startUpdates() and stopUpdates(). When an update is available,
    positionUpdated() is emitted. The last known position can be accessed with
    lastKnownPosition().

    If regular position updates are required, setUpdateInterval() can be used
    to specify how often these updates should be emitted. If no interval is
    specified, updates are simply provided whenever they are available.
    For example:

    \code
        // Emit updates every 10 seconds if available
        QGeoPositionInfoSource *source = QGeoPositionInfoSource::createDefaultSource(0);
        if (source)
            source->setUpdateInterval(10000);
    \endcode

    To remove an update interval that was previously set, call
    setUpdateInterval() with a value of 0.

    Note that the position source may have a minimum value requirement for
    update intervals, as returned by minimumUpdateInterval().
*/

/*!
    Constructs a QGeoPositionInfoSourceBb instance with the given \a parent
    and \a updateMode.
*/
QGeoPositionInfoSourceBb::QGeoPositionInfoSourceBb(QObject *parent)
        : QGeoPositionInfoSource(parent),
        d_ptr(new QGeoPositionInfoSourceBbPrivate(this))
{
}

/*!
    Destroys the position source.
*/
QGeoPositionInfoSourceBb::~QGeoPositionInfoSourceBb()
{
}

/*!
    \reimp
*/
void QGeoPositionInfoSourceBb::setUpdateInterval(int msec)
{
    int interval = msec;
    if (interval != 0)
        interval = qMax(msec, minimumUpdateInterval());

    if (interval == updateInterval())
        return;

    QGeoPositionInfoSource::setUpdateInterval(interval);

    Q_D(QGeoPositionInfoSourceBb);
    if (d->_startUpdatesInvoked) {
        d->stopUpdates();
        d->startUpdates();
    }
}

/*!
    \reimp
*/
void QGeoPositionInfoSourceBb::setPreferredPositioningMethods(PositioningMethods methods)
{
    PositioningMethods previousPreferredPositioningMethods = preferredPositioningMethods();
    QGeoPositionInfoSource::setPreferredPositioningMethods(methods);
    if (previousPreferredPositioningMethods == preferredPositioningMethods())
        return;

    Q_D(QGeoPositionInfoSourceBb);
    if (d->_startUpdatesInvoked) {
        d->stopUpdates();
        d->startUpdates();
    }
}

/*!
    \reimp
*/
void QGeoPositionInfoSourceBb::startUpdates()
{
    Q_D(QGeoPositionInfoSourceBb);
    d->startUpdates();
}

/*!
    \reimp
*/
void QGeoPositionInfoSourceBb::stopUpdates()
{
    Q_D(QGeoPositionInfoSourceBb);
    d->stopUpdates();
}

/*!
    \reimp
*/
void QGeoPositionInfoSourceBb::requestUpdate(int msec)
{
    Q_D(QGeoPositionInfoSourceBb);
    d->requestUpdate(msec);
}

/*!
    \reimp
*/
QGeoPositionInfo
    QGeoPositionInfoSourceBb::lastKnownPosition(bool fromSatellitePositioningMethodsOnly) const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->lastKnownPosition(fromSatellitePositioningMethodsOnly);
}

/*!
    \reimp
*/
QGeoPositionInfoSource::PositioningMethods
    QGeoPositionInfoSourceBb::supportedPositioningMethods() const
{
    return AllPositioningMethods;
}

/*!
    \reimp
*/
int QGeoPositionInfoSourceBb::minimumUpdateInterval() const
{
    return global::minUpdateInterval;
}

QGeoPositionInfoSource::Error QGeoPositionInfoSourceBb::error() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_sourceError;
}

// these properties extend QGeoPositionInfoSource allowing use of additional features of the Qnx Location Manager

// the following properties are the fields of the dat parameter of the location request

/*!
    \property QGeoPositionInfoSourceBb::period
    \brief This property specifies the period of the location request, in seconds. A value of
    '0' indicates a one-time location request.
*/
double QGeoPositionInfoSourceBb::period() const
{
    // convert from msec to sec
    return updateInterval() / 1000.0;
}

void QGeoPositionInfoSourceBb::setPeriod(double period)
{
    // convert from sec to msec, round to nearest msec
    setUpdateInterval(qRound(static_cast<qreal>(period * 1000.0)));
}

/*!
    \property QGeoPositionInfoSourceBb::accuracy
    \brief This property specifies the desired accuracy of the fix, in meters. A value of '0'
    disables accuracy criteria.
*/
double QGeoPositionInfoSourceBb::accuracy() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_accuracy;
}

void QGeoPositionInfoSourceBb::setAccuracy(double accuracy)
{
    Q_D(QGeoPositionInfoSourceBb);
    d->_accuracy = accuracy;
}

/*!
    \property QGeoPositionInfoSourceBb::responseTime
    \brief This property specifies the desired response time of the fix, in seconds. A value of
    '0' disables response time criteria.
*/
double QGeoPositionInfoSourceBb::responseTime() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_responseTime;
}

void QGeoPositionInfoSourceBb::setResponseTime(double responseTime)
{
    Q_D(QGeoPositionInfoSourceBb);
    d->_responseTime = responseTime;
}

/*!
    \property QGeoPositionInfoSourceBb::canRunInBackground
    \brief This property determines whether or not requests are allowed to run with the device
    in standby (i.e. screen off)
*/
bool QGeoPositionInfoSourceBb::canRunInBackground() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_canRunInBackground;
}

void QGeoPositionInfoSourceBb::setCanRunInBackground(bool canRunInBackground)
{
    Q_D(QGeoPositionInfoSourceBb);
    d->_canRunInBackground = canRunInBackground;
}

/*!
    \property QGeoPositionInfoSourceBb::provider
    \brief This property specifies the location provider you wish to use (hybrid, gnss,
    network).
*/
QString QGeoPositionInfoSourceBb::provider() const
{
    return positioningMethodsToProviderMap.value(preferredPositioningMethods());
}

void QGeoPositionInfoSourceBb::setProvider(const QString &provider)
{
    setPreferredPositioningMethods(positioningMethodsToProviderMap.key(provider));
}

/*!
    \property QGeoPositionInfoSourceBb::fixType
    \brief This property specifies the fix type you wish to use (best, gps_ms_based,
    gps_ms_assisted, gps_autonomous, cellsite, wifi).
*/
QString QGeoPositionInfoSourceBb::fixType() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_fixType;
}

void QGeoPositionInfoSourceBb::setFixType(const QString &fixType)
{
    Q_D(QGeoPositionInfoSourceBb);
    d->_fixType = fixType;
}

/*!
    \property QGeoPositionInfoSourceBb::appId
    \brief This property specifies a special application id.
*/
QString QGeoPositionInfoSourceBb::appId() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_appId;
}

void QGeoPositionInfoSourceBb::setAppId(const QString &appId)
{
    Q_D(QGeoPositionInfoSourceBb);
    d->_appId = appId;
}

/*!
    \property QGeoPositionInfoSourceBb::appPassword
    \brief This property specifies a special application password, goes with appId above.
*/
QString QGeoPositionInfoSourceBb::appPassword() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_appPassword;
}

void QGeoPositionInfoSourceBb::setAppPassword(const QString &appPassword)
{
    Q_D(QGeoPositionInfoSourceBb);
    d->_appPassword = appPassword;
}

/*!
    \property QGeoPositionInfoSourceBb::pdeUrl
    \brief This property specifies the PDE URL (i.e. tcp://user:pass@address.dom:1234).
*/
QUrl QGeoPositionInfoSourceBb::pdeUrl() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_pdeUrl;
}

void QGeoPositionInfoSourceBb::setPdeUrl(const QUrl &pdeUrl)
{
    Q_D(QGeoPositionInfoSourceBb);
    d->_pdeUrl = pdeUrl;
}

/*!
    \property QGeoPositionInfoSourceBb::slpUrl
    \brief This property specifies the SLP URL (i.e. tcp://user:pass@address.dom:1234).
*/
QUrl QGeoPositionInfoSourceBb::slpUrl() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_slpUrl;
}

void QGeoPositionInfoSourceBb::setSlpUrl(const QUrl &slpUrl)
{
    Q_D(QGeoPositionInfoSourceBb);
    d->_slpUrl = slpUrl;
}

// the following read-only properties are the fields of the Location Manager generic reply

/*!
    \property QGeoPositionInfoSourceBb::replyDat
    \brief This property specifies the object containing the reply data (such as latitude,
    longitude, satellites, etc). If the replyErr is not empty then replyDat may be empty or
    stale. replyDat is expected to be consumed in the slot connected to the positionUpdated()
    signal, otherwise its contents are undefined.
*/
QVariantMap QGeoPositionInfoSourceBb::replyDat() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_replyDat;
}

/*!
    \property QGeoPositionInfoSourceBb::replyErrorCode
    \brief If not empty this property indicates that an error has occurred, and identifies the
    error.

    Possible values are:

    Code | Description
    None | No error
    FatalDisabled | Location services is disabled
    FatalNoLastKnownPosition | There is no last known position on the device
    FatalInsufficientProviders | There are insufficient available location technology providers
                                 to process your request
    FatalInvalidRequest | One or more of the request parameters are invalid.
    WarnTimeout | A timeout has occurred while processing your request. The request will
                  continue until your location is obtained
    WarnLostTracking | The location fix has been lost due to insufficient coverage. The request
                       will continue until your location is reacquired
    WarnStationary | The device is stationary. No further updates until the device resumes
                     movement

    replyErrorCode is expected to be consumed in the slot connected to the updateTimeout()
    signal, which is emitted when an error is detected. Otherwise its contents are undefined.
*/
bb::location::PositionErrorCode::Type QGeoPositionInfoSourceBb::replyErrorCode() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_replyErrorCode;
}

/*!
    \property QGeoPositionInfoSourceBb::replyErr
    \brief If not empty this property indicates that an error has occurred, and identifies the
    error. replyErr is expected to be consumed in the slot connected to the updateTimeout()
    signal, which is emitted when an error is detected. Otherwise its contents are undefined.
*/
QString QGeoPositionInfoSourceBb::replyErr() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_replyErr;
}

/*!
    \property QGeoPositionInfoSourceBb::replyErrStr
    \brief This property is not empty if and only if the replyErr parameter is present, it
    describes the error. replyErrStr is expected to be consumed in the slot connected to the
    updateTimeout() signal, which is emitted when an error is detected. Otherwise its contents
    are undefined.
*/
QString QGeoPositionInfoSourceBb::replyErrStr() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_replyErrStr;
}

/*!
    \property QGeoPositionInfoSourceBb::locationServicesEnabled
    \brief This property indicates whether the location services are enabled or not. If
    location services are disabled then no position updates will occur. The user must enable
    location services through the Settings app before any position updates will be available.
*/
bool QGeoPositionInfoSourceBb::locationServicesEnabled() const
{
    bool locationEnabled = false;
    if (location_manager_get_status(&locationEnabled) != 0) {
        qWarning() << "QGeoPositionInfoSourceBb::locationServicesEnabled() : "
                      "call to location_manager_get_status() failed.";
    }
    return locationEnabled;
}

/*!
    \property QGeoPositionInfoSourceBb::reset
    \brief By setting this property a reset of all location providers is requested through the
    Location Manager. The value of reset specifies the type of reset to be performed. Valid
    reset types are "cold", "warm", "hot", and "factory". The reset is not actually carried out
    until position updates are restarted. The current value of this property, i.e.
    property("reset"), is not particularly useful, it is simply the reset type corresponding to
    the last time setProperty("reset", resetType) was called. A Qt property must have a READ
    method, hence the reason for defining resetType().
*/
QString QGeoPositionInfoSourceBb::resetType() const
{
    Q_D(const QGeoPositionInfoSourceBb);
    return d->_resetType;
}

void QGeoPositionInfoSourceBb::requestReset(const QString &resetType)
{
    if (validResetTypes.contains(resetType)) {
        Q_D(QGeoPositionInfoSourceBb);
        d->_resetType = resetType;
        d->resetLocationProviders();
    }
}
