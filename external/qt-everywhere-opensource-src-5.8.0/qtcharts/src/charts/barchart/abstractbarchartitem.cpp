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
#include <QtCharts/QChart>
#include <private/chartpresenter_p.h>
#include <private/charttheme_p.h>
#include <private/baranimation_p.h>

#include <private/chartdataset_p.h>
#include <QtGui/QPainter>
#include <QtGui/QTextDocument>

QT_CHARTS_BEGIN_NAMESPACE

AbstractBarChartItem::AbstractBarChartItem(QAbstractBarSeries *series, QGraphicsItem* item) :
    ChartItem(series->d_func(),item),
    m_animation(0),
    m_series(series)
{

    setFlag(ItemClipsChildrenToShape);
    setFlag(QGraphicsItem::ItemIsSelectable);
    connect(series->d_func(), SIGNAL(updatedLayout()), this, SLOT(handleLayoutChanged()));
    connect(series->d_func(), SIGNAL(updatedBars()), this, SLOT(handleUpdatedBars()));
    connect(series->d_func(), SIGNAL(labelsVisibleChanged(bool)), this, SLOT(handleLabelsVisibleChanged(bool)));
    connect(series->d_func(), SIGNAL(restructuredBars()), this, SLOT(handleDataStructureChanged()));
    connect(series, SIGNAL(visibleChanged()), this, SLOT(handleVisibleChanged()));
    connect(series, SIGNAL(opacityChanged()), this, SLOT(handleOpacityChanged()));
    connect(series, SIGNAL(labelsFormatChanged(QString)), this, SLOT(handleUpdatedBars()));
    connect(series, SIGNAL(labelsFormatChanged(QString)), this, SLOT(positionLabels()));
    connect(series, SIGNAL(labelsPositionChanged(QAbstractBarSeries::LabelsPosition)),
            this, SLOT(handleLabelsPositionChanged()));
    connect(series, SIGNAL(labelsAngleChanged(qreal)), this, SLOT(positionLabels()));
    setZValue(ChartPresenter::BarSeriesZValue);
    handleDataStructureChanged();
    handleVisibleChanged();
    handleUpdatedBars();
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

void AbstractBarChartItem::applyLayout(const QVector<QRectF> &layout)
{
    QSizeF size = geometry().size();
    if (geometry().size().isValid()) {
        if (m_animation) {
            if (m_layout.count() == 0 || m_oldSize != size) {
                initializeLayout();
                m_oldSize = size;
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
}

void AbstractBarChartItem::setLayout(const QVector<QRectF> &layout)
{
    if (layout.count() != m_bars.count())
        return;

    m_layout = layout;

    for (int i = 0; i < m_bars.count(); i++)
        m_bars.at(i)->setRect(layout.at(i));

    positionLabels();
}
//handlers

void AbstractBarChartItem::handleDomainUpdated()
{
    m_domainMinX = domain()->minX();
    m_domainMaxX = domain()->maxX();
    m_domainMinY = domain()->minY();
    m_domainMaxY = domain()->maxY();

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
    QVector<QRectF> layout = calculateLayout();
    applyLayout(layout);
    handleUpdatedBars();
}

void AbstractBarChartItem::handleLabelsVisibleChanged(bool visible)
{
    foreach (QGraphicsTextItem *label, m_labels)
        label->setVisible(visible);
    update();
}

void AbstractBarChartItem::handleDataStructureChanged()
{
    foreach (QGraphicsItem *item, childItems())
        delete item;

    m_bars.clear();
    m_labels.clear();
    m_layout.clear();

    // Create new graphic items for bars
    for (int c = 0; c < m_series->d_func()->categoryCount(); c++) {
        for (int s = 0; s < m_series->count(); s++) {
            QBarSet *set = m_series->d_func()->barsetAt(s);

            // Bars
            Bar *bar = new Bar(set, c, this);
            m_bars.append(bar);
            connect(bar, SIGNAL(clicked(int,QBarSet*)), m_series, SIGNAL(clicked(int,QBarSet*)));
            connect(bar, SIGNAL(hovered(bool, int, QBarSet*)), m_series, SIGNAL(hovered(bool, int, QBarSet*)));
            connect(bar, SIGNAL(pressed(int, QBarSet*)), m_series, SIGNAL(pressed(int, QBarSet*)));
            connect(bar, SIGNAL(released(int, QBarSet*)),
                    m_series, SIGNAL(released(int, QBarSet*)));
            connect(bar, SIGNAL(doubleClicked(int, QBarSet*)),
                    m_series, SIGNAL(doubleClicked(int, QBarSet*)));
            connect(bar, SIGNAL(clicked(int,QBarSet*)), set, SIGNAL(clicked(int)));
            connect(bar, SIGNAL(hovered(bool, int, QBarSet*)), set, SIGNAL(hovered(bool, int)));
            connect(bar, SIGNAL(pressed(int, QBarSet*)), set, SIGNAL(pressed(int)));
            connect(bar, SIGNAL(released(int, QBarSet*)), set, SIGNAL(released(int)));
            connect(bar, SIGNAL(doubleClicked(int, QBarSet*)), set, SIGNAL(doubleClicked(int)));
            //            m_layout.append(QRectF(0, 0, 1, 1));

            // Labels
            QGraphicsTextItem *newLabel = new QGraphicsTextItem(this);
            newLabel->document()->setDocumentMargin(ChartPresenter::textMargin());
            m_labels.append(newLabel);
        }
    }

    if(themeManager()) themeManager()->updateSeries(m_series);
    handleLayoutChanged();
    handleVisibleChanged();
}

void AbstractBarChartItem::handleVisibleChanged()
{
    bool visible = m_series->isVisible();
    if (visible)
        handleLabelsVisibleChanged(m_series->isLabelsVisible());
    else
        handleLabelsVisibleChanged(visible);

    foreach (QGraphicsItem *bar, m_bars)
        bar->setVisible(visible);
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
        int categoryCount = m_series->d_func()->categoryCount();
        int setCount = m_series->count();
        int itemIndex(0);
        static const QString valueTag(QLatin1String("@value"));

        for (int category = 0; category < categoryCount; category++) {
            for (int set = 0; set < setCount; set++) {
                QBarSetPrivate *barSet = m_series->d_func()->barsetAt(set)->d_ptr.data();
                Bar *bar = m_bars.at(itemIndex);
                bar->setPen(barSet->m_pen);
                bar->setBrush(barSet->m_brush);
                bar->update();

                QGraphicsTextItem *label = m_labels.at(itemIndex);
                QString valueLabel;
                if (presenter()) { // At startup presenter is not yet set, yet somehow update comes
                    if (barSet->value(category) == 0) {
                        label->setVisible(false);
                    } else {
                        label->setVisible(m_series->isLabelsVisible());
                        if (m_series->labelsFormat().isEmpty()) {
                            valueLabel = presenter()->numberToString(barSet->value(category));
                        } else {
                            valueLabel = m_series->labelsFormat();
                            valueLabel.replace(valueTag,
                                           presenter()->numberToString(barSet->value(category)));
                        }
                    }
                }
                label->setHtml(valueLabel);
                label->setFont(barSet->m_labelFont);
                label->setDefaultTextColor(barSet->m_labelBrush.color());
                label->update();
                itemIndex++;
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

    QTransform transform;
    const qreal angle = m_series->d_func()->labelsAngle();
    if (angle != 0.0)
        transform.rotate(angle);

    for (int i = 0; i < m_layout.count(); i++) {
        QGraphicsTextItem *label = m_labels.at(i);

        QRectF labelRect = label->boundingRect();
        QPointF center = labelRect.center();

        qreal xPos = 0;
        qreal yPos = m_layout.at(i).center().y() - center.y();

        int xDiff = 0;
        if (angle != 0.0) {
            label->setTransformOriginPoint(center.x(), center.y());
            label->setRotation(m_series->d_func()->labelsAngle());
            qreal oldWidth = labelRect.width();
            labelRect = transform.mapRect(labelRect);
            xDiff = (labelRect.width() - oldWidth) / 2;
        }

        int offset = m_bars.at(i)->pen().width() / 2 + 2;

        switch (m_series->labelsPosition()) {
        case QAbstractBarSeries::LabelsCenter:
            xPos = m_layout.at(i).center().x() - center.x();
            break;
        case QAbstractBarSeries::LabelsInsideEnd:
            xPos = m_layout.at(i).right() - labelRect.width() - offset + xDiff;
            break;
        case QAbstractBarSeries::LabelsInsideBase:
            xPos = m_layout.at(i).left() + offset + xDiff;
            break;
        case QAbstractBarSeries::LabelsOutsideEnd:
            xPos = m_layout.at(i).right() + offset + xDiff;
            break;
        default:
            // Invalid position, never comes here
            break;
        }

        label->setPos(xPos, yPos);
        label->setZValue(zValue() + 1);
    }
}

void AbstractBarChartItem::positionLabelsVertical()
{
    QTransform transform;
    const qreal angle = m_series->d_func()->labelsAngle();
    if (angle != 0.0)
        transform.rotate(angle);

    for (int i = 0; i < m_layout.count(); i++) {
        QGraphicsTextItem *label = m_labels.at(i);

        QRectF labelRect = label->boundingRect();
        QPointF center = labelRect.center();

        qreal xPos = m_layout.at(i).center().x() - center.x();
        qreal yPos = 0;

        int yDiff = 0;
        if (angle != 0.0) {
            label->setTransformOriginPoint(center.x(), center.y());
            label->setRotation(m_series->d_func()->labelsAngle());
            qreal oldHeight = labelRect.height();
            labelRect = transform.mapRect(labelRect);
            yDiff = (labelRect.height() - oldHeight) / 2;
        }

        int offset = m_bars.at(i)->pen().width() / 2 + 2;

        switch (m_series->labelsPosition()) {
        case QAbstractBarSeries::LabelsCenter:
            yPos = m_layout.at(i).center().y() - center.y();
            break;
        case QAbstractBarSeries::LabelsInsideEnd:
            yPos = m_layout.at(i).top() + offset + yDiff;
            break;
        case QAbstractBarSeries::LabelsInsideBase:
            yPos = m_layout.at(i).bottom() - labelRect.height() - offset + yDiff;
            break;
        case QAbstractBarSeries::LabelsOutsideEnd:
            yPos = m_layout.at(i).top() - labelRect.height() - offset + yDiff;
            break;
        default:
            // Invalid position, never comes here
            break;
        }

        label->setPos(xPos, yPos);
        label->setZValue(zValue() + 1);
    }
}

#include "moc_abstractbarchartitem_p.cpp"

QT_CHARTS_END_NAMESPACE
