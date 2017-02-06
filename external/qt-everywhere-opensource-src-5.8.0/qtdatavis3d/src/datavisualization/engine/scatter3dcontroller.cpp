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

#include "scatter3dcontroller_p.h"
#include "scatter3drenderer_p.h"
#include "qvalue3daxis_p.h"
#include "qscatterdataproxy_p.h"
#include "qscatter3dseries_p.h"
#include <QtCore/QMutexLocker>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

static const int insertRemoveRecordReserveSize = 31;

Scatter3DController::Scatter3DController(QRect boundRect, Q3DScene *scene)
    : Abstract3DController(boundRect, scene),
      m_renderer(0),
      m_selectedItem(invalidSelectionIndex()),
      m_selectedItemSeries(0),
      m_recordInsertsAndRemoves(false)
{
    // Setting a null axis creates a new default axis according to orientation and graph type.
    // Note: These cannot be set in Abstract3DController constructor, as they will call virtual
    //       functions implemented by subclasses.
    setAxisX(0);
    setAxisY(0);
    setAxisZ(0);
}

Scatter3DController::~Scatter3DController()
{
}

void Scatter3DController::initializeOpenGL()
{
    QMutexLocker mutexLocker(&m_renderMutex);

    // Initialization is called multiple times when Qt Quick components are used
    if (isInitialized())
        return;

    m_renderer = new Scatter3DRenderer(this);
    setRenderer(m_renderer);

    mutexLocker.unlock();
    synchDataToRenderer();

    emitNeedRender();
}

void Scatter3DController::synchDataToRenderer()
{
    QMutexLocker mutexLocker(&m_renderMutex);

    if (!isInitialized())
        return;

    Abstract3DController::synchDataToRenderer();

    // Notify changes to renderer
    if (m_changeTracker.itemChanged) {
        m_renderer->updateItems(m_changedItems);
        m_changeTracker.itemChanged = false;
        m_changedItems.clear();
    }

    if (m_changeTracker.selectedItemChanged) {
        m_renderer->updateSelectedItem(m_selectedItem, m_selectedItemSeries);
        m_changeTracker.selectedItemChanged = false;
    }
}

void Scatter3DController::addSeries(QAbstract3DSeries *series)
{
    Q_ASSERT(series && series->type() == QAbstract3DSeries::SeriesTypeScatter);

    Abstract3DController::addSeries(series);

    QScatter3DSeries *scatterSeries =  static_cast<QScatter3DSeries *>(series);
    if (scatterSeries->selectedItem() != invalidSelectionIndex())
        setSelectedItem(scatterSeries->selectedItem(), scatterSeries);
}

void Scatter3DController::removeSeries(QAbstract3DSeries *series)
{
    bool wasVisible = (series && series->d_ptr->m_controller == this && series->isVisible());

    Abstract3DController::removeSeries(series);

    if (m_selectedItemSeries == series)
        setSelectedItem(invalidSelectionIndex(), 0);

    if (wasVisible)
        adjustAxisRanges();
}

QList<QScatter3DSeries *> Scatter3DController::scatterSeriesList()
{
    QList<QAbstract3DSeries *> abstractSeriesList = seriesList();
    QList<QScatter3DSeries *> scatterSeriesList;
    foreach (QAbstract3DSeries *abstractSeries, abstractSeriesList) {
        QScatter3DSeries *scatterSeries = qobject_cast<QScatter3DSeries *>(abstractSeries);
        if (scatterSeries)
            scatterSeriesList.append(scatterSeries);
    }

    return scatterSeriesList;
}

void Scatter3DController::handleArrayReset()
{
    QScatter3DSeries *series = static_cast<QScatterDataProxy *>(sender())->series();
    if (series->isVisible()) {
        adjustAxisRanges();
        m_isDataDirty = true;
    }
    if (!m_changedSeriesList.contains(series))
        m_changedSeriesList.append(series);
    setSelectedItem(m_selectedItem, m_selectedItemSeries);
    series->d_ptr->markItemLabelDirty();
    emitNeedRender();
}

void Scatter3DController::handleItemsAdded(int startIndex, int count)
{
    Q_UNUSED(startIndex)
    Q_UNUSED(count)
    QScatter3DSeries *series = static_cast<QScatterDataProxy *>(sender())->series();
    if (series->isVisible()) {
        adjustAxisRanges();
        m_isDataDirty = true;
    }
    if (!m_changedSeriesList.contains(series))
        m_changedSeriesList.append(series);
    emitNeedRender();
}

void Scatter3DController::handleItemsChanged(int startIndex, int count)
{
    QScatter3DSeries *series = static_cast<QScatterDataProxy *>(sender())->series();
    int oldChangeCount = m_changedItems.size();
    if (!oldChangeCount)
        m_changedItems.reserve(count);

    for (int i = 0; i < count; i++) {
        bool newItem = true;
        int candidate = startIndex + i;
        for (int j = 0; j < oldChangeCount; j++) {
            const ChangeItem &oldChangeItem = m_changedItems.at(j);
            if (oldChangeItem.index == candidate && series == oldChangeItem.series) {
                newItem = false;
                break;
            }
        }
        if (newItem) {
            ChangeItem newChangeItem = {series, candidate};
            m_changedItems.append(newChangeItem);
            if (series == m_selectedItemSeries && m_selectedItem == candidate)
                series->d_ptr->markItemLabelDirty();
        }
    }

    if (count) {
        m_changeTracker.itemChanged = true;
        if (series->isVisible())
            adjustAxisRanges();
        emitNeedRender();
    }
}

void Scatter3DController::handleItemsRemoved(int startIndex, int count)
{
    Q_UNUSED(startIndex)
    Q_UNUSED(count)
    QScatter3DSeries *series = static_cast<QScatterDataProxy *>(sender())->series();
    if (series == m_selectedItemSeries) {
        // If items removed from selected series before the selection, adjust the selection
        int selectedItem = m_selectedItem;
        if (startIndex <= selectedItem) {
            if ((startIndex + count) > selectedItem)
                selectedItem = -1; // Selected item removed
            else
                selectedItem -= count; // Move selected item down by amount of item removed

            setSelectedItem(selectedItem, m_selectedItemSeries);
        }
    }

    if (series->isVisible()) {
        adjustAxisRanges();
        m_isDataDirty = true;
    }
    if (!m_changedSeriesList.contains(series))
        m_changedSeriesList.append(series);

    if (m_recordInsertsAndRemoves) {
        InsertRemoveRecord record(false, startIndex, count, series);
        m_insertRemoveRecords.append(record);
    }

    emitNeedRender();
}

void Scatter3DController::handleItemsInserted(int startIndex, int count)
{
    Q_UNUSED(startIndex)
    Q_UNUSED(count)
    QScatter3DSeries *series = static_cast<QScatterDataProxy *>(sender())->series();
    if (series == m_selectedItemSeries) {
        // If items inserted to selected series before the selection, adjust the selection
        int selectedItem = m_selectedItem;
        if (startIndex <= selectedItem) {
            selectedItem += count;
            setSelectedItem(selectedItem, m_selectedItemSeries);
        }
    }

    if (series->isVisible()) {
        adjustAxisRanges();
        m_isDataDirty = true;
    }
    if (!m_changedSeriesList.contains(series))
        m_changedSeriesList.append(series);

    if (m_recordInsertsAndRemoves) {
        InsertRemoveRecord record(true, startIndex, count, series);
        m_insertRemoveRecords.append(record);
    }

    emitNeedRender();
}

void Scatter3DController::startRecordingRemovesAndInserts()
{
    m_recordInsertsAndRemoves = false;

    if (m_scene->selectionQueryPosition() != Q3DScene::invalidSelectionPoint()) {
        m_recordInsertsAndRemoves = true;
        if (m_insertRemoveRecords.size()) {
            m_insertRemoveRecords.clear();
            // Reserve some space for remove/insert records to avoid unnecessary reallocations.
            m_insertRemoveRecords.reserve(insertRemoveRecordReserveSize);
        }
    }
}

void Scatter3DController::handleAxisAutoAdjustRangeChangedInOrientation(
        QAbstract3DAxis::AxisOrientation orientation, bool autoAdjust)
{
    Q_UNUSED(orientation)
    Q_UNUSED(autoAdjust)
    adjustAxisRanges();
}

void Scatter3DController::handleAxisRangeChangedBySender(QObject *sender)
{
    Abstract3DController::handleAxisRangeChangedBySender(sender);

    // Update selected index - may be moved offscreen
    setSelectedItem(m_selectedItem, m_selectedItemSeries);
}

void Scatter3DController::handlePendingClick()
{
    int index = m_renderer->clickedIndex();
    QScatter3DSeries *series = static_cast<QScatter3DSeries *>(m_renderer->clickedSeries());

    // Adjust position according to recorded events
    int recordCount = m_insertRemoveRecords.size();
    if (recordCount) {
        for (int i = 0; i < recordCount; i++) {
            const InsertRemoveRecord &record = m_insertRemoveRecords.at(i);
            if (series == record.m_series && record.m_startIndex <= index) {
                if (record.m_isInsert) {
                    index += record.m_count;
                } else {
                    if ((record.m_startIndex + record.m_count) > index) {
                        index = -1; // Selected row removed
                        break;
                    } else {
                        index -= record.m_count; // Move selected item down by amount of items removed
                    }
                }
            }
        }
    }

    setSelectedItem(index, series);

    Abstract3DController::handlePendingClick();

    m_renderer->resetClickedStatus();
}

void Scatter3DController::setSelectionMode(QAbstract3DGraph::SelectionFlags mode)
{
    // We only support single item selection mode and no selection mode
    if (mode != QAbstract3DGraph::SelectionItem && mode != QAbstract3DGraph::SelectionNone) {
        qWarning("Unsupported selection mode - only none and item selection modes are supported.");
        return;
    }

    Abstract3DController::setSelectionMode(mode);
}

void Scatter3DController::setSelectedItem(int index, QScatter3DSeries *series)
{
    const QScatterDataProxy *proxy = 0;

    // Series may already have been removed, so check it before setting the selection.
    if (!m_seriesList.contains(series))
        series = 0;

    if (series)
        proxy = series->dataProxy();

    if (!proxy || index < 0 || index >= proxy->itemCount())
        index = invalidSelectionIndex();

    if (index != m_selectedItem || series != m_selectedItemSeries) {
        bool seriesChanged = (series != m_selectedItemSeries);
        m_selectedItem = index;
        m_selectedItemSeries = series;
        m_changeTracker.selectedItemChanged = true;

        // Clear selection from other series and finally set new selection to the specified series
        foreach (QAbstract3DSeries *otherSeries, m_seriesList) {
            QScatter3DSeries *scatterSeries = static_cast<QScatter3DSeries *>(otherSeries);
            if (scatterSeries != m_selectedItemSeries)
                scatterSeries->dptr()->setSelectedItem(invalidSelectionIndex());
        }
        if (m_selectedItemSeries)
            m_selectedItemSeries->dptr()->setSelectedItem(m_selectedItem);

        if (seriesChanged)
            emit selectedSeriesChanged(m_selectedItemSeries);

        emitNeedRender();
    }
}

void Scatter3DController::clearSelection()
{
    setSelectedItem(invalidSelectionIndex(), 0);
}

void Scatter3DController::adjustAxisRanges()
{
    QValue3DAxis *valueAxisX = static_cast<QValue3DAxis *>(m_axisX);
    QValue3DAxis *valueAxisY = static_cast<QValue3DAxis *>(m_axisY);
    QValue3DAxis *valueAxisZ = static_cast<QValue3DAxis *>(m_axisZ);
    bool adjustX = (valueAxisX && valueAxisX->isAutoAdjustRange());
    bool adjustY = (valueAxisY && valueAxisY->isAutoAdjustRange());
    bool adjustZ = (valueAxisZ && valueAxisZ->isAutoAdjustRange());

    if (adjustX || adjustY || adjustZ) {
        float minValueX = 0.0f;
        float maxValueX = 0.0f;
        float minValueY = 0.0f;
        float maxValueY = 0.0f;
        float minValueZ = 0.0f;
        float maxValueZ = 0.0f;
        int seriesCount = m_seriesList.size();
        for (int series = 0; series < seriesCount; series++) {
            const QScatter3DSeries *scatterSeries =
                    static_cast<QScatter3DSeries *>(m_seriesList.at(series));
            const QScatterDataProxy *proxy = scatterSeries->dataProxy();
            if (scatterSeries->isVisible() && proxy) {
                QVector3D minLimits;
                QVector3D maxLimits;
                proxy->dptrc()->limitValues(minLimits, maxLimits, valueAxisX, valueAxisY, valueAxisZ);
                if (adjustX) {
                    if (!series) {
                        // First series initializes the values
                        minValueX = minLimits.x();
                        maxValueX = maxLimits.x();
                    } else {
                        minValueX = qMin(minValueX, minLimits.x());
                        maxValueX = qMax(maxValueX, maxLimits.x());
                    }
                }
                if (adjustY) {
                    if (!series) {
                        // First series initializes the values
                        minValueY = minLimits.y();
                        maxValueY = maxLimits.y();
                    } else {
                        minValueY = qMin(minValueY, minLimits.y());
                        maxValueY = qMax(maxValueY, maxLimits.y());
                    }
                }
                if (adjustZ) {
                    if (!series) {
                        // First series initializes the values
                        minValueZ = minLimits.z();
                        maxValueZ = maxLimits.z();
                    } else {
                        minValueZ = qMin(minValueZ, minLimits.z());
                        maxValueZ = qMax(maxValueZ, maxLimits.z());
                    }
                }
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
