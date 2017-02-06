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

#include <QtCharts/QLegendMarker>
#include <private/qlegendmarker_p.h>
#include <private/legendmarkeritem_p.h>
#include <QtCharts/QLegend>
#include <private/qlegend_p.h>
#include <private/legendlayout_p.h>
#include <QtGui/QFontMetrics>
#include <QtWidgets/QGraphicsSceneEvent>
#include <QtCharts/QAbstractSeries>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QLegendMarker
    \inmodule Qt Charts
    \brief The QLegendMarker class is an abstract object that can be used to access
    markers within a legend.

    A legend marker consists of an icon and a label. The icon color corresponds to the color
    used to draw a series and the label displays the name of the series (or the label of the
    slice for a pie series or bar set for a bar series). A legend marker is always related to
    one series, slice, or bar set.

    \image examples_percentbarchart_legend.png

    \sa QLegend
*/
/*!
    \enum QLegendMarker::LegendMarkerType
    \since 5.8

    The type of the legend marker object.

    \value LegendMarkerTypeArea
           A legend marker for an area series.
    \value LegendMarkerTypeBar
           A legend marker for a bar set.
    \value LegendMarkerTypePie
           A legend marker for a pie slice.
    \value LegendMarkerTypeXY
           A legend marker for a line, spline, or scatter series.
    \value LegendMarkerTypeBoxPlot
           A legend marker for a box plot series.
    \value LegendMarkerTypeCandlestick
           A legend marker for a candlestick series.
*/

/*!
    \fn virtual LegendMarkerType QLegendMarker::type() = 0;
    Returns the type of the legend marker for the related series, pie slice, or bar set.

    \sa LegendMarkerType
*/

/*!
    \fn virtual QAbstractSeries* QLegendMarker::series() = 0;
    Returns a pointer to the series that is related to this legend marker. A legend marker
    is always related to a series.
*/

/*!
  \fn void QLegendMarker::clicked();
  This signal is emitted when the legend marker is clicked.
*/

/*!
  \fn void QLegendMarker::hovered(bool status);
  This signal is emitted when a mouse is hovered over the legend marker.
  When the mouse moves over the marker, \a status turns \c true, and when
  the mouse moves away again, it turns \c false.
*/

/*!
    \fn void QLegendMarker::labelChanged()
    This signal is emitted when the label of the legend marker has changed.
*/

/*!
    \fn void QLegendMarker::labelBrushChanged()
    This signal is emitted when the label brush of the legend marker has changed.
*/

/*!
    \fn void QLegendMarker::fontChanged()
    This signal is emitted when the (label) font of the legend marker has changed.
*/

/*!
    \fn void QLegendMarker::penChanged()
    This signal is emitted when the pen of the legend marker has changed.
*/

/*!
    \fn void QLegendMarker::brushChanged()
    This signal is emitted when the brush of the legend marker has changed.
*/

/*!
    \fn void QLegendMarker::visibleChanged()
    This signal is emitted when the visibility of the legend marker has changed.
*/

/*!
    \property QLegendMarker::label
    \brief The text shown in the legend for a legend marker.
*/

/*!
    \property QLegendMarker::labelBrush
    \brief The brush of the label.
*/

/*!
    \property QLegendMarker::font
    \brief The font of the label.
*/

/*!
    \property QLegendMarker::pen
    \brief The pen used to draw the outline of the icon.
*/

/*!
    \property QLegendMarker::brush
    \brief The brush used to fill the icon.
*/

/*!
    \property QLegendMarker::visible
    \brief The visibility of the legend marker.

    The visibility affects both the legend marker label and the icon.
*/


/*!
    \internal
 */
QLegendMarker::QLegendMarker(QLegendMarkerPrivate &d, QObject *parent) :
    QObject(parent),
    d_ptr(&d)
{
    d_ptr->m_item->setVisible(d_ptr->series()->isVisible());
}

/*!
    Removes the legend marker.
*/
QLegendMarker::~QLegendMarker()
{
}

/*!
  Returns the label of the marker.
*/
QString QLegendMarker::label() const
{
    return d_ptr->m_item->label();
}

/*!
    Sets the label of the marker to \a label.

    \note Changing the name of a series also changes the label of its marker.
*/
void QLegendMarker::setLabel(const QString &label)
{
    if (label.isEmpty()) {
        d_ptr->m_customLabel = false;
    } else {
        d_ptr->m_customLabel = true;
        d_ptr->m_item->setLabel(label);
    }
}
/*!
    Returns the brush that is used to draw the label.
*/
QBrush QLegendMarker::labelBrush() const
{
    return d_ptr->m_item->labelBrush();
}

/*!
    Sets the the brush used to draw to label to \a brush.
*/
void QLegendMarker::setLabelBrush(const QBrush &brush)
{
    d_ptr->m_item->setLabelBrush(brush);
}

/*!
    Retuns the font of the label.
*/
QFont QLegendMarker::font() const
{
    return d_ptr->m_item->font();
}

/*!
    Sets the font of the label to \a font.
*/
void QLegendMarker::setFont(const QFont &font)
{
    d_ptr->m_item->setFont(font);
}

/*!
    Returns the pen used to draw the outline of the icon.
*/
QPen QLegendMarker::pen() const
{
    return d_ptr->m_item->pen();
}

/*!
    Sets the \a pen used to draw the outline of the icon to \a pen.
*/
void QLegendMarker::setPen(const QPen &pen)
{
    if (pen == QPen(Qt::NoPen)) {
        d_ptr->m_customPen = false;
    } else {
        d_ptr->m_customPen = true;
        d_ptr->m_item->setPen(pen);
    }
}

/*!
    Returns the brush used to fill the icon.
*/
QBrush QLegendMarker::brush() const
{
    return d_ptr->m_item->brush();
}

/*!
    Sets the brush used to fill the icon to \a brush.

    \note Changing the color of the series also changes the color of the icon.
*/
void QLegendMarker::setBrush(const QBrush &brush)
{
    if (brush == QBrush(Qt::NoBrush)) {
        d_ptr->m_customBrush = false;
    } else {
        d_ptr->m_customBrush = true;
        d_ptr->m_item->setBrush(brush);
    }
}

/*!
    Returns the visibility of the marker.
*/
bool QLegendMarker::isVisible() const
{
    return d_ptr->m_item->isVisible();
}

/*!
    Sets the marker's visibility to \a visible.
*/
void QLegendMarker::setVisible(bool visible)
{
    d_ptr->m_item->setVisible(visible);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QLegendMarkerPrivate::QLegendMarkerPrivate(QLegendMarker *q, QLegend *legend) :
    m_legend(legend),
    m_customLabel(false),
    m_customBrush(false),
    m_customPen(false),
    q_ptr(q)
{
    m_item = new LegendMarkerItem(this);
}

QLegendMarkerPrivate::~QLegendMarkerPrivate()
{
    delete m_item;
}

void QLegendMarkerPrivate::invalidateLegend()
{
    m_legend->d_ptr->m_layout->invalidate();
}

#include "moc_qlegendmarker.cpp"
#include "moc_qlegendmarker_p.cpp"

QT_CHARTS_END_NAMESPACE
