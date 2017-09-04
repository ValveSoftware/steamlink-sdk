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


#include "declarativexyseries.h"
#include "declarativexypoint.h"
#include <QtCharts/QVXYModelMapper>
#include <QtCharts/QHXYModelMapper>

QT_CHARTS_BEGIN_NAMESPACE

DeclarativeXySeries::DeclarativeXySeries()
{
}

DeclarativeXySeries::~DeclarativeXySeries()
{
}

void DeclarativeXySeries::classBegin()
{
}

void DeclarativeXySeries::componentComplete()
{
    QXYSeries *series = qobject_cast<QXYSeries *>(xySeries());
    Q_ASSERT(series);

    foreach (QObject *child, series->children()) {
        if (qobject_cast<DeclarativeXYPoint *>(child)) {
            DeclarativeXYPoint *point = qobject_cast<DeclarativeXYPoint *>(child);
            series->append(point->x(), point->y());
        } else if (qobject_cast<QVXYModelMapper *>(child)) {
            QVXYModelMapper *mapper = qobject_cast<QVXYModelMapper *>(child);
            mapper->setSeries(series);
        } else if (qobject_cast<QHXYModelMapper *>(child)) {
            QHXYModelMapper *mapper = qobject_cast<QHXYModelMapper *>(child);
            mapper->setSeries(series);
        }
    }
}

void DeclarativeXySeries::append(qreal x, qreal y)
{
    QXYSeries *series = qobject_cast<QXYSeries *>(xySeries());
    Q_ASSERT(series);
    series->append(x, y);
}

void DeclarativeXySeries::replace(qreal oldX, qreal oldY, qreal newX, qreal newY)
{
    QXYSeries *series = qobject_cast<QXYSeries *>(xySeries());
    Q_ASSERT(series);
    series->replace(oldX, oldY, newX, newY);
}

void DeclarativeXySeries::replace(int index, qreal newX, qreal newY)
{
    QXYSeries *series = qobject_cast<QXYSeries *>(xySeries());
    Q_ASSERT(series);
    series->replace(index, newX, newY);
}

void DeclarativeXySeries::remove(qreal x, qreal y)
{
    QXYSeries *series = qobject_cast<QXYSeries *>(xySeries());
    Q_ASSERT(series);
    series->remove(x, y);
}

void DeclarativeXySeries::remove(int index)
{
    QXYSeries *series = qobject_cast<QXYSeries *>(xySeries());
    Q_ASSERT(series);
    series->remove(index);
}

void DeclarativeXySeries::removePoints(int index, int count)
{
    QXYSeries *series = qobject_cast<QXYSeries *>(xySeries());
    Q_ASSERT(series);
    series->removePoints(index, count);
}

void DeclarativeXySeries::insert(int index, qreal x, qreal y)
{
    QXYSeries *series = qobject_cast<QXYSeries *>(xySeries());
    Q_ASSERT(series);
    series->insert(index, QPointF(x, y));
}

void DeclarativeXySeries::clear()
{
    QXYSeries *series = qobject_cast<QXYSeries *>(xySeries());
    Q_ASSERT(series);
    series->clear();
}

QPointF DeclarativeXySeries::at(int index)
{
    QXYSeries *series = qobject_cast<QXYSeries *>(xySeries());
    Q_ASSERT(series);
    if (index >= 0 && index < series->count())
        return series->points().at(index);
    return QPointF(0, 0);
}

QT_CHARTS_END_NAMESPACE
