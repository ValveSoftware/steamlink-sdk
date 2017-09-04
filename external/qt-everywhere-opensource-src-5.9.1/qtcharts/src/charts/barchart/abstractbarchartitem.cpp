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

#include <private/abstractbarchartitem_p.h>
#include <private/bar_p.h>
#include <QtCharts/QBarSet>
#include <private/qbarset_p.h>
#include <QtCharts/QAbstractBarSeries>
#include <private/qabstractbarseries_p.h>
#include <private/qchart_p.h>
#include <private/chartpresenter_p.h>
#include <private/charttheme_p.h>
#include <private/baranimation_p.h>
#include <private/chartdataset_p.h>
#include <QtCore/QtMath>
#include <QtGui/QPainter>
#include <QtGui/QTextDocument>

QT_CHARTS_BEGIN_NAMESPACE

AbstractBarChartItem::AbstractBarChartItem(QAbstractBarSeries *series, QGraphicsItem* item) :
    ChartItem(series->d_func(),item),
    m_animation(0),
    m_series(series),
    m_firstCategory(-1),
    m_lastCategory(-2),
    m_categoryCount(0),
    m_labelItemsMissing(false),
    m_orientation(Qt::Horizontal),
    m_resetAnimation(true)
{
    setAcceptedMouseButtons(0);
    setFlag(ItemClipsChildrenToShape);
    setFlag(QGraphicsItem::ItemIsSelectable);
    connect(series->d_func(), SIGNAL(updatedLayout()), this, SLOT(handleLayoutChanged()));
    connect(series->d_func(), SIGNAL(updatedBars()), this, SLOT(handleUpdatedBars()));
    connect(series->d_func(), SIGNAL(labelsVisibleChanged(bool)), this, SLOT(handleLabelsVisibleChanged(bool)));
    connect(series->d_func(), SIGNAL(restructuredBars()), this, SLOT(handleDataStructureChanged()));
    connect(series->d_func(), &QAbstractBarSeriesPrivate::setValueChanged,
            this, &AbstractBarChartItem::handleBarValueChange);
    connect(series->d_func(), &QAbstractBarSeriesPrivate::setValueAdded,
            this, &AbstractBarChartItem::handleBarValueAdd);
    connect(series->d_func(), &QAbstractBarSeriesPrivate::setValueRemoved,
            this, &AbstractBarChartItem::handleBarValueRemove);
    connect(series, SIGNAL(visibleChanged()), this, SLOT(handleVisibleChanged()));
    connect(series, SIGNAL(opacityChanged()), this, SLOT(handleOpacityChanged()));
    connect(series, SIGNAL(labelsFormatChanged(QString)), this, SLOT(handleUpdatedBars()));
    connect(series, SIGNAL(labelsFormatChanged(QString)), this, SLOT(positionLabels()));
    connect(series, SIGNAL(labelsPositionChanged(QAbstractBarSeries::LabelsPosition)),
            this, SLOT(handleLabelsPositionChanged()));
    connect(series, SIGNAL(labelsAngleChanged(qreal)), this, SLOT(positionLabels()));
    connect(series->chart()->d_ptr->m_dataset, &ChartDataSet::seriesAdded,
            this, &AbstractBarChartItem::handleSeriesAdded);
    connect(series->chart()->d_ptr->m_dataset, &ChartDataSet::seriesRemoved,
            this, &AbstractBarChartItem::handleSeriesRemoved);
    setZValue(ChartPresenter::BarSeriesZValue);
    calculateSeriesPositionAdjustmentAndWidth();
    handleDataStructureChanged();
}

AbstractBarChartItem::~AbstractBarChartItem()
{
}

void AbstractBarChartItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(painter);
    Q_UNUSED(option);
    Q_UNUSED(widget);
}

QRectF AbstractBarChartItem::boundingRect() const
{
    return m_rect;
}

void AbstractBarChartItem::initializeFullLayout()
{
    qreal setCount = m_series->count();

    for (int set = 0; set < setCount; set++) {
        QBarSet *barSet = m_series->barSets().at(set);
        const QList<Bar *> bars = m_barMap.value(barSet);
        for (int i = 0; i < bars.size(); i++) {
            Bar *bar = bars.at(i);
            initializeLayout(set, bar->index(), bar->layoutIndex(), true);
            // Make bar initially hidden to avoid artifacts, layout setting will show it
            bar->setVisible(false);
        }
    }
}

void AbstractBarChartItem::applyLayout(const QVector<QRectF> &layout)
{
    QSizeF size = geometry().size();
    if (geometry().size().isValid()) {
        if (m_animation) {
            // If geometry changes along the value axis, do full animation reset, as
            // it can cause "ungrounded" bars otherwise.
            // Changes to axis labels on bar axis can occur naturally with scrolling,
            // which can cause geometry changes that shouldn't trigger animation reset.
            const bool sizeChanged = m_orientation == Qt::Horizontal
                    ? m_oldSize.width() != size.width()
                    : m_oldSize.height() != size.height();
            m_oldSize = size;
            if (m_resetAnimation || sizeChanged) {
                initializeFullLayout();
                m_resetAnimation = false;
            }
            m_animation->setup(m_layout, layout);
            presenter()->startAnimation(m_animation);
        } else {
            setLayout(layout);
            update();
        }
    }
}

void AbstractBarChartItem::setAnimation(BarAnimation *animation)
{
    m_animation = animation;
    m_resetAnimation = true;
}

void AbstractBarChartItem::setLayout(const QVector<QRectF> &layout)
{
    int setCount = m_series->count();
    if (layout.size() != m_layout.size() || m_barMap.size() != setCount)
        return;

    m_layout = layout;

    const bool visible = m_series->isVisible();
    for (int set = 0; set < setCount; set++) {
        QBarSet *barSet = m_series->d_func()->barsetAt(set);
        const QList<Bar *> bars = m_barMap.value(barSet);
        for (int i = 0; i < bars.size(); i++) {
            Bar *bar = bars.at(i);
            const QRectF &rect = layout.at(bar->layoutIndex());
            bar->setRect(rect);
            // Hide empty bars to avoid artifacts at animation start when adding a new series
            // as it doesn't have correct axes yet
            bar->setVisible(visible && !rect.isEmpty());
        }
    }

    positionLabels();
}

void AbstractBarChartItem::resetAnimation()
{
    m_resetAnimation = true;
}

void AbstractBarChartItem::handleDomainUpdated()
{
    QRectF rect(QPointF(0,0),domain()->size());

    if(m_rect != rect){
        prepareGeometryChange();
        m_rect = rect;
    }

    handleLayoutChanged();
}

void AbstractBarChartItem::handleLayoutChanged()
{
    if ((m_rect.width() <= 0) || (m_rect.height() <= 0))
        return; // rect size zero.
    updateBarItems();
    QVector<QRectF> layout = calculateLayout();
    handleUpdatedBars();
    applyLayout(layout);
}

void AbstractBarChartItem::handleLabelsVisibleChanged(bool visible)
{
    bool newVisible = visible && m_series->isVisible();
    QMapIterator<QBarSet *, QList<Bar *> > i(m_barMap);
    while (i.hasNext()) {
        i.next();
        const QList<Bar *> &bars = i.value();
        for (int j = 0; j < bars.size(); j++) {
            QGraphicsTextItem *label = bars.at(j)->labelItem();
            if (label)
                label->setVisible(newVisible);
        }
    }
    if (newVisible) {
        handleUpdatedBars();
        positionLabels();
    }
    update();
}

void AbstractBarChartItem::handleDataStructureChanged()
{
    handleSetStructureChange();
    handleLayoutChanged();
}

void AbstractBarChartItem::handleVisibleChanged()
{
    bool visible = m_series->isVisible();
    handleLabelsVisibleChanged(m_series->isLabelsVisible());

    QMapIterator<QBarSet *, QList<Bar *> > i(m_barMap);
    while (i.hasNext()) {
        i.next();
        const QList<Bar *> &bars = i.value();
        for (int j = 0; j < bars.size(); j++) {
            Bar *bar = bars.at(j);
            bar->setVisible(visible && i.key()->at(bar->index()) != 0.0);
        }
    }
}

void AbstractBarChartItem::handleOpacityChanged()
{
    foreach (QGraphicsItem *item, childItems())
        item->setOpacity(m_series->opacity());
}

void AbstractBarChartItem::handleUpdatedBars()
{
    if (!m_series->d_func()->blockBarUpdate()) {
        // Handle changes in pen, brush, labels etc.
        int setCount = m_series->count();
        const bool seriesVisualsDirty = m_series->d_func()->visualsDirty();
        const bool seriesLabelsDirty = m_series->d_func()->labelsDirty();
        m_series->d_func()->setVisualsDirty(false);

        const bool updateLabels =
                m_series->isLabelsVisible() && m_series->isVisible() && presenter();
        if (updateLabels) {
            createLabelItems();
            m_series->d_func()->setLabelsDirty(false);
        }

        for (int set = 0; set < setCount; set++) {
            QBarSet *barSet = m_series->d_func()->barsetAt(set);
            QBarSetPrivate *barSetP = barSet->d_ptr.data();
            const bool setVisualsDirty = barSetP->visualsDirty();
            const bool setLabelsDirty = barSetP->labelsDirty();
            barSetP->setVisualsDirty(false);
            if (updateLabels)
                barSetP->setLabelsDirty(false);
            const int actualBarCount = barSet->count();
            const QList<Bar *> bars = m_barMap.value(barSet);
            for (int i = 0; i < bars.size(); i++) {
                Bar *bar = bars.at(i);
                if (seriesVisualsDirty || setVisualsDirty || bar->visualsDirty()) {
                    bar->setPen(barSetP->m_pen);
                    bar->setBrush(barSetP->m_brush);
                    bar->setVisualsDirty(false);
                    bar->update();
                }
                if (updateLabels && actualBarCount > bar->index()) {
                    if (seriesLabelsDirty || setLabelsDirty || bar->labelDirty()) {
                        bar->setLabelDirty(false);
                        QGraphicsTextItem *label = bar->labelItem();
                        QString valueLabel;
                        qreal value = barSetP->value(bar->index());
                        if (value == 0.0) {
                            label->setVisible(false);
                        } else {
                            label->setVisible(m_series->isLabelsVisible());
                            valueLabel = generateLabelText(set, bar->index(), value);
                        }
                        label->setHtml(valueLabel);
                        label->setFont(barSetP->m_labelFont);
                        label->setDefaultTextColor(barSetP->m_labelBrush.color());
                        label->update();
                    }
                }
            }
        }
    }
}

void AbstractBarChartItem::handleLabelsPositionChanged()
{
    positionLabels();
}

void AbstractBarChartItem::positionLabels()
{
    // By default position labels on horizontal bar series
    // Vertical bar series overload positionLabels() to call positionLabelsVertical()

    if (!m_series->isLabelsVisible())
        return;
    createLabelItems();

    QTransform transform;
    const qreal angle = m_series->d_func()->labelsAngle();
    if (angle != 0.0)
        transform.rotate(angle);

    int setCount = m_series->count();
    for (int set = 0; set < setCount; set++) {
        QBarSet *barSet = m_series->d_func()->barsetAt(set);
        const QList<Bar *> bars = m_barMap.value(barSet);
        for (int i = 0; i < bars.size(); i++) {
            Bar *bar = bars.at(i);
            QGraphicsTextItem *label = bar->labelItem();

            QRectF labelRect = label->boundingRect();
            QPointF center = labelRect.center();

            qreal xPos = 0;
            qreal yPos = m_layout.at(bar->layoutIndex()).center().y() - center.y();

            int xDiff = 0;
            if (angle != 0.0) {
                label->setTransformOriginPoint(center.x(), center.y());
                label->setRotation(m_series->d_func()->labelsAngle());
                qreal oldWidth = labelRect.width();
                labelRect = transform.mapRect(labelRect);
                xDiff = (labelRect.width() - oldWidth) / 2;
            }

            int offset = bar->pen().width() / 2 + 2;

            switch (m_series->labelsPosition()) {
            case QAbstractBarSeries::LabelsCenter:
                xPos = m_layout.at(bar->layoutIndex()).center().x() - center.x();
                break;
            case QAbstractBarSeries::LabelsInsideEnd:
                xPos = m_layout.at(bar->layoutIndex()).right() - labelRect.width() - offset + xDiff;
                break;
            case QAbstractBarSeries::LabelsInsideBase:
                xPos = m_layout.at(bar->layoutIndex()).left() + offset + xDiff;
                break;
            case QAbstractBarSeries::LabelsOutsideEnd:
                xPos = m_layout.at(bar->layoutIndex()).right() + offset + xDiff;
                break;
            default:
                // Invalid position, never comes here
                break;
            }

            label->setPos(xPos, yPos);
            label->setZValue(zValue() + 1);
        }
    }
}

void AbstractBarChartItem::handleBarValueChange(int index, QtCharts::QBarSet *barset)
{
    markLabelsDirty(barset, index, 1);
    handleLayoutChanged();
}

void AbstractBarChartItem::handleBarValueAdd(int index, int count, QBarSet *barset)
{
    Q_UNUSED(count)

    // Value insertions into middle of barset need to dirty the rest of the labels of the set
    markLabelsDirty(barset, index, -1);
    handleLayoutChanged();
}

void AbstractBarChartItem::handleBarValueRemove(int index, int count, QBarSet *barset)
{
    Q_UNUSED(count)

    // Value removals from the middle of barset need to dirty the rest of the labels of the set.
    markLabelsDirty(barset, index, -1);
    handleLayoutChanged();
}

void AbstractBarChartItem::handleSeriesAdded(QAbstractSeries *series)
{
    Q_UNUSED(series)

    // If the parent series was added, do nothing, as series pos and width calculations will
    // happen anyway.
    QAbstractBarSeries *addedSeries = static_cast<QAbstractBarSeries *>(series);
    if (addedSeries != m_series) {
        calculateSeriesPositionAdjustmentAndWidth();
        handleLayoutChanged();
    }
}

void AbstractBarChartItem::handleSeriesRemoved(QAbstractSeries *series)
{
    // If the parent series was removed, disconnect everything connected by this item,
    // as the item will be scheduled for deletion but it is done asynchronously with deleteLater.
    QAbstractBarSeries *removedSeries = static_cast<QAbstractBarSeries *>(series);
    if (removedSeries == m_series) {
        disconnect(m_series->d_func(), 0, this, 0);
        disconnect(m_series, 0, this, 0);
        disconnect(m_series->chart()->d_ptr->m_dataset, 0, this, 0);
    } else {
        calculateSeriesPositionAdjustmentAndWidth();
        handleLayoutChanged();
    }
}

void AbstractBarChartItem::positionLabelsVertical()
{
    if (!m_series->isLabelsVisible())
        return;
    createLabelItems();

    QTransform transform;
    const qreal angle = m_series->d_func()->labelsAngle();
    if (angle != 0.0)
        transform.rotate(angle);

    int setCount = m_series->count();
    for (int set = 0; set < setCount; set++) {
        QBarSet *barSet = m_series->d_func()->barsetAt(set);
        const QList<Bar *> bars = m_barMap.value(barSet);
        for (int i = 0; i < bars.size(); i++) {
            Bar *bar = bars.at(i);
            QGraphicsTextItem *label = bar->labelItem();

            QRectF labelRect = label->boundingRect();
            QPointF center = labelRect.center();

            qreal xPos = m_layout.at(bar->layoutIndex()).center().x() - center.x();
            qreal yPos = 0;

            int yDiff = 0;
            if (angle != 0.0) {
                label->setTransformOriginPoint(center.x(), center.y());
                label->setRotation(m_series->d_func()->labelsAngle());
                qreal oldHeight = labelRect.height();
                labelRect = transform.mapRect(labelRect);
                yDiff = (labelRect.height() - oldHeight) / 2;
            }

            int offset = bar->pen().width() / 2 + 2;

            switch (m_series->labelsPosition()) {
            case QAbstractBarSeries::LabelsCenter:
                yPos = m_layout.at(bar->layoutIndex()).center().y() - center.y();
                break;
            case QAbstractBarSeries::LabelsInsideEnd:
                yPos = m_layout.at(bar->layoutIndex()).top() + offset + yDiff;
                break;
            case QAbstractBarSeries::LabelsInsideBase:
                yPos = m_layout.at(bar->layoutIndex()).bottom() - labelRect.height() - offset + yDiff;
                break;
            case QAbstractBarSeries::LabelsOutsideEnd:
                yPos = m_layout.at(bar->layoutIndex()).top() - labelRect.height() - offset + yDiff;
                break;
            default:
                // Invalid position, never comes here
                break;
            }

            label->setPos(xPos, yPos);
            label->setZValue(zValue() + 1);
        }
    }
}

void AbstractBarChartItem::createLabelItems()
{
    if (!m_labelItemsMissing)
        return;

    m_labelItemsMissing = false;

    QMapIterator<QBarSet *, QList<Bar *> > i(m_barMap);
    while (i.hasNext()) {
        i.next();
        const QList<Bar *> &bars = i.value();
        for (int j = 0; j < bars.size(); j++) {
            QGraphicsTextItem *label = bars.at(j)->labelItem();
            if (!label) {
                QGraphicsTextItem *newLabel = new QGraphicsTextItem(this);
                newLabel->document()->setDocumentMargin(ChartPresenter::textMargin());
                bars.at(j)->setLabelItem(newLabel);
            }
        }
    }
}

// This function is called whenever barsets change
void AbstractBarChartItem::handleSetStructureChange()
{
    QList<QBarSet *> newSets = m_series->barSets();
    QList<QBarSet *> oldSets = m_barMap.keys();

    // Remove obsolete sets
    for (int i = 0; i < oldSets.size(); i++) {
        if (!newSets.contains(oldSets.at(i))) {
            qDeleteAll(m_barMap.value(oldSets.at(i)));
            m_barMap.remove(oldSets.at(i));
        }
    }

    // Create new sets
    for (int s = 0; s < newSets.size(); s++) {
        QBarSet *set = newSets.at(s);
        if (!m_barMap.contains(set)) {
            QList<Bar *> bars;
            m_barMap.insert(set, bars);
        } else {
            // Dirty the old set labels to ensure labels are updated correctly on all series types
            markLabelsDirty(set, 0, -1);
        }
    }

    if (themeManager())
        themeManager()->updateSeries(m_series);
}

QString AbstractBarChartItem::generateLabelText(int set, int category, qreal value)
{
    Q_UNUSED(set);
    Q_UNUSED(category);
    static const QString valueTag(QLatin1String("@value"));
    QString valueLabel;
    if (m_series->labelsFormat().isEmpty()) {
        valueLabel = presenter()->numberToString(value);
    } else {
        valueLabel = m_series->labelsFormat();
        valueLabel.replace(valueTag, presenter()->numberToString(value));
    }
    return valueLabel;
}

void AbstractBarChartItem::updateBarItems()
{
    int min(0);
    int max(0);
    if (m_orientation == Qt::Vertical) {
        min = qFloor(domain()->minX()) - 1;
        max = qCeil(domain()->maxX()) + 1;
    } else {
        min = qFloor(domain()->minY()) - 1;
        max = qCeil(domain()->maxY()) + 1;
    }

    int lastBarIndex = m_series->d_func()->categoryCount() - 1;

    if (lastBarIndex < 0) {
        // Indicate invalid categories by negatives
        // Last should be one less than the first to make loops work right in case of no categories
        m_firstCategory = -1;
        m_lastCategory = -2;
        m_categoryCount = 0;
    } else {
        m_firstCategory = qMax(qMin(min, lastBarIndex), 0);
        m_lastCategory = qMax(qMin(max, lastBarIndex), m_firstCategory);
        m_categoryCount = m_lastCategory - m_firstCategory + 1;
    }

    QList<QBarSet *> newSets = m_series->barSets();
    QList<QBarSet *> oldSets = m_barMap.keys();

    Q_ASSERT(newSets.size() == oldSets.size());

    int layoutSize = m_categoryCount * newSets.size();

    QVector<QRectF> oldLayout = m_layout;
    if (layoutSize != m_layout.size())
        m_layout.resize(layoutSize);

    // Create new graphic items for bars or remove excess ones
    int layoutIndex = 0;
    for (int s = 0; s < newSets.size(); s++) {
        QBarSet *set = newSets.at(s);
        QList<Bar *> bars = m_barMap.value(set);
        int addCount = m_categoryCount - bars.size();
        if (addCount > 0) {
            for (int c = 0; c < addCount; c++) {
                Bar *bar = new Bar(set, this);
                bars.append(bar);
                connect(bar, &Bar::clicked, m_series, &QAbstractBarSeries::clicked);
                connect(bar, &Bar::hovered, m_series, &QAbstractBarSeries::hovered);
                connect(bar, &Bar::pressed, m_series, &QAbstractBarSeries::pressed);
                connect(bar, &Bar::released, m_series, &QAbstractBarSeries::released);
                connect(bar, &Bar::doubleClicked, m_series, &QAbstractBarSeries::doubleClicked);

                connect(bar, &Bar::clicked, set, &QBarSet::clicked);
                connect(bar, &Bar::hovered, set, &QBarSet::hovered);
                connect(bar, &Bar::pressed, set, &QBarSet::pressed);
                connect(bar, &Bar::released, set, &QBarSet::released);
                connect(bar, &Bar::doubleClicked, set, &QBarSet::doubleClicked);

                m_labelItemsMissing = true;
            }
        }
        // Update bar indexes
        QHash<int, Bar*> indexMap;
        QVector<Bar *> unassignedBars(m_categoryCount, nullptr);
        int unassignedIndex(0);
        QList<Bar *> newBars;
        newBars.reserve(m_categoryCount);
        for (int c = 0; c < bars.size(); c++) {
            Bar *bar = bars.at(c);
            if (bar->index() < m_firstCategory || bar->index() > m_lastCategory) {
                // Delete excess unassigned bars first
                if (addCount < 0) {
                    addCount++;
                    delete bar;
                    bar = nullptr;
                } else {
                    unassignedBars[unassignedIndex++] = bar;
                    bar->setLayoutIndex(layoutIndex);
                    newBars.append(bar);
                    layoutIndex++;
                }
            } else {
                indexMap.insert(bar->index(), bar);
                newBars.append(bar);
                m_layout[layoutIndex] = oldLayout.at(bar->layoutIndex());
                bar->setLayoutIndex(layoutIndex);
                layoutIndex++;
            }
        }
        unassignedIndex = 0;
        for (int c = m_firstCategory; c <= m_lastCategory; c++) {
            Bar *bar = indexMap.value(c);
            if (!bar) {
                bar = unassignedBars.at(unassignedIndex++);
                bar->setIndex(c);
                indexMap.insert(bar->index(), bar);
            }
        }

        m_indexForBarMap.insert(set, indexMap);

        if (m_animation) {
            for (int i = 0; i < unassignedIndex; i++) {
                Bar *bar = unassignedBars.at(i);
                initializeLayout(s, bar->index(), bar->layoutIndex(), m_resetAnimation);
                bar->setRect(m_layout.at(bar->layoutIndex()));
                // Make bar initially hidden to avoid artifacts, layout setting will show it
                bar->setVisible(false);
            }
        }

        m_barMap.insert(set, newBars);
    }
}

void AbstractBarChartItem::markLabelsDirty(QBarSet *barset, int index, int count)
{
    Q_ASSERT(barset);

    if (index <= 0 && count < 0) {
        barset->d_ptr.data()->setLabelsDirty(true);
    } else {
        const QList<Bar *> bars = m_barMap.value(barset);
        const int maxIndex = count > 0 ? index + count : barset->count();
        for (int i = 0; i < bars.size(); i++) {
            Bar *bar = bars.at(i);
            if (bar->index() >= index && bar->index() < maxIndex)
                bar->setLabelDirty(true);
        }
    }
}

void AbstractBarChartItem::calculateSeriesPositionAdjustmentAndWidth()
{
    m_seriesPosAdjustment = 0.0;
    m_seriesWidth = 1.0;

    if (!m_series->chart())
        return;

    // Find out total number of bar series in chart and the index of this series among them
    const QList<QAbstractSeries *> seriesList = m_series->chart()->series();
    int index = -1;
    int count = 0;
    for (QAbstractSeries *series : seriesList) {
        if (qobject_cast<QAbstractBarSeries *>(series)){
            if (series == m_series)
                index = count;
            count++;
        }
    }
    if (index > -1 && count > 1) {
        m_seriesWidth = 1.0 / count;
        m_seriesPosAdjustment = (m_seriesWidth * index) + (m_seriesWidth / 2.0) - 0.5;
    }
}

#include "moc_abstractbarchartitem_p.cpp"

QT_CHARTS_END_NAMESPACE
