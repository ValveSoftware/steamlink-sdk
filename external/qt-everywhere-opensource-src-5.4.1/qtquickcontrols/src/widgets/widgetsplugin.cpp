/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>
#include "qquickqmessagebox_p.h"
#include "qquickqfiledialog_p.h"
#include "qquickqcolordialog_p.h"
#include "qquickqfontdialog_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmlmodule QtQuick.PrivateWidgets 1
    \title QWidget QML Types
    \ingroup qmlmodules
    \brief Provides QML types for certain QWidgets
    \internal

    This QML module contains types which should not be depended upon in Qt Quick
    applications, but are available if the Widgets module is linked. It is
    recommended to load components from this module conditionally, if at all,
    and to provide fallback implementations in case they fail to load.

    \code
    import QtQuick.PrivateWidgets 1.1
    \endcode

    \since 5.1
*/

class QtQuick2PrivateWidgetsPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface/1.0")

public:
    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtQuick.PrivateWidgets"));

        qmlRegisterType<QQuickQMessageBox>(uri, 1, 1, "QtMessageDialog");
        qmlRegisterType<QQuickQFileDialog>(uri, 1, 0, "QtFileDialog");
        qmlRegisterType<QQuickQColorDialog>(uri, 1, 0, "QtColorDialog");
        qmlRegisterType<QQuickQFontDialog>(uri, 1, 1, "QtFontDialog");
    }
};

QT_END_NAMESPACE

#include "widgetsplugin.moc"
