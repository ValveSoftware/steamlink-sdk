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

#include "axisrendercache_p.h"

#include <QtGui/QFontMetrics>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

AxisRenderCache::AxisRenderCache()
    : m_type(QAbstract3DAxis::AxisTypeNone),
      m_min(0.0f),
      m_max(10.0f),
      m_segmentCount(5),
      m_subSegmentCount(1),
      m_reversed(false),
      m_font(QFont(QStringLiteral("Arial"))),
      m_formatter(0),
      m_ctrlFormatter(0),
      m_drawer(0),
      m_positionsDirty(true),
      m_translate(0.0f),
      m_scale(1.0f),
      m_labelAutoRotation(0.0f),
      m_titleVisible(false),
      m_titleFixed(false)
{
}

AxisRenderCache::~AxisRenderCache()
{
    foreach (LabelItem *label, m_labelItems)
        delete label;
    m_titleItem.clear();

    delete m_formatter;
}

void AxisRenderCache::setDrawer(Drawer *drawer)
{
    m_drawer = drawer;
    m_font = m_drawer->font();
    if (m_drawer)
        updateTextures();
}

void AxisRenderCache::setType(QAbstract3DAxis::AxisType type)
{
    m_type = type;

    // If type is set, it means completely new axis instance, so clear all old data
    m_labels.clear();
    m_title.clear();
    m_min = 0.0f;
    m_max = 10.0f;
    m_segmentCount = 5;
    m_subSegmentCount = 1;
    m_labelFormat.clear();

    m_titleItem.clear();
    foreach (LabelItem *label, m_labelItems)
        delete label;
    m_labelItems.clear();
}

void AxisRenderCache::setTitle(const QString &title)
{
    if (m_title != title) {
        m_title = title;
        // Generate axis label texture
        if (m_drawer)
            m_drawer->generateLabelItem(m_titleItem, title);
    }
}

void AxisRenderCache::setLabels(const QStringList &labels)
{
    if (m_labels != labels) {
        int newSize(labels.size());
        int oldSize(m_labels.size());

        for (int i = newSize; i < oldSize; i++)
            delete m_labelItems.takeLast();

        m_labelItems.reserve(newSize);

        int widest = maxLabelWidth(labels);

        for (int i = 0; i < newSize; i++) {
            if (i >= oldSize)
                m_labelItems.append(new LabelItem);
            if (m_drawer) {
                if (labels.at(i).isEmpty()) {
                    m_labelItems[i]->clear();
                } else if (i >= oldSize || labels.at(i) != m_labels.at(i)
                           || m_labelItems[i]->size().width() != widest) {
                    m_drawer->generateLabelItem(*m_labelItems[i], labels.at(i), widest);
                }
            }
        }
        m_labels = labels;
    }
}

void AxisRenderCache::updateAllPositions()
{
    // As long as grid and subgrid lines are drawn identically, we can further optimize
    // by caching all grid and subgrid positions into a single vector.
    // If subgrid lines are ever themed separately, this array will probably become obsolete.
    if (m_formatter) {
        int gridCount = m_formatter->gridPositions().size();
        int subGridCount = m_formatter->subGridPositions().size();
        int labelCount = m_formatter->labelPositions().size();
        int fullSize = gridCount + subGridCount;

        m_adjustedGridLinePositions.resize(fullSize);
        m_adjustedLabelPositions.resize(labelCount);
        int index = 0;
        int grid = 0;
        int label = 0;
        float position = 0.0f;
        for (; label < labelCount; label++) {
            position = m_formatter->labelPositions().at(label);
            if (m_reversed)
                position = 1.0f - position;
            m_adjustedLabelPositions[label] = position * m_scale + m_translate;
        }
        for (; grid < gridCount; grid++) {
            position = m_formatter->gridPositions().at(grid);
            if (m_reversed)
                position = 1.0f - position;
            m_adjustedGridLinePositions[index++] = position * m_scale + m_translate;
        }
        for (int subGrid = 0; subGrid < subGridCount; subGrid++) {
            position = m_formatter->subGridPositions().at(subGrid);
            if (m_reversed)
                position = 1.0f - position;
            m_adjustedGridLinePositions[index++] = position * m_scale + m_translate;
        }

        m_positionsDirty = false;
    }
}

void AxisRenderCache::updateTextures()
{
    m_font = m_drawer->font();

    if (m_title.isEmpty())
        m_titleItem.clear();
    else
        m_drawer->generateLabelItem(m_titleItem, m_title);

    int widest = maxLabelWidth(m_labels);

    for (int i = 0; i < m_labels.size(); i++) {
        if (m_labels.at(i).isEmpty())
            m_labelItems[i]->clear();
        else
            m_drawer->generateLabelItem(*m_labelItems[i], m_labels.at(i), widest);
    }
}

void AxisRenderCache::clearLabels()
{
    m_titleItem.clear();
    for (int i = 0; i < m_labels.size(); i++)
        m_labelItems[i]->clear();
}

int AxisRenderCache::maxLabelWidth(const QStringList &labels) const
{
    int labelWidth = 0;
    QFont labelFont = m_font;
    labelFont.setPointSize(textureFontSize);
    QFontMetrics labelFM(labelFont);
    for (int i = 0; i < labels.size(); i++) {
        int newWidth = labelFM.width(labels.at(i));
        if (labelWidth < newWidth)
            labelWidth = newWidth;
    }
    return labelWidth;
}

QT_END_NAMESPACE_DATAVISUALIZATION
