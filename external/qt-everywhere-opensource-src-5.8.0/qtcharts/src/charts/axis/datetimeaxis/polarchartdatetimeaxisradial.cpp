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

#include <private/polarchartdatetimeaxisradial_p.h>
#include <private/chartpresenter_p.h>
#include <private/abstractchartlayout_p.h>
#include <QtCharts/QDateTimeAxis>

QT_CHARTS_BEGIN_NAMESPACE

PolarChartDateTimeAxisRadial::PolarChartDateTimeAxisRadial(QDateTimeAxis *axis, QGraphicsItem *item)
    : PolarChartAxisRadial(axis, item)
{
    QObject::connect(axis, SIGNAL(tickCountChanged(int)), this, SLOT(handleTickCountChanged(int)));
    QObject::connect(axis, SIGNAL(formatChanged(QString)), this, SLOT(handleFormatChanged(QString)));
}

PolarChartDateTimeAxisRadial::~PolarChartDateTimeAxisRadial()
{
}

QVector<qreal> PolarChartDateTimeAxisRadial::calculateLayout() const
{
    int tickCount = static_cast<QDateTimeAxis *>(axis())->tickCount();
    Q_ASSERT(tickCount >= 2);

    QVector<qreal> points;
    points.resize(tickCount);

    const qreal d = (axisGeometry().width() / 2) / qreal(tickCount - 1);

    for (int i = 0; i < tickCount; ++i) {
        qreal radialCoordinate = qreal(i) * d;
        points[i] = radialCoordinate;
    }

    return points;
}
void PolarChartDateTimeAxisRadial::createAxisLabels(const QVector<qreal> &layout)
{
    setLabels(createDateTimeLabels(min(), max(), layout.size(), static_cast<QDateTimeAxis *>(axis())->format()));
}

void PolarChartDateTimeAxisRadial::handleTickCountChanged(int tick)
{
    Q_UNUSED(tick);
    QGraphicsLayoutItem::updateGeometry();
    if (presenter())
        presenter()->layout()->invalidate();
}

void PolarChartDateTimeAxisRadial::handleFormatChanged(const QString &format)
{
    Q_UNUSED(format);
    QGraphicsLayoutItem::updateGeometry();
    if (presenter())
        presenter()->layout()->invalidate();
}

#include "moc_polarchartdatetimeaxisradial_p.cpp"

QT_CHARTS_END_NAMESPACE
