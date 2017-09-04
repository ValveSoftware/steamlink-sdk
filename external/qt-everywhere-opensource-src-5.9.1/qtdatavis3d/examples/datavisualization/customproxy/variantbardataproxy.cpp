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

#include "variantbardataproxy.h"

using namespace QtDataVisualization;

VariantBarDataProxy::VariantBarDataProxy() :
    QBarDataProxy()
{
}

VariantBarDataProxy::VariantBarDataProxy(VariantDataSet *newSet,
                                         VariantBarDataMapping *mapping) :
    QBarDataProxy()
{
    setDataSet(newSet);
    setMapping(mapping);
}

VariantBarDataProxy::~VariantBarDataProxy()
{
    delete m_dataSet;
}

void VariantBarDataProxy::setDataSet(VariantDataSet *newSet)
{
    if (!m_dataSet.isNull())
        QObject::disconnect(m_dataSet.data(), 0, this, 0);

    m_dataSet = newSet;

    if (!m_dataSet.isNull()) {
        QObject::connect(m_dataSet.data(), &VariantDataSet::itemsAdded, this,
                         &VariantBarDataProxy::handleItemsAdded);
        QObject::connect(m_dataSet.data(), &VariantDataSet::dataCleared, this,
                         &VariantBarDataProxy::handleDataCleared);
    }
    resolveDataSet();
}

VariantDataSet *VariantBarDataProxy::dataSet()
{
    return m_dataSet.data();
}

void VariantBarDataProxy::setMapping(VariantBarDataMapping *mapping)
{
    if (!m_mapping.isNull())
        QObject::disconnect(m_mapping.data(), &VariantBarDataMapping::mappingChanged, this,
                            &VariantBarDataProxy::handleMappingChanged);

    m_mapping = mapping;

    if (!m_mapping.isNull())
        QObject::connect(m_mapping.data(), &VariantBarDataMapping::mappingChanged, this,
                         &VariantBarDataProxy::handleMappingChanged);

    resolveDataSet();
}

VariantBarDataMapping *VariantBarDataProxy::mapping()
{
    return m_mapping.data();
}

void VariantBarDataProxy::handleItemsAdded(int index, int count)
{
    Q_UNUSED(index)
    Q_UNUSED(count)

    // Resolve new items
    resolveDataSet();
}

void VariantBarDataProxy::handleDataCleared()
{
    // Data cleared, reset array
    resetArray(0);
}

void VariantBarDataProxy::handleMappingChanged()
{
    resolveDataSet();
}

// Resolve entire dataset into QBarDataArray.
//! [0]
void VariantBarDataProxy::resolveDataSet()
{
    // If we have no data or mapping, or the categories are not defined, simply clear the array
    if (m_dataSet.isNull() || m_mapping.isNull() || !m_mapping->rowCategories().size()
            || !m_mapping->columnCategories().size()) {
        resetArray(0);
        return;
    }
    const VariantDataItemList &itemList = m_dataSet->itemList();

    int rowIndex = m_mapping->rowIndex();
    int columnIndex = m_mapping->columnIndex();
    int valueIndex = m_mapping->valueIndex();
    const QStringList &rowList = m_mapping->rowCategories();
    const QStringList &columnList = m_mapping->columnCategories();

    // Sort values into rows and columns
    typedef QHash<QString, float> ColumnValueMap;
    QHash <QString, ColumnValueMap> itemValueMap;
    foreach (const VariantDataItem *item, itemList) {
        itemValueMap[item->at(rowIndex).toString()][item->at(columnIndex).toString()]
                = item->at(valueIndex).toReal();
    }

    // Create a new data array in format the parent class understands
    QBarDataArray *newProxyArray = new QBarDataArray;
    foreach (QString rowKey, rowList) {
        QBarDataRow *newProxyRow = new QBarDataRow(columnList.size());
        for (int i = 0; i < columnList.size(); i++)
            (*newProxyRow)[i].setValue(itemValueMap[rowKey][columnList.at(i)]);
        newProxyArray->append(newProxyRow);
    }

    // Finally, reset the data array in the parent class
    resetArray(newProxyArray);
}
//! [0]
