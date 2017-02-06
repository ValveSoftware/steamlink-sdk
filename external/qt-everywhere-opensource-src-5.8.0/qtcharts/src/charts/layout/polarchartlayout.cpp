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

#include <private/polarchartlayout_p.h>
#include <private/chartpresenter_p.h>
#include <private/polarchartaxis_p.h>
#include <QtCore/QDebug>

QT_CHARTS_BEGIN_NAMESPACE

static const qreal golden_ratio = 0.4;

PolarChartLayout::PolarChartLayout(ChartPresenter *presenter)
    : AbstractChartLayout(presenter)
{
}

PolarChartLayout::~PolarChartLayout()
{
}

QRectF PolarChartLayout::calculateAxisGeometry(const QRectF &geometry, const QList<ChartAxisElement *> &axes) const
{
    // How to handle multiple angular/radial axes?
    qreal axisRadius = geometry.height() / 2.0;
    if (geometry.width() < geometry.height())
        axisRadius = geometry.width() / 2.0;

    int titleHeight = 0;
    foreach (ChartAxisElement *chartAxis, axes) {
        if (!chartAxis->isVisible())
            continue;

        PolarChartAxis *polarChartAxis = static_cast<PolarChartAxis *>(chartAxis);
        qreal radius = polarChartAxis->preferredAxisRadius(geometry.size());
        if (radius < axisRadius)
            axisRadius = radius;

        if (chartAxis->axis()->orientation() == Qt::Horizontal
            && chartAxis->axis()->isTitleVisible()
            && !chartAxis->axis()->titleText().isEmpty()) {
            // If axis has angular title, adjust geometry down by the space title takes
            QRectF dummyRect = ChartPresenter::textBoundingRect(chartAxis->axis()->titleFont(), chartAxis->axis()->titleText());
            titleHeight = (dummyRect.height() / 2.0) + chartAxis->titlePadding();
        }
    }

    QRectF axisRect;
    axisRect.setSize(QSizeF(axisRadius * 2.0, axisRadius * 2.0));
    axisRect.moveCenter(geometry.center());
    axisRect.adjust(0, titleHeight, 0, titleHeight);

    foreach (ChartAxisElement *chartAxis, axes)
        chartAxis->setGeometry(axisRect, QRectF());

    return axisRect;
}

QRectF PolarChartLayout::calculateAxisMinimum(const QRectF &minimum, const QList<ChartAxisElement *> &axes) const
{
    Q_UNUSED(axes);
    return minimum;
}

QT_CHARTS_END_NAMESPACE
