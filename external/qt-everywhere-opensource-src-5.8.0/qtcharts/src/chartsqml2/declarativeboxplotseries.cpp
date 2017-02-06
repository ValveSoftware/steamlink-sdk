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

#include "declarativebarseries.h"
#include "declarativeboxplotseries.h"
#include <QtCharts/QBoxSet>
#include <QtCharts/QVBoxPlotModelMapper>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \qmltype BoxSet
    \instantiates QBoxSet
    \inqmlmodule QtCharts

    \brief Represents one item in a box-and-whiskers chart.

    A box-and-whiskers item is a graphical representation of a range and three median values
    that is constructed from five different values. There are two ways to specify the values.
    The first one is by using a constructor or the append() method. The values have to be
    specified in the following order: lower extreme, lower quartile, median, upper quartile,
    and upper extreme.

    The second way is to create an empty BoxSet instance and specify the values using the
    setValue() method.

    \sa BoxPlotSeries
*/

/*!
    \qmlproperty enumeration BoxSet::ValuePositions

    This enum type defines the values of a box-and-whiskers item:

    \value BoxSet.LowerExtreme The smallest value of the box-and-whiskers item.
    \value BoxSet.LowerQuartile The median value of the lower half of the box-and-whiskers item.
    \value BoxSet.Median The median value of the box-and-whiskers item.
    \value BoxSet.UpperQuartile The median value of the upper half of the box-and-whiskers item.
    \value BoxSet.UpperExtreme The largest value of the box-and-whiskers item.
*/

/*!
    \qmlproperty string BoxSet::values
    The values of the box-and-whiskers item.
*/
/*!
    \qmlproperty string BoxSet::label
    The label of the category of the box-and-whiskers item.
*/
/*!
    \qmlproperty int BoxSet::count
    The number of values of the box-and-whiskers item.
*/
/*!
    \qmlmethod void BoxSet::at(int index)
    Returns the value in the position specified by \a index.
*/
/*!
    \qmlmethod void BoxSet::append(qreal value)
    Appends the new value specified by \a value to the end of the box-and-whiskers item.
*/
/*!
    \qmlmethod void BoxSet::clear()
    Sets all the values of the box-and-whiskers item to 0.
*/
/*!
    \qmlmethod void BoxSet::setValue(int index, qreal value)
    Sets the value specified by \a value in the position specified by \a index.
*/
/*!
    \qmlsignal BoxSet::onClicked()
    This signal is emitted when the user clicks a box-and-whiskers item in the chart.
*/
/*!
    \qmlsignal BoxSet::onPressed()
    This signal is emitted when the user clicks a box-and-whiskers item in the chart
    and holds down the mouse button.
*/
/*!
    \qmlsignal BoxSet::onReleased()
    This signal is emitted when the user releases the mouse press on a box-and-whiskers item.
*/
/*!
    \qmlsignal BoxSet::onDoubleClicked()
    This signal is emitted when the user double-clicks a box-and-whiskers item.
*/
/*!
    \qmlsignal BoxSet::onHovered(bool status)
    This signal is emitted when a mouse is hovered over a box-and-whiskers item in a chart.
    When the mouse moves over the item, \a status turns \c true, and when the mouse moves
    away again, it turns \c false.
*/
/*!
    \qmlsignal BoxSet::onPenChanged()
    This signal is emitted when the pen used to draw the lines of the box-and-whiskers item changes.
*/
/*!
    \qmlsignal BoxSet::onBrushChanged()
    This signal is emitted when the brush used to fill the box of the box-and-whiskers item changes.
*/
/*!
    \qmlsignal BoxSet::onChangedValues()
    This signal is emitted when multiple values of the box-and-whiskers item change.
*/
/*!
    \qmlsignal BoxSet::onChangedValue(int index)
    This signal is emitted when the value of the box-and-whiskers item specified by \a index
    changes.
*/
/*!
    \qmlsignal BoxSet::onCleared()
    This signal is emitted when all the values of the box-and-whiskers item are set to 0.
*/

/*!
    \qmltype BoxPlotSeries
    \instantiates QBoxPlotSeries
    \inqmlmodule QtCharts

    \inherits AbstractSeries

    \brief Presents data in box-and-whiskers charts.

    A box plot series acts as a container for box-and-whiskers items. Items from multiple series
    are grouped into categories according to their index value.

    The BarCategoryAxis class is used to add the categories to the chart's axis. Category labels
    have to be unique. If the same category label is defined for several box-and-whiskers items,
    only the first one is drawn.

    The following QML code snippet shows how to create a simple box-and-whiskers chart:
    \code
    import QtQuick 2.0
    import QtCharts 2.0

    ChartView {
        title: "Box Plot series"
        width: 400
        height: 300
        theme: ChartView.ChartThemeBrownSand
        legend.alignment: Qt.AlignBottom

        BoxPlotSeries {
            id: plotSeries
            name: "Income"
            BoxSet { label: "Jan"; values: [3, 4, 5.1, 6.2, 8.5] }
            BoxSet { label: "Feb"; values: [5, 6, 7.5, 8.6, 11.8] }
            BoxSet { label: "Mar"; values: [3.2, 5, 5.7, 8, 9.2] }
            BoxSet { label: "Apr"; values: [3.8, 5, 6.4, 7, 8] }
            BoxSet { label: "May"; values: [4, 5, 5.2, 6, 7] }
        }
    }
    \endcode

    \beginfloatleft
    \image examples_qmlboxplot.png
    \endfloat
    \clearfloat

    \sa BoxSet, BarCategoryAxis
*/

/*!
    \qmlmethod BoxPlotSeries::append(string label, VariantList values)
    Appends a new box-and-whiskers item with the label specified by \a label and the values
    specified by \a values to the series.
 */
/*!
    \qmlmethod BoxPlotSeries::append(BoxSet box)
    Appends the box-and-whiskers item specified by \a box to the series.
*/
/*!
    \qmlmethod BoxPlotSeries::insert(int index, string label, VariantList values)
    Inserts a new box-and-whiskers item with the label specified by \a label and the values
    specified by \a values to the series at the position specified by \a index.
*/
/*!
    \qmlmethod BoxPlotSeries::remove(QBoxSet boxset)
    Removes the box-and-whiskers item specified by \a boxset from the series.
*/
/*!
    \qmlmethod BoxPlotSeries::clear()
    Removes all box-and-whiskers items from the series and permanently deletes them.
*/
/*!
    \qmlsignal BoxPlotSeries::onClicked(BoxSet boxset);
    This signal is emitted when the user clicks the box-and-whiskers item specified by
    \a boxset in the chart.
*/
/*!
    \qmlsignal BoxPlotSeries::onHovered(bool status, BoxSet boxset);
    This signal is emitted when a mouse is hovered over the box-and-whiskers item specified by
    \a boxset in the chart. When the mouse moves over the item, \a status turns \c true, and
    when the mouse moves away again, it turns \c false.
*/
/*!
    \qmlsignal BoxPlotSeries::onPressed(BoxSet boxset)
    This signal is emitted when the user presses the \a boxset on the chart.
*/
/*!
    \qmlsignal BoxPlotSeries::onReleased(BoxSet boxset)
    This signal is emitted when the user releases the mouse press on the box-and-whiskers
    item specified by \a boxset in the chart.
*/
/*!
    \qmlsignal BoxPlotSeries::onDoubleClicked(BoxSet boxset)
    This signal is emitted when the user double-clicks the box-and-whiskers item specified by
    \a boxset in the chart.
*/
/*!
    \qmlsignal BoxPlotSeries::onCountChanged();
    This signal is emitted when the number of box-and-whiskers items in the series changes.
*/
/*!
    \qmlsignal BoxPlotSeries::onBoxsetsAdded()
    This signal is emitted when new box-and-whiskers items are added to the series.
 */
/*!
    \qmlsignal BoxPlotSeries::onBoxsetsRemoved()
    This signal is emitted when box-and-whiskers items are removed from the series.
 */
/*!
    \qmlproperty AbstractAxis BoxPlotSeries::axisX
    The x-axis used for the series. If you leave both axisX and axisXTop undefined, a
    BarCategoryAxis is created for the series.
    \sa axisXTop
*/
/*!
    \qmlproperty AbstractAxis BoxPlotSeries::axisY
    The y-axis used for the series. If you leave both axisY and axisYRight undefined, a
    ValueAxis is created for the series.
    \sa axisYRight
*/
/*!
    \qmlproperty AbstractAxis BoxPlotSeries::axisXTop
    The x-axis used for the series, drawn on top of the chart view.

    \note You can only provide either axisX or axisXTop, but not both.
    \sa axisX

    \sa axisX
*/
/*!
    \qmlproperty AbstractAxis BoxPlotSeries::axisYRight
    The y-axis used for the series, drawn to the right on the chart view.

    \note You can only provide either axisY or axisYRight, but not both.
    \sa axisY
*/
/*!
    \qmlproperty bool BoxPlotSeries::boxOutlineVisible
    The visibility of the box outline.
*/
/*!
    \qmlproperty real BoxPlotSeries::boxWidth
    \brief The width of the box-and-whiskers item. The value indicates the relative
    width of the item within its category. The value can be between 0.0 and 1.0. Negative values
    are replaced with 0.0 and values greater than 1.0 are replaced with 1.0.
*/
/*!
    \qmlproperty Pen BoxPlotSeries::pen
    The pen used to draw the lines of the box-and-whiskers items.
*/
/*!
    \qmlproperty Brush BoxPlotSeries::brush
    The brush used to fill the boxes of the box-and-whiskers items.
*/

/*!
    \qmlproperty string BoxPlotSeries::brushFilename
    The name of the file used as a brush for the series.
*/

/*!
    \qmlproperty int BoxPlotSeries::count
    The number of box-and-whiskers items in a box plot series.
*/
/*!
    \qmlsignal BoxPlotSeries::onBoxOutlineVisibilityChanged()
    This signal is emitted when the box outline visibility changes.
 */
/*!
    \qmlsignal BoxPlotSeries::onBoxWidthChanged()
    This signal is emitted when the width of the box-and-whiskers item changes.
 */
/*!
    \qmlsignal BoxPlotSeries::onPenChanged()
    This signal is emitted when the pen used to draw the lines of the box-and-whiskers
    items changes.
*/
/*!
    \qmlsignal BoxPlotSeries::onBrushChanged()
    This signal is emitted when the brush used to fill the boxes of the box-and-whiskers
    items changes.
*/
/*!
    \qmlsignal BoxPlotSeries::onAxisXChanged(AbstractAxis axis)
    This signal is emitted when the x-axis changes to \a axis.
*/
/*!
    \qmlsignal BoxPlotSeries::onAxisYChanged(AbstractAxis axis)
    This signal is emitted when the y-axis changes to \a axis.
*/
/*!
    \qmlsignal BoxPlotSeries::onAxisXTopChanged(AbstractAxis axis)
    This signal is emitted when the top x-axis changes to \a axis.
*/
/*!
    \qmlsignal BoxPlotSeries::onAxisYRightChanged(AbstractAxis axis)
    This signal is emitted when the right y-axis changes to \a axis.
*/


DeclarativeBoxSet::DeclarativeBoxSet(const QString label, QObject *parent)
    : QBoxSet(label, parent)
{
    connect(this, SIGNAL(valuesChanged()), this, SIGNAL(changedValues()));
    connect(this, SIGNAL(valueChanged(int)), this, SIGNAL(changedValue(int)));
    connect(this, SIGNAL(brushChanged()), this, SLOT(handleBrushChanged()));
}

QVariantList DeclarativeBoxSet::values()
{
    QVariantList values;
    for (int i(0); i < 5; i++)
        values.append(QVariant(QBoxSet::at(i)));
    return values;
}

void DeclarativeBoxSet::setValues(QVariantList values)
{
    for (int i(0); i < values.count(); i++) {
        if (values.at(i).canConvert(QVariant::Double))
            QBoxSet::append(values[i].toDouble());
    }
}

QString DeclarativeBoxSet::brushFilename() const
{
    return m_brushFilename;
}

void DeclarativeBoxSet::setBrushFilename(const QString &brushFilename)
{
    QImage brushImage(brushFilename);
    if (QBoxSet::brush().textureImage() != brushImage) {
        QBrush brush = QBoxSet::brush();
        brush.setTextureImage(brushImage);
        QBoxSet::setBrush(brush);
        m_brushFilename = brushFilename;
        m_brushImage = brushImage;
        emit brushFilenameChanged(brushFilename);
    }
}

void DeclarativeBoxSet::handleBrushChanged()
{
    // If the texture image of the brush has changed along the brush
    // the brush file name needs to be cleared.
    if (!m_brushFilename.isEmpty() && QBoxSet::brush().textureImage() != m_brushImage) {
        m_brushFilename.clear();
        emit brushFilenameChanged(QString(""));
    }
}

// =====================================================

DeclarativeBoxPlotSeries::DeclarativeBoxPlotSeries(QQuickItem *parent) :
    QBoxPlotSeries(parent),
    m_axes(new DeclarativeAxes(this))
{
    connect(m_axes, SIGNAL(axisXChanged(QAbstractAxis*)), this, SIGNAL(axisXChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYChanged(QAbstractAxis*)), this, SIGNAL(axisYChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisXTopChanged(QAbstractAxis*)), this, SIGNAL(axisXTopChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYRightChanged(QAbstractAxis*)), this, SIGNAL(axisYRightChanged(QAbstractAxis*)));
    connect(this, SIGNAL(hovered(bool, QBoxSet*)), this, SLOT(onHovered(bool, QBoxSet*)));
    connect(this, SIGNAL(clicked(QBoxSet*)), this, SLOT(onClicked(QBoxSet*)));
    connect(this, SIGNAL(brushChanged()), this, SLOT(handleBrushChanged()));
    connect(this, SIGNAL(pressed(QBoxSet*)), this, SLOT(onPressed(QBoxSet*)));
    connect(this, SIGNAL(released(QBoxSet*)), this, SLOT(onReleased(QBoxSet*)));
    connect(this, SIGNAL(doubleClicked(QBoxSet*)), this, SLOT(onDoubleClicked(QBoxSet*)));
}

void DeclarativeBoxPlotSeries::classBegin()
{
}

void DeclarativeBoxPlotSeries::componentComplete()
{
    foreach (QObject *child, children()) {
        if (qobject_cast<DeclarativeBoxSet *>(child)) {
            QBoxPlotSeries::append(qobject_cast<DeclarativeBoxSet *>(child));
        } else if (qobject_cast<QVBoxPlotModelMapper *>(child)) {
            QVBoxPlotModelMapper *mapper = qobject_cast<QVBoxPlotModelMapper *>(child);
            mapper->setSeries(this);
        }
    }
}

QQmlListProperty<QObject> DeclarativeBoxPlotSeries::seriesChildren()
{
    return QQmlListProperty<QObject>(this, 0, &DeclarativeBoxPlotSeries::appendSeriesChildren ,0,0,0);
}

void DeclarativeBoxPlotSeries::appendSeriesChildren(QQmlListProperty<QObject> *list, QObject *element)
{
    // Empty implementation; the children are parsed in componentComplete instead
    Q_UNUSED(list);
    Q_UNUSED(element);
}

DeclarativeBoxSet *DeclarativeBoxPlotSeries::at(int index)
{
    QList<QBoxSet *> setList = boxSets();
    if (index >= 0 && index < setList.count())
        return qobject_cast<DeclarativeBoxSet *>(setList[index]);

    return 0;
}

DeclarativeBoxSet *DeclarativeBoxPlotSeries::insert(int index, const QString label, QVariantList values)
{
    DeclarativeBoxSet *barset = new DeclarativeBoxSet(label, this);
    barset->setValues(values);
    if (QBoxPlotSeries::insert(index, barset))
        return barset;
    delete barset;
    return 0;
}

void DeclarativeBoxPlotSeries::onHovered(bool status, QBoxSet *boxset)
{
    emit hovered(status, qobject_cast<DeclarativeBoxSet *>(boxset));
}

void DeclarativeBoxPlotSeries::onClicked(QBoxSet *boxset)
{
    emit clicked(qobject_cast<DeclarativeBoxSet *>(boxset));
}

void DeclarativeBoxPlotSeries::onPressed(QBoxSet *boxset)
{
    emit pressed(qobject_cast<DeclarativeBoxSet *>(boxset));
}

void DeclarativeBoxPlotSeries::onReleased(QBoxSet *boxset)
{
    emit released(qobject_cast<DeclarativeBoxSet *>(boxset));
}

void DeclarativeBoxPlotSeries::onDoubleClicked(QBoxSet *boxset)
{
    emit doubleClicked(qobject_cast<DeclarativeBoxSet *>(boxset));
}

QString DeclarativeBoxPlotSeries::brushFilename() const
{
    return m_brushFilename;
}

void DeclarativeBoxPlotSeries::setBrushFilename(const QString &brushFilename)
{
    QImage brushImage(brushFilename);
    if (QBoxPlotSeries::brush().textureImage() != brushImage) {
        QBrush brush = QBoxPlotSeries::brush();
        brush.setTextureImage(brushImage);
        QBoxPlotSeries::setBrush(brush);
        m_brushFilename = brushFilename;
        m_brushImage = brushImage;
        emit brushFilenameChanged(brushFilename);
    }
}

void DeclarativeBoxPlotSeries::handleBrushChanged()
{
    // If the texture image of the brush has changed along the brush
    // the brush file name needs to be cleared.
    if (!m_brushFilename.isEmpty() && QBoxPlotSeries::brush().textureImage() != m_brushImage) {
        m_brushFilename.clear();
        emit brushFilenameChanged(QString(""));
    }
}

#include "moc_declarativeboxplotseries.cpp"

QT_CHARTS_END_NAMESPACE
