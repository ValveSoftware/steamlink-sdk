/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "qgeomappingmanagerengine_p.h"
#include "qgeomappingmanagerengine_p_p.h"
#include "qgeotiledmapreply_p.h"
#include "qgeotilespec_p.h"

#include <QThread>

QT_BEGIN_NAMESPACE

/*!
    \class QGeoMappingManagerEngine
    \inmodule QtLocation
    \ingroup QtLocation-impl
    \since Qt Location 5.0
    \internal

    \brief Provides support functionality for map display with QGeoServiceProvider.

    The QGeoMappingManagerEngine class provides an interface and convenience
    methods to implementors of QGeoServiceProvider plugins who want to
    provide support for displaying and interacting with maps.
*/

/*!
    Constructs a new engine with the specified \a parent.
*/
QGeoMappingManagerEngine::QGeoMappingManagerEngine(QObject *parent)
    : QObject(parent),
      d_ptr(new QGeoMappingManagerEnginePrivate()) {}

/*!
    Destroys this engine.
*/
QGeoMappingManagerEngine::~QGeoMappingManagerEngine()
{
    Q_D(QGeoMappingManagerEngine);
    delete d;
}

/*!
    Marks the engine as initialized. Subclasses of QGeoMappingManagerEngine are to
    call this method after performing implementation-specific initializatioin within
    the constructor.
*/
void QGeoMappingManagerEngine::engineInitialized()
{
    Q_D(QGeoMappingManagerEngine);
    d->initialized = true;
    emit initialized();
}

/*!
    Sets the name which this engine implementation uses to distinguish itself
    from the implementations provided by other plugins to \a managerName.

    The combination of managerName() and managerVersion() should be unique
    amongst plugin implementations.
*/
void QGeoMappingManagerEngine::setManagerName(const QString &managerName)
{
    d_ptr->managerName = managerName;
}

/*!
    Returns the name which this engine implementation uses to distinguish
    itself from the implementations provided by other plugins.

    The combination of managerName() and managerVersion() should be unique
    amongst plugin implementations.
*/
QString QGeoMappingManagerEngine::managerName() const
{
    return d_ptr->managerName;
}

/*!
    Sets the version of this engine implementation to \a managerVersion.

    The combination of managerName() and managerVersion() should be unique
    amongst plugin implementations.
*/
void QGeoMappingManagerEngine::setManagerVersion(int managerVersion)
{
    d_ptr->managerVersion = managerVersion;
}

/*!
    Returns the version of this engine implementation.

    The combination of managerName() and managerVersion() should be unique
    amongst plugin implementations.
*/
int QGeoMappingManagerEngine::managerVersion() const
{
    return d_ptr->managerVersion;
}

QList<QGeoMapType> QGeoMappingManagerEngine::supportedMapTypes() const
{
    Q_D(const QGeoMappingManagerEngine);
    return d->supportedMapTypes;
}

/*!
    Sets the list of map types supported by this engine to \a mapTypes.

    Subclasses of QGeoMappingManagerEngine should use this function to ensure
    that supportedMapTypes() provides accurate information.
*/
void QGeoMappingManagerEngine::setSupportedMapTypes(const QList<QGeoMapType> &supportedMapTypes)
{
    Q_D(QGeoMappingManagerEngine);
    d->supportedMapTypes = supportedMapTypes;
}

QGeoCameraCapabilities QGeoMappingManagerEngine::cameraCapabilities()
{
    Q_D(QGeoMappingManagerEngine);
    return d->capabilities_;
}

void QGeoMappingManagerEngine::setCameraCapabilities(const QGeoCameraCapabilities &capabilities)
{
    Q_D(QGeoMappingManagerEngine);
    d->capabilities_ = capabilities;
}

/*!
    Return whether the engine has been initialized and is ready to be used.
*/

bool QGeoMappingManagerEngine::isInitialized() const
{
    Q_D(const QGeoMappingManagerEngine);
    return d->initialized;
}

/*!
    Sets the locale to be used by the this manager to \a locale.

    If this mapping manager supports returning map labels
    in different languages, they will be returned in the language of \a locale.

    The locale used defaults to the system locale if this is not set.
*/
void QGeoMappingManagerEngine::setLocale(const QLocale &locale)
{
    d_ptr->locale = locale;
}

/*!
    Returns the locale used to hint to this mapping manager about what
    language to use for map labels.
*/
QLocale QGeoMappingManagerEngine::locale() const
{
    return d_ptr->locale;
}

/*******************************************************************************
*******************************************************************************/

QGeoMappingManagerEnginePrivate::QGeoMappingManagerEnginePrivate()
    : managerVersion(-1),
      initialized(false) {}

QGeoMappingManagerEnginePrivate::~QGeoMappingManagerEnginePrivate() {}

#include "moc_qgeomappingmanagerengine_p.cpp"

QT_END_NAMESPACE
