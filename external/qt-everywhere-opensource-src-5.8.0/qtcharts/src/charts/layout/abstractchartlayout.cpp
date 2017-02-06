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

#include <private/abstractchartlayout_p.h>
#include <private/chartpresenter_p.h>
#include <private/qlegend_p.h>
#include <private/chartaxiselement_p.h>
#include <private/charttitle_p.h>
#include <private/chartbackground_p.h>
#include <QtCore/QDebug>

QT_CHARTS_BEGIN_NAMESPACE

static const qreal golden_ratio = 0.4;

AbstractChartLayout::AbstractChartLayout(ChartPresenter *presenter)
    : m_presenter(presenter),
      m_margins(20, 20, 20, 20),
      m_minChartRect(0, 0, 200, 200)
{
}

AbstractChartLayout::~AbstractChartLayout()
{
}

void AbstractChartLayout::setGeometry(const QRectF &rect)
{
    if (!rect.isValid())
        return;

    if (m_presenter->chart()->isVisible()) {
        QList<ChartAxisElement *> axes = m_presenter->axisItems();
        ChartTitle *title = m_presenter->titleElement();
        QLegend *legend = m_presenter->legend();
        ChartBackground *background = m_presenter->backgroundElement();

        QRectF contentGeometry = calculateBackgroundGeometry(rect, background);

        contentGeometry = calculateContentGeometry(contentGeometry);

        if (title && title->isVisible())
            contentGeometry = calculateTitleGeometry(contentGeometry, title);

        if (legend->isAttachedToChart() && legend->isVisible())
            contentGeometry = calculateLegendGeometry(contentGeometry, legend);

        contentGeometry = calculateAxisGeometry(contentGeometry, axes);

        m_presenter->setGeometry(contentGeometry);
        if (m_presenter->chart()->chartType() == QChart::ChartTypeCartesian)
            static_cast<QGraphicsRectItem *>(m_presenter->plotAreaElement())->setRect(contentGeometry);
        else
            static_cast<QGraphicsEllipseItem *>(m_presenter->plotAreaElement())->setRect(contentGeometry);
    }

    QGraphicsLayout::setGeometry(rect);
}

QRectF AbstractChartLayout::calculateContentGeometry(const QRectF &geometry) const
{
    return geometry.adjusted(m_margins.left(), m_margins.top(), -m_margins.right(), -m_margins.bottom());
}

QRectF AbstractChartLayout::calculateContentMinimum(const QRectF &minimum) const
{
    return  minimum.adjusted(0, 0, m_margins.left() + m_margins.right(), m_margins.top() + m_margins.bottom());
}


QRectF AbstractChartLayout::calculateBackgroundGeometry(const QRectF &geometry, ChartBackground *background) const
{
    qreal left;
    qreal top;
    qreal right;
    qreal bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    QRectF backgroundGeometry = geometry.adjusted(left, top, -right, -bottom);
    if (background)
        background->setRect(backgroundGeometry);
    return backgroundGeometry;
}

QRectF AbstractChartLayout::calculateBackgroundMinimum(const QRectF &minimum) const
{
    qreal left;
    qreal top;
    qreal right;
    qreal bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    return minimum.adjusted(0, 0, left + right, top + bottom);
}

QRectF AbstractChartLayout::calculateLegendGeometry(const QRectF &geometry, QLegend *legend) const
{
    QSizeF size = legend->effectiveSizeHint(Qt::PreferredSize, QSizeF(-1, -1));
    QRectF legendRect;
    QRectF result;

    switch (legend->alignment()) {
    case Qt::AlignTop: {
        legendRect = QRectF(geometry.topLeft(), QSizeF(geometry.width(), size.height()));
        result = geometry.adjusted(0, legendRect.height(), 0, 0);
        break;
    }
    case Qt::AlignBottom: {
        legendRect = QRectF(QPointF(geometry.left(), geometry.bottom() - size.height()), QSizeF(geometry.width(), size.height()));
        result = geometry.adjusted(0, 0, 0, -legendRect.height());
        break;
    }
    case Qt::AlignLeft: {
        qreal width = qMin(size.width(), geometry.width() * golden_ratio);
        legendRect = QRectF(geometry.topLeft(), QSizeF(width, geometry.height()));
        result = geometry.adjusted(width, 0, 0, 0);
        break;
    }
    case Qt::AlignRight: {
        qreal width = qMin(size.width(), geometry.width() * golden_ratio);
        legendRect = QRectF(QPointF(geometry.right() - width, geometry.top()), QSizeF(width, geometry.height()));
        result = geometry.adjusted(0, 0, -width, 0);
        break;
    }
    default: {
        legendRect = QRectF(0, 0, 0, 0);
        result = geometry;
        break;
    }
    }

    legend->setGeometry(legendRect);

    return result;
}

QRectF AbstractChartLayout::calculateLegendMinimum(const QRectF &geometry, QLegend *legend) const
{
    QSizeF minSize = legend->effectiveSizeHint(Qt::MinimumSize, QSizeF(-1, -1));
    return geometry.adjusted(0, 0, minSize.width(), minSize.height());
}

QRectF AbstractChartLayout::calculateTitleGeometry(const QRectF &geometry, ChartTitle *title) const
{
    title->setGeometry(geometry);
    if (title->text().isEmpty()) {
        return geometry;
    } else {
        // Round to full pixel via QPoint to avoid one pixel clipping on the edge in some cases
        QPointF center((geometry.center() - title->boundingRect().center()).toPoint());

        title->setPos(center.x(), title->pos().y());
        return geometry.adjusted(0, title->boundingRect().height() + 1, 0, 0);
    }
}

QRectF AbstractChartLayout::calculateTitleMinimum(const QRectF &minimum, ChartTitle *title) const
{
    QSizeF min = title->sizeHint(Qt::MinimumSize);
    return  minimum.adjusted(0, 0, min.width(), min.height());
}

QSizeF AbstractChartLayout::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    Q_UNUSED(constraint);
    if (which == Qt::MinimumSize) {
        QList<ChartAxisElement *> axes = m_presenter->axisItems();
        ChartTitle *title = m_presenter->titleElement();
        QLegend *legend = m_presenter->legend();
        QRectF minimumRect(0, 0, 0, 0);
        minimumRect = calculateBackgroundMinimum(minimumRect);
        minimumRect = calculateContentMinimum(minimumRect);
        minimumRect = calculateTitleMinimum(minimumRect, title);
        minimumRect = calculateLegendMinimum(minimumRect, legend);
        minimumRect = calculateAxisMinimum(minimumRect, axes);
        return  minimumRect.united(m_minChartRect).size().toSize();
    }
    return QSize(-1, -1);
}

void AbstractChartLayout::setMargins(const QMargins &margins)
{
    if (m_margins != margins) {
        m_margins = margins;
        updateGeometry();
    }
}

QMargins AbstractChartLayout::margins() const
{
    return m_margins;
}

QT_CHARTS_END_NAMESPACE
