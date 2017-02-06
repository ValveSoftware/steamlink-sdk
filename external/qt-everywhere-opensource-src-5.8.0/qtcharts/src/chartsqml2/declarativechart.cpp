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

#include "declarativechart.h"
#include <QtGui/QPainter>
#include "declarativelineseries.h"
#include "declarativeareaseries.h"
#include "declarativebarseries.h"
#include "declarativepieseries.h"
#include "declarativesplineseries.h"
#include "declarativeboxplotseries.h"
#include "declarativecandlestickseries.h"
#include "declarativescatterseries.h"
#include "declarativechartnode.h"
#include "declarativeabstractrendernode.h"
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLogValueAxis>
#include <QtCharts/QCategoryAxis>
#include <private/qabstractseries_p.h>
#include "declarativemargins.h"
#include <private/chartdataset_p.h>
#include "declarativeaxes.h"
#include <private/qchart_p.h>
#include <private/chartpresenter_p.h>
#include <QtCharts/QPolarChart>

#ifndef QT_QREAL_IS_FLOAT
    #include <QtCharts/QDateTimeAxis>
#endif

#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsSceneHoverEvent>
#include <QtWidgets/QApplication>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtQuick/QQuickWindow>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \qmltype ChartView
    \instantiates DeclarativeChart
    \inqmlmodule QtCharts

    \brief Chart element.

    ChartView element is the parent that is responsible for showing different chart series types.

    The following QML shows how to create a simple chart with one pie series:
    \snippet qmlpiechart/qml/qmlpiechart/main.qml 1
    \snippet qmlpiechart/qml/qmlpiechart/main.qml 2
    \snippet qmlpiechart/qml/qmlpiechart/main.qml 3

    \beginfloatleft
    \image examples_qmlpiechart.png
    \endfloat
    \clearfloat
*/

/*!
  \qmlproperty Theme ChartView::theme
  Theme defines the visual appearance of the chart, including for example colors, fonts, line
  widths and chart background.
*/

/*!
  \qmlproperty Animation ChartView::animationOptions
  Animation configuration of the chart. One of ChartView.NoAnimation, ChartView.GridAxisAnimations,
  ChartView.SeriesAnimations or ChartView.AllAnimations.
*/

/*!
 \qmlproperty int ChartView::animationDuration
 The duration of the animation for the chart.
 */

/*!
 \qmlproperty easing ChartView::animationEasingCurve
 The easing curve of the animation for the chart.
*/

/*!
  \qmlproperty Font ChartView::titleFont
  The title font of the chart.

  See the Qt documentation for more details of Font.
*/

/*!
  \qmlproperty string ChartView::title
  The title of the chart, shown on top of the chart.
  \sa ChartView::titleColor
*/

/*!
  \qmlproperty color ChartView::titleColor
  The color of the title text.
*/

/*!
  \qmlproperty Legend ChartView::legend
  The legend of the chart. Legend lists all the series, pie slices and bar sets added on the chart.
*/

/*!
  \qmlproperty int ChartView::count
  The count of series added to the chart.
*/

/*!
  \qmlproperty color ChartView::backgroundColor
  The color of the chart's background. By default background color is defined by chart theme.
  \sa ChartView::theme
*/

/*!
  \qmlproperty real ChartView::backgroundRoundness
  The diameter of the rounding circle at the corners of the chart background.
*/

/*!
  \qmlproperty color ChartView::plotAreaColor
  The color of the background of the chart's plot area. By default plot area background uses chart's
  background color.
  \sa ChartView::backgroundColor
*/

/*!
  \qmlproperty list<AbstractAxis> ChartView::axes
  The axes of the ChartView.
*/

/*!
  \qmlproperty bool ChartView::dropShadowEnabled
  The chart's border drop shadow. Set to true to enable drop shadow.
*/

/*!
  \qmlproperty rect ChartView::plotArea
  The area on the ChartView that is used for drawing series. This is the ChartView rect without the
  margins.
  \sa ChartView::margins
*/

/*!
  \qmlproperty Margins ChartView::margins
  The minimum margins allowed between the outer bounds and the plotArea of the ChartView. Margins
  area of ChartView is used for drawing title, axes and legend.
*/

/*!
  \qmlproperty bool ChartView::localizeNumbers
  \since QtCharts 2.0
  When \c{true}, all generated numbers appearing in various series and axis labels will be
  localized using the default QLocale of the application, which defaults to the system locale.
  When \c{false}, the "C" locale is always used.
  Defaults to \c{false}.

  \sa locale
*/

/*!
  \qmlproperty locale ChartView::locale
  \since QtCharts 2.0
  Sets the locale used to format various chart labels when localizeNumbers is \c{true}.
  This also determines the locale used to format DateTimeAxis labels regardless of
  localizeNumbers property.
  Defaults to application default locale at the time the chart is constructed.

  \sa localizeNumbers
*/

/*!
  \qmlmethod AbstractSeries ChartView::series(int index)
  Returns the series with \a index on the chart. This allows you to loop through the series of a chart together with
  the count property of the chart.
*/

/*!
  \qmlmethod AbstractSeries ChartView::series(string name)
  Returns the first series on the chart with \a name. If there is no series with that name, returns null.
*/

/*!
  \qmlmethod AbstractSeries ChartView::createSeries(SeriesType type, string name, AbstractAxis axisX, AbstractAxis axisY)
  Creates a series object of \a type to the chart with name \a name, optional axis \a axisX and
  optional axis \a axisY. For example:
  \code
    // lineSeries is a LineSeries object that has already been added to the ChartView; re-use it's axes
    var myAxisX = chartView.axisX(lineSeries);
    var myAxisY = chartView.axisY(lineSeries);
    var scatter = chartView.createSeries(ChartView.SeriesTypeScatter, "scatter series", myAxisX, myAxisY);
  \endcode
*/

/*!
  \qmlmethod ChartView::removeSeries(AbstractSeries series)
  Removes the \a series from the chart. The series object is also destroyed.
*/

/*!
  \qmlmethod ChartView::removeAllSeries()
  Removes all series from the chart. All the series objects are also destroyed.
*/

/*!
  \qmlmethod Axis ChartView::axisX(AbstractSeries series)
  The x-axis of the series.
*/

/*!
  \qmlmethod ChartView::setAxisX(AbstractAxis axis, AbstractSeries series)
  Set the x-axis of the series.
*/

/*!
  \qmlmethod Axis ChartView::axisY(AbstractSeries series)
  The y-axis of the series.
*/

/*!
  \qmlmethod ChartView::setAxisY(AbstractAxis axis, AbstractSeries series)
  Set the y-axis of the series.
*/

/*!
  \qmlmethod ChartView::zoom(real factor)
  Zooms in by \a factor on the center of the chart.

  A factor over 1.0 zooms the view in and factor between 0.0 and 1.0 zooms out.
*/

/*!
  \qmlmethod ChartView::zoomIn()
  Zooms in the view by a factor of two.
*/

/*!
  \qmlmethod ChartView::zoomIn(rect rectangle)
  Zooms in the view to a maximum level at which \a rectangle is still fully visible.
  \note This is not supported for polar charts.
*/

/*!
  \qmlmethod ChartView::zoomOut()
  Zooms out the view by a factor of two.
*/

/*!
  \qmlmethod ChartView::zoomReset()
  Resets the series domains to what they were before any zoom method was called.
  Note that this will also reset any scrolls and explicit axis range settings done between
  the first zoom operation and calling this method. If no zoom operation has been
  done, this method does nothing.
*/

/*!
  \qmlmethod ChartView::isZoomed()
  Returns true if any series has a zoomed domain.
*/

/*!
  \qmlmethod ChartView::scrollLeft(real pixels)
  Scrolls to left by \a pixels. This is a convenience function that suits for example for key navigation.
*/

/*!
  \qmlmethod ChartView::scrollRight(real pixels)
  Scrolls to right by \a pixels. This is a convenience function that suits for example for key navigation.
*/

/*!
  \qmlmethod ChartView::scrollUp(real pixels)
  Scrolls up by \a pixels. This is a convenience function that suits for example for key navigation.
*/

/*!
  \qmlmethod ChartView::scrollDown(real pixels)
  Scrolls down by \a pixels. This is a convenience function that suits for example for key navigation.
*/

/*!
  \qmlmethod point ChartView::mapToValue(point position, AbstractSeries series)
  Returns the value in the \a series domain that corresponds to the \a position relative to the
  chart.
*/

/*!
  \qmlmethod point ChartView::mapToPosition(point value, AbstractSeries series)
  Returns the position on the chart that corresponds to the \a value in the \a series domain.
*/

/*!
  \qmlsignal ChartView::seriesAdded(AbstractSeries series)
  The \a series has been added to the chart.
*/

/*!
  \qmlsignal ChartView::seriesRemoved(AbstractSeries series)
  The \a series has been removed from the chart. Please note that \a series is no longer a valid
  object after the signal handler has completed.
*/

DeclarativeChart::DeclarativeChart(QQuickItem *parent)
    : QQuickItem(parent)
{
    initChart(QChart::ChartTypeCartesian);
}

DeclarativeChart::DeclarativeChart(QChart::ChartType type, QQuickItem *parent)
    : QQuickItem(parent)
{
    initChart(type);
}

void DeclarativeChart::initChart(QChart::ChartType type)
{
    m_sceneImage = 0;
    m_sceneImageDirty = false;
    m_sceneImageNeedsClear = false;
    m_guiThreadId = QThread::currentThreadId();
    m_paintThreadId = 0;
    m_updatePending = false;

    setFlag(ItemHasContents, true);

    if (type == QChart::ChartTypePolar)
        m_chart = new QPolarChart();
    else
        m_chart = new QChart();

    m_chart->d_ptr->m_presenter->glSetUseWidget(false);
    m_glXYDataManager = m_chart->d_ptr->m_dataset->glXYSeriesDataManager();

    m_scene = new QGraphicsScene(this);
    m_scene->addItem(m_chart);

    setAntialiasing(QQuickItem::antialiasing());
    connect(m_scene, &QGraphicsScene::changed, this, &DeclarativeChart::sceneChanged);
    connect(this, &DeclarativeChart::needRender, this, &DeclarativeChart::renderScene,
            Qt::QueuedConnection);
    connect(this, SIGNAL(antialiasingChanged(bool)), this, SLOT(handleAntialiasingChanged(bool)));
    connect(this, &DeclarativeChart::pendingRenderNodeMouseEventResponses,
            this, &DeclarativeChart::handlePendingRenderNodeMouseEventResponses,
            Qt::QueuedConnection);

    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);

    m_margins = new DeclarativeMargins(this);
    m_margins->setTop(m_chart->margins().top());
    m_margins->setLeft(m_chart->margins().left());
    m_margins->setRight(m_chart->margins().right());
    m_margins->setBottom(m_chart->margins().bottom());
    connect(m_margins, SIGNAL(topChanged(int,int,int,int)),
            this, SLOT(changeMargins(int,int,int,int)));
    connect(m_margins, SIGNAL(bottomChanged(int,int,int,int)),
            this, SLOT(changeMargins(int,int,int,int)));
    connect(m_margins, SIGNAL(leftChanged(int,int,int,int)),
            this, SLOT(changeMargins(int,int,int,int)));
    connect(m_margins, SIGNAL(rightChanged(int,int,int,int)),
            this, SLOT(changeMargins(int,int,int,int)));
    connect(m_chart->d_ptr->m_dataset, SIGNAL(seriesAdded(QAbstractSeries*)), this, SLOT(handleSeriesAdded(QAbstractSeries*)));
    connect(m_chart->d_ptr->m_dataset, SIGNAL(seriesRemoved(QAbstractSeries*)), this, SIGNAL(seriesRemoved(QAbstractSeries*)));
    connect(m_chart, &QChart::plotAreaChanged, this, &DeclarativeChart::plotAreaChanged);
}

void DeclarativeChart::handleSeriesAdded(QAbstractSeries *series)
{
    emit seriesAdded(series);
}

void DeclarativeChart::handlePendingRenderNodeMouseEventResponses()
{
    const int count = m_pendingRenderNodeMouseEventResponses.size();
    if (count) {
        QXYSeries *lastSeries = nullptr; // Small optimization; events are likely for same series
        QList<QAbstractSeries *> seriesList = m_chart->series();
        for (int i = 0; i < count; i++) {
            const MouseEventResponse &response = m_pendingRenderNodeMouseEventResponses.at(i);
            QXYSeries *series = nullptr;
            if (lastSeries == response.series) {
                series = lastSeries;
            } else {
                for (int j = 0; j < seriesList.size(); j++) {
                    QAbstractSeries *chartSeries = seriesList.at(j);
                    if (response.series == chartSeries) {
                        series = qobject_cast<QXYSeries *>(chartSeries);
                        break;
                    }
                }
            }
            if (series) {
                lastSeries = series;
                QSizeF normalizedPlotSize(
                            m_chart->plotArea().size().width()
                            / m_adjustedPlotArea.size().width(),
                            m_chart->plotArea().size().height()
                            / m_adjustedPlotArea.size().height());

                QPoint adjustedPoint(
                            response.point.x() * normalizedPlotSize.width(),
                            response.point.y() * normalizedPlotSize.height());

                QPointF domPoint = series->d_ptr->domain()->calculateDomainPoint(adjustedPoint);
                switch (response.type) {
                case MouseEventResponse::Pressed:
                    emit series->pressed(domPoint);
                    break;
                case MouseEventResponse::Released:
                    emit series->released(domPoint);
                    break;
                case MouseEventResponse::Clicked:
                    emit series->clicked(domPoint);
                    break;
                case MouseEventResponse::DoubleClicked:
                    emit series->doubleClicked(domPoint);
                    break;
                case MouseEventResponse::HoverEnter:
                    emit series->hovered(domPoint, true);
                    break;
                case MouseEventResponse::HoverLeave:
                    emit series->hovered(domPoint, false);
                    break;
                default:
                    // No action
                    break;
                }
            }
        }
        m_pendingRenderNodeMouseEventResponses.clear();
    }
}

void DeclarativeChart::changeMargins(int top, int bottom, int left, int right)
{
    m_chart->setMargins(QMargins(left, top, right, bottom));
    emit marginsChanged();
}

DeclarativeChart::~DeclarativeChart()
{
    delete m_chart;
    delete m_sceneImage;
}

void DeclarativeChart::childEvent(QChildEvent *event)
{
    if (event->type() == QEvent::ChildAdded) {
        if (qobject_cast<QAbstractSeries *>(event->child())) {
            m_chart->addSeries(qobject_cast<QAbstractSeries *>(event->child()));
        }
    }
}

void DeclarativeChart::componentComplete()
{
    foreach (QObject *child, children()) {
        if (qobject_cast<QAbstractSeries *>(child)) {
            // Add series to the chart
            QAbstractSeries *series = qobject_cast<QAbstractSeries *>(child);
            m_chart->addSeries(series);

            // Connect to axis changed signals (unless this is a pie series)
            if (!qobject_cast<DeclarativePieSeries *>(series)) {
                connect(series, SIGNAL(axisXChanged(QAbstractAxis*)), this, SLOT(handleAxisXSet(QAbstractAxis*)));
                connect(series, SIGNAL(axisXTopChanged(QAbstractAxis*)), this, SLOT(handleAxisXTopSet(QAbstractAxis*)));
                connect(series, SIGNAL(axisYChanged(QAbstractAxis*)), this, SLOT(handleAxisYSet(QAbstractAxis*)));
                connect(series, SIGNAL(axisYRightChanged(QAbstractAxis*)), this, SLOT(handleAxisYRightSet(QAbstractAxis*)));
            }

            initializeAxes(series);
        }
    }

    QQuickItem::componentComplete();
}

void DeclarativeChart::seriesAxisAttachHelper(QAbstractSeries *series, QAbstractAxis *axis,
                                              Qt::Orientations orientation,
                                              Qt::Alignment alignment)
{
    if (!series->attachedAxes().contains(axis)) {
        // Remove & delete old axes that are not attached to any other series
        foreach (QAbstractAxis* oldAxis, m_chart->axes(orientation, series)) {
            bool otherAttachments = false;
            if (oldAxis != axis) {
                foreach (QAbstractSeries *oldSeries, m_chart->series()) {
                    if (oldSeries != series && oldSeries->attachedAxes().contains(oldAxis)) {
                        otherAttachments = true;
                        break;
                    }
                }
                if (!otherAttachments) {
                    m_chart->removeAxis(oldAxis);
                    delete oldAxis;
                }
            }
        }
        if (!m_chart->axes(orientation).contains(axis))
            m_chart->addAxis(axis, alignment);

        series->attachAxis(axis);
    }
}

void DeclarativeChart::handleAxisXSet(QAbstractAxis *axis)
{
    QAbstractSeries *s = qobject_cast<QAbstractSeries *>(sender());
    if (axis && s) {
        seriesAxisAttachHelper(s, axis, Qt::Horizontal, Qt::AlignBottom);
    } else {
        qWarning() << "Trying to set axisX to null.";
    }
}

void DeclarativeChart::handleAxisXTopSet(QAbstractAxis *axis)
{
    QAbstractSeries *s = qobject_cast<QAbstractSeries *>(sender());
    if (axis && s) {
        seriesAxisAttachHelper(s, axis, Qt::Horizontal, Qt::AlignTop);
    } else {
        qWarning() << "Trying to set axisXTop to null.";
    }
}

void DeclarativeChart::handleAxisYSet(QAbstractAxis *axis)
{
    QAbstractSeries *s = qobject_cast<QAbstractSeries *>(sender());
    if (axis && s) {
        seriesAxisAttachHelper(s, axis, Qt::Vertical, Qt::AlignLeft);
    } else {
        qWarning() << "Trying to set axisY to null.";
    }
}

void DeclarativeChart::handleAxisYRightSet(QAbstractAxis *axis)
{
    QAbstractSeries *s = qobject_cast<QAbstractSeries *>(sender());
    if (axis && s) {
        seriesAxisAttachHelper(s, axis, Qt::Vertical, Qt::AlignRight);
    } else {
        qWarning() << "Trying to set axisYRight to null.";
    }
}

void DeclarativeChart::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (newGeometry.isValid()) {
        if (newGeometry.width() > 0 && newGeometry.height() > 0) {
            m_chart->resize(newGeometry.width(), newGeometry.height());
        }
    }
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}

QSGNode *DeclarativeChart::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    DeclarativeChartNode *node = static_cast<DeclarativeChartNode *>(oldNode);

    if (!node) {
        node =  new DeclarativeChartNode(window());
        // Ensure that chart is rendered whenever node is recreated
        if (m_sceneImage)
            m_sceneImageDirty = true;
    }

    const QRectF &bRect = boundingRect();
    // Update renderNode data
    if (node->renderNode()) {
        if (m_glXYDataManager->dataMap().size() || m_glXYDataManager->mapDirty()) {
            // Convert plotArea to QRect and back to QRectF to get rid of sub-pixel widths/heights
            // which can cause unwanted partial antialising of the graph.
            const QRectF plotArea = QRectF(m_chart->plotArea().toRect());
            const QSizeF &chartAreaSize = m_chart->size();

            // We can't use chart's plot area directly, as chart enforces a minimum size
            // internally, so that axes and labels always fit the chart area.
            const qreal normalizedX = plotArea.x() / chartAreaSize.width();
            const qreal normalizedY = plotArea.y() / chartAreaSize.height();
            const qreal normalizedWidth = plotArea.width() / chartAreaSize.width();
            const qreal normalizedHeight = plotArea.height() / chartAreaSize.height();

            m_adjustedPlotArea = QRectF(normalizedX * bRect.width(),
                                        normalizedY * bRect.height(),
                                        normalizedWidth * bRect.width(),
                                        normalizedHeight * bRect.height());

            const QSize &adjustedPlotSize = m_adjustedPlotArea.size().toSize();
            if (adjustedPlotSize != node->renderNode()->textureSize())
                node->renderNode()->setTextureSize(adjustedPlotSize);

            node->renderNode()->setRect(m_adjustedPlotArea);
            node->renderNode()->setSeriesData(m_glXYDataManager->mapDirty(),
                                              m_glXYDataManager->dataMap());
            node->renderNode()->setAntialiasing(antialiasing());

            // Clear dirty flags from original xy data
            m_glXYDataManager->clearAllDirty();
        }

        node->renderNode()->takeMouseEventResponses(m_pendingRenderNodeMouseEventResponses);
        if (m_pendingRenderNodeMouseEventResponses.size())
            emit pendingRenderNodeMouseEventResponses();
        if (m_pendingRenderNodeMouseEvents.size()) {
            node->renderNode()->addMouseEvents(m_pendingRenderNodeMouseEvents);
            // Queue another update to receive responses
            update();
        }
    }

    m_pendingRenderNodeMouseEvents.clear();

    // Copy chart (if dirty) to chart node
    if (m_sceneImageDirty) {
        node->createTextureFromImage(*m_sceneImage);
        m_sceneImageDirty = false;
    }

    node->setRect(bRect);

    return node;
}

void DeclarativeChart::sceneChanged(QList<QRectF> region)
{
    const int count = region.size();
    const qreal limitSize = 0.01;
    if (count && !m_updatePending) {
        qreal totalSize = 0.0;
        for (int i = 0; i < count; i++) {
            const QRectF &reg = region.at(i);
            totalSize += (reg.height() * reg.width());
            if (totalSize >= limitSize)
                break;
        }
        // Ignore region updates that change less than small fraction of a pixel, as there is
        // little point regenerating the image in these cases. These are typically cases
        // where OpenGL series are drawn to otherwise static chart.
        if (totalSize >= limitSize) {
            m_updatePending = true;
            // Do async render to avoid some unnecessary renders.
            emit needRender();
        } else {
            // We do want to call update to trigger possible gl series updates.
            update();
        }
    }
}

void DeclarativeChart::renderScene()
{
    m_updatePending = false;
    m_sceneImageDirty = true;
    QSize chartSize = m_chart->size().toSize();
    if (!m_sceneImage || chartSize != m_sceneImage->size()) {
        delete m_sceneImage;
        qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
        m_sceneImage = new QImage(chartSize * dpr, QImage::Format_ARGB32);
        m_sceneImage->setDevicePixelRatio(dpr);
        m_sceneImageNeedsClear = true;
    }

    if (m_sceneImageNeedsClear) {
        m_sceneImage->fill(Qt::transparent);
        // Don't clear the flag if chart background has any transparent element to it
        if (m_chart->backgroundBrush().color().alpha() == 0xff && !m_chart->isDropShadowEnabled())
            m_sceneImageNeedsClear = false;
    }
    QPainter painter(m_sceneImage);
    if (antialiasing()) {
        painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                               | QPainter::SmoothPixmapTransform);
    }
    QRect renderRect(QPoint(0, 0), chartSize);
    m_scene->render(&painter, renderRect, renderRect);
    update();
}

void DeclarativeChart::mousePressEvent(QMouseEvent *event)
{
    m_mousePressScenePoint = event->pos();
    m_mousePressScreenPoint = event->globalPos();
    m_lastMouseMoveScenePoint = m_mousePressScenePoint;
    m_lastMouseMoveScreenPoint = m_mousePressScreenPoint;
    m_mousePressButton = event->button();
    m_mousePressButtons = event->buttons();

    QGraphicsSceneMouseEvent mouseEvent(QEvent::GraphicsSceneMousePress);
    mouseEvent.setWidget(0);
    mouseEvent.setButtonDownScenePos(m_mousePressButton, m_mousePressScenePoint);
    mouseEvent.setButtonDownScreenPos(m_mousePressButton, m_mousePressScreenPoint);
    mouseEvent.setScenePos(m_mousePressScenePoint);
    mouseEvent.setScreenPos(m_mousePressScreenPoint);
    mouseEvent.setLastScenePos(m_lastMouseMoveScenePoint);
    mouseEvent.setLastScreenPos(m_lastMouseMoveScreenPoint);
    mouseEvent.setButtons(m_mousePressButtons);
    mouseEvent.setButton(m_mousePressButton);
    mouseEvent.setModifiers(event->modifiers());
    mouseEvent.setAccepted(false);

    QApplication::sendEvent(m_scene, &mouseEvent);

    queueRendererMouseEvent(event);
}

void DeclarativeChart::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsSceneMouseEvent mouseEvent(QEvent::GraphicsSceneMouseRelease);
    mouseEvent.setWidget(0);
    mouseEvent.setButtonDownScenePos(m_mousePressButton, m_mousePressScenePoint);
    mouseEvent.setButtonDownScreenPos(m_mousePressButton, m_mousePressScreenPoint);
    mouseEvent.setScenePos(event->pos());
    mouseEvent.setScreenPos(event->globalPos());
    mouseEvent.setLastScenePos(m_lastMouseMoveScenePoint);
    mouseEvent.setLastScreenPos(m_lastMouseMoveScreenPoint);
    mouseEvent.setButtons(event->buttons());
    mouseEvent.setButton(event->button());
    mouseEvent.setModifiers(event->modifiers());
    mouseEvent.setAccepted(false);

    QApplication::sendEvent(m_scene, &mouseEvent);

    m_mousePressButtons = event->buttons();
    m_mousePressButton = Qt::NoButton;

    queueRendererMouseEvent(event);
}

void DeclarativeChart::hoverMoveEvent(QHoverEvent *event)
{
    QPointF previousLastScenePoint = m_lastMouseMoveScenePoint;

    // Convert hover move to mouse move, since we don't seem to get actual mouse move events.
    // QGraphicsScene generates hover events from mouse move events, so we don't need
    // to pass hover events there.
    QGraphicsSceneMouseEvent mouseEvent(QEvent::GraphicsSceneMouseMove);
    mouseEvent.setWidget(0);
    mouseEvent.setButtonDownScenePos(m_mousePressButton, m_mousePressScenePoint);
    mouseEvent.setButtonDownScreenPos(m_mousePressButton, m_mousePressScreenPoint);
    mouseEvent.setScenePos(event->pos());
    // Hover events do not have global pos in them, and the screen position doesn't seem to
    // matter anyway in this use case, so just pass event pos instead of trying to
    // calculate the real screen position.
    mouseEvent.setScreenPos(event->pos());
    mouseEvent.setLastScenePos(m_lastMouseMoveScenePoint);
    mouseEvent.setLastScreenPos(m_lastMouseMoveScreenPoint);
    mouseEvent.setButtons(m_mousePressButtons);
    mouseEvent.setButton(m_mousePressButton);
    mouseEvent.setModifiers(event->modifiers());
    m_lastMouseMoveScenePoint = mouseEvent.scenePos();
    m_lastMouseMoveScreenPoint = mouseEvent.screenPos();
    mouseEvent.setAccepted(false);

    QApplication::sendEvent(m_scene, &mouseEvent);

    // Update triggers another hover event, so let's not handle successive hovers at same
    // position to avoid infinite loop.
    if (m_glXYDataManager->dataMap().size() && previousLastScenePoint != m_lastMouseMoveScenePoint) {
        QMouseEvent *newEvent = new QMouseEvent(QEvent::MouseMove,
                                                event->pos() - m_adjustedPlotArea.topLeft(),
                                                m_mousePressButton,
                                                m_mousePressButtons,
                                                event->modifiers());
        m_pendingRenderNodeMouseEvents.append(newEvent);
        update();
    }
}

void DeclarativeChart::mouseDoubleClickEvent(QMouseEvent *event)
{
    m_mousePressScenePoint = event->pos();
    m_mousePressScreenPoint = event->globalPos();
    m_lastMouseMoveScenePoint = m_mousePressScenePoint;
    m_lastMouseMoveScreenPoint = m_mousePressScreenPoint;
    m_mousePressButton = event->button();
    m_mousePressButtons = event->buttons();

    QGraphicsSceneMouseEvent mouseEvent(QEvent::GraphicsSceneMouseDoubleClick);
    mouseEvent.setWidget(0);
    mouseEvent.setButtonDownScenePos(m_mousePressButton, m_mousePressScenePoint);
    mouseEvent.setButtonDownScreenPos(m_mousePressButton, m_mousePressScreenPoint);
    mouseEvent.setScenePos(m_mousePressScenePoint);
    mouseEvent.setScreenPos(m_mousePressScreenPoint);
    mouseEvent.setLastScenePos(m_lastMouseMoveScenePoint);
    mouseEvent.setLastScreenPos(m_lastMouseMoveScreenPoint);
    mouseEvent.setButtons(m_mousePressButtons);
    mouseEvent.setButton(m_mousePressButton);
    mouseEvent.setModifiers(event->modifiers());
    mouseEvent.setAccepted(false);

    QApplication::sendEvent(m_scene, &mouseEvent);

    queueRendererMouseEvent(event);
}

void DeclarativeChart::handleAntialiasingChanged(bool enable)
{
    setAntialiasing(enable);
    emit needRender();
}

void DeclarativeChart::setTheme(DeclarativeChart::Theme theme)
{
    QChart::ChartTheme chartTheme = (QChart::ChartTheme) theme;
    if (chartTheme != m_chart->theme())
        m_chart->setTheme(chartTheme);
}

DeclarativeChart::Theme DeclarativeChart::theme()
{
    return (DeclarativeChart::Theme) m_chart->theme();
}

void DeclarativeChart::setAnimationOptions(DeclarativeChart::Animation animations)
{
    QChart::AnimationOption animationOptions = (QChart::AnimationOption) animations;
    if (animationOptions != m_chart->animationOptions())
        m_chart->setAnimationOptions(animationOptions);
}

DeclarativeChart::Animation DeclarativeChart::animationOptions()
{
    if (m_chart->animationOptions().testFlag(QChart::AllAnimations))
        return DeclarativeChart::AllAnimations;
    else if (m_chart->animationOptions().testFlag(QChart::GridAxisAnimations))
        return DeclarativeChart::GridAxisAnimations;
    else if (m_chart->animationOptions().testFlag(QChart::SeriesAnimations))
        return DeclarativeChart::SeriesAnimations;
    else
        return DeclarativeChart::NoAnimation;
}

void DeclarativeChart::setAnimationDuration(int msecs)
{
    if (msecs != m_chart->animationDuration()) {
        m_chart->setAnimationDuration(msecs);
        emit animationDurationChanged(msecs);
    }
}

int DeclarativeChart::animationDuration() const
{
    return m_chart->animationDuration();
}

void DeclarativeChart::setAnimationEasingCurve(const QEasingCurve &curve)
{
    if (curve != m_chart->animationEasingCurve()) {
        m_chart->setAnimationEasingCurve(curve);
        emit animationEasingCurveChanged(curve);
    }
}

QEasingCurve DeclarativeChart::animationEasingCurve() const
{
    return m_chart->animationEasingCurve();
}

void DeclarativeChart::setTitle(QString title)
{
    if (title != m_chart->title())
        m_chart->setTitle(title);
}
QString DeclarativeChart::title()
{
    return m_chart->title();
}

QAbstractAxis *DeclarativeChart::axisX(QAbstractSeries *series)
{
    QList<QAbstractAxis *> axes = m_chart->axes(Qt::Horizontal, series);
    if (axes.count())
        return axes[0];
    return 0;
}

QAbstractAxis *DeclarativeChart::axisY(QAbstractSeries *series)
{
    QList<QAbstractAxis *> axes = m_chart->axes(Qt::Vertical, series);
    if (axes.count())
        return axes[0];
    return 0;
}

QLegend *DeclarativeChart::legend()
{
    return m_chart->legend();
}

void DeclarativeChart::setTitleColor(QColor color)
{
    QBrush b = m_chart->titleBrush();
    if (color != b.color()) {
        b.setColor(color);
        m_chart->setTitleBrush(b);
        emit titleColorChanged(color);
    }
}

QFont DeclarativeChart::titleFont() const
{
    return m_chart->titleFont();
}

void DeclarativeChart::setTitleFont(const QFont &font)
{
    m_chart->setTitleFont(font);
}

QColor DeclarativeChart::titleColor()
{
    return m_chart->titleBrush().color();
}

void DeclarativeChart::setBackgroundColor(QColor color)
{
    QBrush b = m_chart->backgroundBrush();
    if (b.style() != Qt::SolidPattern || color != b.color()) {
        if (color.alpha() < 0xff)
            m_sceneImageNeedsClear = true;
        b.setStyle(Qt::SolidPattern);
        b.setColor(color);
        m_chart->setBackgroundBrush(b);
        emit backgroundColorChanged();
    }
}

QColor DeclarativeChart::backgroundColor()
{
    return m_chart->backgroundBrush().color();
}

void QtCharts::DeclarativeChart::setPlotAreaColor(QColor color)
{
    QBrush b = m_chart->plotAreaBackgroundBrush();
    if (b.style() != Qt::SolidPattern || color != b.color()) {
        b.setStyle(Qt::SolidPattern);
        b.setColor(color);
        m_chart->setPlotAreaBackgroundBrush(b);
        m_chart->setPlotAreaBackgroundVisible(true);
        emit plotAreaColorChanged();
    }
}

QColor QtCharts::DeclarativeChart::plotAreaColor()
{
    return m_chart->plotAreaBackgroundBrush().color();
}

void DeclarativeChart::setLocalizeNumbers(bool localize)
{
    if (m_chart->localizeNumbers() != localize) {
        m_chart->setLocalizeNumbers(localize);
        emit localizeNumbersChanged();
    }
}

bool DeclarativeChart::localizeNumbers() const
{
    return m_chart->localizeNumbers();
}

void QtCharts::DeclarativeChart::setLocale(const QLocale &locale)
{
    if (m_chart->locale() != locale) {
        m_chart->setLocale(locale);
        emit localeChanged();
    }
}

QLocale QtCharts::DeclarativeChart::locale() const
{
    return m_chart->locale();
}

int DeclarativeChart::count()
{
    return m_chart->series().count();
}

void DeclarativeChart::setDropShadowEnabled(bool enabled)
{
    if (enabled != m_chart->isDropShadowEnabled()) {
        m_sceneImageNeedsClear = true;
        m_chart->setDropShadowEnabled(enabled);
        dropShadowEnabledChanged(enabled);
    }
}

bool DeclarativeChart::dropShadowEnabled()
{
    return m_chart->isDropShadowEnabled();
}

qreal DeclarativeChart::backgroundRoundness() const
{
    return m_chart->backgroundRoundness();
}

void DeclarativeChart::setBackgroundRoundness(qreal diameter)
{
    if (m_chart->backgroundRoundness() != diameter) {
        m_sceneImageNeedsClear = true;
        m_chart->setBackgroundRoundness(diameter);
        emit backgroundRoundnessChanged(diameter);
    }
}

void DeclarativeChart::zoom(qreal factor)
{
    m_chart->zoom(factor);
}

void DeclarativeChart::zoomIn()
{
    m_chart->zoomIn();
}

void DeclarativeChart::zoomIn(const QRectF &rectangle)
{
    m_chart->zoomIn(rectangle);
}

void DeclarativeChart::zoomOut()
{
    m_chart->zoomOut();
}

void DeclarativeChart::zoomReset()
{
    m_chart->zoomReset();
}

bool DeclarativeChart::isZoomed()
{
    return m_chart->isZoomed();
}

void DeclarativeChart::scrollLeft(qreal pixels)
{
    m_chart->scroll(-pixels, 0);
}

void DeclarativeChart::scrollRight(qreal pixels)
{
    m_chart->scroll(pixels, 0);
}

void DeclarativeChart::scrollUp(qreal pixels)
{
    m_chart->scroll(0, pixels);
}

void DeclarativeChart::scrollDown(qreal pixels)
{
    m_chart->scroll(0, -pixels);
}

QQmlListProperty<QAbstractAxis> DeclarativeChart::axes()
{
    return QQmlListProperty<QAbstractAxis>(this, 0,
                &DeclarativeChart::axesAppendFunc,
                &DeclarativeChart::axesCountFunc,
                &DeclarativeChart::axesAtFunc,
                &DeclarativeChart::axesClearFunc);
}

void DeclarativeChart::axesAppendFunc(QQmlListProperty<QAbstractAxis> *list, QAbstractAxis *element)
{
    // Empty implementation
    Q_UNUSED(list);
    Q_UNUSED(element);
}

int DeclarativeChart::axesCountFunc(QQmlListProperty<QAbstractAxis> *list)
{
    if (qobject_cast<DeclarativeChart *>(list->object)) {
        DeclarativeChart *chart = qobject_cast<DeclarativeChart *>(list->object);
        return chart->m_chart->axes(Qt::Horizontal | Qt::Vertical).count();
    }
    return 0;
}

QAbstractAxis *DeclarativeChart::axesAtFunc(QQmlListProperty<QAbstractAxis> *list, int index)
{
    if (qobject_cast<DeclarativeChart *>(list->object)) {
        DeclarativeChart *chart = qobject_cast<DeclarativeChart *>(list->object);
        QList<QAbstractAxis *> axes = chart->m_chart->axes(Qt::Horizontal | Qt::Vertical);
        return axes.at(index);
    }
    return 0;
}

void DeclarativeChart::axesClearFunc(QQmlListProperty<QAbstractAxis> *list)
{
    // Empty implementation
    Q_UNUSED(list);
}


QAbstractSeries *DeclarativeChart::series(int index)
{
    if (index < m_chart->series().count()) {
        return m_chart->series().at(index);
    }
    return 0;
}

QAbstractSeries *DeclarativeChart::series(QString seriesName)
{
    foreach (QAbstractSeries *series, m_chart->series()) {
        if (series->name() == seriesName)
            return series;
    }
    return 0;
}

QAbstractSeries *DeclarativeChart::createSeries(int type, QString name, QAbstractAxis *axisX, QAbstractAxis *axisY)
{
    QAbstractSeries *series = 0;

    switch (type) {
    case DeclarativeChart::SeriesTypeLine:
        series = new DeclarativeLineSeries();
        break;
    case DeclarativeChart::SeriesTypeArea: {
        DeclarativeAreaSeries *area = new DeclarativeAreaSeries();
        DeclarativeLineSeries *line = new DeclarativeLineSeries();
        line->setParent(area);
        area->setUpperSeries(line);
        series = area;
        break;
    }
    case DeclarativeChart::SeriesTypeStackedBar:
        series = new DeclarativeStackedBarSeries();
        break;
    case DeclarativeChart::SeriesTypePercentBar:
        series = new DeclarativePercentBarSeries();
        break;
    case DeclarativeChart::SeriesTypeBar:
        series = new DeclarativeBarSeries();
        break;
    case DeclarativeChart::SeriesTypeHorizontalBar:
        series = new DeclarativeHorizontalBarSeries();
        break;
    case DeclarativeChart::SeriesTypeHorizontalPercentBar:
        series = new DeclarativeHorizontalPercentBarSeries();
        break;
    case DeclarativeChart::SeriesTypeHorizontalStackedBar:
        series = new DeclarativeHorizontalStackedBarSeries();
        break;
    case DeclarativeChart::SeriesTypeBoxPlot:
        series = new DeclarativeBoxPlotSeries();
        break;
    case DeclarativeChart::SeriesTypeCandlestick:
        series = new DeclarativeCandlestickSeries();
        break;
    case DeclarativeChart::SeriesTypePie:
        series = new DeclarativePieSeries();
        break;
    case DeclarativeChart::SeriesTypeScatter:
        series = new DeclarativeScatterSeries();
        break;
    case DeclarativeChart::SeriesTypeSpline:
        series = new DeclarativeSplineSeries();
        break;
    default:
        qWarning() << "Illegal series type";
    }

    if (series) {
        // Connect to axis changed signals (unless this is a pie series)
        if (!qobject_cast<DeclarativePieSeries *>(series)) {
            connect(series, SIGNAL(axisXChanged(QAbstractAxis*)), this, SLOT(handleAxisXSet(QAbstractAxis*)));
            connect(series, SIGNAL(axisXTopChanged(QAbstractAxis*)), this, SLOT(handleAxisXSet(QAbstractAxis*)));
            connect(series, SIGNAL(axisYChanged(QAbstractAxis*)), this, SLOT(handleAxisYSet(QAbstractAxis*)));
            connect(series, SIGNAL(axisYRightChanged(QAbstractAxis*)), this, SLOT(handleAxisYRightSet(QAbstractAxis*)));
        }

        series->setName(name);
        m_chart->addSeries(series);

        if (!axisX || !axisY)
            initializeAxes(series);

        if (axisX)
            setAxisX(axisX, series);
        if (axisY)
            setAxisY(axisY, series);
    }

    return series;
}

void DeclarativeChart::removeSeries(QAbstractSeries *series)
{
    if (series)
        m_chart->removeSeries(series);
    else
        qWarning("removeSeries: cannot remove null");
}

void DeclarativeChart::setAxisX(QAbstractAxis *axis, QAbstractSeries *series)
{
    if (axis && series)
        seriesAxisAttachHelper(series, axis, Qt::Horizontal, Qt::AlignBottom);
}

void DeclarativeChart::setAxisY(QAbstractAxis *axis, QAbstractSeries *series)
{
    if (axis && series)
        seriesAxisAttachHelper(series, axis, Qt::Vertical, Qt::AlignLeft);
}

QAbstractAxis *DeclarativeChart::defaultAxis(Qt::Orientation orientation, QAbstractSeries *series)
{
    if (!series) {
        qWarning() << "No axis type defined for null series";
        return 0;
    }

    foreach (QAbstractAxis *existingAxis, m_chart->axes(orientation)) {
        if (existingAxis->type() == series->d_ptr->defaultAxisType(orientation))
            return existingAxis;
    }

    switch (series->d_ptr->defaultAxisType(orientation)) {
    case QAbstractAxis::AxisTypeValue:
        return new QValueAxis(this);
    case QAbstractAxis::AxisTypeBarCategory:
        return new QBarCategoryAxis(this);
    case QAbstractAxis::AxisTypeCategory:
        return new QCategoryAxis(this);
#ifndef QT_QREAL_IS_FLOAT
    case QAbstractAxis::AxisTypeDateTime:
        return new QDateTimeAxis(this);
#endif
    case QAbstractAxis::AxisTypeLogValue:
        return new QLogValueAxis(this);
    default:
        // assume AxisTypeNoAxis
        return 0;
    }
}

void DeclarativeChart::initializeAxes(QAbstractSeries *series)
{
    if (qobject_cast<DeclarativeLineSeries *>(series))
        doInitializeAxes(series, qobject_cast<DeclarativeLineSeries *>(series)->m_axes);
    else if (qobject_cast<DeclarativeScatterSeries *>(series))
        doInitializeAxes(series, qobject_cast<DeclarativeScatterSeries *>(series)->m_axes);
    else if (qobject_cast<DeclarativeSplineSeries *>(series))
        doInitializeAxes(series, qobject_cast<DeclarativeSplineSeries *>(series)->m_axes);
    else if (qobject_cast<DeclarativeAreaSeries *>(series))
        doInitializeAxes(series, qobject_cast<DeclarativeAreaSeries *>(series)->m_axes);
    else if (qobject_cast<DeclarativeBarSeries *>(series))
        doInitializeAxes(series, qobject_cast<DeclarativeBarSeries *>(series)->m_axes);
    else if (qobject_cast<DeclarativeStackedBarSeries *>(series))
        doInitializeAxes(series, qobject_cast<DeclarativeStackedBarSeries *>(series)->m_axes);
    else if (qobject_cast<DeclarativePercentBarSeries *>(series))
        doInitializeAxes(series, qobject_cast<DeclarativePercentBarSeries *>(series)->m_axes);
    else if (qobject_cast<DeclarativeHorizontalBarSeries *>(series))
        doInitializeAxes(series, qobject_cast<DeclarativeHorizontalBarSeries *>(series)->m_axes);
    else if (qobject_cast<DeclarativeHorizontalStackedBarSeries *>(series))
        doInitializeAxes(series, qobject_cast<DeclarativeHorizontalStackedBarSeries *>(series)->m_axes);
    else if (qobject_cast<DeclarativeHorizontalPercentBarSeries *>(series))
        doInitializeAxes(series, qobject_cast<DeclarativeHorizontalPercentBarSeries *>(series)->m_axes);
    else if (qobject_cast<DeclarativeBoxPlotSeries *>(series))
        doInitializeAxes(series, qobject_cast<DeclarativeBoxPlotSeries *>(series)->m_axes);
    else if (qobject_cast<DeclarativeCandlestickSeries *>(series))
        doInitializeAxes(series, qobject_cast<DeclarativeCandlestickSeries *>(series)->m_axes);
    // else: do nothing
}

void DeclarativeChart::doInitializeAxes(QAbstractSeries *series, DeclarativeAxes *axes)
{
    qreal min;
    qreal max;
    // Initialize axis X
    if (axes->axisX()) {
        axes->emitAxisXChanged();
    } else if (axes->axisXTop()) {
        axes->emitAxisXTopChanged();
    } else {
        axes->setAxisX(defaultAxis(Qt::Horizontal, series));
        findMinMaxForSeries(series, Qt::Horizontal, min, max);
        axes->axisX()->setRange(min, max);
    }

    // Initialize axis Y
    if (axes->axisY()) {
        axes->emitAxisYChanged();
    } else if (axes->axisYRight()) {
        axes->emitAxisYRightChanged();
    } else {
        axes->setAxisY(defaultAxis(Qt::Vertical, series));
        findMinMaxForSeries(series, Qt::Vertical, min, max);
        axes->axisY()->setRange(min, max);
    }
}

void DeclarativeChart::findMinMaxForSeries(QAbstractSeries *series, Qt::Orientations orientation,
                                           qreal &min, qreal &max)
{
    if (!series) {
        min = 0.5;
        max = 0.5;
    } else {
        AbstractDomain *domain = series->d_ptr->domain();
        min = (orientation == Qt::Vertical) ? domain->minY() : domain->minX();
        max = (orientation == Qt::Vertical) ? domain->maxY() : domain->maxX();

        if (min == max) {
            min -= 0.5;
            max += 0.5;
        }
    }
}

void DeclarativeChart::queueRendererMouseEvent(QMouseEvent *event)
{
    if (m_glXYDataManager->dataMap().size()) {
        QMouseEvent *newEvent = new QMouseEvent(event->type(),
                                                event->pos() - m_adjustedPlotArea.topLeft(),
                                                event->button(),
                                                event->buttons(),
                                                event->modifiers());

        m_pendingRenderNodeMouseEvents.append(newEvent);

        update();
    }
}

QPointF DeclarativeChart::mapToValue(const QPointF &position, QAbstractSeries *series)
{
    return m_chart->mapToValue(position, series);
}

QPointF DeclarativeChart::mapToPosition(const QPointF &value, QAbstractSeries *series)
{
    return m_chart->mapToPosition(value, series);
}

#include "moc_declarativechart.cpp"

QT_CHARTS_END_NAMESPACE
