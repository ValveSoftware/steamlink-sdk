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

#include "surfaceitemmodelhandler_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

static const int noRoleIndex = -1;

SurfaceItemModelHandler::SurfaceItemModelHandler(QItemModelSurfaceDataProxy *proxy, QObject *parent)
    : AbstractItemModelHandler(parent),
      m_proxy(proxy),
      m_proxyArray(0),
      m_xPosRole(noRoleIndex),
      m_yPosRole(noRoleIndex),
      m_zPosRole(noRoleIndex),
      m_haveXPosPattern(false),
      m_haveYPosPattern(false),
      m_haveZPosPattern(false)
{
}

SurfaceItemModelHandler::~SurfaceItemModelHandler()
{
}

void SurfaceItemModelHandler::handleDataChanged(const QModelIndex &topLeft,
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
                    QSurfaceDataItem item;
                    QVariant xValueVar = index.data(m_xPosRole);
                    QVariant yValueVar = index.data(m_yPosRole);
                    QVariant zValueVar = index.data(m_zPosRole);
                    const QSurfaceDataItem *oldItem = m_proxy->itemAt(i, j);
                    float xPos;
                    float yPos;
                    float zPos;
                    if (m_xPosRole != noRoleIndex) {
                        if (m_haveXPosPattern)
                            xPos = xValueVar.toString().replace(m_xPosPattern, m_xPosReplace).toFloat();
                        else
                            xPos = xValueVar.toFloat();
                    } else {
                        xPos = oldItem->x();
                    }

                    if (m_haveYPosPattern)
                        yPos = yValueVar.toString().replace(m_yPosPattern, m_yPosReplace).toFloat();
                    else
                        yPos = yValueVar.toFloat();

                    if (m_zPosRole != noRoleIndex) {
                        if (m_haveZPosPattern)
                            zPos = zValueVar.toString().replace(m_zPosPattern, m_zPosReplace).toFloat();
                        else
                            zPos = zValueVar.toFloat();
                    } else {
                        zPos = oldItem->z();
                    }
                    item.setPosition(QVector3D(xPos, yPos, zPos));
                    m_proxy->setItem(i, j, item);
                }
            }
        }
    }
}

// Resolve entire item model into QSurfaceDataArray.
void SurfaceItemModelHandler::resolveModel()
{
    if (m_itemModel.isNull()) {
        m_proxy->resetArray(0);
        m_proxyArray = 0;
        return;
    }

    if (!m_proxy->useModelCategories()
            && (m_proxy->rowRole().isEmpty() || m_proxy->columnRole().isEmpty())) {
        m_proxy->resetArray(0);
        m_proxyArray = 0;
        return;
    }

    // Position patterns can be reused on single item changes, so store them to member variables.
    QRegExp rowPattern(m_proxy->rowRolePattern());
    QRegExp colPattern(m_proxy->columnRolePattern());
    m_xPosPattern = m_proxy->xPosRolePattern();
    m_yPosPattern = m_proxy->yPosRolePattern();
    m_zPosPattern = m_proxy->zPosRolePattern();
    QString rowReplace = m_proxy->rowRoleReplace();
    QString colReplace = m_proxy->columnRoleReplace();
    m_xPosReplace = m_proxy->xPosRoleReplace();
    m_yPosReplace = m_proxy->yPosRoleReplace();
    m_zPosReplace = m_proxy->zPosRoleReplace();
    bool haveRowPattern = !rowPattern.isEmpty() && rowPattern.isValid();
    bool haveColPattern = !colPattern.isEmpty() && colPattern.isValid();
    m_haveXPosPattern = !m_xPosPattern.isEmpty() && m_xPosPattern.isValid();
    m_haveYPosPattern = !m_yPosPattern.isEmpty() && m_yPosPattern.isValid();
    m_haveZPosPattern = !m_zPosPattern.isEmpty() && m_zPosPattern.isValid();

    QHash<int, QByteArray> roleHash = m_itemModel->roleNames();

    // Default to display role if no mapping
    m_xPosRole = roleHash.key(m_proxy->xPosRole().toLatin1(), noRoleIndex);
    m_yPosRole = roleHash.key(m_proxy->yPosRole().toLatin1(), Qt::DisplayRole);
    m_zPosRole = roleHash.key(m_proxy->zPosRole().toLatin1(), noRoleIndex);
    int rowCount = m_itemModel->rowCount();
    int columnCount = m_itemModel->columnCount();

    if (m_proxy->useModelCategories()) {
        // If dimensions have changed, recreate the array
        if (m_proxyArray != m_proxy->array() || columnCount != m_proxy->columnCount()
                || rowCount != m_proxyArray->size()) {
            m_proxyArray = new QSurfaceDataArray;
            m_proxyArray->reserve(rowCount);
            for (int i = 0; i < rowCount; i++)
                m_proxyArray->append(new QSurfaceDataRow(columnCount));
        }
        for (int i = 0; i < rowCount; i++) {
            QSurfaceDataRow &newProxyRow = *m_proxyArray->at(i);
            for (int j = 0; j < columnCount; j++) {
                QModelIndex index = m_itemModel->index(i, j);
                QVariant xValueVar = index.data(m_xPosRole);
                QVariant yValueVar = index.data(m_yPosRole);
                QVariant zValueVar = index.data(m_zPosRole);
                float xPos;
                float yPos;
                float zPos;
                if (m_xPosRole != noRoleIndex) {
                    if (m_haveXPosPattern)
                        xPos = xValueVar.toString().replace(m_xPosPattern, m_xPosReplace).toFloat();
                    else
                        xPos = xValueVar.toFloat();
                } else {
                    QString header = m_itemModel->headerData(j, Qt::Horizontal).toString();
                    bool ok = false;
                    float headerValue = header.toFloat(&ok);
                    if (ok)
                        xPos = headerValue;
                    else
                        xPos = float(j);
                }

                if (m_haveYPosPattern)
                    yPos = yValueVar.toString().replace(m_yPosPattern, m_yPosReplace).toFloat();
                else
                    yPos = yValueVar.toFloat();

                if (m_zPosRole != noRoleIndex) {
                    if (m_haveZPosPattern)
                        zPos = zValueVar.toString().replace(m_zPosPattern, m_zPosReplace).toFloat();
                    else
                        zPos = zValueVar.toFloat();
                } else {
                    QString header = m_itemModel->headerData(i, Qt::Vertical).toString();
                    bool ok = false;
                    float headerValue = header.toFloat(&ok);
                    if (ok)
                        zPos = headerValue;
                    else
                        zPos = float(i);
                }

                newProxyRow[j].setPosition(QVector3D(xPos, yPos, zPos));
            }
        }
    } else {
        int rowRole = roleHash.key(m_proxy->rowRole().toLatin1());
        int columnRole = roleHash.key(m_proxy->columnRole().toLatin1());
        if (m_xPosRole == noRoleIndex)
            m_xPosRole = columnRole;
        if (m_zPosRole == noRoleIndex)
            m_zPosRole = rowRole;

        bool generateRows = m_proxy->autoRowCategories();
        bool generateColumns = m_proxy->autoColumnCategories();

        QStringList rowList;
        QStringList columnList;
        // For detecting duplicates in categories generation, using QHashes should be faster than
        // simple QStringList::contains() check.
        QHash<QString, bool> rowListHash;
        QHash<QString, bool> columnListHash;

        bool cumulative = m_proxy->multiMatchBehavior() == QItemModelSurfaceDataProxy::MMBAverage
                || m_proxy->multiMatchBehavior() == QItemModelSurfaceDataProxy::MMBCumulativeY;
        bool average = m_proxy->multiMatchBehavior() == QItemModelSurfaceDataProxy::MMBAverage;
        bool takeFirst = m_proxy->multiMatchBehavior() == QItemModelSurfaceDataProxy::MMBFirst;
        QHash<QString, QHash<QString, int> > *matchCountMap = 0;
        if (cumulative)
            matchCountMap = new QHash<QString, QHash<QString, int> >;

        // Sort values into rows and columns
        typedef QHash<QString, QVector3D> ColumnValueMap;
        QHash <QString, ColumnValueMap> itemValueMap;
        for (int i = 0; i < rowCount; i++) {
            for (int j = 0; j < columnCount; j++) {
                QModelIndex index = m_itemModel->index(i, j);
                QString rowRoleStr = index.data(rowRole).toString();
                if (haveRowPattern)
                    rowRoleStr.replace(rowPattern, rowReplace);
                QString columnRoleStr = index.data(columnRole).toString();
                if (haveColPattern)
                    columnRoleStr.replace(colPattern, colReplace);
                QVariant xValueVar = index.data(m_xPosRole);
                QVariant yValueVar = index.data(m_yPosRole);
                QVariant zValueVar = index.data(m_zPosRole);
                float xPos;
                float yPos;
                float zPos;
                if (m_haveXPosPattern)
                    xPos = xValueVar.toString().replace(m_xPosPattern, m_xPosReplace).toFloat();
                else
                    xPos = xValueVar.toFloat();
                if (m_haveYPosPattern)
                    yPos = yValueVar.toString().replace(m_yPosPattern, m_yPosReplace).toFloat();
                else
                    yPos = yValueVar.toFloat();
                if (m_haveZPosPattern)
                    zPos = zValueVar.toString().replace(m_zPosPattern, m_zPosReplace).toFloat();
                else
                    zPos = zValueVar.toFloat();

                QVector3D itemPos(xPos, yPos, zPos);

                if (cumulative)
                    (*matchCountMap)[rowRoleStr][columnRoleStr]++;

                if (cumulative) {
                    itemValueMap[rowRoleStr][columnRoleStr] += itemPos;
                } else {
                    if (takeFirst && itemValueMap.contains(rowRoleStr)) {
                        if (itemValueMap.value(rowRoleStr).contains(columnRoleStr))
                            continue; // We already have a value for this row/column combo
                    }
                    itemValueMap[rowRoleStr][columnRoleStr] = itemPos;
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
        if (m_proxyArray != m_proxy->array() || columnList.size() != m_proxy->columnCount()
                || rowList.size() != m_proxyArray->size()) {
            m_proxyArray = new QSurfaceDataArray;
            m_proxyArray->reserve(rowList.size());
            for (int i = 0; i < rowList.size(); i++)
                m_proxyArray->append(new QSurfaceDataRow(columnList.size()));
        }
        // Create data array from itemValueMap
        for (int i = 0; i < rowList.size(); i++) {
            QString rowKey = rowList.at(i);
            QSurfaceDataRow &newProxyRow = *m_proxyArray->at(i);
            for (int j = 0; j < columnList.size(); j++) {
                QVector3D &itemPos = itemValueMap[rowKey][columnList.at(j)];
                if (cumulative) {
                    if (average) {
                        itemPos /= float((*matchCountMap)[rowKey][columnList.at(j)]);
                    } else { // cumulativeY
                        float divisor = float((*matchCountMap)[rowKey][columnList.at(j)]);
                        itemPos.setX(itemPos.x() / divisor);
                        itemPos.setZ(itemPos.z() / divisor);
                    }
                }
                newProxyRow[j].setPosition(itemPos);
            }
        }

        delete matchCountMap;
    }

    m_proxy->resetArray(m_proxyArray);
}

QT_END_NAMESPACE_DATAVISUALIZATION
