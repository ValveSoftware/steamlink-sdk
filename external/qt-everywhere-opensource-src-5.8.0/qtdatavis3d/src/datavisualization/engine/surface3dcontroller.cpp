/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
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

#include "surface3dcontroller_p.h"
#include "surface3drenderer_p.h"
#include "qvalue3daxis_p.h"
#include "qsurfacedataproxy_p.h"
#include "qsurface3dseries_p.h"
#include <QtCore/QMutexLocker>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

Surface3DController::Surface3DController(QRect rect, Q3DScene *scene)
    : Abstract3DController(rect, scene),
      m_renderer(0),
      m_selectedPoint(invalidSelectionPosition()),
      m_selectedSeries(0),
      m_flatShadingSupported(true),
      m_flipHorizontalGrid(false)
{
    // Setting a null axis creates a new default axis according to orientation and graph type.
    // Note: these cannot be set in the Abstract3DController constructor, as they will call virtual
    //       functions implemented by subclasses.
    setAxisX(0);
    setAxisY(0);
    setAxisZ(0);
}

Surface3DController::~Surface3DController()
{
}

void Surface3DController::initializeOpenGL()
{
    QMutexLocker mutexLocker(&m_renderMutex);

    // Initialization is called multiple times when Qt Quick components are used
    if (isInitialized())
        return;

    m_renderer = new Surface3DRenderer(this);
    setRenderer(m_renderer);

    emitNeedRender();
}

void Surface3DController::synchDataToRenderer()
{
    QMutexLocker mutexLocker(&m_renderMutex);

    if (!isInitialized())
        return;

    Abstract3DController::synchDataToRenderer();

    // Notify changes to renderer
    if (m_changeTracker.rowsChanged) {
        m_renderer->updateRows(m_changedRows);
        m_changeTracker.rowsChanged = false;
        m_changedRows.clear();
    }

    if (m_changeTracker.itemChanged) {
        m_renderer->updateItems(m_changedItems);
        m_changeTracker.itemChanged = false;
        m_changedItems.clear();
    }

    if (m_changeTracker.selectedPointChanged) {
        m_renderer->updateSelectedPoint(m_selectedPoint, m_selectedSeries);
        m_changeTracker.selectedPointChanged = false;
    }

    if (m_changeTracker.flipHorizontalGridChanged) {
        m_renderer->updateFlipHorizontalGrid(m_flipHorizontalGrid);
        m_changeTracker.flipHorizontalGridChanged = false;
    }

    if (m_changeTracker.surfaceTextureChanged) {
        m_renderer->updateSurfaceTextures(m_changedTextures);
        m_changeTracker.surfaceTextureChanged = false;
        m_changedTextures.clear();
    }
}

void Surface3DController::handleAxisAutoAdjustRangeChangedInOrientation(
        QAbstract3DAxis::AxisOrientation orientation, bool autoAdjust)
{
    Q_UNUSED(orientation)
    Q_UNUSED(autoAdjust)

    adjustAxisRanges();
}

void Surface3DController::handleAxisRangeChangedBySender(QObject *sender)
{
    Abstract3DController::handleAxisRangeChangedBySender(sender);

    // Update selected point - may be moved offscreen
    setSelectedPoint(m_selectedPoint, m_selectedSeries, false);
}

void Surface3DController::handleSeriesVisibilityChangedBySender(QObject *sender)
{
    Abstract3DController::handleSeriesVisibilityChangedBySender(sender);

    // Visibility changes may require disabling slicing,
    // so just reset selection to ensure everything is still valid.
    setSelectedPoint(m_selectedPoint, m_selectedSeries, false);
}

void Surface3DController::handlePendingClick()
{
    // This function is called while doing the sync, so it is okay to query from renderer
    QPoint position = m_renderer->clickedPosition();
    QSurface3DSeries *series = static_cast<QSurface3DSeries *>(m_renderer->clickedSeries());

    setSelectedPoint(position, series, true);

    Abstract3DController::handlePendingClick();

    m_renderer->resetClickedStatus();
}

QPoint Surface3DController::invalidSelectionPosition()
{
    static QPoint invalidSelectionPoint(-1, -1);
    return invalidSelectionPoint;
}

bool Surface3DController::isFlatShadingSupported()
{
    return m_flatShadingSupported;
}

void Surface3DController::addSeries(QAbstract3DSeries *series)
{
    Q_ASSERT(series && series->type() == QAbstract3DSeries::SeriesTypeSurface);

    Abstract3DController::addSeries(series);

    QSurface3DSeries *surfaceSeries = static_cast<QSurface3DSeries *>(series);
    if (surfaceSeries->selectedPoint() != invalidSelectionPosition())
        setSelectedPoint(surfaceSeries->selectedPoint(), surfaceSeries, false);

    if (!surfaceSeries->texture().isNull())
        updateSurfaceTexture(surfaceSeries);
}

void Surface3DController::removeSeries(QAbstract3DSeries *series)
{
    bool wasVisible = (series && series->d_ptr->m_controller == this && series->isVisible());

    Abstract3DController::removeSeries(series);

    if (m_selectedSeries == series)
        setSelectedPoint(invalidSelectionPosition(), 0, false);

    if (wasVisible)
        adjustAxisRanges();
}

QList<QSurface3DSeries *> Surface3DController::surfaceSeriesList()
{
    QList<QAbstract3DSeries *> abstractSeriesList = seriesList();
    QList<QSurface3DSeries *> surfaceSeriesList;
    foreach (QAbstract3DSeries *abstractSeries, abstractSeriesList) {
        QSurface3DSeries *surfaceSeries = qobject_cast<QSurface3DSeries *>(abstractSeries);
        if (surfaceSeries)
            surfaceSeriesList.append(surfaceSeries);
    }

    return surfaceSeriesList;
}

void Surface3DController::setFlipHorizontalGrid(bool flip)
{
    if (m_flipHorizontalGrid != flip) {
        m_flipHorizontalGrid = flip;
        m_changeTracker.flipHorizontalGridChanged = true;
        emit flipHorizontalGridChanged(flip);
        emitNeedRender();
    }
}

bool Surface3DController::flipHorizontalGrid() const
{
    return m_flipHorizontalGrid;
}

void Surface3DController::setSelectionMode(QAbstract3DGraph::SelectionFlags mode)
{
    // Currently surface only supports row and column modes when also slicing
    if ((mode.testFlag(QAbstract3DGraph::SelectionRow)
         || mode.testFlag(QAbstract3DGraph::SelectionColumn))
            && !mode.testFlag(QAbstract3DGraph::SelectionSlice)) {
        qWarning("Unsupported selection mode.");
        return;
    } else if (mode.testFlag(QAbstract3DGraph::SelectionSlice)
               && (mode.testFlag(QAbstract3DGraph::SelectionRow)
                   == mode.testFlag(QAbstract3DGraph::SelectionColumn))) {
        qWarning("Must specify one of either row or column selection mode in conjunction with slicing mode.");
    } else {
        QAbstract3DGraph::SelectionFlags oldMode = selectionMode();

        Abstract3DController::setSelectionMode(mode);

        if (mode != oldMode) {
            // Refresh selection upon mode change to ensure slicing is correctly updated
            // according to series the visibility.
            setSelectedPoint(m_selectedPoint, m_selectedSeries, true);

            // Special case: Always deactivate slicing when changing away from slice
            // automanagement, as this can't be handled in setSelectedBar.
            if (!mode.testFlag(QAbstract3DGraph::SelectionSlice)
                    && oldMode.testFlag(QAbstract3DGraph::SelectionSlice)) {
                scene()->setSlicingActive(false);
            }
        }
    }
}

void Surface3DController::setSelectedPoint(const QPoint &position, QSurface3DSeries *series,
                                           bool enterSlice)
{
    // If the selection targets non-existent point, clear selection instead.
    QPoint pos = position;

    // Series may already have been removed, so check it before setting the selection.
    if (!m_seriesList.contains(series))
        series = 0;

    const QSurfaceDataProxy *proxy = 0;
    if (series)
        proxy = series->dataProxy();

    if (!proxy)
        pos = invalidSelectionPosition();

    if (pos != invalidSelectionPosition()) {
        int maxRow = proxy->rowCount() - 1;
        int maxCol = proxy->columnCount() - 1;

        if (pos.x() < 0 || pos.x() > maxRow || pos.y() < 0 || pos.y() > maxCol)
            pos = invalidSelectionPosition();
    }

    if (selectionMode().testFlag(QAbstract3DGraph::SelectionSlice)) {
        if (pos == invalidSelectionPosition() || !series->isVisible()) {
            scene()->setSlicingActive(false);
        } else {
            // If the selected point is outside data window, or there is no selected point, disable slicing
            float axisMinX = m_axisX->min();
            float axisMaxX = m_axisX->max();
            float axisMinZ = m_axisZ->min();
            float axisMaxZ = m_axisZ->max();

            QSurfaceDataItem item = proxy->array()->at(pos.x())->at(pos.y());
            if (item.x() < axisMinX || item.x() > axisMaxX
                    || item.z() < axisMinZ || item.z() > axisMaxZ) {
                scene()->setSlicingActive(false);
            } else if (enterSlice) {
                scene()->setSlicingActive(true);
            }
        }
        emitNeedRender();
    }

    if (pos != m_selectedPoint || series != m_selectedSeries) {
        bool seriesChanged = (series != m_selectedSeries);
        m_selectedPoint = pos;
        m_selectedSeries = series;
        m_changeTracker.selectedPointChanged = true;

        // Clear selection from other series and finally set new selection to the specified series
        foreach (QAbstract3DSeries *otherSeries, m_seriesList) {
            QSurface3DSeries *surfaceSeries = static_cast<QSurface3DSeries *>(otherSeries);
            if (surfaceSeries != m_selectedSeries)
                surfaceSeries->dptr()->setSelectedPoint(invalidSelectionPosition());
        }
        if (m_selectedSeries)
            m_selectedSeries->dptr()->setSelectedPoint(m_selectedPoint);

        if (seriesChanged)
            emit selectedSeriesChanged(m_selectedSeries);

        emitNeedRender();
    }
}

void Surface3DController::clearSelection()
{
    setSelectedPoint(invalidSelectionPosition(), 0, false);
}

void Surface3DController::handleArrayReset()
{
    QSurface3DSeries *series = static_cast<QSurfaceDataProxy *>(sender())->series();
    if (series->isVisible()) {
        adjustAxisRanges();
        m_isDataDirty = true;
    }
    if (!m_changedSeriesList.contains(series))
        m_changedSeriesList.append(series);

    // Clear selection unless still valid
    setSelectedPoint(m_selectedPoint, m_selectedSeries, false);
    series->d_ptr->markItemLabelDirty();
    emitNeedRender();
}

void Surface3DController::handleFlatShadingSupportedChange(bool supported)
{
    // Handle renderer flat surface support indicator signal. This happens exactly once per renderer.
    if (m_flatShadingSupported != supported) {
        m_flatShadingSupported = supported;
        // Emit the change for all added surfaces
        foreach (QAbstract3DSeries *series, m_seriesList) {
            QSurface3DSeries *surfaceSeries = static_cast<QSurface3DSeries *>(series);
            emit surfaceSeries->flatShadingSupportedChanged(m_flatShadingSupported);
        }
    }
}

void Surface3DController::handleRowsChanged(int startIndex, int count)
{
    QSurface3DSeries *series = static_cast<QSurfaceDataProxy *>(QObject::sender())->series();
    int oldChangeCount = m_changedRows.size();
    if (!oldChangeCount)
        m_changedRows.reserve(count);

    int selectedRow = m_selectedPoint.x();
    for (int i = 0; i < count; i++) {
        bool newItem = true;
        int candidate = startIndex + i;
        for (int j = 0; j < oldChangeCount; j++) {
            const ChangeRow &oldChangeItem = m_changedRows.at(j);
            if (oldChangeItem.row == candidate && series == oldChangeItem.series) {
                newItem = false;
                break;
            }
        }
        if (newItem) {
            ChangeRow newChangeItem = {series, candidate};
            m_changedRows.append(newChangeItem);
            if (series == m_selectedSeries && selectedRow == candidate)
                series->d_ptr->markItemLabelDirty();
        }
    }
    if (count) {
        m_changeTracker.rowsChanged = true;

        if (series->isVisible())
            adjustAxisRanges();
        emitNeedRender();
    }
}

void Surface3DController::handleItemChanged(int rowIndex, int columnIndex)
{
    QSurfaceDataProxy *sender = static_cast<QSurfaceDataProxy *>(QObject::sender());
    QSurface3DSeries *series = sender->series();

    bool newItem = true;
    QPoint candidate(rowIndex, columnIndex);
    foreach (ChangeItem item, m_changedItems) {
        if (item.point == candidate && item.series == series) {
            newItem = false;
            break;
        }
    }
    if (newItem) {
        ChangeItem newItem = {series, candidate};
        m_changedItems.append(newItem);
        m_changeTracker.itemChanged = true;

        if (series == m_selectedSeries && m_selectedPoint == candidate)
            series->d_ptr->markItemLabelDirty();

        if (series->isVisible())
            adjustAxisRanges();
        emitNeedRender();
    }
}

void Surface3DController::handleRowsAdded(int startIndex, int count)
{
    Q_UNUSED(startIndex)
    Q_UNUSED(count)
    QSurface3DSeries *series = static_cast<QSurfaceDataProxy *>(sender())->series();
    if (series->isVisible()) {
        adjustAxisRanges();
        m_isDataDirty = true;
    }
    if (!m_changedSeriesList.contains(series))
        m_changedSeriesList.append(series);
    emitNeedRender();
}

void Surface3DController::handleRowsInserted(int startIndex, int count)
{
    Q_UNUSED(startIndex)
    Q_UNUSED(count)
    QSurface3DSeries *series = static_cast<QSurfaceDataProxy *>(sender())->series();
    if (series == m_selectedSeries) {
        // If rows inserted to selected series before the selection, adjust the selection
        int selectedRow = m_selectedPoint.x();
        if (startIndex <= selectedRow) {
            selectedRow += count;
            setSelectedPoint(QPoint(selectedRow, m_selectedPoint.y()), m_selectedSeries, false);
        }
    }

    if (series->isVisible()) {
        adjustAxisRanges();
        m_isDataDirty = true;
    }
    if (!m_changedSeriesList.contains(series))
        m_changedSeriesList.append(series);

    emitNeedRender();
}

void Surface3DController::handleRowsRemoved(int startIndex, int count)
{
    Q_UNUSED(startIndex)
    Q_UNUSED(count)
    QSurface3DSeries *series = static_cast<QSurfaceDataProxy *>(sender())->series();
    if (series == m_selectedSeries) {
        // If rows removed from selected series before the selection, adjust the selection
        int selectedRow = m_selectedPoint.x();
        if (startIndex <= selectedRow) {
            if ((startIndex + count) > selectedRow)
                selectedRow = -1; // Selected row removed
            else
                selectedRow -= count; // Move selected row down by amount of rows removed

            setSelectedPoint(QPoint(selectedRow, m_selectedPoint.y()), m_selectedSeries, false);
        }
    }

    if (series->isVisible()) {
        adjustAxisRanges();
        m_isDataDirty = true;
    }
    if (!m_changedSeriesList.contains(series))
        m_changedSeriesList.append(series);

    emitNeedRender();
}

void Surface3DController::updateSurfaceTexture(QSurface3DSeries *series)
{
    m_changeTracker.surfaceTextureChanged = true;

    if (!m_changedTextures.contains(series))
        m_changedTextures.append(series);

    emitNeedRender();
}

void Surface3DController::adjustAxisRanges()
{
    QValue3DAxis *valueAxisX = static_cast<QValue3DAxis *>(m_axisX);
    QValue3DAxis *valueAxisY = static_cast<QValue3DAxis *>(m_axisY);
    QValue3DAxis *valueAxisZ = static_cast<QValue3DAxis *>(m_axisZ);
    bool adjustX = (valueAxisX && valueAxisX->isAutoAdjustRange());
    bool adjustY = (valueAxisY && valueAxisY->isAutoAdjustRange());
    bool adjustZ = (valueAxisZ && valueAxisZ->isAutoAdjustRange());
    bool first = true;

    if (adjustX || adjustY || adjustZ) {
        float minValueX = 0.0f;
        float maxValueX = 0.0f;
        float minValueY = 0.0f;
        float maxValueY = 0.0f;
        float minValueZ = 0.0f;
        float maxValueZ = 0.0f;
        int seriesCount = m_seriesList.size();
        for (int series = 0; series < seriesCount; series++) {
            const QSurface3DSeries *surfaceSeries =
                    static_cast<QSurface3DSeries *>(m_seriesList.at(series));
            const QSurfaceDataProxy *proxy = surfaceSeries->dataProxy();
            if (surfaceSeries->isVisible() && proxy) {
                QVector3D minLimits;
                QVector3D maxLimits;
                proxy->dptrc()->limitValues(minLimits, maxLimits, valueAxisX, valueAxisY, valueAxisZ);
                if (adjustX) {
                    if (first) {
                        // First series initializes the values
                        minValueX = minLimits.x();
                        maxValueX = maxLimits.x();
                    } else {
                        minValueX = qMin(minValueX, minLimits.x());
                        maxValueX = qMax(maxValueX, maxLimits.x());
                    }
                }
                if (adjustY) {
                    if (first) {
                        // First series initializes the values
                        minValueY = minLimits.y();
                        maxValueY = maxLimits.y();
                    } else {
                        minValueY = qMin(minValueY, minLimits.y());
                        maxValueY = qMax(maxValueY, maxLimits.y());
                    }
                }
                if (adjustZ) {
                    if (first) {
                        // First series initializes the values
                        minValueZ = minLimits.z();
                        maxValueZ = maxLimits.z();
                    } else {
                        minValueZ = qMin(minValueZ, minLimits.z());
                        maxValueZ = qMax(maxValueZ, maxLimits.z());
                    }
                }
                first = false;
            }
        }

        static const float adjustmentRatio = 20.0f;
        static const float defaultAdjustment = 1.0f;

        if (adjustX) {
            // If all points at same coordinate, need to default to some valid range
            float adjustment = 0.0f;
            if (minValueX == maxValueX) {
                if (adjustZ) {
                    // X and Z are linked to have similar unit size, so choose the valid range based on it
                    if (minValueZ == maxValueZ)
                        adjustment = defaultAdjustment;
                    else
                        adjustment = qAbs(maxValueZ - minValueZ) / adjustmentRatio;
                } else {
                    if (valueAxisZ)
                        adjustment = qAbs(valueAxisZ->max() - valueAxisZ->min()) / adjustmentRatio;
                    else
                        adjustment = defaultAdjustment;
                }
            }
            valueAxisX->dptr()->setRange(minValueX - adjustment, maxValueX + adjustment, true);
        }
        if (adjustY) {
            // If all points at same coordinate, need to default to some valid range
            // Y-axis unit is not dependent on other axes, so simply adjust +-1.0f
            float adjustment = 0.0f;
            if (minValueY == maxValueY)
                adjustment = defaultAdjustment;
            valueAxisY->dptr()->setRange(minValueY - adjustment, maxValueY + adjustment, true);
        }
        if (adjustZ) {
            // If all points at same coordinate, need to default to some valid range
            float adjustment = 0.0f;
            if (minValueZ == maxValueZ) {
                if (adjustX) {
                    // X and Z are linked to have similar unit size, so choose the valid range based on it
                    if (minValueX == maxValueX)
                        adjustment = defaultAdjustment;
                    else
                        adjustment = qAbs(maxValueX - minValueX) / adjustmentRatio;
                } else {
                    if (valueAxisX)
                        adjustment = qAbs(valueAxisX->max() - valueAxisX->min()) / adjustmentRatio;
                    else
                        adjustment = defaultAdjustment;
                }
            }
            valueAxisZ->dptr()->setRange(minValueZ - adjustment, maxValueZ + adjustment, true);
        }
    }
}

QT_END_NAMESPACE_DATAVISUALIZATION
