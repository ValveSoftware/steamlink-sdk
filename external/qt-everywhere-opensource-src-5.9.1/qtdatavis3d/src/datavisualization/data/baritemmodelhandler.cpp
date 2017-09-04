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

#include "baritemmodelhandler_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

static const int noRoleIndex = -1;

BarItemModelHandler::BarItemModelHandler(QItemModelBarDataProxy *proxy, QObject *parent)
    : AbstractItemModelHandler(parent),
      m_proxy(proxy),
      m_proxyArray(0),
      m_columnCount(0),
      m_valueRole(noRoleIndex),
      m_rotationRole(noRoleIndex),
      m_haveValuePattern(false),
      m_haveRotationPattern(false)
{
}

BarItemModelHandler::~BarItemModelHandler()
{
}

void BarItemModelHandler::handleDataChanged(const QModelIndex &topLeft,
                                            const QModelIndex &bottomRight,
                                            const QVector<int> &roles)
{
    // Do nothing if full reset already pending
    if (!m_fullReset) {
        if (!m_proxy->useModelCategories()) {
            // If the data model doesn't directly map rows and columns, we cannot optimize
            AbstractItemModelHandler::handleDataChanged(topLeft, bottomRight, roles);
        } else {
            int startRow = qMin(topLeft.row(), bottomRight.row());
            int endRow = qMax(topLeft.row(), bottomRight.row());
            int startCol = qMin(topLeft.column(), bottomRight.column());
            int endCol = qMax(topLeft.column(), bottomRight.column());

            for (int i = startRow; i <= endRow; i++) {
                for (int j = startCol; j <= endCol; j++) {
                    QModelIndex index = m_itemModel->index(i, j);
                    QBarDataItem item;
                    QVariant valueVar = index.data(m_valueRole);
                    float value;
                    if (m_haveValuePattern)
                        value = valueVar.toString().replace(m_valuePattern, m_valueReplace).toFloat();
                    else
                        value = valueVar.toFloat();
                    item.setValue(value);
                    if (m_rotationRole != noRoleIndex) {
                        QVariant rotationVar = index.data(m_rotationRole);
                        float rotation;
                        if (m_haveRotationPattern) {
                            rotation = rotationVar.toString().replace(m_rotationPattern,
                                                                      m_rotationReplace).toFloat();
                        } else {
                            rotation = rotationVar.toFloat();
                        }
                        item.setRotation(rotation);
                    }
                    m_proxy->setItem(i, j, item);
                }
            }
        }
    }
}

// Resolve entire item model into QBarDataArray.
void BarItemModelHandler::resolveModel()
{
    if (m_itemModel.isNull()) {
        m_proxy->resetArray(0);
        return;
    }

    if (!m_proxy->useModelCategories()
            && (m_proxy->rowRole().isEmpty() || m_proxy->columnRole().isEmpty())) {
        m_proxy->resetArray(0);
        return;
    }

    // Value and rotation patterns can be reused on single item changes,
    // so store them to member variables.
    QRegExp rowPattern(m_proxy->rowRolePattern());
    QRegExp colPattern(m_proxy->columnRolePattern());
    m_valuePattern = m_proxy->valueRolePattern();
    m_rotationPattern = m_proxy->rotationRolePattern();
    QString rowReplace = m_proxy->rowRoleReplace();
    QString colReplace = m_proxy->columnRoleReplace();
    m_valueReplace = m_proxy->valueRoleReplace();
    m_rotationReplace = m_proxy->rotationRoleReplace();
    bool haveRowPattern = !rowPattern.isEmpty() && rowPattern.isValid();
    bool haveColPattern = !colPattern.isEmpty() && colPattern.isValid();
    m_haveValuePattern = !m_valuePattern.isEmpty() && m_valuePattern.isValid();
    m_haveRotationPattern = !m_rotationPattern.isEmpty() && m_rotationPattern.isValid();

    QStringList rowLabels;
    QStringList columnLabels;

    QHash<int, QByteArray> roleHash = m_itemModel->roleNames();

    // Default value role to display role if no mapping
    m_valueRole = roleHash.key(m_proxy->valueRole().toLatin1(), Qt::DisplayRole);
    m_rotationRole = roleHash.key(m_proxy->rotationRole().toLatin1(), noRoleIndex);
    int rowCount = m_itemModel->rowCount();
    int columnCount = m_itemModel->columnCount();

    if (m_proxy->useModelCategories()) {
        // If dimensions have changed, recreate the array
        if (m_proxyArray != m_proxy->array() || columnCount != m_columnCount
                || rowCount != m_proxyArray->size()) {
            m_proxyArray = new QBarDataArray;
            m_proxyArray->reserve(rowCount);
            for (int i = 0; i < rowCount; i++)
                m_proxyArray->append(new QBarDataRow(columnCount));
        }
        for (int i = 0; i < rowCount; i++) {
            QBarDataRow &newProxyRow = *m_proxyArray->at(i);
            for (int j = 0; j < columnCount; j++) {
                QModelIndex index = m_itemModel->index(i, j);
                QVariant valueVar = index.data(m_valueRole);
                float value;
                if (m_haveValuePattern)
                    value = valueVar.toString().replace(m_valuePattern, m_valueReplace).toFloat();
                else
                    value = valueVar.toFloat();
                newProxyRow[j].setValue(value);
                if (m_rotationRole != noRoleIndex) {
                    QVariant rotationVar = index.data(m_rotationRole);
                    float rotation;
                    if (m_haveRotationPattern) {
                        rotation = rotationVar.toString().replace(m_rotationPattern,
                                                                  m_rotationReplace).toFloat();
                    } else {
                        rotation = rotationVar.toFloat();
                    }
                    newProxyRow[j].setRotation(rotation);
                }
            }
        }
        // Generate labels from headers if using model rows/columns
        for (int i = 0; i < rowCount; i++)
            rowLabels << m_itemModel->headerData(i, Qt::Vertical).toString();
        for (int i = 0; i < columnCount; i++)
            columnLabels << m_itemModel->headerData(i, Qt::Horizontal).toString();
        m_columnCount = columnCount;
    } else {
        int rowRole = roleHash.key(m_proxy->rowRole().toLatin1());
        int columnRole = roleHash.key(m_proxy->columnRole().toLatin1());

        bool generateRows = m_proxy->autoRowCategories();
        bool generateColumns = m_proxy->autoColumnCategories();
        QStringList rowList;
        QStringList columnList;
        // For detecting duplicates in categories generation, using QHashes should be faster than
        // simple QStringList::contains() check.
        QHash<QString, bool> rowListHash;
        QHash<QString, bool> columnListHash;

        // Sort values into rows and columns
        typedef QHash<QString, float> ColumnValueMap;
        QHash<QString, ColumnValueMap> itemValueMap;
        QHash<QString, ColumnValueMap> itemRotationMap;

        bool cumulative = m_proxy->multiMatchBehavior() == QItemModelBarDataProxy::MMBAverage
                || m_proxy->multiMatchBehavior() == QItemModelBarDataProxy::MMBCumulative;
        bool countMatches = m_proxy->multiMatchBehavior() == QItemModelBarDataProxy::MMBAverage;
        bool takeFirst = m_proxy->multiMatchBehavior() == QItemModelBarDataProxy::MMBFirst;
        QHash<QString, QHash<QString, int> > *matchCountMap = 0;
        if (countMatches)
            matchCountMap = new QHash<QString, QHash<QString, int> >;

        for (int i = 0; i < rowCount; i++) {
            for (int j = 0; j < columnCount; j++) {
                QModelIndex index = m_itemModel->index(i, j);
                QString rowRoleStr = index.data(rowRole).toString();
                if (haveRowPattern)
                    rowRoleStr.replace(rowPattern, rowReplace);
                QString columnRoleStr = index.data(columnRole).toString();
                if (haveColPattern)
                    columnRoleStr.replace(colPattern, colReplace);
                QVariant valueVar = index.data(m_valueRole);
                float value;
                if (m_haveValuePattern)
                    value = valueVar.toString().replace(m_valuePattern, m_valueReplace).toFloat();
                else
                    value = valueVar.toFloat();
                if (countMatches)
                    (*matchCountMap)[rowRoleStr][columnRoleStr]++;

                if (cumulative) {
                    itemValueMap[rowRoleStr][columnRoleStr] += value;
                } else {
                    if (takeFirst && itemValueMap.contains(rowRoleStr)) {
                        if (itemValueMap.value(rowRoleStr).contains(columnRoleStr))
                            continue; // We already have a value for this row/column combo
                    }
                    itemValueMap[rowRoleStr][columnRoleStr] = value;
                }

                if (m_rotationRole != noRoleIndex) {
                    QVariant rotationVar = index.data(m_rotationRole);
                    float rotation;
                    if (m_haveRotationPattern) {
                        rotation = rotationVar.toString().replace(m_rotationPattern,
                                                                  m_rotationReplace).toFloat();
                    } else {
                        rotation = rotationVar.toFloat();
                    }
                    if (cumulative) {
                        itemRotationMap[rowRoleStr][columnRoleStr] += rotation;
                    } else {
                        // We know we are in take last mode if we get here,
                        // as take first mode skips to next loop already earlier
                        itemRotationMap[rowRoleStr][columnRoleStr] = rotation;
                    }
                }
                if (generateRows && !rowListHash.value(rowRoleStr, false)) {
                    rowListHash.insert(rowRoleStr, true);
                    rowList << rowRoleStr;
                }
                if (generateColumns && !columnListHash.value(columnRoleStr, false)) {
                    columnListHash.insert(columnRoleStr, true);
                    columnList << columnRoleStr;
                }
            }
        }

        if (generateRows)
            m_proxy->dptr()->m_rowCategories = rowList;
        else
            rowList = m_proxy->rowCategories();

        if (generateColumns)
            m_proxy->dptr()->m_columnCategories = columnList;
        else
            columnList = m_proxy->columnCategories();

        // If dimensions have changed, recreate the array
        if (m_proxyArray != m_proxy->array() || columnList.size() != m_columnCount
                || rowList.size() != m_proxyArray->size()) {
            m_proxyArray = new QBarDataArray;
            m_proxyArray->reserve(rowList.size());
            for (int i = 0; i < rowList.size(); i++)
                m_proxyArray->append(new QBarDataRow(columnList.size()));
        }
        // Create new data array from itemValueMap
        for (int i = 0; i < rowList.size(); i++) {
            QString rowKey = rowList.at(i);
            QBarDataRow &newProxyRow = *m_proxyArray->at(i);
            for (int j = 0; j < columnList.size(); j++) {
                float value = itemValueMap[rowKey][columnList.at(j)];
                if (countMatches)
                    value /= float((*matchCountMap)[rowKey][columnList.at(j)]);
                newProxyRow[j].setValue(value);
                if (m_rotationRole != noRoleIndex) {
                    float angle = itemRotationMap[rowKey][columnList.at(j)];
                    if (countMatches)
                        angle /= float((*matchCountMap)[rowKey][columnList.at(j)]);
                    newProxyRow[j].setRotation(angle);
                }
            }
        }

        rowLabels = rowList;
        columnLabels = columnList;
        m_columnCount = columnList.size();

        delete matchCountMap;
    }

    m_proxy->resetArray(m_proxyArray, rowLabels, columnLabels);
}

QT_END_NAMESPACE_DATAVISUALIZATION
