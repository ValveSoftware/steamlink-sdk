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

#include <private/polarchartlogvalueaxisradial_p.h>
#include <private/abstractchartlayout_p.h>
#include <private/chartpresenter_p.h>
#include <QtCharts/QLogValueAxis>
#include <QtCore/QtMath>
#include <QtCore/QDebug>
#include <cmath>

QT_CHARTS_BEGIN_NAMESPACE

PolarChartLogValueAxisRadial::PolarChartLogValueAxisRadial(QLogValueAxis *axis, QGraphicsItem *item)
    : PolarChartAxisRadial(axis, item)
{
    QObject::connect(axis, SIGNAL(baseChanged(qreal)), this, SLOT(handleBaseChanged(qreal)));
    QObject::connect(axis, SIGNAL(labelFormatChanged(QString)), this, SLOT(handleLabelFormatChanged(QString)));
}

PolarChartLogValueAxisRadial::~PolarChartLogValueAxisRadial()
{
}

QVector<qreal> PolarChartLogValueAxisRadial::calculateLayout() const
{
    QLogValueAxis *logValueAxis = static_cast<QLogValueAxis *>(axis());
    const qreal logMax = std::log10(logValueAxis->max()) / std::log10(logValueAxis->base());
    const qreal logMin = std::log10(logValueAxis->min()) / std::log10(logValueAxis->base());
    const qreal innerEdge = logMin < logMax ? logMin : logMax;
    const qreal outerEdge = logMin > logMax ? logMin : logMax;
    const qreal delta = (axisGeometry().width() / 2.0) / qAbs(logMax - logMin);
    const qreal initialSpan = (qCeil(innerEdge) - innerEdge) * delta;
    int tickCount = qAbs(qCeil(logMax) - qCeil(logMin));

    // Extra tick if outer edge is exactly at the tick
    if (outerEdge == qCeil(outerEdge))
        tickCount++;

    QVector<qreal> points;
    points.resize(tickCount);

    for (int i = 0; i < tickCount; ++i) {
        qreal radialCoordinate = initialSpan + (delta * qreal(i));
        points[i] = radialCoordinate;
    }

    return points;
}

void PolarChartLogValueAxisRadial::createAxisLabels(const QVector<qreal> &layout)
{
    QLogValueAxis *logValueAxis = static_cast<QLogValueAxis *>(axis());
    setLabels(createLogValueLabels(logValueAxis->min(),
                                   logValueAxis->max(),
                                   logValueAxis->base(),
                                   layout.size(),
                                   logValueAxis->labelFormat()));
}

void PolarChartLogValueAxisRadial::handleBaseChanged(qreal base)
{
    Q_UNUSED(base);
    QGraphicsLayoutItem::updateGeometry();
    if (presenter())
        presenter()->layout()->invalidate();
}

void PolarChartLogValueAxisRadial::handleLabelFormatChanged(const QString &format)
{
    Q_UNUSED(format);
    QGraphicsLayoutItem::updateGeometry();
    if (presenter())
        presenter()->layout()->invalidate();
}

#include "moc_polarchartlogvalueaxisradial_p.cpp"

QT_CHARTS_END_NAMESPACE
