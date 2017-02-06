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

#include "variantbardatamapping.h"

VariantBarDataMapping::VariantBarDataMapping()
    : QObject(0),
      m_rowIndex(0),
      m_columnIndex(1),
      m_valueIndex(2)
{
}

VariantBarDataMapping::VariantBarDataMapping(const VariantBarDataMapping &other)
    : QObject(0),
      m_rowIndex(0),
      m_columnIndex(1),
      m_valueIndex(2)
{
    operator=(other);
}

VariantBarDataMapping::VariantBarDataMapping(int rowIndex, int columnIndex, int valueIndex,
                                             const QStringList &rowCategories,
                                             const QStringList &columnCategories)
    : QObject(0),
      m_rowIndex(0),
      m_columnIndex(1),
      m_valueIndex(2)
{
    m_rowIndex = rowIndex;
    m_columnIndex = columnIndex;
    m_valueIndex = valueIndex;
    m_rowCategories = rowCategories;
    m_columnCategories = columnCategories;
}

VariantBarDataMapping::~VariantBarDataMapping()
{
}

VariantBarDataMapping &VariantBarDataMapping::operator=(const VariantBarDataMapping &other)
{
    m_rowIndex = other.m_rowIndex;
    m_columnIndex = other.m_columnIndex;
    m_valueIndex = other.m_valueIndex;
    m_rowCategories = other.m_rowCategories;
    m_columnCategories = other.m_columnCategories;

    return *this;
}

void VariantBarDataMapping::setRowIndex(int index)
{
    m_rowIndex = index;
    emit mappingChanged();
}

int VariantBarDataMapping::rowIndex() const
{
    return m_rowIndex;
}

void VariantBarDataMapping::setColumnIndex(int index)
{
    m_columnIndex = index;
    emit mappingChanged();
}

int VariantBarDataMapping::columnIndex() const
{
    return m_columnIndex;
}

void VariantBarDataMapping::setValueIndex(int index)
{
    m_valueIndex = index;
    emit mappingChanged();
}

int VariantBarDataMapping::valueIndex() const
{
    return m_valueIndex;
}

void VariantBarDataMapping::setRowCategories(const QStringList &categories)
{
    m_rowCategories = categories;
    emit mappingChanged();
}

const QStringList &VariantBarDataMapping::rowCategories() const
{
    return m_rowCategories;
}

void VariantBarDataMapping::setColumnCategories(const QStringList &categories)
{
    m_columnCategories = categories;
    emit mappingChanged();
}

const QStringList &VariantBarDataMapping::columnCategories() const
{
    return m_columnCategories;
}

void VariantBarDataMapping::remap(int rowIndex, int columnIndex, int valueIndex,
                                  const QStringList &rowCategories,
                                  const QStringList &columnCategories)
{
    m_rowIndex = rowIndex;
    m_columnIndex = columnIndex;
    m_valueIndex = valueIndex;
    m_rowCategories = rowCategories;
    m_columnCategories = columnCategories;
    emit mappingChanged();
}
