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

#include <QtCharts/QBarCategoryAxis>
#include <private/qbarcategoryaxis_p.h>
#include <private/chartbarcategoryaxisx_p.h>
#include <private/chartbarcategoryaxisy_p.h>
#include <private/abstractdomain_p.h>
#include <QtCharts/QChart>
#include <QtCore/QtMath>

QT_CHARTS_BEGIN_NAMESPACE
/*!
    \class QBarCategoryAxis
    \inmodule Qt Charts
    \brief The QBarCategoryAxis class adds categories to a chart's axes.

    QBarCategoryAxis can be set up to show an axis line with tick marks, grid lines, and shades.
    Categories are drawn between the ticks. It can be used also with a line series, as demonstrated
    by the \l {Line and BarChart Example} {Line and BarChart Example}.

    The following code illustrates how to use QBarCategoryAxis:
    \code
    QChartView *chartView = new QChartView;
    QBarSeries *series = new QBarSeries;
    // ...
    chartView->chart()->addSeries(series);
    chartView->chart()->createDefaultAxes();

    QBarCategoryAxis *axisX = new QBarCategoryAxis;
    QStringList categories;
    categories << "Jan" << "Feb" << "Mar" << "Apr" << "May" << "Jun";
    axisX->append(categories);
    axisX->setRange("Feb", "May");
    chartView->chart()->setAxisX(axisX, series);
    \endcode
*/

/*!
    \qmltype BarCategoryAxis
    \instantiates QBarCategoryAxis
    \inqmlmodule QtCharts

    \inherits AbstractAxis

    \brief Adds categories to a chart's axes.

    The BarCategoryAxis type can be set up to show an axis line with tick marks, grid lines, and
    shades. Categories are drawn between the ticks. It can be used also with a line series.

    The following QML snippet illustrates how to use BarCategoryAxis:
    \code
        ChartView {
            BarCategoryAxis {
                id: categoryAxis
                categories: ["Jan", "Feb", "Mar", "Apr", "May", "Jun" ]
            }
        // Add a few series...
        }
    \endcode
*/

/*!
  \property QBarCategoryAxis::categories
  \brief The categories of an axis.
*/
/*!
  \qmlproperty QStringList BarCategoryAxis::categories
  The categories of an axis.
*/

/*!
  \property QBarCategoryAxis::min
  \brief The minimum value on the axis.
*/
/*!
  \qmlproperty string BarCategoryAxis::min
  The minimum value on the axis.
*/

/*!
  \property QBarCategoryAxis::max
  \brief The maximum value on the axis.
*/
/*!
  \qmlproperty string BarCategoryAxis::max
  The maximum value on the axis.
*/

/*!
  \property QBarCategoryAxis::count
  \brief The number of categories of an axis.
*/
/*!
  \qmlproperty int BarCategoryAxis::count
  The number of categories of an axis.
*/

/*!
  \fn void QBarCategoryAxis::categoriesChanged()
  This signal is emitted when the categories of the axis change.
*/

/*!
  \fn void QBarCategoryAxis::minChanged(const QString &min)
  This signal is emitted when the \a min value of the axis changes.
*/

/*!
  \fn void QBarCategoryAxis::maxChanged(const QString &max)
  This signal is emitted when the \a max value of the axis changes.
*/

/*!
  \fn void QBarCategoryAxis::countChanged()
  This signal is emitted when the number of categories of an axis changes.
*/

/*!
  \fn void QBarCategoryAxis::rangeChanged(const QString &min, const QString &max)
  This signal is emitted when \a min or \a max value of the axis changes.
*/

/*!
  \qmlsignal BarCategoryAxis::rangeChanged(string min, string max)
  This signal is emitted when \a min or \a max value of the axis changes.

  The corresponding signal handler is \c onRangeChanged.
*/

/*!
  \qmlmethod void BarCategoryAxis::clear()
  Removes all categories. Sets the maximum and minimum values of the axis range to QString::null.
*/

/*!
    Constructs an axis object that is the child of \a parent.
*/
QBarCategoryAxis::QBarCategoryAxis(QObject *parent):
    QAbstractAxis(*new QBarCategoryAxisPrivate(this), parent)
{
}

/*!
    Destroys the axis object.
*/
QBarCategoryAxis::~QBarCategoryAxis()
{
    Q_D(QBarCategoryAxis);
    if (d->m_chart)
        d->m_chart->removeAxis(this);
}

/*!
    \internal
*/
QBarCategoryAxis::QBarCategoryAxis(QBarCategoryAxisPrivate &d, QObject *parent)
    : QAbstractAxis(d, parent)
{

}

/*!
    Appends \a categories to an axis. The maximum value on the axis will be changed
    to match the last category in \a categories. If no categories were previously defined,
    the minimum value on the axis will also be changed to match the first category in
    \a categories.

    A category has to be a valid QString and it cannot be duplicated. Duplicated
    categories will not be appended.
*/
void QBarCategoryAxis::append(const QStringList &categories)
{
    if (categories.isEmpty())
        return;

    Q_D(QBarCategoryAxis);

    int count = d->m_categories.count();

    foreach(QString category, categories) {
        if (!d->m_categories.contains(category) && !category.isNull()) {
            d->m_categories.append(category);
        }
    }

    if (d->m_categories.count() == count)
        return;

    if (count == 0)
        setRange(d->m_categories.first(), d->m_categories.last());
    else
        setRange(d->m_minCategory, d->m_categories.last());

    emit categoriesChanged();
    emit countChanged();
}

/*!
    Appends \a category to an axis. The maximum value on the axis will be changed
    to match the last \a  category. If no categories were previously defined, the minimum
    value on the axis will also be changed to match \a category.

    A category has to be a valid QString and it cannot be duplicated. Duplicated
    categories will not be appended.
*/
void QBarCategoryAxis::append(const QString &category)
{
    Q_D(QBarCategoryAxis);

    int count = d->m_categories.count();

    if (!d->m_categories.contains(category) && !category.isNull())
        d->m_categories.append(category);

    if (d->m_categories.count() == count)
        return;

    if (count == 0)
        setRange(d->m_categories.last(), d->m_categories.last());
    else
        setRange(d->m_minCategory, d->m_categories.last());

    emit categoriesChanged();
    emit countChanged();
}

/*!
    Removes \a category from the axis. Removing a category that currently sets the
    maximum or minimum value on the axis will affect the axis range.
*/
void QBarCategoryAxis::remove(const QString &category)
{
    Q_D(QBarCategoryAxis);

    if (d->m_categories.contains(category)) {
        d->m_categories.removeAt(d->m_categories.indexOf(category));
        if (!d->m_categories.isEmpty()) {
            if (d->m_minCategory == category) {
                setRange(d->m_categories.first(), d->m_maxCategory);
            } else if (d->m_maxCategory == category) {
                setRange(d->m_minCategory, d->m_categories.last());
            } else {
                d->updateCategoryDomain();
            }
        } else {
            setRange(QString::null, QString::null);
        }
        emit categoriesChanged();
        emit countChanged();
    }
}

/*!
    Inserts \a category to the axis at \a index. \a category has to be a valid QString
    and it cannot be duplicated. If \a category is prepended or appended to other
    categories, the minimum and maximum values on the axis are updated accordingly.
*/
void QBarCategoryAxis::insert(int index, const QString &category)
{
    Q_D(QBarCategoryAxis);

    int count = d->m_categories.count();

    if (!d->m_categories.contains(category) && !category.isNull())
        d->m_categories.insert(index, category);

    if (d->m_categories.count() == count)
        return;

    if (count == 0) {
        setRange(d->m_categories.first(), d->m_categories.first());
    } else if (index == 0) {
        setRange(d->m_categories.first(), d->m_maxCategory);
    } else if (index == count) {
        setRange(d->m_minCategory, d->m_categories.last());
    } else {
        d->updateCategoryDomain();
    }

    emit categoriesChanged();
    emit countChanged();
}

/*!
    Replaces \a oldCategory with \a newCategory. If \a oldCategory does not exist on the axis,
    nothing is done. \a newCategory has to be a valid QString and it cannot be duplicated. If
    the minimum or maximum category is replaced, the minimum and maximum values on the axis are
    updated accordingly.
*/
void QBarCategoryAxis::replace(const QString &oldCategory, const QString &newCategory)
{
    Q_D(QBarCategoryAxis);

    int pos = d->m_categories.indexOf(oldCategory);

    if (pos != -1 && !d->m_categories.contains(newCategory) && !newCategory.isNull()) {
        d->m_categories.replace(pos, newCategory);
        if (d->m_minCategory == oldCategory)
            setRange(newCategory, d->m_maxCategory);
        else if (d->m_maxCategory == oldCategory)
            setRange(d->m_minCategory, newCategory);

        emit categoriesChanged();
        emit countChanged();
    }
}

/*!
    Removes all categories. Sets the maximum and minimum values of the axis range to QString::null.
 */
void QBarCategoryAxis::clear()
{
    Q_D(QBarCategoryAxis);
    d->m_categories.clear();
    setRange(QString::null, QString::null);
    emit categoriesChanged();
    emit countChanged();
}

/*!
    Sets \a categories and discards the old ones. The axis range is adjusted to match the
    first and last category in \a categories.

    A category has to be a valid QString and it cannot be duplicated.
*/
void QBarCategoryAxis::setCategories(const QStringList &categories)
{
    Q_D(QBarCategoryAxis);
    d->m_categories.clear();
    d->m_minCategory = QString::null;
    d->m_maxCategory = QString::null;
    d->m_min = 0;
    d->m_max = 0;
    d->m_count = 0;
    append(categories);
}

/*!
    Returns categories.
*/
QStringList QBarCategoryAxis::categories()
{
    Q_D(QBarCategoryAxis);
    return d->m_categories;
}

/*!
    Returns the number of categories.
 */
int QBarCategoryAxis::count() const
{
    Q_D(const QBarCategoryAxis);
    return d->m_categories.count();
}

/*!
    Returns the category at \a index. The index must be valid.
*/
QString QBarCategoryAxis::at(int index) const
{
    Q_D(const QBarCategoryAxis);
    return d->m_categories.at(index);
}

/*!
    Sets the minimum category to \a min.
*/
void QBarCategoryAxis::setMin(const QString &min)
{
    Q_D(QBarCategoryAxis);
    d->setRange(min, d->m_maxCategory);
}

/*!
    Returns the minimum category.
*/
QString QBarCategoryAxis::min() const
{
    Q_D(const QBarCategoryAxis);
    return d->m_minCategory;
}

/*!
    Sets the maximum category to \a max.
*/
void QBarCategoryAxis::setMax(const QString &max)
{
    Q_D(QBarCategoryAxis);
    d->setRange(d->m_minCategory, max);
}

/*!
    Returns the maximum category.
*/
QString QBarCategoryAxis::max() const
{
    Q_D(const QBarCategoryAxis);
    return d->m_maxCategory;
}

/*!
    Sets the axis range from \a minCategory to \a maxCategory.
*/
void QBarCategoryAxis::setRange(const QString &minCategory, const QString &maxCategory)
{
    Q_D(QBarCategoryAxis);
    d->setRange(minCategory,maxCategory);
}

/*!
    Returns the type of the axis.
*/
QAbstractAxis::AxisType QBarCategoryAxis::type() const
{
    return AxisTypeBarCategory;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QBarCategoryAxisPrivate::QBarCategoryAxisPrivate(QBarCategoryAxis *q)
    : QAbstractAxisPrivate(q),
      m_min(0.0),
      m_max(0.0),
      m_count(0)
{

}

QBarCategoryAxisPrivate::~QBarCategoryAxisPrivate()
{

}

void QBarCategoryAxisPrivate::setMin(const QVariant &min)
{
    setRange(min, m_maxCategory);
}

void QBarCategoryAxisPrivate::setMax(const QVariant &max)
{
    setRange(m_minCategory, max);
}

void QBarCategoryAxisPrivate::setRange(const QVariant &min, const QVariant &max)
{
    QString value1 = min.toString();
    QString value2 = max.toString();
    setRange(value1, value2);
}

void QBarCategoryAxisPrivate::setRange(qreal min, qreal max)
{
    Q_Q(QBarCategoryAxis);

    bool categoryChanged = false;
    bool changed = false;

    if (min > max)
        return;

    if (!qFuzzyIsNull(m_min - min)) {
        m_min = min;
        changed = true;

        int imin = m_min + 0.5;
        if (imin >= 0 && imin < m_categories.count()) {
            QString minCategory = m_categories.at(imin);
            if (m_minCategory != minCategory && !minCategory.isEmpty()) {
                m_minCategory = minCategory;
                categoryChanged = true;
                emit q->minChanged(minCategory);
            }
        }

    }

    if (!qFuzzyIsNull(m_max - max)) {
        m_max = max;
        changed = true;

        int imax = m_max - 0.5;
        if (imax >= 0 && imax < m_categories.count()) {
            QString maxCategory = m_categories.at(imax);
            if (m_maxCategory != maxCategory && !maxCategory.isEmpty()) {
                m_maxCategory = maxCategory;
                categoryChanged = true;
                emit q->maxChanged(maxCategory);
            }
        }
    }

    if (categoryChanged){
        emit q->rangeChanged(m_minCategory, m_maxCategory);
    }

    if (changed) {
        emit rangeChanged(m_min,m_max);
    }
}

void  QBarCategoryAxisPrivate::setRange(const QString &minCategory, const QString &maxCategory)
{
	Q_Q(QBarCategoryAxis);
    bool changed = false;

    //special case in case or clearing all categories
    if (minCategory.isNull() && maxCategory.isNull()) {
        m_minCategory = minCategory;
        m_maxCategory = maxCategory;
        m_min = 0;
        m_max = 0;
        m_count = 0;
        emit q->minChanged(minCategory);
        emit q->maxChanged(maxCategory);
        emit q->rangeChanged(m_minCategory, m_maxCategory);
        emit rangeChanged(m_min,m_max);
        return;
    }

    if (m_categories.indexOf(maxCategory) < m_categories.indexOf(minCategory))
        return;

    if (!minCategory.isNull() && (m_minCategory != minCategory || m_minCategory.isNull())
            && m_categories.contains(minCategory)) {
        m_minCategory = minCategory;
        m_min = m_categories.indexOf(m_minCategory) - 0.5;
        changed = true;
        emit q->minChanged(minCategory);
    }

    if (!maxCategory.isNull() && (m_maxCategory != maxCategory || m_maxCategory.isNull())
            && m_categories.contains(maxCategory)) {
        m_maxCategory = maxCategory;
        m_max = m_categories.indexOf(m_maxCategory) + 0.5;
        changed = true;
        emit q->maxChanged(maxCategory);
    }

    if (changed) {
        m_count = m_max - m_min;
        emit q->rangeChanged(m_minCategory, m_maxCategory);
        emit rangeChanged(m_min,m_max);
    }
}

void QBarCategoryAxisPrivate::initializeGraphics(QGraphicsItem* parent)
{
    Q_Q(QBarCategoryAxis);
    ChartAxisElement* axis(0);
    if (orientation() == Qt::Vertical)
        axis = new ChartBarCategoryAxisY(q,parent);
    if (orientation() == Qt::Horizontal)
        axis = new ChartBarCategoryAxisX(q,parent);

    m_item.reset(axis);
    QAbstractAxisPrivate::initializeGraphics(parent);
}

void QBarCategoryAxisPrivate::updateCategoryDomain()
{
    bool changed = false;

    qreal tmpMin = m_categories.indexOf(m_minCategory) - 0.5;
    if (!qFuzzyIsNull(m_min - tmpMin)) {
        m_min = tmpMin;
        changed = true;
    }
    qreal tmpMax = m_categories.indexOf(m_maxCategory) + 0.5;
    if (!qFuzzyIsNull(m_max - tmpMax)) {
        m_max = tmpMax;
        changed = true;
    }
    m_count = m_max - m_min;

    if (changed)
        emit rangeChanged(m_min,m_max);
}


void QBarCategoryAxisPrivate::initializeDomain(AbstractDomain *domain)
{
    Q_Q(QBarCategoryAxis);
    if (m_max == m_min) {
        int min;
        int max;
        if (orientation() == Qt::Vertical) {
            min = domain->minY() + 0.5;
            max = domain->maxY() - 0.5;
        } else {
            min = domain->minX() + 0.5;
            max = domain->maxX() - 0.5;
        }

        if (min > 0 && min < m_categories.count() && max > 0 && max < m_categories.count())
            q->setRange(m_categories.at(min), m_categories.at(max));
    } else {
        if (orientation() == Qt::Vertical)
            domain->setRangeY(m_min, m_max);
        else
            domain->setRangeX(m_min, m_max);
    }
}

#include "moc_qbarcategoryaxis.cpp"
#include "moc_qbarcategoryaxis_p.cpp"

QT_CHARTS_END_NAMESPACE
