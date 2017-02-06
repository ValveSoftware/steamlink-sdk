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

#include "barrenderitem_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

BarRenderItem::BarRenderItem()
    : AbstractRenderItem(),
      m_value(0),
      m_height(0.0f)
{
}

BarRenderItem::BarRenderItem(const BarRenderItem &other)
    : AbstractRenderItem(other)
{
    m_value = other.m_value;
    m_position = other.m_position;
    m_height = other.m_height;
}

BarRenderItem::~BarRenderItem()
{
}

BarRenderSliceItem::BarRenderSliceItem()
    : BarRenderItem(),
      m_sliceLabelItem(0)
{
}

BarRenderSliceItem::BarRenderSliceItem(const BarRenderSliceItem &other)
    : BarRenderItem(other)
{
    m_sliceLabel = other.m_sliceLabel;
    m_sliceLabelItem = 0;
}

BarRenderSliceItem::~BarRenderSliceItem()
{
    delete m_sliceLabelItem;
}

void BarRenderSliceItem::setItem(const BarRenderItem &renderItem)
{
    m_translation = renderItem.translation();
    m_rotation = renderItem.rotation();
    m_value = renderItem.value();
    m_position = renderItem.position();
    m_height = renderItem.height();
    m_sliceLabel = QString();
    delete m_sliceLabelItem;
    m_sliceLabelItem = 0;
}

LabelItem &BarRenderSliceItem::sliceLabelItem()
{
    if (!m_sliceLabelItem)
        m_sliceLabelItem = new LabelItem;
    return *m_sliceLabelItem;
}

void BarRenderSliceItem::setSliceLabel(const QString &label)
{
    if (m_sliceLabelItem)
        m_sliceLabelItem->clear();
    m_sliceLabel = label;
}

const QString &BarRenderSliceItem::sliceLabel() const
{
    return m_sliceLabel;
}

QT_END_NAMESPACE_DATAVISUALIZATION
