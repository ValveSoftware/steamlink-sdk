/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Extras module of the Qt Toolkit.
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

#include "plugin.h"

#include <QtCore/qfile.h>

#include "qquickpicture_p.h"
#include "qquicktriggermode_p.h"
#include "Private/qquickcircularprogressbar_p.h"
#include "Private/qquickflatprogressbar_p.h"
#include "Private/qquickmousethief_p.h"
#include "Private/qquickmathutils_p.h"

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtQuick_Extras);
#endif
}

QT_BEGIN_NAMESPACE

static QObject *registerMathUtilsSingleton(QQmlEngine *engine, QJSEngine *jsEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(jsEngine);
    return new QQuickMathUtils();
}

QtQuickExtrasPlugin::QtQuickExtrasPlugin(QObject *parent) :
    QQmlExtensionPlugin(parent)
{
    initResources();
}

void QtQuickExtrasPlugin::registerTypes(const char *uri)
{
#ifndef QT_STATIC
    const QString prefix = baseUrl().toString();
#else
    const QString prefix = "qrc:/qt-project.org/imports/QtQuick/Extras";
#endif
    // Register public API.
    qmlRegisterUncreatableType<QQuickActivationMode>(uri, 1, 0, "ActivationMode", QLatin1String("Do not create objects of type ActivationMode"));
    // register 1.0
    qmlRegisterType(QUrl(prefix + "/CircularGauge.qml"), uri, 1, 0, "CircularGauge");
    qmlRegisterType(QUrl(prefix + "/DelayButton.qml"), uri, 1, 0, "DelayButton");
    qmlRegisterType(QUrl(prefix + "/Dial.qml"), uri, 1, 0, "Dial");
    qmlRegisterType(QUrl(prefix + "/Gauge.qml"), uri, 1, 0, "Gauge");
    qmlRegisterType(QUrl(prefix + "/PieMenu.qml"), uri, 1, 0, "PieMenu");
    qmlRegisterType(QUrl(prefix + "/StatusIndicator.qml"), uri, 1, 0, "StatusIndicator");
    qmlRegisterType(QUrl(prefix + "/ToggleButton.qml"), uri, 1, 0, "ToggleButton");
    // register 1.1
    qmlRegisterType(QUrl(prefix + "/Dial.qml"), uri, 1, 1, "Dial");
    qmlRegisterType(QUrl(prefix + "/StatusIndicator.qml"), uri, 1, 1, "StatusIndicator");
    // register 1.2
    qmlRegisterType(QUrl(prefix + "/Tumbler.qml"), uri, 1, 2, "Tumbler");
    qmlRegisterType(QUrl(prefix + "/TumblerColumn.qml"), uri, 1, 2, "TumblerColumn");
    // register 1.3
    qmlRegisterUncreatableType<QQuickTriggerMode>(uri, 1, 3, "TriggerMode", QLatin1String("Do not create objects of type TriggerMode"));
    // register 1.4
#if QT_CONFIG(picture)
    qmlRegisterType<QQuickPicture>(uri, 1, 4, "Picture");
#endif
}

void QtQuickExtrasPlugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(uri);
    Q_UNUSED(engine);
    qmlRegisterType<QQuickMouseThief>("QtQuick.Extras.Private.CppUtils", 1, 0, "MouseThief");
    qmlRegisterType<QQuickCircularProgressBar>("QtQuick.Extras.Private.CppUtils", 1, 1, "CircularProgressBar");
    qmlRegisterType<QQuickFlatProgressBar>("QtQuick.Extras.Private.CppUtils", 1, 1, "FlatProgressBar");
    qmlRegisterSingletonType<QQuickMathUtils>("QtQuick.Extras.Private.CppUtils", 1, 0, "MathUtils", registerMathUtilsSingleton);

#ifndef QT_STATIC
    const QString prefix = baseUrl().toString();
#else
    const QString prefix = "qrc:/qt-project.org/imports/QtQuick/Extras";
#endif
    const char *private_uri = "QtQuick.Extras.Private";
    qmlRegisterType(QUrl(prefix + "/Private/CircularButton.qml"), private_uri, 1, 0, "CircularButton");
    qmlRegisterType(QUrl(prefix + "/Private/CircularButtonStyleHelper.qml"), private_uri, 1, 0, "CircularButtonStyleHelper");
    qmlRegisterType(QUrl(prefix + "/Private/CircularTickmarkLabel.qml"), private_uri, 1, 0, "CircularTickmarkLabel");
    qmlRegisterType(QUrl(prefix + "/Private/Handle.qml"), private_uri, 1, 0, "Handle");
    qmlRegisterType(QUrl(prefix + "/Private/PieMenuIcon.qml"), private_uri, 1, 0, "PieMenuIcon");
    qmlRegisterSingletonType(QUrl(prefix + "/Private/TextSingleton.qml"), private_uri, 1, 0, "TextSingleton");
}

QT_END_NAMESPACE
