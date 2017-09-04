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
#include <QtTest/QtTest>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>
#include "tst_definitions.h"

class tst_qml : public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
private slots:
    void checkPlugin_data();
    void checkPlugin();
private:
    QString componentErrors(const QQmlComponent* component) const;
    QString imports_1_1();
    QString imports_1_3();
    QString imports_1_4();
    QString imports_2_0();
    QString imports_2_1();

};

QString tst_qml::componentErrors(const QQmlComponent* component) const
{
    Q_ASSERT(component);

    QStringList errors;

    foreach (QQmlError const& error, component->errors()) {
        errors  << error.toString();
    }

    return errors.join("\n");
}

QString tst_qml::imports_1_1()
{
    return "import QtQuick 2.0 \n"
           "import QtCharts 1.1 \n";
}

QString tst_qml::imports_1_3()
{
    return "import QtQuick 2.0 \n"
           "import QtCharts 1.3 \n";
}

QString tst_qml::imports_1_4()
{
    return "import QtQuick 2.0 \n"
           "import QtCharts 1.4 \n";
}

QString tst_qml::imports_2_0()
{
    return "import QtQuick 2.0 \n"
           "import QtCharts 2.0 \n";
}

QString tst_qml::imports_2_1()
{
    return "import QtQuick 2.1 \n"
           "import QtCharts 2.1 \n";
}

void tst_qml::initTestCase()
{
}

void tst_qml::cleanupTestCase()
{
}

void tst_qml::init()
{

}

void tst_qml::cleanup()
{

}

void tst_qml::checkPlugin_data()
{
    QTest::addColumn<QString>("source");

    QTest::newRow("createChartView") <<  imports_1_1() + "ChartView{}";
    QTest::newRow("XYPoint") << imports_1_1() + "XYPoint{}";
    QTest::newRow("scatterSeries") <<  imports_1_1() + "ScatterSeries{}";
    QTest::newRow("lineSeries") <<  imports_1_1() + "LineSeries{}";
    QTest::newRow("splineSeries") <<  imports_1_1() + "SplineSeries{}";
    QTest::newRow("areaSeries") <<  imports_1_1() + "AreaSeries{}";
    QTest::newRow("barSeries") << imports_1_1() + "BarSeries{}";
    QTest::newRow("stackedBarSeries") << imports_1_1() + "StackedBarSeries{}";
    QTest::newRow("precentBarSeries") << imports_1_1() + "PercentBarSeries{}";
    QTest::newRow("horizonatlBarSeries") << imports_1_1() + "HorizontalBarSeries{}";
    QTest::newRow("horizonatlStackedBarSeries") << imports_1_1() + "HorizontalStackedBarSeries{}";
    QTest::newRow("horizonatlstackedBarSeries") << imports_1_1() + "HorizontalPercentBarSeries{}";
    QTest::newRow("pieSeries") << imports_1_1() + "PieSeries{}";
    QTest::newRow("PieSlice") << imports_1_1() + "PieSlice{}";
    QTest::newRow("BarSet") << imports_1_1() + "BarSet{}";
    QTest::newRow("HXYModelMapper") << imports_1_1() + "HXYModelMapper{}";
    QTest::newRow("VXYModelMapper") << imports_1_1() + "VXYModelMapper{}";
    QTest::newRow("HPieModelMapper") << imports_1_1() + "HPieModelMapper{}";
    QTest::newRow("HPieModelMapper") << imports_1_1() + "HPieModelMapper{}";
    QTest::newRow("HBarModelMapper") << imports_1_1() + "HBarModelMapper{}";
    QTest::newRow("VBarModelMapper") << imports_1_1() + "VBarModelMapper{}";
    QTest::newRow("ValueAxis") << imports_1_1() + "ValueAxis{}";
#ifndef QT_QREAL_IS_FLOAT
    QTest::newRow("DateTimeAxis") << imports_1_1() + "DateTimeAxis{}";
#endif
    QTest::newRow("CategoryAxis") << imports_1_1() + "CategoryAxis{}";
    QTest::newRow("CategoryRange") << imports_1_1() + "CategoryRange{}";
    QTest::newRow("BarCategoryAxis") << imports_1_1() + "BarCategoryAxis{}";

    QTest::newRow("createPolarChartView") <<  imports_1_3() + "PolarChartView{}";
    QTest::newRow("LogValueAxis") <<  imports_1_3() + "LogValueAxis{}";
    QTest::newRow("BoxPlotSeries") <<  imports_1_3() + "BoxPlotSeries{}";
    QTest::newRow("BoxSet") <<  imports_1_3() + "BoxSet{}";

    QTest::newRow("createChartView_2_0") <<  imports_2_0() + "ChartView{}";
    QTest::newRow("XYPoint_2_0") << imports_2_0() + "XYPoint{}";
    QTest::newRow("scatterSeries_2_0") <<  imports_2_0() + "ScatterSeries{}";
    QTest::newRow("lineSeries_2_0") <<  imports_2_0() + "LineSeries{}";
    QTest::newRow("splineSeries_2_0") <<  imports_2_0() + "SplineSeries{}";
    QTest::newRow("areaSeries_2_0") <<  imports_2_0() + "AreaSeries{}";
    QTest::newRow("barSeries_2_0") << imports_2_0() + "BarSeries{}";
    QTest::newRow("stackedBarSeries_2_0") << imports_2_0() + "StackedBarSeries{}";
    QTest::newRow("precentBarSeries_2_0") << imports_2_0() + "PercentBarSeries{}";
    QTest::newRow("horizonatlBarSeries_2_0") << imports_2_0() + "HorizontalBarSeries{}";
    QTest::newRow("horizonatlStackedBarSeries_2_0")
            << imports_2_0() + "HorizontalStackedBarSeries{}";
    QTest::newRow("horizonatlstackedBarSeries_2_0")
            << imports_2_0() + "HorizontalPercentBarSeries{}";
    QTest::newRow("pieSeries_2_0") << imports_2_0() + "PieSeries{}";
    QTest::newRow("PieSlice_2_0") << imports_2_0() + "PieSlice{}";
    QTest::newRow("BarSet_2_0") << imports_2_0() + "BarSet{}";
    QTest::newRow("HXYModelMapper_2_0") << imports_2_0() + "HXYModelMapper{}";
    QTest::newRow("VXYModelMapper_2_0") << imports_2_0() + "VXYModelMapper{}";
    QTest::newRow("HPieModelMapper_2_0") << imports_2_0() + "HPieModelMapper{}";
    QTest::newRow("HPieModelMapper_2_0") << imports_2_0() + "HPieModelMapper{}";
    QTest::newRow("HBarModelMapper_2_0") << imports_2_0() + "HBarModelMapper{}";
    QTest::newRow("VBarModelMapper_2_0") << imports_2_0() + "VBarModelMapper{}";
    QTest::newRow("ValueAxis_2_0") << imports_2_0() + "ValueAxis{}";
#ifndef QT_QREAL_IS_FLOAT
    QTest::newRow("DateTimeAxis_2_0") << imports_2_0() + "DateTimeAxis{}";
#endif
    QTest::newRow("CategoryAxis_2_0") << imports_2_0() + "CategoryAxis{}";
    QTest::newRow("CategoryRange_2_0") << imports_2_0() + "CategoryRange{}";
    QTest::newRow("BarCategoryAxis_2_0") << imports_2_0() + "BarCategoryAxis{}";
    QTest::newRow("createPolarChartView_2_0") <<  imports_2_0() + "PolarChartView{}";
    QTest::newRow("LogValueAxis_2_0") <<  imports_2_0() + "LogValueAxis{}";
    QTest::newRow("BoxPlotSeries_2_0") <<  imports_2_0() + "BoxPlotSeries{}";
    QTest::newRow("BoxSet_2_0") <<  imports_2_0() + "BoxSet{}";

    QTest::newRow("CategoryAxis_2_1") << imports_2_1() + "CategoryAxis{}";
    QTest::newRow("ScatterSeries_2_1") << imports_2_1() + "ScatterSeries{}";
    QTest::newRow("LineSeries_2_1") << imports_2_1() + "LineSeries{}";
    QTest::newRow("SplineSeries_2_1") << imports_2_1() + "SplineSeries{}";
}

void tst_qml::checkPlugin()
{
    QFETCH(QString, source);
    QQmlEngine engine;
    engine.addImportPath(QString::fromLatin1("%1/%2").arg(QCoreApplication::applicationDirPath(), QLatin1String("qml")));
    QQmlComponent component(&engine);
    component.setData(source.toLatin1(), QUrl());
    QVERIFY2(!component.isError(), qPrintable(componentErrors(&component)));
    TRY_COMPARE(component.status(), QQmlComponent::Ready);
    QObject *obj = component.create();
    QVERIFY(obj != 0);
    delete obj;
}

QTEST_MAIN(tst_qml)

#include "tst_qml.moc"

