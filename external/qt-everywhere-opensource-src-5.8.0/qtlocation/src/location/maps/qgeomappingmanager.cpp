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

#include "qgeomappingmanager_p.h"
#include "qgeomappingmanager_p_p.h"
#include "qgeomappingmanagerengine_p.h"
#include "qgeotiledmapreply_p.h"
#include "qgeocameracapabilities_p.h"


#include "qgeomap_p.h"

#include <QTimer>
#include <QLocale>

QT_BEGIN_NAMESPACE

/*!
    \class QGeoMappingManager
    \inmodule QtLocation
    \ingroup QtLocation-maps
    \since 5.6
    \internal

    \brief The QGeoMappingManager class provides support for displaying
    and interacting with maps.
*/

/*!
    Constructs a new manager with the specified \a parent and with the
    implementation provided by \a engine.

    This constructor is used internally by QGeoServiceProviderFactory. Regular
    users should acquire instances of QGeoMappingManager with
    QGeoServiceProvider::mappingManager()
*/
QGeoMappingManager::QGeoMappingManager(QGeoMappingManagerEngine *engine, QObject *parent)
    : QObject(parent),
      d_ptr(new QGeoMappingManagerPrivate)
{
    d_ptr->engine = engine;
    if (!d_ptr->engine) {
        qFatal("The mapping manager engine that was set for this mapping manager was NULL.");
    }

    connect(d_ptr->engine,
            SIGNAL(initialized()),
            this,
            SIGNAL(initialized()),
            Qt::QueuedConnection);

    connect(d_ptr->engine,
            SIGNAL(supportedMapTypesChanged()),
            this,
            SIGNAL(supportedMapTypesChanged()),
            Qt::QueuedConnection);
}

/*!
    Destroys this mapping manager.
*/
QGeoMappingManager::~QGeoMappingManager()
{
    delete d_ptr;
}

/*!
    \fn void QGeoMappingManager::initialized()

    This signal is emitted when the mapping manager has been initialized
    and is ready to be used.
*/

/*!
    Returns the name of the engine which implements the behaviour of this
    mapping manager.

    The combination of managerName() and managerVersion() should be unique
    amongst the plugin implementations.
*/
QString QGeoMappingManager::managerName() const
{
    return d_ptr->engine->managerName();
}

/*!
    Returns the version of the engine which implements the behaviour of this
    mapping manager.

    The combination of managerName() and managerVersion() should be unique
    amongst the plugin implementations.
*/
int QGeoMappingManager::managerVersion() const
{
    return d_ptr->engine->managerVersion();
}

QGeoCameraCapabilities QGeoMappingManager::cameraCapabilities() const
{
    return d_ptr->engine->cameraCapabilities();
}

/*!
    Returns a new QGeoMap instance which will be managed by this manager.
*/
QGeoMap *QGeoMappingManager::createMap(QObject *parent)
{
    QGeoMap * map = d_ptr->engine->createMap();
    if (map)
        connect(parent, &QObject::destroyed,map, &QGeoMap::deleteLater);
    return map;
}

QList<QGeoMapType> QGeoMappingManager::supportedMapTypes() const
{
    return d_ptr->engine->supportedMapTypes();
}

/*!
    Return whether the manager has been initialized
    (will be done automatically but may take some time).

*/
bool QGeoMappingManager::isInitialized() const
{
    return d_ptr->engine->isInitialized();
}

/*!
    Sets the locale to be used by the this manager to \a locale.

    If this mapping manager supports returning map labels
    in different languages, they will be returned in the language of \a locale.

    The locale used defaults to the system locale if this is not set.
*/
void QGeoMappingManager::setLocale(const QLocale &locale)
{
    d_ptr->engine->setLocale(locale);
}

/*!
    Returns the locale used to hint to this mapping manager about what
    language to use for map labels.
*/
QLocale QGeoMappingManager::locale() const
{
    return d_ptr->engine->locale();
}

/*******************************************************************************
*******************************************************************************/

QGeoMappingManagerPrivate::QGeoMappingManagerPrivate()
    : engine(0) {}

QGeoMappingManagerPrivate::~QGeoMappingManagerPrivate()
{
    delete engine;
    engine = 0;
}

#include "moc_qgeomappingmanager_p.cpp"

QT_END_NAMESPACE
