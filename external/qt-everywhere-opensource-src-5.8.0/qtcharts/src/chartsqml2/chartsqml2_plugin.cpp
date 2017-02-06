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

#include <QtCharts/QChart>
#include <QtCharts/QAbstractAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLogValueAxis>
#include "declarativecategoryaxis.h"
#include <QtCharts/QBarCategoryAxis>
#include "declarativechart.h"
#include "declarativepolarchart.h"
#include "declarativexypoint.h"
#include "declarativelineseries.h"
#include "declarativesplineseries.h"
#include "declarativeareaseries.h"
#include "declarativescatterseries.h"
#include "declarativebarseries.h"
#include "declarativeboxplotseries.h"
#include "declarativecandlestickseries.h"
#include "declarativepieseries.h"
#include "declarativeaxes.h"
#include <QtCharts/QVXYModelMapper>
#include <QtCharts/QHXYModelMapper>
#include <QtCharts/QHPieModelMapper>
#include <QtCharts/QVPieModelMapper>
#include <QtCharts/QHBarModelMapper>
#include <QtCharts/QVBarModelMapper>
#include "declarativemargins.h"
#include <QtCharts/QAreaLegendMarker>
#include <QtCharts/QBarLegendMarker>
#include <QtCharts/QPieLegendMarker>
#include <QtCharts/QXYLegendMarker>
#include <QtCharts/QBoxPlotModelMapper>
#include <QtCharts/QHBoxPlotModelMapper>
#include <QtCharts/QVBoxPlotModelMapper>
#include <QtCharts/QCandlestickModelMapper>
#include <QtCharts/QHCandlestickModelMapper>
#include <QtCharts/QVCandlestickModelMapper>
#ifndef QT_QREAL_IS_FLOAT
    #include <QtCharts/QDateTimeAxis>
#endif
#include <QtCore/QAbstractItemModel>
#include <QtQml>

QT_CHARTS_USE_NAMESPACE

QML_DECLARE_TYPE(QList<QPieSlice *>)
QML_DECLARE_TYPE(QList<QBarSet *>)
QML_DECLARE_TYPE(QList<QAbstractAxis *>)

QML_DECLARE_TYPE(DeclarativeChart)
QML_DECLARE_TYPE(DeclarativePolarChart)
QML_DECLARE_TYPE(DeclarativeMargins)
QML_DECLARE_TYPE(DeclarativeAreaSeries)
QML_DECLARE_TYPE(DeclarativeBarSeries)
QML_DECLARE_TYPE(DeclarativeBarSet)
QML_DECLARE_TYPE(DeclarativeBoxPlotSeries)
QML_DECLARE_TYPE(DeclarativeBoxSet)
QML_DECLARE_TYPE(DeclarativeCandlestickSeries)
QML_DECLARE_TYPE(DeclarativeCandlestickSet)
QML_DECLARE_TYPE(DeclarativeLineSeries)
QML_DECLARE_TYPE(DeclarativePieSeries)
QML_DECLARE_TYPE(DeclarativePieSlice)
QML_DECLARE_TYPE(DeclarativeScatterSeries)
QML_DECLARE_TYPE(DeclarativeSplineSeries)

QML_DECLARE_TYPE(QAbstractAxis)
QML_DECLARE_TYPE(QValueAxis)
QML_DECLARE_TYPE(QBarCategoryAxis)
QML_DECLARE_TYPE(QCategoryAxis)
#ifndef QT_QREAL_IS_FLOAT
    QML_DECLARE_TYPE(QDateTimeAxis)
#endif
QML_DECLARE_TYPE(QLogValueAxis)

QML_DECLARE_TYPE(QLegend)
QML_DECLARE_TYPE(QLegendMarker)
QML_DECLARE_TYPE(QAreaLegendMarker)
QML_DECLARE_TYPE(QBarLegendMarker)
QML_DECLARE_TYPE(QPieLegendMarker)

QML_DECLARE_TYPE(QHPieModelMapper)
QML_DECLARE_TYPE(QHXYModelMapper)
QML_DECLARE_TYPE(QPieModelMapper)
QML_DECLARE_TYPE(QHBarModelMapper)
QML_DECLARE_TYPE(QBarModelMapper)
QML_DECLARE_TYPE(QVBarModelMapper)
QML_DECLARE_TYPE(QVPieModelMapper)
QML_DECLARE_TYPE(QVXYModelMapper)
QML_DECLARE_TYPE(QXYLegendMarker)
QML_DECLARE_TYPE(QXYModelMapper)
QML_DECLARE_TYPE(QBoxPlotModelMapper)
QML_DECLARE_TYPE(QHBoxPlotModelMapper)
QML_DECLARE_TYPE(QVBoxPlotModelMapper)
QML_DECLARE_TYPE(QCandlestickModelMapper)
QML_DECLARE_TYPE(QHCandlestickModelMapper)
QML_DECLARE_TYPE(QVCandlestickModelMapper)

QML_DECLARE_TYPE(QAbstractSeries)
QML_DECLARE_TYPE(QXYSeries)
QML_DECLARE_TYPE(QAbstractBarSeries)
QML_DECLARE_TYPE(QBarSeries)
QML_DECLARE_TYPE(QBarSet)
QML_DECLARE_TYPE(QAreaSeries)
QML_DECLARE_TYPE(QHorizontalBarSeries)
QML_DECLARE_TYPE(QHorizontalPercentBarSeries)
QML_DECLARE_TYPE(QHorizontalStackedBarSeries)
QML_DECLARE_TYPE(QLineSeries)
QML_DECLARE_TYPE(QPercentBarSeries)
QML_DECLARE_TYPE(QPieSeries)
QML_DECLARE_TYPE(QPieSlice)
QML_DECLARE_TYPE(QScatterSeries)
QML_DECLARE_TYPE(QSplineSeries)
QML_DECLARE_TYPE(QStackedBarSeries)

QT_CHARTS_BEGIN_NAMESPACE

class QtChartsQml2Plugin : public QQmlExtensionPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtCharts"));

        // @uri QtCharts

        qRegisterMetaType<QList<QPieSlice *> >();
        qRegisterMetaType<QList<QBarSet *> >();
        qRegisterMetaType<QList<QAbstractAxis *> >();

        // QtCharts 1.0
        qmlRegisterType<DeclarativeChart>(uri, 1, 0, "ChartView");
        qmlRegisterType<DeclarativeXYPoint>(uri, 1, 0, "XYPoint");
        qmlRegisterType<DeclarativeScatterSeries>(uri, 1, 0, "ScatterSeries");
        qmlRegisterType<DeclarativeLineSeries>(uri, 1, 0, "LineSeries");
        qmlRegisterType<DeclarativeSplineSeries>(uri, 1, 0, "SplineSeries");
        qmlRegisterType<DeclarativeAreaSeries>(uri, 1, 0, "AreaSeries");
        qmlRegisterType<DeclarativeBarSeries>(uri, 1, 0, "BarSeries");
        qmlRegisterType<DeclarativeStackedBarSeries>(uri, 1, 0, "StackedBarSeries");
        qmlRegisterType<DeclarativePercentBarSeries>(uri, 1, 0, "PercentBarSeries");
        qmlRegisterType<DeclarativePieSeries>(uri, 1, 0, "PieSeries");
        qmlRegisterType<QPieSlice>(uri, 1, 0, "PieSlice");
        qmlRegisterType<DeclarativeBarSet>(uri, 1, 0, "BarSet");
        qmlRegisterType<QHXYModelMapper>(uri, 1, 0, "HXYModelMapper");
        qmlRegisterType<QVXYModelMapper>(uri, 1, 0, "VXYModelMapper");
        qmlRegisterType<QHPieModelMapper>(uri, 1, 0, "HPieModelMapper");
        qmlRegisterType<QVPieModelMapper>(uri, 1, 0, "VPieModelMapper");
        qmlRegisterType<QHBarModelMapper>(uri, 1, 0, "HBarModelMapper");
        qmlRegisterType<QVBarModelMapper>(uri, 1, 0, "VBarModelMapper");

        qmlRegisterType<QValueAxis>(uri, 1, 0, "ValuesAxis");
        qmlRegisterType<QBarCategoryAxis>(uri, 1, 0, "BarCategoriesAxis");
        qmlRegisterUncreatableType<QLegend>(uri, 1, 0, "Legend",
                                            QLatin1String("Trying to create uncreatable: Legend."));
        qmlRegisterUncreatableType<QXYSeries>(uri, 1, 0, "XYSeries",
                                              QLatin1String("Trying to create uncreatable: XYSeries."));
        qmlRegisterUncreatableType<QAbstractItemModel>(uri, 1, 0, "AbstractItemModel",
                                                       QLatin1String("Trying to create uncreatable: AbstractItemModel."));
        qmlRegisterUncreatableType<QXYModelMapper>(uri, 1, 0, "XYModelMapper",
                                                   QLatin1String("Trying to create uncreatable: XYModelMapper."));
        qmlRegisterUncreatableType<QPieModelMapper>(uri, 1, 0, "PieModelMapper",
                                                    QLatin1String("Trying to create uncreatable: PieModelMapper."));
        qmlRegisterUncreatableType<QBarModelMapper>(uri, 1, 0, "BarModelMapper",
                                                    QLatin1String("Trying to create uncreatable: BarModelMapper."));
        qmlRegisterUncreatableType<QAbstractSeries>(uri, 1, 0, "AbstractSeries",
                                                    QLatin1String("Trying to create uncreatable: AbstractSeries."));
        qmlRegisterUncreatableType<QAbstractBarSeries>(uri, 1, 0, "AbstractBarSeries",
                                                       QLatin1String("Trying to create uncreatable: AbstractBarSeries."));
        qmlRegisterUncreatableType<QAbstractAxis>(uri, 1, 0, "AbstractAxis",
                                                  QLatin1String("Trying to create uncreatable: AbstractAxis. Use specific types of axis instead."));
        qmlRegisterUncreatableType<QBarSet>(uri, 1, 0, "BarSetBase",
                                            QLatin1String("Trying to create uncreatable: BarsetBase."));
        qmlRegisterUncreatableType<QPieSeries>(uri, 1, 0, "QPieSeries",
                                               QLatin1String("Trying to create uncreatable: QPieSeries. Use PieSeries instead."));
        qmlRegisterUncreatableType<DeclarativeAxes>(uri, 1, 0, "DeclarativeAxes",
                                               QLatin1String("Trying to create uncreatable: DeclarativeAxes."));

        // QtCharts 1.1
        qmlRegisterType<DeclarativeChart, 1>(uri, 1, 1, "ChartView");
        qmlRegisterType<DeclarativeScatterSeries, 1>(uri, 1, 1, "ScatterSeries");
        qmlRegisterType<DeclarativeLineSeries, 1>(uri, 1, 1, "LineSeries");
        qmlRegisterType<DeclarativeSplineSeries, 1>(uri, 1, 1, "SplineSeries");
        qmlRegisterType<DeclarativeAreaSeries, 1>(uri, 1, 1, "AreaSeries");
        qmlRegisterType<DeclarativeBarSeries, 1>(uri, 1, 1, "BarSeries");
        qmlRegisterType<DeclarativeStackedBarSeries, 1>(uri, 1, 1, "StackedBarSeries");
        qmlRegisterType<DeclarativePercentBarSeries, 1>(uri, 1, 1, "PercentBarSeries");
        qmlRegisterType<DeclarativeHorizontalBarSeries, 1>(uri, 1, 1, "HorizontalBarSeries");
        qmlRegisterType<DeclarativeHorizontalStackedBarSeries, 1>(uri, 1, 1, "HorizontalStackedBarSeries");
        qmlRegisterType<DeclarativeHorizontalPercentBarSeries, 1>(uri, 1, 1, "HorizontalPercentBarSeries");
        qmlRegisterType<DeclarativePieSeries>(uri, 1, 1, "PieSeries");
        qmlRegisterType<DeclarativeBarSet>(uri, 1, 1, "BarSet");
        qmlRegisterType<QValueAxis>(uri, 1, 1, "ValueAxis");
#ifndef QT_QREAL_IS_FLOAT
        qmlRegisterType<QDateTimeAxis>(uri, 1, 1, "DateTimeAxis");
#endif
        qmlRegisterType<DeclarativeCategoryAxis>(uri, 1, 1, "CategoryAxis");
        qmlRegisterType<DeclarativeCategoryRange>(uri, 1, 1, "CategoryRange");
        qmlRegisterType<QBarCategoryAxis>(uri, 1, 1, "BarCategoryAxis");
        qmlRegisterUncreatableType<DeclarativeMargins>(uri, 1, 1, "Margins",
                                                       QLatin1String("Trying to create uncreatable: Margins."));

        // QtCharts 1.2
        qmlRegisterType<DeclarativeChart, 2>(uri, 1, 2, "ChartView");
        qmlRegisterType<DeclarativeScatterSeries, 2>(uri, 1, 2, "ScatterSeries");
        qmlRegisterType<DeclarativeLineSeries, 2>(uri, 1, 2, "LineSeries");
        qmlRegisterType<DeclarativeSplineSeries, 2>(uri, 1, 2, "SplineSeries");
        qmlRegisterType<DeclarativeAreaSeries, 2>(uri, 1, 2, "AreaSeries");
        qmlRegisterType<DeclarativeBarSeries, 2>(uri, 1, 2, "BarSeries");
        qmlRegisterType<DeclarativeStackedBarSeries, 2>(uri, 1, 2, "StackedBarSeries");
        qmlRegisterType<DeclarativePercentBarSeries, 2>(uri, 1, 2, "PercentBarSeries");
        qmlRegisterType<DeclarativeHorizontalBarSeries, 2>(uri, 1, 2, "HorizontalBarSeries");
        qmlRegisterType<DeclarativeHorizontalStackedBarSeries, 2>(uri, 1, 2, "HorizontalStackedBarSeries");
        qmlRegisterType<DeclarativeHorizontalPercentBarSeries, 2>(uri, 1, 2, "HorizontalPercentBarSeries");

        // QtCharts 1.3
        qmlRegisterType<DeclarativeChart, 3>(uri, 1, 3, "ChartView");
        qmlRegisterType<DeclarativePolarChart, 1>(uri, 1, 3, "PolarChartView");
        qmlRegisterType<DeclarativeSplineSeries, 3>(uri, 1, 3, "SplineSeries");
        qmlRegisterType<DeclarativeScatterSeries, 3>(uri, 1, 3, "ScatterSeries");
        qmlRegisterType<DeclarativeLineSeries, 3>(uri, 1, 3, "LineSeries");
        qmlRegisterType<DeclarativeAreaSeries, 3>(uri, 1, 3, "AreaSeries");
        qmlRegisterType<QLogValueAxis>(uri, 1, 3, "LogValueAxis");
        qmlRegisterType<DeclarativeBoxPlotSeries>(uri, 1, 3, "BoxPlotSeries");
        qmlRegisterType<DeclarativeBoxSet>(uri, 1, 3, "BoxSet");

        // QtCharts 1.4
        qmlRegisterType<DeclarativeAreaSeries, 4>(uri, 1, 4, "AreaSeries");
        qmlRegisterType<DeclarativeBarSet, 2>(uri, 1, 4, "BarSet");
        qmlRegisterType<DeclarativeBoxPlotSeries, 1>(uri, 1, 4, "BoxPlotSeries");
        qmlRegisterType<DeclarativeBoxSet, 1>(uri, 1, 4, "BoxSet");
        qmlRegisterType<DeclarativePieSlice>(uri, 1, 4, "PieSlice");
        qmlRegisterType<DeclarativeScatterSeries, 4>(uri, 1, 4, "ScatterSeries");

        // QtCharts 2.0
        qmlRegisterType<QHBoxPlotModelMapper>(uri, 2, 0, "HBoxPlotModelMapper");
        qmlRegisterType<QVBoxPlotModelMapper>(uri, 2, 0, "VBoxPlotModelMapper");
        qmlRegisterUncreatableType<QBoxPlotModelMapper>(uri, 2, 0, "BoxPlotModelMapper",
            QLatin1String("Trying to create uncreatable: BoxPlotModelMapper."));
        qmlRegisterType<DeclarativeChart, 4>(uri, 2, 0, "ChartView");
        qmlRegisterType<DeclarativeXYPoint>(uri, 2, 0, "XYPoint");
        qmlRegisterType<DeclarativeScatterSeries, 4>(uri, 2, 0, "ScatterSeries");
        qmlRegisterType<DeclarativeLineSeries, 3>(uri, 2, 0, "LineSeries");
        qmlRegisterType<DeclarativeSplineSeries, 3>(uri, 2, 0, "SplineSeries");
        qmlRegisterType<DeclarativeAreaSeries, 4>(uri, 2, 0, "AreaSeries");
        qmlRegisterType<DeclarativeBarSeries, 2>(uri, 2, 0, "BarSeries");
        qmlRegisterType<DeclarativeStackedBarSeries, 2>(uri, 2, 0, "StackedBarSeries");
        qmlRegisterType<DeclarativePercentBarSeries, 2>(uri, 2, 0, "PercentBarSeries");
        qmlRegisterType<DeclarativePieSeries>(uri, 2, 0, "PieSeries");
        qmlRegisterType<QPieSlice>(uri, 2, 0, "PieSlice");
        qmlRegisterType<DeclarativeBarSet, 2>(uri, 2, 0, "BarSet");
        qmlRegisterType<QHXYModelMapper>(uri, 2, 0, "HXYModelMapper");
        qmlRegisterType<QVXYModelMapper>(uri, 2, 0, "VXYModelMapper");
        qmlRegisterType<QHPieModelMapper>(uri, 2, 0, "HPieModelMapper");
        qmlRegisterType<QVPieModelMapper>(uri, 2, 0, "VPieModelMapper");
        qmlRegisterType<QHBarModelMapper>(uri, 2, 0, "HBarModelMapper");
        qmlRegisterType<QVBarModelMapper>(uri, 2, 0, "VBarModelMapper");
        qmlRegisterType<QValueAxis>(uri, 2, 0, "ValueAxis");
#ifndef QT_QREAL_IS_FLOAT
        qmlRegisterType<QDateTimeAxis>(uri, 2, 0, "DateTimeAxis");
#endif
        qmlRegisterType<DeclarativeCategoryAxis>(uri, 2, 0, "CategoryAxis");
        qmlRegisterType<DeclarativeCategoryRange>(uri, 2, 0, "CategoryRange");
        qmlRegisterType<QBarCategoryAxis>(uri, 2, 0, "BarCategoryAxis");
        qmlRegisterType<DeclarativePolarChart, 1>(uri, 2, 0, "PolarChartView");
        qmlRegisterType<QLogValueAxis, 1>(uri, 2, 0, "LogValueAxis");
        qmlRegisterType<DeclarativeBoxPlotSeries, 1>(uri, 2, 0, "BoxPlotSeries");
        qmlRegisterType<DeclarativeBoxSet, 1>(uri, 2, 0, "BoxSet");
        qmlRegisterType<DeclarativeHorizontalBarSeries, 2>(uri, 2, 0, "HorizontalBarSeries");
        qmlRegisterType<DeclarativeHorizontalStackedBarSeries, 2>(uri, 2, 0, "HorizontalStackedBarSeries");
        qmlRegisterType<DeclarativeHorizontalPercentBarSeries, 2>(uri, 2, 0, "HorizontalPercentBarSeries");
        qmlRegisterType<DeclarativePieSlice>(uri, 2, 0, "PieSlice");
        qmlRegisterUncreatableType<QLegend>(uri, 2, 0, "Legend",
                                            QLatin1String("Trying to create uncreatable: Legend."));
        qmlRegisterUncreatableType<QXYSeries>(uri, 2, 0, "XYSeries",
                                              QLatin1String("Trying to create uncreatable: XYSeries."));
        qmlRegisterUncreatableType<QAbstractItemModel>(uri, 2, 0, "AbstractItemModel",
                                                       QLatin1String("Trying to create uncreatable: AbstractItemModel."));
        qmlRegisterUncreatableType<QXYModelMapper>(uri, 2, 0, "XYModelMapper",
                                                   QLatin1String("Trying to create uncreatable: XYModelMapper."));
        qmlRegisterUncreatableType<QPieModelMapper>(uri, 2, 0, "PieModelMapper",
                                                    QLatin1String("Trying to create uncreatable: PieModelMapper."));
        qmlRegisterUncreatableType<QBarModelMapper>(uri, 2, 0, "BarModelMapper",
                                                    QLatin1String("Trying to create uncreatable: BarModelMapper."));
        qmlRegisterUncreatableType<QAbstractSeries>(uri, 2, 0, "AbstractSeries",
                                                    QLatin1String("Trying to create uncreatable: AbstractSeries."));
        qmlRegisterUncreatableType<QAbstractBarSeries>(uri, 2, 0, "AbstractBarSeries",
                                                       QLatin1String("Trying to create uncreatable: AbstractBarSeries."));
        qmlRegisterUncreatableType<QAbstractAxis>(uri, 2, 0, "AbstractAxis",
                                                  QLatin1String("Trying to create uncreatable: AbstractAxis. Use specific types of axis instead."));
        qmlRegisterUncreatableType<QBarSet>(uri, 2, 0, "BarSetBase",
                                            QLatin1String("Trying to create uncreatable: BarsetBase."));
        qmlRegisterUncreatableType<QPieSeries>(uri, 2, 0, "QPieSeries",
                                               QLatin1String("Trying to create uncreatable: QPieSeries. Use PieSeries instead."));
        qmlRegisterUncreatableType<DeclarativeAxes>(uri, 2, 0, "DeclarativeAxes",
                                                    QLatin1String("Trying to create uncreatable: DeclarativeAxes."));
        qmlRegisterUncreatableType<DeclarativeMargins>(uri, 2, 0, "Margins",
                                                       QLatin1String("Trying to create uncreatable: Margins."));

        // QtCharts 2.1
        qmlRegisterType<DeclarativeCategoryAxis, 1>(uri, 2, 1, "CategoryAxis");
        qmlRegisterUncreatableType<QAbstractAxis>(uri, 2, 1, "AbstractAxis",
                                                  QLatin1String("Trying to create uncreatable: AbstractAxis. Use specific types of axis instead."));
        qmlRegisterType<DeclarativeChart, 5>(uri, 2, 1, "ChartView");
        qmlRegisterType<DeclarativeScatterSeries, 5>(uri, 2, 1, "ScatterSeries");
        qmlRegisterType<DeclarativeLineSeries, 4>(uri, 2, 1, "LineSeries");
        qmlRegisterType<DeclarativeSplineSeries, 4>(uri, 2, 1, "SplineSeries");

        // QtCharts 2.2
        qmlRegisterType<DeclarativeCandlestickSeries>(uri, 2, 2, "CandlestickSeries");
        qmlRegisterType<DeclarativeCandlestickSet>(uri, 2, 2, "CandlestickSet");
        qmlRegisterUncreatableType<QCandlestickModelMapper>(uri, 2, 2, "CandlestickModelMapper",
            QLatin1String("Trying to create uncreatable: CandlestickModelMapper."));
        qmlRegisterType<QHCandlestickModelMapper>(uri, 2, 2, "HCandlestickModelMapper");
        qmlRegisterType<QVCandlestickModelMapper>(uri, 2, 2, "VCandlestickModelMapper");
    }

};

QT_CHARTS_END_NAMESPACE

#include "chartsqml2_plugin.moc"

QT_CHARTS_USE_NAMESPACE
