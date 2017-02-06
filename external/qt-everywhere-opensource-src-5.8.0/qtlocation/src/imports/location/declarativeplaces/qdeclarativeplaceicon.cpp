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

#include "qdeclarativeplaceicon_p.h"
#include "error_messages.h"

#include <QtLocation/QGeoServiceProvider>
#include <QtLocation/QPlaceManager>
#include <QtQml/QQmlInfo>
#include <QCoreApplication>

QT_USE_NAMESPACE

/*!
    \qmltype Icon
    \instantiates QDeclarativePlaceIcon
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-data
    \since Qt Location 5.5

    \brief The Icon type represents an icon image source which can have multiple sizes.

    The Icon type can be used in conjunction with an \l Image type to display an icon.
    The \l url() function is used to construct an icon URL of a requested size,
    the icon which most closely matches the requested size is returned.

    The Icon type also has a parameters map which is a set of key value pairs.  The precise
    keys to use depend on the
    \l {Qt Location#Plugin References and Parameters}{plugin} being used.
    The parameters map is used by the \l Plugin to determine which URL to return.

    In the case where an icon can only possibly have one image URL, the
    parameter key of \c "singleUrl" can be used with a QUrl value.  Any Icon with this
    parameter will always return the specified URL regardless of the requested icon
    size and not defer to any Plugin.

    The following code shows how to display a 64x64 pixel icon:

    \snippet declarative/places.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/places.qml Icon

    Alternatively, a default sized icon can be specified like so:
    \snippet declarative/places.qml Icon default
*/

QDeclarativePlaceIcon::QDeclarativePlaceIcon(QObject *parent)
:   QObject(parent), m_plugin(0), m_parameters(new QQmlPropertyMap(this))
{
}

QDeclarativePlaceIcon::QDeclarativePlaceIcon(const QPlaceIcon &icon, QDeclarativeGeoServiceProvider *plugin, QObject *parent)
:   QObject(parent), m_parameters(new QQmlPropertyMap(this))
{
    if (icon.isEmpty())
        m_plugin = 0;
    else
        m_plugin = plugin;

    initParameters(icon.parameters());
}

QDeclarativePlaceIcon::~QDeclarativePlaceIcon()
{
}

/*!
    \qmlproperty QPlaceIcon Icon::icon

    For details on how to use this property to interface between C++ and QML see
    "\l {Icon - QPlaceIcon} {Interfaces between C++ and QML Code}".
*/
QPlaceIcon QDeclarativePlaceIcon::icon() const
{
    QPlaceIcon result;

    if (m_plugin)
        result.setManager(manager());
    else
        result.setManager(0);

    QVariantMap params;
    foreach (const QString &key, m_parameters->keys()) {
        const QVariant value = m_parameters->value(key);
        if (value.isValid()) {
            params.insert(key, value);
        }
    }

    result.setParameters(params);

    return result;
}

void QDeclarativePlaceIcon::setIcon(const QPlaceIcon &src)
{
    initParameters(src.parameters());
}

/*!
    \qmlmethod url Icon::url(size size)

    Returns a URL for the icon image that most closely matches the given \a size.

    If no plugin has been assigned to the icon, and the parameters do not contain the 'singleUrl' key, a default constructed URL
    is returned.

*/
QUrl QDeclarativePlaceIcon::url(const QSize &size) const
{
    return icon().url(size);
}

/*!
    \qmlproperty Object Icon::parameters

    This property holds the parameters of the icon and is a map.  These parameters
    are used by the plugin to return the appropriate URL when url() is called and to
    specify locations to save to when saving icons.

    Consult the \l {Qt Location#Plugin References and Parameters}{plugin documentation}
    for what parameters are supported and how they should be used.

    Note, due to limitations of the QQmlPropertyMap, it is not possible
    to declaratively specify the parameters in QML, assignment of parameters keys
    and values can only be accomplished by JavaScript.

*/
QQmlPropertyMap *QDeclarativePlaceIcon::parameters() const
{
    return m_parameters;
}

/*!
    \qmlproperty Plugin Icon::plugin

    The property holds the plugin that is responsible for managing this icon.
*/
QDeclarativeGeoServiceProvider *QDeclarativePlaceIcon::plugin() const
{
    return m_plugin;
}

void QDeclarativePlaceIcon::setPlugin(QDeclarativeGeoServiceProvider *plugin)
{
    if (m_plugin == plugin)
        return;

    m_plugin = plugin;
    emit pluginChanged();

    if (!m_plugin)
        return;

    if (m_plugin->isAttached()) {
        pluginReady();
    } else {
        connect(m_plugin, SIGNAL(attached()),
                this, SLOT(pluginReady()));
    }
}

/*!
    \internal
*/
void QDeclarativePlaceIcon::pluginReady()
{
    QGeoServiceProvider *serviceProvider = m_plugin->sharedGeoServiceProvider();
    QPlaceManager *placeManager = serviceProvider->placeManager();
    if (!placeManager || serviceProvider->error() != QGeoServiceProvider::NoError) {
        qmlInfo(this) << QCoreApplication::translate(CONTEXT_NAME, PLUGIN_ERROR)
                         .arg(m_plugin->name()).arg(serviceProvider->errorString());
        return;
    }
}

/*!
    \internal
    Helper function to return the manager from the plugin
*/
QPlaceManager *QDeclarativePlaceIcon::manager() const
{
    if (!m_plugin) {
           qmlInfo(this) << QStringLiteral("Plugin is not assigned to place.");
           return 0;
    }

    QGeoServiceProvider *serviceProvider = m_plugin->sharedGeoServiceProvider();
    if (!serviceProvider)
        return 0;

    QPlaceManager *placeManager = serviceProvider->placeManager();

    if (!placeManager)
        return 0;

    return placeManager;
}

/*!
    \internal
*/
void QDeclarativePlaceIcon::initParameters(const QVariantMap &parameterMap)
{
    //clear out old parameters
    foreach (const QString &key, m_parameters->keys())
        m_parameters->clear(key);

    foreach (const QString &key, parameterMap.keys()) {
        QVariant value = parameterMap.value(key);
        m_parameters->insert(key, value);
    }
}
