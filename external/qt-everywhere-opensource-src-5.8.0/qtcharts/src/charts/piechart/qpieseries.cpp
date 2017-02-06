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

#include <QtCharts/QPieSeries>
#include <private/qpieseries_p.h>
#include <QtCharts/QPieSlice>
#include <private/qpieslice_p.h>
#include <private/pieslicedata_p.h>
#include <private/chartdataset_p.h>
#include <private/charttheme_p.h>
#include <QtCharts/QAbstractAxis>
#include <private/pieanimation_p.h>
#include <private/charthelpers_p.h>

#include <QtCharts/QPieLegendMarker>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QPieSeries
    \inmodule Qt Charts
    \brief Pie series API for Qt Charts.

    The pie series defines a pie chart which consists of pie slices which are defined as QPieSlice objects.
    The slices can have any values as the QPieSeries will calculate its relative value to the sum of all slices.
    The actual slice size is determined by that relative value.

    Pie size and position on the chart is controlled by using relative values which range from 0.0 to 1.0.
    These relate to the actual chart rectangle.

    By default the pie is defined as a full pie but it can also be a partial pie.
    This can be done by setting a starting angle and angle span to the series.
    Full pie is 360 degrees where 0 is at 12 a'clock.

    See the \l {PieChart Example} {pie chart example} or \l {DonutChart Example} {donut chart example} to learn how to use QPieSeries.
    \image examples_piechart.png
    \image examples_donutchart.png
*/
/*!
    \qmltype PieSeries
    \instantiates QPieSeries
    \inqmlmodule QtCharts

    \inherits AbstractSeries

    \brief The PieSeries type is used for making pie charts.

    The following QML shows how to create a simple pie chart.

    \snippet qmlchart/qml/qmlchart/View1.qml 1

    \beginfloatleft
    \image examples_qmlchart1.png
    \endfloat
    \clearfloat
*/

/*!
    \property QPieSeries::horizontalPosition
    \brief Defines the horizontal position of the pie.

    The value is a relative value to the chart rectangle where:

    \list
    \li 0.0 is the absolute left.
    \li 1.0 is the absolute right.
    \endlist
    Default value is 0.5 (center).
    \sa verticalPosition
*/

/*!
    \qmlproperty real PieSeries::horizontalPosition

    Defines the horizontal position of the pie.

    The value is a relative value to the chart rectangle where:

    \list
    \li 0.0 is the absolute left.
    \li 1.0 is the absolute right.
    \endlist
    Default value is 0.5 (center).
    \sa verticalPosition
*/

/*!
    \property QPieSeries::verticalPosition
    \brief Defines the vertical position of the pie.

    The value is a relative value to the chart rectangle where:

    \list
    \li 0.0 is the absolute top.
    \li 1.0 is the absolute bottom.
    \endlist
    Default value is 0.5 (center).
    \sa horizontalPosition
*/

/*!
    \qmlproperty real PieSeries::verticalPosition

    Defines the vertical position of the pie.

    The value is a relative value to the chart rectangle where:

    \list
    \li 0.0 is the absolute top.
    \li 1.0 is the absolute bottom.
    \endlist
    Default value is 0.5 (center).
    \sa horizontalPosition
*/

/*!
    \property QPieSeries::size
    \brief Defines the pie size.

    The value is a relative value to the chart rectangle where:

    \list
    \li 0.0 is the minimum size (pie not drawn).
    \li 1.0 is the maximum size that can fit the chart.
    \endlist

    When setting this property the holeSize property is adjusted if necessary, to ensure that the hole size is not greater than the outer size.

    Default value is 0.7.
*/

/*!
    \qmlproperty real PieSeries::size

    Defines the pie size.

    The value is a relative value to the chart rectangle where:

    \list
    \li 0.0 is the minimum size (pie not drawn).
    \li 1.0 is the maximum size that can fit the chart.
    \endlist

    Default value is 0.7.
*/

/*!
    \property QPieSeries::holeSize
    \brief Defines the donut hole size.

    The value is a relative value to the chart rectangle where:

    \list
    \li 0.0 is the minimum size (full pie drawn, without any hole inside).
    \li 1.0 is the maximum size that can fit the chart. (donut has no width)
    \endlist

    The value is never greater then size property.
    Default value is 0.0.
*/

/*!
    \qmlproperty real PieSeries::holeSize

    Defines the donut hole size.

    The value is a relative value to the chart rectangle where:

    \list
    \li 0.0 is the minimum size (full pie drawn, without any hole inside).
    \li 1.0 is the maximum size that can fit the chart. (donut has no width)
    \endlist

    When setting this property the size property is adjusted if necessary, to ensure that the inner size is not greater than the outer size.

    Default value is 0.0.
*/

/*!
    \property QPieSeries::startAngle
    \brief Defines the starting angle of the pie.

    Full pie is 360 degrees where 0 degrees is at 12 a'clock.

    Default is value is 0.
*/

/*!
    \qmlproperty real PieSeries::startAngle

    Defines the starting angle of the pie.

    Full pie is 360 degrees where 0 degrees is at 12 a'clock.

    Default is value is 0.
*/

/*!
    \property QPieSeries::endAngle
    \brief Defines the ending angle of the pie.

    Full pie is 360 degrees where 0 degrees is at 12 a'clock.

    Default is value is 360.
*/

/*!
    \qmlproperty real PieSeries::endAngle

    Defines the ending angle of the pie.

    Full pie is 360 degrees where 0 degrees is at 12 a'clock.

    Default is value is 360.
*/

/*!
    \property QPieSeries::count

    Number of slices in the series.
*/

/*!
    \qmlproperty int PieSeries::count

    Number of slices in the series.
*/

/*!
    \fn void QPieSeries::countChanged()
    Emitted when the slice count has changed.
    \sa count
*/
/*!
    \qmlsignal PieSeries::onCountChanged()
    Emitted when the slice count has changed.
*/

/*!
    \property QPieSeries::sum

    Sum of all slices.

    The series keeps track of the sum of all slices it holds.
*/

/*!
    \qmlproperty real PieSeries::sum

    Sum of all slices.

    The series keeps track of the sum of all slices it holds.
*/

/*!
    \fn void QPieSeries::sumChanged()
    Emitted when the sum of all slices has changed.
    \sa sum
*/
/*!
    \qmlsignal PieSeries::onSumChanged()
    Emitted when the sum of all slices has changed. This may happen for example if you add or remove slices, or if you
    change value of a slice.
*/

/*!
    \fn void QPieSeries::added(QList<QPieSlice*> slices)

    This signal is emitted when \a slices have been added to the series.

    \sa append(), insert()
*/
/*!
    \qmlsignal PieSeries::onAdded(list<PieSlice> slices)
    Emitted when \a slices have been added to the series.
*/

/*!
    \fn void QPieSeries::removed(QList<QPieSlice*> slices)
    This signal is emitted when \a slices have been removed from the series.
    \sa remove()
*/
/*!
    \qmlsignal PieSeries::onRemoved(list<PieSlice> slices)
    Emitted when \a slices have been removed from the series.
*/

/*!
    \qmlsignal PieSeries::onSliceAdded(PieSlice slice)
    Emitted when \a slice has been added to the series.
*/

/*!
    \qmlsignal PieSeries::onSliceRemoved(PieSlice slice)
    Emitted when \a slice has been removed from the series.
*/

/*!
    \fn void QPieSeries::clicked(QPieSlice *slice)
    This signal is emitted when a \a slice has been clicked.
    \sa QPieSlice::clicked()
*/
/*!
  \qmlsignal PieSeries::onClicked(PieSlice slice)
  This signal is emitted when a \a slice has been clicked.
*/

/*!
    \fn void QPieSeries::pressed(QPieSlice *slice)
    This signal is emitted when a \a slice has been pressed.
    \sa QPieSlice::pressed()
*/
/*!
    \qmlsignal PieSeries::onPressed(PieSlice slice)
    This signal is emitted when a \a slice has been pressed.
*/

/*!
    \fn void QPieSeries::released(QPieSlice *slice)
    This signal is emitted when a \a slice has been released.
    \sa QPieSlice::released()
*/
/*!
    \qmlsignal PieSeries::onReleased(PieSlice slice)
    This signal is emitted when a \a slice has been released.
*/

/*!
    \fn void QPieSeries::doubleClicked(QPieSlice *slice)
    This signal is emitted when a \a slice has been doubleClicked.
    \sa QPieSlice::doubleClicked()
*/
/*!
    \qmlsignal PieSeries::onDoubleClicked(PieSlice slice)
    This signal is emitted when a \a slice has been doubleClicked.
*/

/*!
    \fn void QPieSeries::hovered(QPieSlice* slice, bool state)
    This signal is emitted when user has hovered over or away from the \a slice.
    \a state is true when user has hovered over the slice and false when hover has moved away from the slice.
    \sa QPieSlice::hovered()
*/
/*!
    \qmlsignal PieSeries::onHovered(PieSlice slice, bool state)
    This signal is emitted when user has hovered over or away from the \a slice. \a state is true when user has hovered
    over the slice and false when hover has moved away from the slice.
*/

/*!
    \qmlmethod PieSlice PieSeries::at(int index)
    Returns slice at \a index. Returns null if the index is not valid.
*/

/*!
    \qmlmethod PieSlice PieSeries::find(string label)
    Returns the first slice with \a label. Returns null if the index is not valid.
*/

/*!
    \qmlmethod PieSlice PieSeries::append(string label, real value)
    Adds a new slice with \a label and \a value to the pie.
*/

/*!
    \qmlmethod bool PieSeries::remove(PieSlice slice)
    Removes the \a slice from the pie. Returns true if the removal was successful, false otherwise.
*/

/*!
    \qmlmethod PieSeries::clear()
    Removes all slices from the pie.
*/

/*!
    Constructs a series object which is a child of \a parent.
*/
QPieSeries::QPieSeries(QObject *parent)
    : QAbstractSeries(*new QPieSeriesPrivate(this), parent)
{
    Q_D(QPieSeries);
    QObject::connect(this, SIGNAL(countChanged()), d, SIGNAL(countChanged()));
}

/*!
    Destroys the series and its slices.
*/
QPieSeries::~QPieSeries()
{
    // NOTE: d_prt destroyed by QObject
    clear();
}

/*!
    Returns QAbstractSeries::SeriesTypePie.
*/
QAbstractSeries::SeriesType QPieSeries::type() const
{
    return QAbstractSeries::SeriesTypePie;
}

/*!
    Appends a single \a slice to the series.
    Slice ownership is passed to the series.

    Returns true if append was succesfull.
*/
bool QPieSeries::append(QPieSlice *slice)
{
    return append(QList<QPieSlice *>() << slice);
}

/*!
    Appends an array of \a slices to the series.
    Slice ownership is passed to the series.

    Returns true if append was successful.
*/
bool QPieSeries::append(QList<QPieSlice *> slices)
{
    Q_D(QPieSeries);

    if (slices.count() == 0)
        return false;

    foreach (QPieSlice *s, slices) {
        if (!s || d->m_slices.contains(s))
            return false;
        if (s->series()) // already added to some series
            return false;
        if (!isValidValue(s->value()))
            return false;
    }

    foreach (QPieSlice *s, slices) {
        s->setParent(this);
        QPieSlicePrivate::fromSlice(s)->m_series = this;
        d->m_slices << s;
    }

    d->updateDerivativeData();

    foreach(QPieSlice * s, slices) {
        connect(s, SIGNAL(valueChanged()), d, SLOT(sliceValueChanged()));
        connect(s, SIGNAL(clicked()), d, SLOT(sliceClicked()));
        connect(s, SIGNAL(hovered(bool)), d, SLOT(sliceHovered(bool)));
        connect(s, SIGNAL(pressed()), d, SLOT(slicePressed()));
        connect(s, SIGNAL(released()), d, SLOT(sliceReleased()));
        connect(s, SIGNAL(doubleClicked()), d, SLOT(sliceDoubleClicked()));
    }

    emit added(slices);
    emit countChanged();

    return true;
}

/*!
    Appends a single \a slice to the series and returns a reference to the series.
    Slice ownership is passed to the series.
*/
QPieSeries &QPieSeries::operator << (QPieSlice *slice)
{
    append(slice);
    return *this;
}


/*!
    Appends a single slice to the series with give \a value and \a label.
    Slice ownership is passed to the series.
    Returns NULL if value is NaN, Inf or -Inf and no slice is added to the series.
*/
QPieSlice *QPieSeries::append(QString label, qreal value)
{
    if (isValidValue(value)) {
        QPieSlice *slice = new QPieSlice(label, value);
        append(slice);
        return slice;
    } else {
        return 0;
    }
}

/*!
    Inserts a single \a slice to the series before the slice at \a index position.
    Slice ownership is passed to the series.

    Returns true if insert was successful.
*/
bool QPieSeries::insert(int index, QPieSlice *slice)
{
    Q_D(QPieSeries);

    if (index < 0 || index > d->m_slices.count())
        return false;

    if (!slice || d->m_slices.contains(slice))
        return false;

    if (slice->series()) // already added to some series
        return false;

    if (!isValidValue(slice->value()))
        return false;

    slice->setParent(this);
    QPieSlicePrivate::fromSlice(slice)->m_series = this;
    d->m_slices.insert(index, slice);

    d->updateDerivativeData();

    connect(slice, SIGNAL(valueChanged()), d, SLOT(sliceValueChanged()));
    connect(slice, SIGNAL(clicked()), d, SLOT(sliceClicked()));
    connect(slice, SIGNAL(hovered(bool)), d, SLOT(sliceHovered(bool)));
    connect(slice, SIGNAL(pressed()), d, SLOT(slicePressed()));
    connect(slice, SIGNAL(released()), d, SLOT(sliceReleased()));
    connect(slice, SIGNAL(doubleClicked()), d, SLOT(sliceDoubleClicked()));

    emit added(QList<QPieSlice *>() << slice);
    emit countChanged();

    return true;
}

/*!
    Removes a single \a slice from the series and deletes the slice.

    Do not reference the pointer after this call.

    Returns true if remove was successful.
*/
bool QPieSeries::remove(QPieSlice *slice)
{
    Q_D(QPieSeries);

    if (!d->m_slices.removeOne(slice))
        return false;

    d->updateDerivativeData();

    emit removed(QList<QPieSlice *>() << slice);
    emit countChanged();

    delete slice;
    slice = 0;

    return true;
}

/*!
    Takes a single \a slice from the series. Does not destroy the slice object.

    \note The series remains as the slice's parent object. You must set the
    parent object to take full ownership.

    Returns true if take was successful.
*/
bool QPieSeries::take(QPieSlice *slice)
{
    Q_D(QPieSeries);

    if (!d->m_slices.removeOne(slice))
        return false;

    QPieSlicePrivate::fromSlice(slice)->m_series = 0;
    slice->disconnect(d);

    d->updateDerivativeData();

    emit removed(QList<QPieSlice *>() << slice);
    emit countChanged();

    return true;
}

/*!
    Clears all slices from the series.
*/
void QPieSeries::clear()
{
    Q_D(QPieSeries);
    if (d->m_slices.count() == 0)
        return;

    QList<QPieSlice *> slices = d->m_slices;
    foreach (QPieSlice *s, d->m_slices)
        d->m_slices.removeOne(s);

    d->updateDerivativeData();

    emit removed(slices);
    emit countChanged();

    foreach (QPieSlice *s, slices)
        delete s;
}

/*!
    Returns a list of slices that belong to this series.
*/
QList<QPieSlice *> QPieSeries::slices() const
{
    Q_D(const QPieSeries);
    return d->m_slices;
}

/*!
    returns the number of the slices in this series.
*/
int QPieSeries::count() const
{
    Q_D(const QPieSeries);
    return d->m_slices.count();
}

/*!
    Returns true is the series is empty.
*/
bool QPieSeries::isEmpty() const
{
    Q_D(const QPieSeries);
    return d->m_slices.isEmpty();
}

/*!
    Returns the sum of all slice values in this series.

    \sa QPieSlice::value(), QPieSlice::setValue(), QPieSlice::percentage()
*/
qreal QPieSeries::sum() const
{
    Q_D(const QPieSeries);
    return d->m_sum;
}

void QPieSeries::setHoleSize(qreal holeSize)
{
    Q_D(QPieSeries);
    holeSize = qBound((qreal)0.0, holeSize, (qreal)1.0);
    d->setSizes(holeSize, qMax(d->m_pieRelativeSize, holeSize));
}

qreal QPieSeries::holeSize() const
{
    Q_D(const QPieSeries);
    return d->m_holeRelativeSize;
}

void QPieSeries::setHorizontalPosition(qreal relativePosition)
{
    Q_D(QPieSeries);

    if (relativePosition < 0.0)
        relativePosition = 0.0;
    if (relativePosition > 1.0)
        relativePosition = 1.0;

    if (!qFuzzyCompare(d->m_pieRelativeHorPos, relativePosition)) {
        d->m_pieRelativeHorPos = relativePosition;
        emit d->horizontalPositionChanged();
    }
}

qreal QPieSeries::horizontalPosition() const
{
    Q_D(const QPieSeries);
    return d->m_pieRelativeHorPos;
}

void QPieSeries::setVerticalPosition(qreal relativePosition)
{
    Q_D(QPieSeries);

    if (relativePosition < 0.0)
        relativePosition = 0.0;
    if (relativePosition > 1.0)
        relativePosition = 1.0;

    if (!qFuzzyCompare(d->m_pieRelativeVerPos, relativePosition)) {
        d->m_pieRelativeVerPos = relativePosition;
        emit d->verticalPositionChanged();
    }
}

qreal QPieSeries::verticalPosition() const
{
    Q_D(const QPieSeries);
    return d->m_pieRelativeVerPos;
}

void QPieSeries::setPieSize(qreal relativeSize)
{
    Q_D(QPieSeries);
    relativeSize = qBound((qreal)0.0, relativeSize, (qreal)1.0);
    d->setSizes(qMin(d->m_holeRelativeSize, relativeSize), relativeSize);

}

qreal QPieSeries::pieSize() const
{
    Q_D(const QPieSeries);
    return d->m_pieRelativeSize;
}


void QPieSeries::setPieStartAngle(qreal angle)
{
    Q_D(QPieSeries);
    if (qFuzzyCompare(d->m_pieStartAngle, angle))
        return;
    d->m_pieStartAngle = angle;
    d->updateDerivativeData();
    emit d->pieStartAngleChanged();
}

qreal QPieSeries::pieStartAngle() const
{
    Q_D(const QPieSeries);
    return d->m_pieStartAngle;
}

/*!
    Sets the end angle of the pie.

    Full pie is 360 degrees where 0 degrees is at 12 a'clock.

    \a angle must be greater than start angle.

    \sa pieEndAngle(), pieStartAngle(), setPieStartAngle()
*/
void QPieSeries::setPieEndAngle(qreal angle)
{
    Q_D(QPieSeries);
    if (qFuzzyCompare(d->m_pieEndAngle, angle))
        return;
    d->m_pieEndAngle = angle;
    d->updateDerivativeData();
    emit d->pieEndAngleChanged();
}

/*!
    Returns the end angle of the pie.

    Full pie is 360 degrees where 0 degrees is at 12 a'clock.

    \sa setPieEndAngle(), pieStartAngle(), setPieStartAngle()
*/
qreal QPieSeries::pieEndAngle() const
{
    Q_D(const QPieSeries);
    return d->m_pieEndAngle;
}

/*!
    Sets the all the slice labels \a visible or invisible.

    Note that this affects only the current slices in the series.
    If user adds a new slice the default label visibility is false.

    \sa QPieSlice::isLabelVisible(), QPieSlice::setLabelVisible()
*/
void QPieSeries::setLabelsVisible(bool visible)
{
    Q_D(QPieSeries);
    foreach (QPieSlice *s, d->m_slices)
        s->setLabelVisible(visible);
}

/*!
    Sets the all the slice labels \a position

    Note that this affects only the current slices in the series.
    If user adds a new slice the default label position is LabelOutside

    \sa QPieSlice::labelPosition(), QPieSlice::setLabelPosition()
*/
void QPieSeries::setLabelsPosition(QPieSlice::LabelPosition position)
{
    Q_D(QPieSeries);
    foreach (QPieSlice *s, d->m_slices)
        s->setLabelPosition(position);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


QPieSeriesPrivate::QPieSeriesPrivate(QPieSeries *parent) :
    QAbstractSeriesPrivate(parent),
    m_pieRelativeHorPos(0.5),
    m_pieRelativeVerPos(0.5),
    m_pieRelativeSize(0.7),
    m_pieStartAngle(0),
    m_pieEndAngle(360),
    m_sum(0),
    m_holeRelativeSize(0.0)
{
}

QPieSeriesPrivate::~QPieSeriesPrivate()
{
}

void QPieSeriesPrivate::updateDerivativeData()
{
    // calculate sum of all slices
    qreal sum = 0;
    foreach (QPieSlice *s, m_slices)
        sum += s->value();

    if (!qFuzzyCompare(m_sum, sum)) {
        m_sum = sum;
        emit q_func()->sumChanged();
    }

    // nothing to show..
    if (qFuzzyCompare(m_sum, 0))
        return;

    // update slice attributes
    qreal sliceAngle = m_pieStartAngle;
    qreal pieSpan = m_pieEndAngle - m_pieStartAngle;
    QVector<QPieSlice *> changed;
    foreach (QPieSlice *s, m_slices) {
        QPieSlicePrivate *d = QPieSlicePrivate::fromSlice(s);
        d->setPercentage(s->value() / m_sum);
        d->setStartAngle(sliceAngle);
        d->setAngleSpan(pieSpan * s->percentage());
        sliceAngle += s->angleSpan();
    }


    emit calculatedDataChanged();
}

void QPieSeriesPrivate::setSizes(qreal innerSize, qreal outerSize)
{
    bool changed = false;

    if (!qFuzzyCompare(m_holeRelativeSize, innerSize)) {
        m_holeRelativeSize = innerSize;
        changed = true;
    }

    if (!qFuzzyCompare(m_pieRelativeSize, outerSize)) {
        m_pieRelativeSize = outerSize;
        changed = true;
    }

    if (changed)
        emit pieSizeChanged();
}

QPieSeriesPrivate *QPieSeriesPrivate::fromSeries(QPieSeries *series)
{
    return series->d_func();
}

void QPieSeriesPrivate::sliceValueChanged()
{
    Q_ASSERT(m_slices.contains(qobject_cast<QPieSlice *>(sender())));
    updateDerivativeData();
}

void QPieSeriesPrivate::sliceClicked()
{
    QPieSlice *slice = qobject_cast<QPieSlice *>(sender());
    Q_ASSERT(m_slices.contains(slice));
    Q_Q(QPieSeries);
    emit q->clicked(slice);
}

void QPieSeriesPrivate::sliceHovered(bool state)
{
    QPieSlice *slice = qobject_cast<QPieSlice *>(sender());
    if (!m_slices.isEmpty()) {
        Q_ASSERT(m_slices.contains(slice));
        Q_Q(QPieSeries);
        emit q->hovered(slice, state);
    }
}

void QPieSeriesPrivate::slicePressed()
{
    QPieSlice *slice = qobject_cast<QPieSlice *>(sender());
    Q_ASSERT(m_slices.contains(slice));
    Q_Q(QPieSeries);
    emit q->pressed(slice);
}

void QPieSeriesPrivate::sliceReleased()
{
    QPieSlice *slice = qobject_cast<QPieSlice *>(sender());
    Q_ASSERT(m_slices.contains(slice));
    Q_Q(QPieSeries);
    emit q->released(slice);
}

void QPieSeriesPrivate::sliceDoubleClicked()
{
    QPieSlice *slice = qobject_cast<QPieSlice *>(sender());
    Q_ASSERT(m_slices.contains(slice));
    Q_Q(QPieSeries);
    emit q->doubleClicked(slice);
}

void QPieSeriesPrivate::initializeDomain()
{
    // does not apply to pie
}

void QPieSeriesPrivate::initializeGraphics(QGraphicsItem* parent)
{
    Q_Q(QPieSeries);
    PieChartItem *pie = new PieChartItem(q,parent);
    m_item.reset(pie);
    QAbstractSeriesPrivate::initializeGraphics(parent);
}

void QPieSeriesPrivate::initializeAnimations(QtCharts::QChart::AnimationOptions options,
                                             int duration, QEasingCurve &curve)
{
    PieChartItem *item = static_cast<PieChartItem *>(m_item.data());
    Q_ASSERT(item);
    if (item->animation())
        item->animation()->stopAndDestroyLater();

    if (options.testFlag(QChart::SeriesAnimations))
        item->setAnimation(new PieAnimation(item, duration, curve));
    else
        item->setAnimation(0);
    QAbstractSeriesPrivate::initializeAnimations(options, duration, curve);
}

QList<QLegendMarker*> QPieSeriesPrivate::createLegendMarkers(QLegend* legend)
{
    Q_Q(QPieSeries);
    QList<QLegendMarker*> markers;
    foreach(QPieSlice* slice, q->slices()) {
        QPieLegendMarker* marker = new QPieLegendMarker(q,slice,legend);
        markers << marker;
    }
    return markers;
}

void QPieSeriesPrivate::initializeAxes()
{

}

QAbstractAxis::AxisType QPieSeriesPrivate::defaultAxisType(Qt::Orientation orientation) const
{
    Q_UNUSED(orientation);
    return QAbstractAxis::AxisTypeNoAxis;
}

QAbstractAxis* QPieSeriesPrivate::createDefaultAxis(Qt::Orientation orientation) const
{
    Q_UNUSED(orientation);
    return 0;
}

void QPieSeriesPrivate::initializeTheme(int index, ChartTheme* theme, bool forced)
{
    //Q_Q(QPieSeries);
    //const QList<QColor>& colors = theme->seriesColors();
    const QList<QGradient>& gradients = theme->seriesGradients();

    for (int i(0); i < m_slices.count(); i++) {

        QColor penColor = ChartThemeManager::colorAt(gradients.at(index % gradients.size()), 0.0);

        // Get color for a slice from a gradient linearly, beginning from the start of the gradient
        qreal pos = (qreal)(i + 1) / (qreal) m_slices.count();
        QColor brushColor = ChartThemeManager::colorAt(gradients.at(index % gradients.size()), pos);

        QPieSlice *s = m_slices.at(i);
        QPieSlicePrivate *d = QPieSlicePrivate::fromSlice(s);

        if (forced || d->m_data.m_slicePen.isThemed())
            d->setPen(penColor, true);

        if (forced || d->m_data.m_sliceBrush.isThemed())
            d->setBrush(brushColor, true);

        if (forced || d->m_data.m_labelBrush.isThemed())
            d->setLabelBrush(theme->labelBrush().color(), true);

        if (forced || d->m_data.m_labelFont.isThemed())
            d->setLabelFont(theme->labelFont(), true);
    }
}


#include "moc_qpieseries.cpp"
#include "moc_qpieseries_p.cpp"

QT_CHARTS_END_NAMESPACE
