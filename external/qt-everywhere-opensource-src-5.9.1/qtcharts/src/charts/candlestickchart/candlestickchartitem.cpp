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

#include <QtCharts/QCandlestickSeries>
#include <QtCharts/QCandlestickSet>
#include <private/candlestickchartitem_p.h>
#include <private/candlestick_p.h>
#include <private/candlestickdata_p.h>
#include <private/qcandlestickseries_p.h>
#include <private/candlestickanimation_p.h>

QT_CHARTS_BEGIN_NAMESPACE

CandlestickChartItem::CandlestickChartItem(QCandlestickSeries *series, QGraphicsItem *item)
    : ChartItem(series->d_func(), item),
      m_series(series),
      m_seriesIndex(0),
      m_seriesCount(0),
      m_timePeriod(0.0),
      m_animation(nullptr)
{
    setAcceptedMouseButtons(0);
    connect(series, SIGNAL(candlestickSetsAdded(QList<QCandlestickSet *>)),
            this, SLOT(handleCandlestickSetsAdd(QList<QCandlestickSet *>)));
    connect(series, SIGNAL(candlestickSetsRemoved(QList<QCandlestickSet *>)),
            this, SLOT(handleCandlestickSetsRemove(QList<QCandlestickSet *>)));

    connect(series->d_func(), SIGNAL(updated()), this, SLOT(handleCandlesticksUpdated()));
    connect(series->d_func(), SIGNAL(updatedLayout()), this, SLOT(handleLayoutUpdated()));
    connect(series->d_func(), SIGNAL(updatedCandlesticks()),
            this, SLOT(handleCandlesticksUpdated()));

    setZValue(ChartPresenter::CandlestickSeriesZValue);

    handleCandlestickSetsAdd(m_series->sets());
}

CandlestickChartItem::~CandlestickChartItem()
{
}

void CandlestickChartItem::setAnimation(CandlestickAnimation *animation)
{
    m_animation = animation;

    if (m_animation) {
        foreach (Candlestick *item, m_candlesticks.values())
            m_animation->addCandlestick(item);

        handleDomainUpdated();
    }
}

QRectF CandlestickChartItem::boundingRect() const
{
    return m_boundingRect;
}

void CandlestickChartItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                 QWidget *widget)
{
    Q_UNUSED(painter);
    Q_UNUSED(option);
    Q_UNUSED(widget);
}

void CandlestickChartItem::handleDomainUpdated()
{
    if ((domain()->size().width() <= 0) || (domain()->size().height() <= 0))
        return;

    // Set bounding rectangle to same as domain size. Add one pixel at the top (-1.0) and the bottom
    // as 0.0 would snip a bit off from the wick at the grid line.
    m_boundingRect.setRect(0.0, -1.0, domain()->size().width(), domain()->size().height() + 1.0);

    foreach (Candlestick *item, m_candlesticks.values()) {
        item->updateGeometry(domain());

        if (m_animation)
            presenter()->startAnimation(m_animation->candlestickAnimation(item));
    }
}

void CandlestickChartItem::handleLayoutUpdated()
{
    bool timestampChanged = false;
    foreach (QCandlestickSet *set, m_candlesticks.keys()) {
        qreal oldTimestamp = m_candlesticks.value(set)->m_data.m_timestamp;
        qreal newTimestamp = set->timestamp();
        if (Q_UNLIKELY(oldTimestamp != newTimestamp)) {
            removeTimestamp(oldTimestamp);
            addTimestamp(newTimestamp);
            timestampChanged = true;
        }
    }
    if (timestampChanged)
        updateTimePeriod();

    foreach (Candlestick *item, m_candlesticks.values()) {
        if (m_animation)
            m_animation->setAnimationStart(item);

        item->setTimePeriod(m_timePeriod);
        item->setMaximumColumnWidth(m_series->maximumColumnWidth());
        item->setMinimumColumnWidth(m_series->minimumColumnWidth());
        item->setBodyWidth(m_series->bodyWidth());
        item->setCapsWidth(m_series->capsWidth());

        bool dirty = updateCandlestickGeometry(item, item->m_data.m_index);
        if (dirty && m_animation)
            presenter()->startAnimation(m_animation->candlestickChangeAnimation(item));
        else
            item->updateGeometry(domain());
    }
}

void CandlestickChartItem::handleCandlesticksUpdated()
{
    foreach (QCandlestickSet *set, m_candlesticks.keys())
        updateCandlestickAppearance(m_candlesticks.value(set), set);
}

void CandlestickChartItem::handleCandlestickSeriesChange()
{
    int seriesIndex = 0;
    int seriesCount = 0;

    int index = 0;
    foreach (QAbstractSeries *series, m_series->chart()->series()) {
        if (series->type() == QAbstractSeries::SeriesTypeCandlestick) {
            if (m_series == static_cast<QCandlestickSeries *>(series))
                seriesIndex = index;
            index++;
        }
    }
    seriesCount = index;

    bool changed;
    if ((m_seriesIndex != seriesIndex) || (m_seriesCount != seriesCount))
        changed = true;
    else
        changed = false;

    if (changed) {
        m_seriesIndex = seriesIndex;
        m_seriesCount = seriesCount;
        handleDataStructureChanged();
    }
}

void CandlestickChartItem::handleCandlestickSetsAdd(const QList<QCandlestickSet *> &sets)
{
    foreach (QCandlestickSet *set, sets) {
        Candlestick *item = m_candlesticks.value(set, 0);
        if (item) {
            qWarning() << "There is already a candlestick for this set in the hash";
            continue;
        }

        item = new Candlestick(set, domain(), this);
        m_candlesticks.insert(set, item);
        addTimestamp(set->timestamp());

        connect(item, SIGNAL(clicked(QCandlestickSet *)),
                m_series, SIGNAL(clicked(QCandlestickSet *)));
        connect(item, SIGNAL(hovered(bool, QCandlestickSet *)),
                m_series, SIGNAL(hovered(bool, QCandlestickSet *)));
        connect(item, SIGNAL(pressed(QCandlestickSet *)),
                m_series, SIGNAL(pressed(QCandlestickSet *)));
        connect(item, SIGNAL(released(QCandlestickSet *)),
                m_series, SIGNAL(released(QCandlestickSet *)));
        connect(item, SIGNAL(doubleClicked(QCandlestickSet *)),
                m_series, SIGNAL(doubleClicked(QCandlestickSet *)));
        connect(item, SIGNAL(clicked(QCandlestickSet *)), set, SIGNAL(clicked()));
        connect(item, SIGNAL(hovered(bool, QCandlestickSet *)), set, SIGNAL(hovered(bool)));
        connect(item, SIGNAL(pressed(QCandlestickSet *)), set, SIGNAL(pressed()));
        connect(item, SIGNAL(released(QCandlestickSet *)), set, SIGNAL(released()));
        connect(item, SIGNAL(doubleClicked(QCandlestickSet *)), set, SIGNAL(doubleClicked()));
    }

    handleDataStructureChanged();
}

void CandlestickChartItem::handleCandlestickSetsRemove(const QList<QCandlestickSet *> &sets)
{
    foreach (QCandlestickSet *set, sets) {
        Candlestick *item = m_candlesticks.value(set);

        m_candlesticks.remove(set);
        removeTimestamp(set->timestamp());

        if (m_animation) {
            ChartAnimation *animation = m_animation->candlestickAnimation(item);
            if (animation) {
                animation->stop();
                delete animation;
            }
        }

        delete item;
    }

    handleDataStructureChanged();
}

void CandlestickChartItem::handleDataStructureChanged()
{
    updateTimePeriod();

    for (int i = 0; i < m_series->count(); ++i) {
        QCandlestickSet *set = m_series->sets().at(i);
        Candlestick *item = m_candlesticks.value(set);

        updateCandlestickGeometry(item, i);
        updateCandlestickAppearance(item, set);

        item->updateGeometry(domain());

        if (m_animation)
            m_animation->addCandlestick(item);
    }

    handleDomainUpdated();
}

bool CandlestickChartItem::updateCandlestickGeometry(Candlestick *item, int index)
{
    bool changed = false;

    QCandlestickSet *set = m_series->sets().at(index);
    CandlestickData &data = item->m_data;

    if ((data.m_open != set->open())
        || (data.m_high != set->high())
        || (data.m_low != set->low())
        || (data.m_close != set->close())) {
        changed = true;
    }

    data.m_timestamp = set->timestamp();
    data.m_open = set->open();
    data.m_high = set->high();
    data.m_low = set->low();
    data.m_close = set->close();
    data.m_index = index;

    data.m_maxX = domain()->maxX();
    data.m_minX = domain()->minX();
    data.m_maxY = domain()->maxY();
    data.m_minY = domain()->minY();

    data.m_series = m_series;
    data.m_seriesIndex = m_seriesIndex;
    data.m_seriesCount = m_seriesCount;

    return changed;
}

void CandlestickChartItem::updateCandlestickAppearance(Candlestick *item, QCandlestickSet *set)
{
    item->setTimePeriod(m_timePeriod);
    item->setMaximumColumnWidth(m_series->maximumColumnWidth());
    item->setMinimumColumnWidth(m_series->minimumColumnWidth());
    item->setBodyWidth(m_series->bodyWidth());
    item->setBodyOutlineVisible(m_series->bodyOutlineVisible());
    item->setCapsWidth(m_series->capsWidth());
    item->setCapsVisible(m_series->capsVisible());
    item->setIncreasingColor(m_series->increasingColor());
    item->setDecreasingColor(m_series->decreasingColor());

    // Set the decorative issues for the candlestick so that
    // the brush and pen already defined for the set are kept.
    if (set->brush() == Qt::NoBrush)
        item->setBrush(m_series->brush());
    else
        item->setBrush(set->brush());

    if (set->pen() == Qt::NoPen)
        item->setPen(m_series->pen());
    else
        item->setPen(set->pen());
}

void CandlestickChartItem::addTimestamp(qreal timestamp)
{
    int index = 0;
    for (int i = m_timestamps.count() - 1; i >= 0; --i) {
        if (timestamp > m_timestamps.at(i)) {
            index = i + 1;
            break;
        }
    }
    m_timestamps.insert(index, timestamp);
}

void CandlestickChartItem::removeTimestamp(qreal timestamp)
{
    m_timestamps.removeOne(timestamp);
}

void CandlestickChartItem::updateTimePeriod()
{
    if (m_timestamps.count() == 0) {
        m_timePeriod = 0;
        return;
    }

    if (m_timestamps.count() == 1) {
        m_timePeriod = qAbs(domain()->maxX() - domain()->minX());
        return;
    }

    qreal timePeriod = qAbs(m_timestamps.at(1) - m_timestamps.at(0));
    for (int i = 1; i < m_timestamps.count(); ++i) {
        timePeriod = qMin(timePeriod, qAbs(m_timestamps.at(i) - m_timestamps.at(i - 1)));
    }
    m_timePeriod = timePeriod;
}

#include "moc_candlestickchartitem_p.cpp"

QT_CHARTS_END_NAMESPACE
