/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "styles_plugin.h"
#include "svgimageprovider.h"

#include <qqml.h>
#include <QtCore/QLibraryInfo>

/*!
    \qmlmodule QtQuick.VirtualKeyboard.Styles 2.0
    \title Qt Quick Virtual Keyboard Styles QML Types
    \ingroup qmlmodules

    \brief Provides styling for Qt Virtual Keyboard.

    The QML types can be imported into your application using the following
    import statements in your .qml file:

    \badcode
    import QtQuick.VirtualKeyboard.Styles 2.0
    \endcode
*/

void QtVirtualKeyboardStylesPlugin::registerTypes(const char *uri)
{
    Q_INIT_RESOURCE(styles);
    const QString path(QStringLiteral("qrc:///QtQuick/VirtualKeyboard/Styles/content/"));
    qmlRegisterType(QUrl(path + QLatin1String("KeyboardStyle.qml")), uri, 1, 0, "KeyboardStyle");
    qmlRegisterType(QUrl(path + QLatin1String("KeyboardStyle.qml")), uri, 1, 1, "KeyboardStyle");
    qmlRegisterType(QUrl(path + QLatin1String("KeyboardStyle.qml")), uri, 1, 2, "KeyboardStyle");
    qmlRegisterType(QUrl(path + QLatin1String("KeyboardStyle.qml")), uri, 1, 3, "KeyboardStyle");
    qmlRegisterType(QUrl(path + QLatin1String("KeyboardStyle.qml")), uri, 2, 0, "KeyboardStyle");
    qmlRegisterType(QUrl(path + QLatin1String("KeyboardStyle.qml")), uri, 2, 1, "KeyboardStyle");
    qmlRegisterType(QUrl(path + QLatin1String("KeyIcon.qml")), uri, 1, 0, "KeyIcon");
    qmlRegisterType(QUrl(path + QLatin1String("KeyIcon.qml")), uri, 2, 0, "KeyIcon");
    qmlRegisterType(QUrl(path + QLatin1String("KeyPanel.qml")), uri, 1, 0, "KeyPanel");
    qmlRegisterType(QUrl(path + QLatin1String("KeyPanel.qml")), uri, 2, 0, "KeyPanel");
    qmlRegisterType(QUrl(path + QLatin1String("SelectionListItem.qml")), uri, 1, 0, "SelectionListItem");
    qmlRegisterType(QUrl(path + QLatin1String("SelectionListItem.qml")), uri, 2, 0, "SelectionListItem");
    qmlRegisterType(QUrl(path + QLatin1String("TraceInputKeyPanel.qml")), uri, 2, 0, "TraceInputKeyPanel");
    qmlRegisterType(QUrl(path + QLatin1String("TraceCanvas.qml")), uri, 2, 0, "TraceCanvas");
}

void QtVirtualKeyboardStylesPlugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(uri)
    engine->addImageProvider(QStringLiteral("qtvkbsvg"), new SvgImageProvider());
}
