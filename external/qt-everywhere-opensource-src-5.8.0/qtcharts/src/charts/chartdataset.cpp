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

#include <private/chartdataset_p.h>
#include <private/chartpresenter_p.h>
#include <QtCharts/QChart>
#include <private/qchart_p.h>
#include <QtCharts/QValueAxis>
#include <QtCharts/QBarCategoryAxis>
#include <private/qvalueaxis_p.h>
#include <QtCharts/QCategoryAxis>
#include <private/qabstractseries_p.h>
#include <QtCharts/QAbstractBarSeries>
#include <QtCharts/QStackedBarSeries>
#include <QtCharts/QPercentBarSeries>
#include <QtCharts/QPieSeries>
#include <private/chartitem_p.h>
#include <private/xydomain_p.h>
#include <private/xypolardomain_p.h>
#include <private/xlogydomain_p.h>
#include <private/logxydomain_p.h>
#include <private/logxlogydomain_p.h>
#include <private/xlogypolardomain_p.h>
#include <private/logxypolardomain_p.h>
#include <private/logxlogypolardomain_p.h>
#include <private/glxyseriesdata_p.h>

#ifndef QT_QREAL_IS_FLOAT
#include <QtCharts/QDateTimeAxis>
#endif

QT_CHARTS_BEGIN_NAMESPACE

ChartDataSet::ChartDataSet(QChart *chart)
    : QObject(chart),
      m_chart(chart),
      m_glXYSeriesDataManager(new GLXYSeriesDataManager(this))
{

}

ChartDataSet::~ChartDataSet()
{
    deleteAllSeries();
    deleteAllAxes();
}

/*
 * This method adds series to chartdataset, series ownership is taken from caller.
 */
void ChartDataSet::addSeries(QAbstractSeries *series)
{
    if (m_seriesList.contains(series)) {
        qWarning() << QObject::tr("Can not add series. Series already on the chart.");
        return;
    }

    // Ignore unsupported series added to polar chart
    if (m_chart && m_chart->chartType() == QChart::ChartTypePolar) {
        if (!(series->type() == QAbstractSeries::SeriesTypeArea
            || series->type() == QAbstractSeries::SeriesTypeLine
            || series->type() == QAbstractSeries::SeriesTypeScatter
            || series->type() == QAbstractSeries::SeriesTypeSpline)) {
            qWarning() << QObject::tr("Can not add series. Series type is not supported by a polar chart.");
            return;
        }
        // Disable OpenGL for series in polar charts
        series->setUseOpenGL(false);
        series->d_ptr->setDomain(new XYPolarDomain());
        // Set the correct domain for upper and lower series too
        if (series->type() == QAbstractSeries::SeriesTypeArea) {
            foreach (QObject *child, series->children()) {
                if (qobject_cast<QAbstractSeries *>(child)) {
                    QAbstractSeries *childSeries = qobject_cast<QAbstractSeries *>(child);
                    childSeries->d_ptr->setDomain(new XYPolarDomain());
                }
            }
        }
    } else {
        series->d_ptr->setDomain(new XYDomain());
    }

    series->d_ptr->initializeDomain();
    m_seriesList.append(series);

    series->setParent(this); // take ownership
    series->d_ptr->m_chart = m_chart;

    emit seriesAdded(series);
}

/*
 * This method adds axis to chartdataset, axis ownership is taken from caller.
 */
void ChartDataSet::addAxis(QAbstractAxis *axis, Qt::Alignment aligment)
{
    if (m_axisList.contains(axis)) {
        qWarning() << QObject::tr("Can not add axis. Axis already on the chart.");
        return;
    }

    axis->d_ptr->setAlignment(aligment);

    if (!axis->alignment()) {
        qWarning() << QObject::tr("No alignment specified !");
        return;
    };

    AbstractDomain *newDomain;
    if (m_chart && m_chart->chartType() == QChart::ChartTypePolar)
        newDomain = new XYPolarDomain();
    else
        newDomain = new XYDomain();

    QSharedPointer<AbstractDomain> domain(newDomain);
    axis->d_ptr->initializeDomain(domain.data());

    axis->setParent(this);
    axis->d_ptr->m_chart = m_chart;
    m_axisList.append(axis);

    emit axisAdded(axis);
}

/*
 * This method removes series form chartdataset, series ownership is passed back to caller.
 */
void ChartDataSet::removeSeries(QAbstractSeries *series)
{
    if (! m_seriesList.contains(series)) {
        qWarning() << QObject::tr("Can not remove series. Series not found on the chart.");
        return;
    }

    QList<QAbstractAxis *> axes = series->d_ptr->m_axes;

    foreach (QAbstractAxis *axis, axes) {
        detachAxis(series, axis);
    }

    m_seriesList.removeAll(series);
    emit seriesRemoved(series);

    // Reset domain to default
    series->d_ptr->setDomain(new XYDomain());
    series->setParent(0);
    series->d_ptr->m_chart = 0;

    QXYSeries *xySeries = qobject_cast<QXYSeries *>(series);
    if (xySeries)
        m_glXYSeriesDataManager->removeSeries(xySeries);
}

/*
 * This method removes axis form chartdataset, series ownership is passed back to caller.
 */
void ChartDataSet::removeAxis(QAbstractAxis *axis)
{
    if (! m_axisList.contains(axis)) {
        qWarning() << QObject::tr("Can not remove axis. Axis not found on the chart.");
        return;
    }

    QList<QAbstractSeries*> series =  axis->d_ptr->m_series;

    foreach(QAbstractSeries* s, series) {
      detachAxis(s,axis);
    }

    emit axisRemoved(axis);
    m_axisList.removeAll(axis);

    axis->setParent(0);
    axis->d_ptr->m_chart = 0;
}

/*
 * This method attaches axis to series, return true if success.
 */
bool ChartDataSet::attachAxis(QAbstractSeries *series,QAbstractAxis *axis)
{
    Q_ASSERT(axis);

    if (!series)
        return false;

    QList<QAbstractSeries *> attachedSeriesList = axis->d_ptr->m_series;
    QList<QAbstractAxis *> attachedAxisList = series->d_ptr->m_axes;

    if (!m_seriesList.contains(series)) {
        qWarning() << QObject::tr("Can not find series on the chart.");
        return false;
    }

    if (axis && !m_axisList.contains(axis)) {
        qWarning() << QObject::tr("Can not find axis on the chart.");
        return false;
    }

    if (attachedAxisList.contains(axis)) {
        qWarning() << QObject::tr("Axis already attached to series.");
        return false;
    }

    if (attachedSeriesList.contains(series)) {
        qWarning() << QObject::tr("Axis already attached to series.");
        return false;
    }

    AbstractDomain *domain = series->d_ptr->domain();
    AbstractDomain::DomainType type = selectDomain(attachedAxisList<<axis);

    if (type == AbstractDomain::UndefinedDomain) return false;

    if (domain->type() != type) {
        AbstractDomain *old = domain;
        domain = createDomain(type);
        domain->setRange(old->minX(), old->maxX(), old->minY(), old->maxY());
        // Initialize domain size to old domain size, as it won't get updated
        // unless geometry changes.
        domain->setSize(old->size());
    }

    if (!domain)
        return false;

    if (!domain->attachAxis(axis))
        return false;

    QList<AbstractDomain *> blockedDomains;
    domain->blockRangeSignals(true);
    blockedDomains << domain;

    if (domain != series->d_ptr->domain()) {
        foreach (QAbstractAxis *axis, series->d_ptr->m_axes) {
            series->d_ptr->domain()->detachAxis(axis);
            domain->attachAxis(axis);
            foreach (QAbstractSeries *otherSeries, axis->d_ptr->m_series) {
                if (otherSeries != series && otherSeries->d_ptr->domain()) {
                    if (!otherSeries->d_ptr->domain()->rangeSignalsBlocked()) {
                        otherSeries->d_ptr->domain()->blockRangeSignals(true);
                        blockedDomains << otherSeries->d_ptr->domain();
                    }
                }
            }
        }
        series->d_ptr->setDomain(domain);
        series->d_ptr->initializeDomain();
    }

    series->d_ptr->m_axes<<axis;
    axis->d_ptr->m_series<<series;

    series->d_ptr->initializeAxes();
    axis->d_ptr->initializeDomain(domain);
    connect(axis, &QAbstractAxis::reverseChanged, this, &ChartDataSet::reverseChanged);
    foreach (AbstractDomain *blockedDomain, blockedDomains)
        blockedDomain->blockRangeSignals(false);

    return true;
}

/*
 * This method detaches axis to series, return true if success.
 */
bool ChartDataSet::detachAxis(QAbstractSeries* series,QAbstractAxis *axis)
{
    Q_ASSERT(series);
    Q_ASSERT(axis);

    QList<QAbstractSeries* > attachedSeriesList = axis->d_ptr->m_series;
    QList<QAbstractAxis* > attachedAxisList = series->d_ptr->m_axes;
    AbstractDomain* domain =  series->d_ptr->domain();

    if (!m_seriesList.contains(series)) {
        qWarning() << QObject::tr("Can not find series on the chart.");
        return false;
    }

    if (axis && !m_axisList.contains(axis)) {
        qWarning() << QObject::tr("Can not find axis on the chart.");
        return false;
    }

    if (!attachedAxisList.contains(axis)) {
        qWarning() << QObject::tr("Axis not attached to series.");
        return false;
    }

    Q_ASSERT(axis->d_ptr->m_series.contains(series));

    domain->detachAxis(axis);
    series->d_ptr->m_axes.removeAll(axis);
    axis->d_ptr->m_series.removeAll(series);
    disconnect(axis, &QAbstractAxis::reverseChanged, this, &ChartDataSet::reverseChanged);
    return true;
}

void ChartDataSet::createDefaultAxes()
{
    if (m_seriesList.isEmpty())
        return;

    QAbstractAxis::AxisTypes typeX(0);
    QAbstractAxis::AxisTypes typeY(0);

    // Remove possibly existing axes
    deleteAllAxes();

    Q_ASSERT(m_axisList.isEmpty());

    // Select the required axis x and axis y types based on the types of the current series
    foreach(QAbstractSeries* s, m_seriesList) {
        typeX |= s->d_ptr->defaultAxisType(Qt::Horizontal);
        typeY |= s->d_ptr->defaultAxisType(Qt::Vertical);
    }

    createAxes(typeX, Qt::Horizontal);
    createAxes(typeY, Qt::Vertical);
}

void ChartDataSet::createAxes(QAbstractAxis::AxisTypes type, Qt::Orientation orientation)
{
    QAbstractAxis *axis = 0;
    //decide what axis should be created

    switch (type) {
        case QAbstractAxis::AxisTypeValue:
        axis = new QValueAxis(this);
        break;
        case QAbstractAxis::AxisTypeBarCategory:
        axis = new QBarCategoryAxis(this);
        break;
        case QAbstractAxis::AxisTypeCategory:
        axis = new QCategoryAxis(this);
        break;
#ifndef QT_QREAL_IS_FLOAT
        case QAbstractAxis::AxisTypeDateTime:
        axis = new QDateTimeAxis(this);
        break;
#endif
        default:
        axis = 0;
        break;
    }

    if (axis) {
        //create one axis for all

        addAxis(axis,orientation==Qt::Horizontal?Qt::AlignBottom:Qt::AlignLeft);
        qreal min = 0;
        qreal max = 0;
        findMinMaxForSeries(m_seriesList,orientation,min,max);
        foreach(QAbstractSeries *s, m_seriesList) {
            attachAxis(s,axis);
        }
        axis->setRange(min,max);
    } else {
        // Create separate axis for each series
        foreach(QAbstractSeries *s, m_seriesList) {
            QAbstractAxis *axis = s->d_ptr->createDefaultAxis(orientation);
            if(axis) {
                addAxis(axis,orientation==Qt::Horizontal?Qt::AlignBottom:Qt::AlignLeft);
                attachAxis(s,axis);
            }
        }
    }
}

void ChartDataSet::findMinMaxForSeries(QList<QAbstractSeries *> series,Qt::Orientations orientation, qreal &min, qreal &max)
{
    Q_ASSERT(!series.isEmpty());

    AbstractDomain *domain = series.first()->d_ptr->domain();
    min = (orientation == Qt::Vertical) ? domain->minY() : domain->minX();
    max = (orientation == Qt::Vertical) ? domain->maxY() : domain->maxX();

    for (int i = 1; i< series.size(); i++) {
        AbstractDomain *domain = series[i]->d_ptr->domain();
        min = qMin((orientation == Qt::Vertical) ? domain->minY() : domain->minX(), min);
        max = qMax((orientation == Qt::Vertical) ? domain->maxY() : domain->maxX(), max);
    }
    if (min == max) {
        min -= 0.5;
        max += 0.5;
    }
}

void ChartDataSet::deleteAllSeries()
{
    foreach (QAbstractSeries *s , m_seriesList){
        removeSeries(s);
        s->deleteLater();
    }
    Q_ASSERT(m_seriesList.count() == 0);
}

void ChartDataSet::deleteAllAxes()
{
    foreach (QAbstractAxis *a , m_axisList){
        removeAxis(a);
        a->deleteLater();
    }
    Q_ASSERT(m_axisList.count() == 0);
}

void ChartDataSet::zoomInDomain(const QRectF &rect)
{
    QList<AbstractDomain*> domains;
    foreach(QAbstractSeries *s, m_seriesList) {
        AbstractDomain* domain = s->d_ptr->domain();
        s->d_ptr->m_domain->blockRangeSignals(true);
        domains<<domain;
    }

    foreach(AbstractDomain *domain, domains)
        domain->zoomIn(rect);

    foreach(AbstractDomain *domain, domains)
        domain->blockRangeSignals(false);
}

void ChartDataSet::zoomOutDomain(const QRectF &rect)
{
    QList<AbstractDomain*> domains;
    foreach(QAbstractSeries *s, m_seriesList) {
        AbstractDomain* domain = s->d_ptr->domain();
        s->d_ptr->m_domain->blockRangeSignals(true);
        domains<<domain;
    }

    foreach(AbstractDomain *domain, domains)
        domain->zoomOut(rect);

    foreach(AbstractDomain *domain, domains)
        domain->blockRangeSignals(false);
}

void ChartDataSet::zoomResetDomain()
{
    QList<AbstractDomain*> domains;
    foreach (QAbstractSeries *s, m_seriesList) {
        AbstractDomain *domain = s->d_ptr->domain();
        s->d_ptr->m_domain->blockRangeSignals(true);
        domains << domain;
    }

    foreach (AbstractDomain *domain, domains)
        domain->zoomReset();

    foreach (AbstractDomain *domain, domains)
        domain->blockRangeSignals(false);
}

bool ChartDataSet::isZoomedDomain()
{
    foreach (QAbstractSeries *s, m_seriesList) {
        if (s->d_ptr->domain()->isZoomed())
            return true;
    }
    return false;
}

void ChartDataSet::scrollDomain(qreal dx, qreal dy)
{
    QList<AbstractDomain*> domains;
    foreach(QAbstractSeries *s, m_seriesList) {
        AbstractDomain* domain = s->d_ptr->domain();
        s->d_ptr->m_domain->blockRangeSignals(true);
        domains<<domain;
    }

    foreach(AbstractDomain *domain, domains)
        domain->move(dx, dy);

    foreach(AbstractDomain *domain, domains)
        domain->blockRangeSignals(false);
}

QPointF ChartDataSet::mapToValue(const QPointF &position, QAbstractSeries *series)
{
    QPointF point;
    if (series == 0 && !m_seriesList.isEmpty())
        series = m_seriesList.first();

    if (series && series->type() == QAbstractSeries::SeriesTypePie)
        return point;

    if (series && m_seriesList.contains(series))
        point = series->d_ptr->m_domain->calculateDomainPoint(position - m_chart->plotArea().topLeft());
    return point;
}

QPointF ChartDataSet::mapToPosition(const QPointF &value, QAbstractSeries *series)
{
    QPointF point = m_chart->plotArea().topLeft();
    if (series == 0 && !m_seriesList.isEmpty())
        series = m_seriesList.first();

    if (series && series->type() == QAbstractSeries::SeriesTypePie)
        return QPoint(0, 0);

    bool ok;
    if (series && m_seriesList.contains(series))
        point += series->d_ptr->m_domain->calculateGeometryPoint(value, ok);
    return point;
}

QList<QAbstractAxis *> ChartDataSet::axes() const
{
   return m_axisList;
}

QList<QAbstractSeries *> ChartDataSet::series() const
{
    return m_seriesList;
}

AbstractDomain::DomainType ChartDataSet::selectDomain(QList<QAbstractAxis *> axes)
{
    enum Type {
        Undefined = 0,
        LogType = 0x1,
        ValueType = 0x2
    };

    int horizontal(Undefined);
    int vertical(Undefined);

    // Assume cartesian chart type, unless chart is set
    QChart::ChartType chartType(QChart::ChartTypeCartesian);
    if (m_chart)
        chartType = m_chart->chartType();

    foreach (QAbstractAxis *axis, axes)
    {
        switch (axis->type()) {
        case QAbstractAxis::AxisTypeLogValue:
            if (axis->orientation() == Qt::Horizontal)
                horizontal |= LogType;
            if (axis->orientation() == Qt::Vertical)
                vertical |= LogType;
            break;
        case QAbstractAxis::AxisTypeValue:
        case QAbstractAxis::AxisTypeBarCategory:
        case QAbstractAxis::AxisTypeCategory:
        case QAbstractAxis::AxisTypeDateTime:
            if (axis->orientation() == Qt::Horizontal)
                horizontal |= ValueType;
            if (axis->orientation() == Qt::Vertical)
                vertical |= ValueType;
            break;
        default:
            qWarning() << "Undefined type";
            break;
        }
    }

    if (vertical == Undefined)
        vertical = ValueType;
    if (horizontal == Undefined)
        horizontal = ValueType;

    if (vertical == ValueType && horizontal == ValueType) {
        if (chartType == QChart::ChartTypeCartesian)
            return AbstractDomain::XYDomain;
        else if (chartType == QChart::ChartTypePolar)
            return AbstractDomain::XYPolarDomain;
    }

    if (vertical == LogType && horizontal == ValueType) {
        if (chartType == QChart::ChartTypeCartesian)
            return AbstractDomain::XLogYDomain;
        if (chartType == QChart::ChartTypePolar)
            return AbstractDomain::XLogYPolarDomain;
    }

    if (vertical == ValueType && horizontal == LogType) {
        if (chartType == QChart::ChartTypeCartesian)
            return AbstractDomain::LogXYDomain;
        else if (chartType == QChart::ChartTypePolar)
            return AbstractDomain::LogXYPolarDomain;
    }

    if (vertical == LogType && horizontal == LogType) {
        if (chartType == QChart::ChartTypeCartesian)
            return AbstractDomain::LogXLogYDomain;
        else if (chartType == QChart::ChartTypePolar)
            return AbstractDomain::LogXLogYPolarDomain;
    }

    return AbstractDomain::UndefinedDomain;
}

//refactor create factory
AbstractDomain* ChartDataSet::createDomain(AbstractDomain::DomainType type)
{
    switch (type)
    {
    case AbstractDomain::LogXLogYDomain:
        return new LogXLogYDomain();
    case AbstractDomain::XYDomain:
        return new XYDomain();
    case AbstractDomain::XLogYDomain:
        return new XLogYDomain();
    case AbstractDomain::LogXYDomain:
        return new LogXYDomain();
    case AbstractDomain::XYPolarDomain:
        return new XYPolarDomain();
    case AbstractDomain::XLogYPolarDomain:
        return new XLogYPolarDomain();
    case AbstractDomain::LogXYPolarDomain:
        return new LogXYPolarDomain();
    case AbstractDomain::LogXLogYPolarDomain:
        return new LogXLogYPolarDomain();
    default:
        return 0;
    }
}

void ChartDataSet::reverseChanged()
{
    QAbstractAxis *axis = qobject_cast<QAbstractAxis *>(sender());
    if (axis)
        m_glXYSeriesDataManager->handleAxisReverseChanged(axis->d_ptr->m_series);
}

#include "moc_chartdataset_p.cpp"

QT_CHARTS_END_NAMESPACE
