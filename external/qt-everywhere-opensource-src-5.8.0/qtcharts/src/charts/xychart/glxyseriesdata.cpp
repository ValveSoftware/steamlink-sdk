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

#include "private/glxyseriesdata_p.h"
#include "private/abstractdomain_p.h"
#include <QtCharts/QScatterSeries>

QT_CHARTS_BEGIN_NAMESPACE

GLXYSeriesDataManager::GLXYSeriesDataManager(QObject *parent)
    : QObject(parent),
      m_mapDirty(false)
{
}

GLXYSeriesDataManager::~GLXYSeriesDataManager()
{
    cleanup();
}

void GLXYSeriesDataManager::setPoints(QXYSeries *series, const AbstractDomain *domain)
{
    GLXYSeriesData *data = m_seriesDataMap.value(series);
    if (!data) {
        data = new GLXYSeriesData;
        data->type = series->type();
        data->visible = series->isVisible();
        QColor sc;
        if (data->type == QAbstractSeries::SeriesTypeScatter) {
            QScatterSeries *scatter = static_cast<QScatterSeries *>(series);
            data->width = float(scatter->markerSize());
            sc = scatter->color(); // Scatter overwrites color property
            connect(scatter, &QScatterSeries::colorChanged, this,
                    &GLXYSeriesDataManager::handleScatterColorChange);
            connect(scatter, &QScatterSeries::markerSizeChanged, this,
                    &GLXYSeriesDataManager::handleScatterMarkerSizeChange);
        } else {
            data->width = float(series->pen().widthF());
            sc = series->color();
            connect(series, &QXYSeries::penChanged, this,
                    &GLXYSeriesDataManager::handleSeriesPenChange);
        }
        data->color = QVector3D(float(sc.redF()), float(sc.greenF()), float(sc.blueF()));
        connect(series, &QXYSeries::useOpenGLChanged, this,
                &GLXYSeriesDataManager::handleSeriesOpenGLChange);
        connect(series, &QXYSeries::visibleChanged, this,
                &GLXYSeriesDataManager::handleSeriesVisibilityChange);
        m_seriesDataMap.insert(series, data);
        m_mapDirty = true;
    }
    QVector<float> &array = data->array;

    bool logAxis = false;
    bool reverseX = false;
    bool reverseY = false;
    foreach (QAbstractAxis* axis, series->attachedAxes()) {
        if (axis->type() == QAbstractAxis::AxisTypeLogValue) {
            logAxis = true;
        }
        if (axis->isReverse()) {
            if (axis->orientation() == Qt::Horizontal)
                reverseX = true;
            else
                reverseY = true;
            if (reverseX && reverseY)
                break;
        }
    }
    int count = series->count();
    int index = 0;
    array.resize(count * 2);
    QMatrix4x4 matrix;
    if (reverseX)
        matrix.scale(-1.0, 1.0);
    if (reverseY)
        matrix.scale(1.0, -1.0);
    data->matrix = matrix;
    if (logAxis) {
        // Use domain to resolve geometry points. Not as fast as shaders, but simpler that way
        QVector<QPointF> geometryPoints = domain->calculateGeometryPoints(series->pointsVector());
        const float height = domain->size().height();
        if (geometryPoints.size()) {
            for (int i = 0; i < count; i++) {
                const QPointF &point = geometryPoints.at(i);
                array[index++] = float(point.x());
                array[index++] = float(height - point.y());
            }
        } else {
            // If there are invalid log values, geometry points generation fails
            for (int i = 0; i < count; i++) {
                array[index++] = 0.0f;
                array[index++] = 0.0f;
            }
        }
        data->min = QVector2D(0, 0);
        data->delta = QVector2D(domain->size().width() / 2.0f, domain->size().height() / 2.0f);
    } else {
        // Regular value axes, so we can do the math easily on shaders.
        QVector<QPointF> seriesPoints = series->pointsVector();
        for (int i = 0; i < count; i++) {
            const QPointF &point = seriesPoints.at(i);
            array[index++] = float(point.x());
            array[index++] = float(point.y());
        }
        data->min = QVector2D(domain->minX(), domain->minY());
        data->delta = QVector2D((domain->maxX() - domain->minX()) / 2.0f,
                                (domain->maxY() - domain->minY()) / 2.0f);
    }
    data->dirty = true;
}

void GLXYSeriesDataManager::removeSeries(const QXYSeries *series)
{
    GLXYSeriesData *data = m_seriesDataMap.take(series);
    if (data) {
        disconnect(series, 0, this, 0);
        delete data;
        emit seriesRemoved(series);
        m_mapDirty = true;
    }
}

void GLXYSeriesDataManager::cleanup()
{
    foreach (GLXYSeriesData *data, m_seriesDataMap.values())
        delete data;
    m_seriesDataMap.clear();
    m_mapDirty = true;
    // Signal all series removal by using zero as parameter
    emit seriesRemoved(0);
}

void GLXYSeriesDataManager::handleSeriesPenChange()
{
    QXYSeries *series = qobject_cast<QXYSeries *>(sender());
    if (series) {
        GLXYSeriesData *data = m_seriesDataMap.value(series);
        if (data) {
            QColor sc = series->color();
            data->color = QVector3D(float(sc.redF()), float(sc.greenF()), float(sc.blueF()));
            data->width = float(series->pen().widthF());
            data->dirty = true;
        }
    }
}

void GLXYSeriesDataManager::handleSeriesOpenGLChange()
{
    QXYSeries *series = qobject_cast<QXYSeries *>(sender());
    if (!series->useOpenGL())
        removeSeries(series);
}

void GLXYSeriesDataManager::handleSeriesVisibilityChange()
{
    QXYSeries *series = qobject_cast<QXYSeries *>(sender());
    if (series) {
        GLXYSeriesData *data = m_seriesDataMap.value(series);
        if (data) {
            data->visible = series->isVisible();
            data->dirty = true;
        }
    }
}

void GLXYSeriesDataManager::handleScatterColorChange()
{
    QScatterSeries *series = qobject_cast<QScatterSeries *>(sender());
    if (series) {
        GLXYSeriesData *data = m_seriesDataMap.value(series);
        if (data) {
            QColor sc = series->color();
            data->color = QVector3D(float(sc.redF()), float(sc.greenF()), float(sc.blueF()));
            data->dirty = true;
        }
    }
}

void GLXYSeriesDataManager::handleScatterMarkerSizeChange()
{
    QScatterSeries *series = qobject_cast<QScatterSeries *>(sender());
    if (series) {
        GLXYSeriesData *data = m_seriesDataMap.value(series);
        if (data) {
            data->width =float(series->markerSize());
            data->dirty = true;
        }
    }
}

void GLXYSeriesDataManager::handleAxisReverseChanged(const QList<QAbstractSeries *> &seriesList)
{
    bool reverseX = false;
    bool reverseY = false;
    foreach (QAbstractSeries *series, seriesList) {
        if (QXYSeries *xyseries = qobject_cast<QXYSeries *>(series)) {
            GLXYSeriesData *data = m_seriesDataMap.value(xyseries);
            if (data) {
                foreach (QAbstractAxis* axis, xyseries->attachedAxes()) {
                    if (axis->isReverse()) {
                        if (axis->orientation() == Qt::Horizontal)
                            reverseX = true;
                        else
                            reverseY = true;
                    }
                    if (reverseX && reverseY)
                        break;
                }
                QMatrix4x4 matrix;
                if (reverseX)
                    matrix.scale(-1.0, 1.0);
                if (reverseY)
                    matrix.scale(1.0, -1.0);
                data->matrix = matrix;
                data->dirty = true;
            }
        }
    }
}

QT_CHARTS_END_NAMESPACE

