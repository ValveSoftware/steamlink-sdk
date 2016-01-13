/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "qquickwidget_plugin.h"

#include <QtDesigner/QExtensionFactory>
#include <QtDesigner/QExtensionManager>

#include <QtCore/QtPlugin>
#include <QtCore/QDebug>
#include <QtQuickWidgets/QQuickWidget>

QT_BEGIN_NAMESPACE

QQuickWidgetPlugin::QQuickWidgetPlugin(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
{
}

QString QQuickWidgetPlugin::name() const
{
    return QStringLiteral("QQuickWidget");
}

QString QQuickWidgetPlugin::group() const
{
    return QStringLiteral("Display Widgets");
}

QString QQuickWidgetPlugin::toolTip() const
{
    return QStringLiteral("A widget for displaying a Qt Quick user interface.");
}

QString QQuickWidgetPlugin::whatsThis() const
{
    return toolTip();
}

QString QQuickWidgetPlugin::includeFile() const
{
    return QStringLiteral("QQuickWidget");
}

QIcon QQuickWidgetPlugin::icon() const
{
    return QIcon(QStringLiteral(":/qt-project.org/qquickwidget/images/qquickwidget.png"));
}

bool QQuickWidgetPlugin::isContainer() const
{
    return false;
}

QWidget *QQuickWidgetPlugin::createWidget(QWidget *parent)
{
    QQuickWidget *result = new QQuickWidget(parent);
    connect(result, SIGNAL(sceneGraphError(QQuickWindow::SceneGraphError,QString)),
            this, SLOT(sceneGraphError(QQuickWindow::SceneGraphError,QString)));
    return result;
}

bool QQuickWidgetPlugin::isInitialized() const
{
    return m_initialized;
}

void QQuickWidgetPlugin::initialize(QDesignerFormEditorInterface * /*core*/)
{
    if (m_initialized)
        return;

    m_initialized = true;
}

QString QQuickWidgetPlugin::domXml() const
{
    return QStringLiteral("\
    <ui language=\"c++\">\
        <widget class=\"QQuickWidget\" name=\"quickWidget\">\
            <property name=\"resizeMode\">\
                <enum>QQuickWidget::SizeRootObjectToView</enum>\
            </property>\
            <property name=\"geometry\">\
                <rect>\
                    <x>0</x>\
                    <y>0</y>\
                    <width>300</width>\
                    <height>200</height>\
                </rect>\
            </property>\
        </widget>\
    </ui>");
}

void QQuickWidgetPlugin::sceneGraphError(QQuickWindow::SceneGraphError, const QString &message)
{
    qWarning() << Q_FUNC_INFO << ':' << message;
}

QT_END_NAMESPACE
