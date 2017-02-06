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

#include "datasource.h"
#include <QtCore/QtMath>

QT_CHARTS_USE_NAMESPACE

DataSource::DataSource(QObject *parent) :
    QObject(parent),
    m_index(-1)
{
}

void DataSource::update(QXYSeries *series, int seriesIndex)
{
    if (series) {
        const QVector<QVector<QPointF> > &seriesData = m_data.at(seriesIndex);
        if (seriesIndex == 0)
            m_index++;
        if (m_index > seriesData.count() - 1)
            m_index = 0;

        QVector<QPointF> points = seriesData.at(m_index);
        // Use replace instead of clear + append, it's optimized for performance
        series->replace(points);
    }
}

void DataSource::handleSceneChanged()
{
    m_dataUpdater.start();
}

void DataSource::updateAllSeries()
{
    static int frameCount = 0;
    static QString labelText = QStringLiteral("FPS: %1");

    for (int i = 0; i < m_seriesList->size(); i++)
        update(m_seriesList->value(i), i);

    frameCount++;
    int elapsed = m_fpsTimer.elapsed();
    if (elapsed >= 1000) {
        elapsed = m_fpsTimer.restart();
        qreal fps = qreal(0.1 * int(10000.0 * (qreal(frameCount) / qreal(elapsed))));
        m_fpsLabel->setText(labelText.arg(QString::number(fps, 'f', 1)));
        m_fpsLabel->adjustSize();
        frameCount = 0;
    }
}

void DataSource::setInterval(int interval)
{
    m_dataUpdater.setInterval(interval);
}

void DataSource::startUpdates(QList<QXYSeries *> &seriesList, QLabel *fpsLabel, int interval)
{
    m_seriesList = &seriesList;
    m_fpsLabel = fpsLabel;

    m_dataUpdater.setInterval(interval);
    m_dataUpdater.setSingleShot(true);
    QObject::connect(&m_dataUpdater, &QTimer::timeout,
                     this, &DataSource::updateAllSeries);

    m_fpsTimer.start();

    m_data.resize(maxSeriesCount);
    updateAllSeries();
}

void DataSource::generateData(int seriesIndex, int rowCount, int colCount)
{
    // Remove previous data
    QVector<QVector<QPointF> > &seriesData = m_data[seriesIndex];
    seriesData.clear();
    seriesData.reserve(rowCount);

    qreal xAdjustment = 20.0 / (colCount * rowCount);
    qreal yMultiplier = 3.0 / qreal(maxSeriesCount);

    // Append the new data depending on the type
    qreal height = qreal(seriesIndex) * (10.0 / qreal(maxSeriesCount)) + 0.3;
    for (int i(0); i < rowCount; i++) {
        QVector<QPointF> points;
        points.reserve(colCount);
        for (int j(0); j < colCount; j++) {
            qreal x(0);
            qreal y(0);
            // data with sin + random component
            y = height + (yMultiplier * qSin(3.14159265358979 / 50 * j)
                          + (yMultiplier * (qreal) rand() / (qreal) RAND_MAX));
            // 0.000001 added to make values logaxis compatible
            x = 0.000001 + 20.0 * (qreal(j) / qreal(colCount)) + (xAdjustment * qreal(i));
            points.append(QPointF(x, y));
        }
        seriesData.append(points);
    }
}
