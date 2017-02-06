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

#include "scatteritemmodelhandler_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

static const int noRoleIndex = -1;

ScatterItemModelHandler::ScatterItemModelHandler(QItemModelScatterDataProxy *proxy, QObject *parent)
    : AbstractItemModelHandler(parent),
      m_proxy(proxy),
      m_proxyArray(0),
      m_xPosRole(noRoleIndex),
      m_yPosRole(noRoleIndex),
      m_zPosRole(noRoleIndex),
      m_rotationRole(noRoleIndex),
      m_haveXPosPattern(false),
      m_haveYPosPattern(false),
      m_haveZPosPattern(false),
      m_haveRotationPattern(false)
{
}

ScatterItemModelHandler::~ScatterItemModelHandler()
{
}

void ScatterItemModelHandler::handleDataChanged(const QModelIndex &topLeft,
                                                const QModelIndex &bottomRight,
                                                const QVector<int> &roles)
{
    // Do nothing if full reset already pending
    if (!m_fullReset) {
        if (m_itemModel->columnCount() > 1) {
            // If the data model is multi-column, do full asynchronous reset to simplify things
            AbstractItemModelHandler::handleDataChanged(topLeft, bottomRight, roles);
        } else {
            int start = qMin(topLeft.row(), bottomRight.row());
            int end = qMax(topLeft.row(), bottomRight.row());

            QScatterDataArray array(end - start + 1);
            int count = 0;
            for (int i = start; i <= end; i++)
                modelPosToScatterItem(i, 0, array[count++]);

            m_proxy->setItems(start, array);
        }
    }
}

void ScatterItemModelHandler::handleRowsInserted(const QModelIndex &parent, int start, int end)
{
    // Do nothing if full reset already pending
    if (!m_fullReset) {
        if (!m_proxy->itemCount() || m_itemModel->columnCount() > 1) {
            // If inserting into an empty array, do full asynchronous reset to avoid multiple
            // separate inserts when initializing the model.
            // If the data model is multi-column, do full asynchronous reset to simplify things
            AbstractItemModelHandler::handleRowsInserted(parent, start, end);
        } else {
            QScatterDataArray array(end - start + 1);
            int count = 0;
            for (int i = start; i <= end; i++)
                modelPosToScatterItem(i, 0, array[count++]);

            m_proxy->insertItems(start, array);
        }
    }
}

void ScatterItemModelHandler::handleRowsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent)

    // Do nothing if full reset already pending
    if (!m_fullReset) {
        if (m_itemModel->columnCount() > 1) {
            // If the data model is multi-column, do full asynchronous reset to simplify things
            AbstractItemModelHandler::handleRowsRemoved(parent, start, end);
        } else {
            m_proxy->removeItems(start, end - start + 1);
        }
    }
}

static inline QQuaternion toQuaternion(const QVariant &variant)
{
    if (variant.canConvert<QQuaternion>()) {
        return variant.value<QQuaternion>();
    } else if (variant.canConvert<QString>()) {
        QString s = variant.toString();
        if (!s.isEmpty()) {
            bool angleAndAxis = false;
            if (s.startsWith(QLatin1Char('@'))) {
                angleAndAxis = true;
                s = s.mid(1);
            }
            if (s.count(QLatin1Char(',')) == 3) {
                int index = s.indexOf(QLatin1Char(','));
                int index2 = s.indexOf(QLatin1Char(','), index + 1);
                int index3 = s.indexOf(QLatin1Char(','), index2 + 1);

                bool sGood, xGood, yGood, zGood;
                float sCoord = s.left(index).toFloat(&sGood);
                float xCoord = s.mid(index + 1, index2 - index - 1).toFloat(&xGood);
                float yCoord = s.mid(index2 + 1, index3 - index2 - 1).toFloat(&yGood);
                float zCoord = s.mid(index3 + 1).toFloat(&zGood);

                if (sGood && xGood && yGood && zGood) {
                    if (angleAndAxis)
                        return QQuaternion::fromAxisAndAngle(xCoord, yCoord, zCoord, sCoord);
                    else
                        return QQuaternion(sCoord, xCoord, yCoord, zCoord);
                }
            }
        }
    }
    return QQuaternion();
}

void ScatterItemModelHandler::modelPosToScatterItem(int modelRow, int modelColumn,
                                                    QScatterDataItem &item)
{
    QModelIndex index = m_itemModel->index(modelRow, modelColumn);
    float xPos;
    float yPos;
    float zPos;
    if (m_xPosRole != noRoleIndex) {
        QVariant xValueVar = index.data(m_xPosRole);
        if (m_haveXPosPattern)
            xPos = xValueVar.toString().replace(m_xPosPattern, m_xPosReplace).toFloat();
        else
            xPos = xValueVar.toFloat();
    } else {
        xPos = 0.0f;
    }
    if (m_yPosRole != noRoleIndex) {
        QVariant yValueVar = index.data(m_yPosRole);
        if (m_haveYPosPattern)
            yPos = yValueVar.toString().replace(m_yPosPattern, m_yPosReplace).toFloat();
        else
            yPos = yValueVar.toFloat();
    } else {
        yPos = 0.0f;
    }
    if (m_zPosRole != noRoleIndex) {
        QVariant zValueVar = index.data(m_zPosRole);
        if (m_haveZPosPattern)
            zPos = zValueVar.toString().replace(m_zPosPattern, m_zPosReplace).toFloat();
        else
            zPos = zValueVar.toFloat();
    } else {
        zPos = 0.0f;
    }
    if (m_rotationRole != noRoleIndex) {
        QVariant rotationVar = index.data(m_rotationRole);
        if (m_haveRotationPattern) {
            item.setRotation(
                        toQuaternion(
                            QVariant(rotationVar.toString().replace(m_rotationPattern,
                                                                    m_rotationReplace))));
        } else {
            item.setRotation(toQuaternion(rotationVar));
        }
    }

    item.setPosition(QVector3D(xPos, yPos, zPos));
}

// Resolve entire item model into QScatterDataArray.
void ScatterItemModelHandler::resolveModel()
{
    if (m_itemModel.isNull()) {
        m_proxy->resetArray(0);
        m_proxyArray = 0;
        return;
    }

    m_xPosPattern = m_proxy->xPosRolePattern();
    m_yPosPattern = m_proxy->yPosRolePattern();
    m_zPosPattern = m_proxy->zPosRolePattern();
    m_rotationPattern = m_proxy->rotationRolePattern();
    m_xPosReplace = m_proxy->xPosRoleReplace();
    m_yPosReplace = m_proxy->yPosRoleReplace();
    m_zPosReplace = m_proxy->zPosRoleReplace();
    m_rotationReplace = m_proxy->rotationRoleReplace();
    m_haveXPosPattern = !m_xPosPattern.isEmpty() && m_xPosPattern.isValid();
    m_haveYPosPattern = !m_yPosPattern.isEmpty() && m_yPosPattern.isValid();
    m_haveZPosPattern = !m_zPosPattern.isEmpty() && m_zPosPattern.isValid();
    m_haveRotationPattern = !m_rotationPattern.isEmpty() && m_rotationPattern.isValid();

    QHash<int, QByteArray> roleHash = m_itemModel->roleNames();
    m_xPosRole = roleHash.key(m_proxy->xPosRole().toLatin1(), noRoleIndex);
    m_yPosRole = roleHash.key(m_proxy->yPosRole().toLatin1(), noRoleIndex);
    m_zPosRole = roleHash.key(m_proxy->zPosRole().toLatin1(), noRoleIndex);
    m_rotationRole = roleHash.key(m_proxy->rotationRole().toLatin1(), noRoleIndex);
    const int columnCount = m_itemModel->columnCount();
    const int rowCount = m_itemModel->rowCount();
    const int totalCount = rowCount * columnCount;
    int runningCount = 0;

    // If dimensions have changed, recreate the array
    if (m_proxyArray != m_proxy->array() || totalCount != m_proxyArray->size())
        m_proxyArray = new QScatterDataArray(totalCount);

    // Parse data into newProxyArray
    for (int i = 0; i < rowCount; i++) {
        for (int j = 0; j < columnCount; j++) {
            modelPosToScatterItem(i, j, (*m_proxyArray)[runningCount]);
            runningCount++;
        }
    }

    m_proxy->resetArray(m_proxyArray);
}

QT_END_NAMESPACE_DATAVISUALIZATION
