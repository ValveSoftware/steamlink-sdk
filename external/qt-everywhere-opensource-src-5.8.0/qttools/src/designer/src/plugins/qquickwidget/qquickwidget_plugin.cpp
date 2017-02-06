/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
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
    return QStringLiteral("A widget for displaying a Qt Quick 2 user interface.");
}

QString QQuickWidgetPlugin::whatsThis() const
{
    return toolTip();
}

QString QQuickWidgetPlugin::includeFile() const
{
    return QStringLiteral("QtQuickWidgets/QQuickWidget");
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
    connect(result, &QQuickWidget::sceneGraphError,
            this, &QQuickWidgetPlugin::sceneGraphError);
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
