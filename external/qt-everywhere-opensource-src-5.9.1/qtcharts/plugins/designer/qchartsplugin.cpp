/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
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

#include "qchartsplugin.h"
#include <QtCharts/QChartView>
#include <QtCore/QtPlugin>

QT_CHARTS_USE_NAMESPACE

QChartsPlugin::QChartsPlugin(QObject *parent) :
    QObject(parent)
{
}

QChartsPlugin::~QChartsPlugin()
{
}

QString QChartsPlugin::name() const
{
    return QStringLiteral("QtCharts::QChartView");
}

QString QChartsPlugin::includeFile() const
{
#ifdef linux
    QString myNewLine = QStringLiteral("\n");
#endif
#ifdef WIN32
    QString myNewLine = QStringLiteral("\n\r");
#endif
#ifdef __APPLE__
    QString myNewLine = QStringLiteral("\n");
#endif
    return QStringLiteral("<QtCharts/QChartView>") + myNewLine
            + QStringLiteral("#include <QtCharts/chartsnamespace.h>");
}

QString QChartsPlugin::group() const
{
    return tr("Qt Charts Widgets");
}

QIcon QChartsPlugin::icon() const
{
    return QIcon(QStringLiteral(":/images/qcharts.png"));
}

QString QChartsPlugin::toolTip() const
{
    return tr("A Qt Charts view widget");
}

QString QChartsPlugin::whatsThis() const
{
    return tr("This widget is presents QChartView widget");
}

bool QChartsPlugin::isContainer() const
{
    return false;
}

QWidget *QChartsPlugin::createWidget(QWidget *parent)
{
    return new QChartView(new QChart(), parent);
}

QString QChartsPlugin::domXml() const
{
    return QStringLiteral("\
    <ui language=\"c++\">\
        <widget class=\"QtCharts::QChartView\" name=\"chartview\">\
            <property name=\"geometry\">\
                <rect>\
                    <x>0</x>\
                    <y>0</y>\
                    <width>200</width>\
                    <height>200</height>\
                </rect>\
            </property>\
        </widget>\
    </ui>");
}

