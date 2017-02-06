/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgeocodingmanager.h"
#include "qgeocodingmanager_p.h"
#include "qgeocodingmanagerengine.h"

#include "qgeorectangle.h"
#include "qgeocircle.h"

#include <QLocale>

QT_BEGIN_NAMESPACE

/*!
    \class QGeoCodingManager
    \inmodule QtLocation
    \ingroup QtLocation-geocoding
    \since 5.6

    \brief The QGeoCodingManager class provides support for geocoding
    operations.

    The geocode() and reverseGeocode() functions return
    QGeoCodeReply objects, which manage these operations and report on the
    result of the operations and any errors which may have occurred.

    The geocode() and reverseGeocode() functions can be used to convert
    QGeoAddress instances to QGeoCoordinate instances and vice-versa.

    The geocode() function is also overloaded to allow a user to perform a free text
    geocoding operation, if the string provided can be interpreted as
    an address it can be geocoded to coordinate information.

    Instances of QGeoCodingManager can be accessed with
    QGeoServiceProvider::geocodingManager().
*/

/*!
    Constructs a new manager with the specified \a parent and with the
    implementation provided by \a engine.

    This constructor is used interally by QGeoServiceProviderFactory. Regular
    users should acquire instances of QGeoCodingManager with
    QGeoServiceProvider::geocodingManager();
*/
QGeoCodingManager::QGeoCodingManager(QGeoCodingManagerEngine *engine, QObject *parent)
    : QObject(parent),
      d_ptr(new QGeoCodingManagerPrivate())
{
    d_ptr->engine = engine;
    if (d_ptr->engine) {
        d_ptr->engine->setParent(this);

        connect(d_ptr->engine,
                SIGNAL(finished(QGeoCodeReply*)),
                this,
                SIGNAL(finished(QGeoCodeReply*)));

        connect(d_ptr->engine,
                SIGNAL(error(QGeoCodeReply*,QGeoCodeReply::Error,QString)),
                this,
                SIGNAL(error(QGeoCodeReply*,QGeoCodeReply::Error,QString)));
    } else {
        qFatal("The geocoding manager engine that was set for this geocoding manager was NULL.");
    }
}

/*!
    Destroys this manager.
*/
QGeoCodingManager::~QGeoCodingManager()
{
    delete d_ptr;
}

/*!
    Returns the name of the engine which implements the behaviour of this
    geocoding manager.

    The combination of managerName() and managerVersion() should be unique
    amongst the plugin implementations.
*/
QString QGeoCodingManager::managerName() const
{
//    if (!d_ptr->engine)
//        return QString();

    return d_ptr->engine->managerName();
}

/*!
    Returns the version of the engine which implements the behaviour of this
    geocoding manager.

    The combination of managerName() and managerVersion() should be unique
    amongst the plugin implementations.
*/
int QGeoCodingManager::managerVersion() const
{
//    if (!d_ptr->engine)
//        return -1;

    return d_ptr->engine->managerVersion();
}

/*!
    Begins the geocoding of \a address. Geocoding is the process of finding a
    coordinate that corresponds to a given address.

    A QGeoCodeReply object will be returned, which can be used to manage the
    geocoding operation and to return the results of the operation.

    This manager and the returned QGeoCodeReply object will emit signals
    indicating if the operation completes or if errors occur.

    If supportsGeocoding() returns false an
    QGeoCodeReply::UnsupportedOptionError will occur.

    Once the operation has completed, QGeoCodeReply::locations() can be used to
    retrieve the results, which will consist of a list of QGeoLocation objects.
    These objects represent a combination of coordinate and address data.

    The address data returned in the results may be different from \a address.
    This will usually occur if the geocoding service backend uses a different
    canonical form of addresses or if \a address was only partially filled out.

    If \a bounds is non-null and is a valid QGeoShape it will be used to
    limit the results to those that are contained within \a bounds. This is
    particularly useful if \a address is only partially filled out, as the
    service will attempt to geocode all matches for the specified data.

    The user is responsible for deleting the returned reply object, although
    this can be done in the slot connected to QGeoCodingManager::finished(),
    QGeoCodingManager::error(), QGeoCodeReply::finished() or
    QGeoCodeReply::error() with deleteLater().
*/
QGeoCodeReply *QGeoCodingManager::geocode(const QGeoAddress &address, const QGeoShape &bounds)
{
    return d_ptr->engine->geocode(address, bounds);
}


/*!
    Begins the reverse geocoding of \a coordinate. Reverse geocoding is the
    process of finding an address that corresponds to a given coordinate.

    A QGeoCodeReply object will be returned, which can be used to manage the
    reverse geocoding operation and to return the results of the operation.

    This manager and the returned QGeoCodeReply object will emit signals
    indicating if the operation completes or if errors occur.

    If supportsReverseGeocoding() returns false an
    QGeoCodeReply::UnsupportedOptionError will occur.

    At that point QGeoCodeReply::locations() can be used to retrieve the
    results, which will consist of a list of QGeoLocation objects. These objects
    represent a combination of coordinate and address data.

    The coordinate data returned in the results may be different from \a
    coordinate. This will usually occur if the reverse geocoding service
    backend shifts the coordinates to be closer to the matching addresses, or
    if the backend returns results at multiple levels of detail.

    If multiple results are returned by the reverse geocoding service backend
    they will be provided in order of specificity. This normally occurs if the
    backend is configured to reverse geocode across multiple levels of detail.
    As an example, some services will return address and coordinate pairs for
    the street address, the city, the state and the country.

    If \a bounds is non-null and a valid QGeoRectangle it will be used to
    limit the results to those that are contained within \a bounds.

    The user is responsible for deleting the returned reply object, although
    this can be done in the slot connected to QGeoCodingManager::finished(),
    QGeoCodingManager::error(), QGeoCodeReply::finished() or
    QGeoCodeReply::error() with deleteLater().
*/
QGeoCodeReply *QGeoCodingManager::reverseGeocode(const QGeoCoordinate &coordinate, const QGeoShape &bounds)
{
    return d_ptr->engine->reverseGeocode(coordinate, bounds);
}

/*!
    Begins geocoding for a location matching \a address.

    A QGeoCodeReply object will be returned, which can be used to manage the
    geocoding operation and to return the results of the operation.

    This manager and the returned QGeoCodeReply object will emit signals
    indicating if the operation completes or if errors occur.

    Once the operation has completed, QGeoCodeReply::locations() can be used to
    retrieve the results, which will consist of a list of QGeoLocation objects.
    These objects represent a combination of coordinate and address data.

    If \a limit is -1 the entire result set will be returned, otherwise at most
    \a limit results will be returned.

    The \a offset parameter is used to ask the geocoding service to not return the
    first \a offset results.

    The \a limit and \a offset results are used together to implement paging.

    If \a bounds is non-null and a valid QGeoShape it will be used to
    limit the results to those that are contained within \a bounds.

    The user is responsible for deleting the returned reply object, although
    this can be done in the slot connected to QGeoCodingManager::finished(),
    QGeoCodingManager::error(), QGeoCodeReply::finished() or
    QGeoCodeReply::error() with deleteLater().
*/
QGeoCodeReply *QGeoCodingManager::geocode(const QString &address,
        int limit,
        int offset,
        const QGeoShape &bounds)
{
    QGeoCodeReply *reply = d_ptr->engine->geocode(address,
                             limit,
                             offset,
                             bounds);
    return reply;
}

/*!
    Sets the locale to be used by this manager to \a locale.

    If this geocoding manager supports returning the results
    in different languages, they will be returned in the language of \a locale.

    The locale used defaults to the system locale if this is not set.
*/
void QGeoCodingManager::setLocale(const QLocale &locale)
{
    d_ptr->engine->setLocale(locale);
}

/*!
    Returns the locale used to hint to this geocoding manager about what
    language to use for the results.
*/
QLocale QGeoCodingManager::locale() const
{
    return d_ptr->engine->locale();
}

/*!
\fn void QGeoCodingManager::finished(QGeoCodeReply *reply)

    This signal is emitted when \a reply has finished processing.

    If reply::error() equals QGeoCodeReply::NoError then the processing
    finished successfully.

    This signal and QGeoCodeReply::finished() will be emitted at the same
    time.

    \note Do not delete the \a reply object in the slot connected to this
    signal. Use deleteLater() instead.
*/

/*!
\fn void QGeoCodingManager::error(QGeoCodeReply *reply, QGeoCodeReply::Error error, QString errorString)

    This signal is emitted when an error has been detected in the processing of
    \a reply. The QGeoCodingManager::finished() signal will probably follow.

    The error will be described by the error code \a error. If \a errorString is
    not empty it will contain a textual description of the error.

    This signal and QGeoCodeReply::error() will be emitted at the same time.

    \note Do not delete the \a reply object in the slot connected to this
    signal. Use deleteLater() instead.
*/

/*******************************************************************************
*******************************************************************************/

QGeoCodingManagerPrivate::QGeoCodingManagerPrivate()
    : engine(0) {}

QGeoCodingManagerPrivate::~QGeoCodingManagerPrivate()
{
    delete engine;
}

/*******************************************************************************
*******************************************************************************/

#include "moc_qgeocodingmanager.cpp"

QT_END_NAMESPACE
