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

#include "audiolevelsiodevice.h"
#include <QtCore/QDebug>

using namespace QtDataVisualization;

//! [1]
static const int resolution = 8;
static const int rowSize = 800;
static const int rowCount = 7; // Must be odd number
static const int middleRow = rowCount / 2;
//! [1]

AudioLevelsIODevice::AudioLevelsIODevice(QBarDataProxy *proxy, QObject *parent)
    : QIODevice(parent),
      m_proxy(proxy),
      m_array(new QBarDataArray)
{
    // We reuse the existing array for maximum performance, so construct the array here
    //! [0]
    m_array->reserve(rowCount);
    for (int i = 0; i < rowCount; i++)
        m_array->append(new QBarDataRow(rowSize));
    //! [0]

    qDebug() << "Total of" << (rowSize * rowCount) << "items in the array.";
}

// Implementation required for this pure virtual function
qint64 AudioLevelsIODevice::readData(char *data, qint64 maxSize)
{
    Q_UNUSED(data)
    Q_UNUSED(maxSize)
    return -1;
}

//! [2]
qint64 AudioLevelsIODevice::writeData(const char *data, qint64 maxSize)
{
    // The amount of new data available.
    int newDataSize = maxSize / resolution;

    // If we get more data than array size, we need to adjust the start index for new data.
    int newDataStartIndex = qMax(0, (newDataSize - rowSize));

    // Move the old data ahead in the rows (only do first half of rows + middle one now).
    // If the amount of new data was larger than row size, skip copying.
    if (!newDataStartIndex) {
        for (int i = 0; i <= middleRow; i++) {
            QBarDataItem *srcPos = m_array->at(i)->data();
            QBarDataItem *dstPos = srcPos + newDataSize;
            memmove((void *)dstPos, (void *)srcPos, (rowSize - newDataSize) * sizeof(QBarDataItem));
        }
    }

    // Insert data in reverse order, so that newest data is always at the front of the row.
    int index = 0;
    for (int i = newDataSize - 1; i >= newDataStartIndex; i--) {
        // Add 0.01 to the value to avoid gaps in the graph (i.e. zero height bars).
        // Also, scale to 0...100
        float value = float(quint8(data[resolution * i]) - 128) / 1.28f + 0.01f;
        (*m_array->at(middleRow))[index].setValue(value);
        // Insert a fractional value into front half of the rows.
        for (int j = 1; j <= middleRow; j++) {
            float fractionalValue = value / float(j + 1);
            (*m_array->at(middleRow - j))[index].setValue(fractionalValue);
        }
        index++;
    }

    // Copy the front half of rows to the back half for symmetry.
    index = 0;
    for (int i = rowCount - 1; i > middleRow; i--) {
        QBarDataItem *srcPos = m_array->at(index++)->data();
        QBarDataItem *dstPos = m_array->at(i)->data();
        memcpy((void *)dstPos, (void *)srcPos, rowSize * sizeof(QBarDataItem));
    }

    // Reset the proxy array now that data has been updated to trigger a redraw.
    m_proxy->resetArray(m_array);

    return maxSize;
}
//! [2]


