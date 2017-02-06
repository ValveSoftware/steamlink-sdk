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

#include "topographicseries.h"

using namespace QtDataVisualization;

//! [0]
// Value used to encode height data as RGB value on PNG file
const float packingFactor = 11983.0f;
//! [0]

TopographicSeries::TopographicSeries()
{
    setDrawMode(QSurface3DSeries::DrawSurface);
    setFlatShadingEnabled(true);
    setBaseColor(Qt::white);
}

TopographicSeries::~TopographicSeries()
{
}

void TopographicSeries::setTopographyFile(const QString file, float width, float height)
{
//! [1]
    QImage heightMapImage(file);
    uchar *bits = heightMapImage.bits();
    int imageHeight = heightMapImage.height();
    int imageWidth = heightMapImage.width();
    int widthBits = imageWidth * 4;
    float stepX = width / float(imageWidth);
    float stepZ = height / float(imageHeight);

    QSurfaceDataArray *dataArray = new QSurfaceDataArray;
    dataArray->reserve(imageHeight);
    for (int i = 0; i < imageHeight; i++) {
        int p = i * widthBits;
        float z = height - float(i) * stepZ;
        QSurfaceDataRow *newRow = new QSurfaceDataRow(imageWidth);
        for (int j = 0; j < imageWidth; j++) {
            uchar aa = bits[p + 0];
            uchar rr = bits[p + 1];
            uchar gg = bits[p + 2];
            uint color = uint((gg << 16) + (rr << 8) + aa);
            float y = float(color) / packingFactor;
            (*newRow)[j].setPosition(QVector3D(float(j) * stepX, y, z));
            p = p + 4;
        }
        *dataArray << newRow;
    }

    dataProxy()->resetArray(dataArray);
//! [1]

    m_sampleCountX = float(imageWidth);
    m_sampleCountZ = float(imageHeight);
}
