/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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

#include "qwebengineview_plugin.h"

#include <QtDesigner/QExtensionFactory>
#include <QtDesigner/QExtensionManager>

#include <QtCore/qplugin.h>
#include <QWebEngineView>

QT_BEGIN_NAMESPACE

static const char toolTipC[] =
    QT_TRANSLATE_NOOP(QWebEngineViewPlugin,
                      "A widget for displaying a web page, from the Qt WebEngineWidgets module");

QWebEngineViewPlugin::QWebEngineViewPlugin(QObject *parent) :
    QObject(parent),
    m_initialized(false)
{
}

QString QWebEngineViewPlugin::name() const
{
    return QStringLiteral("QWebEngineView");
}

QString QWebEngineViewPlugin::group() const
{
    return QStringLiteral("Display Widgets");
}

QString QWebEngineViewPlugin::toolTip() const
{
    return tr(toolTipC);
}

QString QWebEngineViewPlugin::whatsThis() const
{
    return tr(toolTipC);
}

QString QWebEngineViewPlugin::includeFile() const
{
    return QStringLiteral("<QtWebEngineWidgets/QWebEngineView>");
}

QIcon QWebEngineViewPlugin::icon() const
{
    return QIcon(QStringLiteral(":/qt-project.org/qwebengineview/images/qwebengineview.png"));
}

bool QWebEngineViewPlugin::isContainer() const
{
    return false;
}

QWidget *QWebEngineViewPlugin::createWidget(QWidget *parent)
{
    if (parent)
        return new QWebEngineView(parent);
    return new fake::QWebEngineView;
}

bool QWebEngineViewPlugin::isInitialized() const
{
    return m_initialized;
}

void QWebEngineViewPlugin::initialize(QDesignerFormEditorInterface * /*core*/)
{
    if (m_initialized)
        return;

    m_initialized = true;
}

QString QWebEngineViewPlugin::domXml() const
{
    return QStringLiteral("\
    <ui language=\"c++\">\
        <widget class=\"QWebEngineView\" name=\"webEngineView\">\
            <property name=\"url\">\
                <url>\
                    <string>about:blank</string>\
                </url>\
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

QT_END_NAMESPACE
