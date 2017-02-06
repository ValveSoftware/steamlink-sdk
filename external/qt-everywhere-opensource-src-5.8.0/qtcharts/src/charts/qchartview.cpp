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

#include <QtCharts/QChartView>
#include <private/qchartview_p.h>
#include <private/qchart_p.h>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QRubberBand>

/*!
    \enum QChartView::RubberBand

    This enum describes the different types of rubber bands that can be used for zoom rect selection

    \value NoRubberBand
    \value VerticalRubberBand
    \value HorizontalRubberBand
    \value RectangleRubberBand
*/

/*!
    \class QChartView
    \inmodule Qt Charts
    \brief Standalone charting widget.

    QChartView is a standalone widget that can display charts. It does not require separate
    QGraphicsScene to work. If you want to display a chart in your existing QGraphicsScene,
    you need to use the QChart (or QPolarChart) class instead.

    \sa QChart, QPolarChart
*/

QT_CHARTS_BEGIN_NAMESPACE

/*!
    Constructs a chartView object with parent \a parent.
*/

QChartView::QChartView(QWidget *parent)
    : QGraphicsView(parent),
      d_ptr(new QChartViewPrivate(this))
{

}

/*!
    Constructs a chartview object with parent \a parent to display a \a chart.
    Ownership of the \a chart is passed to chartview.
*/

QChartView::QChartView(QChart *chart, QWidget *parent)
    : QGraphicsView(parent),
      d_ptr(new QChartViewPrivate(this, chart))
{

}


/*!
    Destroys the chartview object and the associated chart.
*/
QChartView::~QChartView()
{
}

/*!
    Returns the pointer to the associated chart.
*/
QChart *QChartView::chart() const
{
    return d_ptr->m_chart;
}

/*!
    Sets the current chart to \a chart. Ownership of the new chart is passed to chartview
    and ownership of the previous chart is released.

    To avoid memory leaks users need to make sure the previous chart is deleted.
*/

void QChartView::setChart(QChart *chart)
{
    d_ptr->setChart(chart);
}

/*!
    Sets the rubber band flags to \a rubberBand.
    Selected flags determine the way zooming is performed.

    \note Rubber band zooming is not supported for polar charts.
*/
void QChartView::setRubberBand(const RubberBands &rubberBand)
{
#ifndef QT_NO_RUBBERBAND
    d_ptr->m_rubberBandFlags = rubberBand;

    if (!d_ptr->m_rubberBandFlags) {
        delete d_ptr->m_rubberBand;
        d_ptr->m_rubberBand = 0;
        return;
    }

    if (!d_ptr->m_rubberBand) {
        d_ptr->m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        d_ptr->m_rubberBand->setEnabled(true);
    }
#else
    Q_UNUSED(rubberBand);
    qWarning("Unable to set rubber band because Qt is configured without it.");
#endif
}

/*!
    Returns the rubber band flags that are currently being used by the widget.
*/
QChartView::RubberBands QChartView::rubberBand() const
{
    return d_ptr->m_rubberBandFlags;
}

/*!
    If Left mouse button is pressed and the rubber band is enabled the \a event is accepted and the rubber band is displayed on the screen allowing the user to select the zoom area.
    If different mouse button is pressed and/or the rubber band is disabled then the \a event is passed to QGraphicsView::mousePressEvent() implementation.
*/
void QChartView::mousePressEvent(QMouseEvent *event)
{
#ifndef QT_NO_RUBBERBAND
    QRectF plotArea = d_ptr->m_chart->plotArea();
    if (d_ptr->m_rubberBand && d_ptr->m_rubberBand->isEnabled()
            && event->button() == Qt::LeftButton && plotArea.contains(event->pos())) {
        d_ptr->m_rubberBandOrigin = event->pos();
        d_ptr->m_rubberBand->setGeometry(QRect(d_ptr->m_rubberBandOrigin, QSize()));
        d_ptr->m_rubberBand->show();
        event->accept();
    } else {
#endif
        QGraphicsView::mousePressEvent(event);
#ifndef QT_NO_RUBBERBAND
    }
#endif
}

/*!
    If the rubber band rectange has been displayed in pressEvent then \a event data is used to update the rubber band geometry.
    Otherwise the default QGraphicsView::mouseMoveEvent implementation is called.
*/
void QChartView::mouseMoveEvent(QMouseEvent *event)
{
#ifndef QT_NO_RUBBERBAND
    if (d_ptr->m_rubberBand && d_ptr->m_rubberBand->isVisible()) {
        QRect rect = d_ptr->m_chart->plotArea().toRect();
        int width = event->pos().x() - d_ptr->m_rubberBandOrigin.x();
        int height = event->pos().y() - d_ptr->m_rubberBandOrigin.y();
        if (!d_ptr->m_rubberBandFlags.testFlag(VerticalRubberBand)) {
            d_ptr->m_rubberBandOrigin.setY(rect.top());
            height = rect.height();
        }
        if (!d_ptr->m_rubberBandFlags.testFlag(HorizontalRubberBand)) {
            d_ptr->m_rubberBandOrigin.setX(rect.left());
            width = rect.width();
        }
        d_ptr->m_rubberBand->setGeometry(QRect(d_ptr->m_rubberBandOrigin.x(), d_ptr->m_rubberBandOrigin.y(), width, height).normalized());
    } else {
#endif
        QGraphicsView::mouseMoveEvent(event);
#ifndef QT_NO_RUBBERBAND
    }
#endif
}

/*!
    If left mouse button is released and the rubber band is enabled then \a event is accepted and
    the view is zoomed into the rect specified by the rubber band.
    If it is a right mouse button \a event then the view is zoomed out.
*/
void QChartView::mouseReleaseEvent(QMouseEvent *event)
{
#ifndef QT_NO_RUBBERBAND
    if (d_ptr->m_rubberBand && d_ptr->m_rubberBand->isVisible()) {
        if (event->button() == Qt::LeftButton) {
            d_ptr->m_rubberBand->hide();
            QRectF rect = d_ptr->m_rubberBand->geometry();
            // Since plotArea uses QRectF and rubberband uses QRect, we can't just blindly use
            // rubberband's dimensions for vertical and horizontal rubberbands, where one
            // dimension must match the corresponding plotArea dimension exactly.
            if (d_ptr->m_rubberBandFlags == VerticalRubberBand) {
                rect.setX(d_ptr->m_chart->plotArea().x());
                rect.setWidth(d_ptr->m_chart->plotArea().width());
            } else if (d_ptr->m_rubberBandFlags == HorizontalRubberBand) {
                rect.setY(d_ptr->m_chart->plotArea().y());
                rect.setHeight(d_ptr->m_chart->plotArea().height());
            }
            d_ptr->m_chart->zoomIn(rect);
            event->accept();
        }

    } else if (d_ptr->m_rubberBand && event->button() == Qt::RightButton) {
            // If vertical or horizontal rubberband mode, restrict zoom out to specified axis.
            // Since there is no suitable API for that, use zoomIn with rect bigger than the
            // plot area.
            if (d_ptr->m_rubberBandFlags == VerticalRubberBand
                || d_ptr->m_rubberBandFlags == HorizontalRubberBand) {
                QRectF rect = d_ptr->m_chart->plotArea();
                if (d_ptr->m_rubberBandFlags == VerticalRubberBand) {
                    qreal adjustment = rect.height() / 2;
                    rect.adjust(0, -adjustment, 0, adjustment);
                } else if (d_ptr->m_rubberBandFlags == HorizontalRubberBand) {
                    qreal adjustment = rect.width() / 2;
                    rect.adjust(-adjustment, 0, adjustment, 0);
                }
                d_ptr->m_chart->zoomIn(rect);
            } else {
                d_ptr->m_chart->zoomOut();
            }
            event->accept();
    } else {
#endif
        QGraphicsView::mouseReleaseEvent(event);
#ifndef QT_NO_RUBBERBAND
    }
#endif
}

/*!
    Resizes and updates the chart area using the \a event data
*/
void QChartView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    d_ptr->resize();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QChartViewPrivate::QChartViewPrivate(QChartView *q, QChart *chart)
    : q_ptr(q),
      m_scene(new QGraphicsScene(q)),
      m_chart(chart),
#ifndef QT_NO_RUBBERBAND
      m_rubberBand(0),
#endif
      m_rubberBandFlags(QChartView::NoRubberBand)
{
    q_ptr->setFrameShape(QFrame::NoFrame);
    q_ptr->setBackgroundRole(QPalette::Window);
    q_ptr->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    q_ptr->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    q_ptr->setScene(m_scene);
    q_ptr->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (!m_chart)
        m_chart = new QChart();
    m_scene->addItem(m_chart);
}

QChartViewPrivate::~QChartViewPrivate()
{
}

void QChartViewPrivate::setChart(QChart *chart)
{
    Q_ASSERT(chart);

    if (m_chart == chart)
        return;

    if (m_chart)
        m_scene->removeItem(m_chart);

    m_chart = chart;
    m_scene->addItem(m_chart);

    resize();
}

void QChartViewPrivate::resize()
{
    // Fit the chart into view if it has been rotated
    qreal sinA = qAbs(q_ptr->transform().m21());
    qreal cosA = qAbs(q_ptr->transform().m11());
    QSize chartSize = q_ptr->size();

    if (sinA == 1.0) {
        chartSize.setHeight(q_ptr->size().width());
        chartSize.setWidth(q_ptr->size().height());
    } else if (sinA != 0.0) {
        // Non-90 degree rotation, find largest square chart that can fit into the view.
        qreal minDimension = qMin(q_ptr->size().width(), q_ptr->size().height());
        qreal h = (minDimension - (minDimension / ((sinA / cosA) + 1.0))) / sinA;
        chartSize.setHeight(h);
        chartSize.setWidth(h);
    }

    m_chart->resize(chartSize);
    q_ptr->setMinimumSize(m_chart->minimumSize().toSize());
    q_ptr->setSceneRect(m_chart->geometry());
}

#include "moc_qchartview.cpp"

QT_CHARTS_END_NAMESPACE
