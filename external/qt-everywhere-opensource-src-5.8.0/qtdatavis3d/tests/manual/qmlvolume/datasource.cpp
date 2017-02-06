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

#include "datasource.h"
#include <QtCore/qmath.h>
#include <QtGui/QRgb>
#include <QtGui/QVector3D>

using namespace QtDataVisualization;

Q_DECLARE_METATYPE(QCustom3DVolume *)

DataSource::DataSource(QObject *parent) :
    QObject(parent)
{
    qRegisterMetaType<QCustom3DVolume *>();
}

DataSource::~DataSource()
{
}

void DataSource::fillVolume(QCustom3DVolume *volumeItem)
{
    // Generate example texture data for an half-ellipsoid with a section missing.
    // This can take a while if the dimensions are large, so we support incremental data generation.

    int index = 0;
    int textureSize = 256;
    QVector3D midPoint(float(textureSize) / 2.0f,
                       float(textureSize) / 2.0f,
                       float(textureSize) / 2.0f);

    QVector<uchar> *textureData = new QVector<uchar>(textureSize * textureSize * textureSize / 2);
    for (int i = 0; i < textureSize; i++) {
        for (int j = 0; j < textureSize / 2; j++) {
            for (int k = 0; k < textureSize; k++) {
                int colorIndex = 0;
                // Take a section out of the ellipsoid
                if (i >= textureSize / 2 || j >= textureSize / 4 || k >= textureSize / 2) {
                    QVector3D distVec = QVector3D(float(k), float(j * 2), float(i)) - midPoint;
                    float adjLen = qMin(255.0f, (distVec.length() * 512.0f / float(textureSize)));
                    colorIndex = 255 - int(adjLen);
                }

                (*textureData)[index] = colorIndex;
                index++;
            }
        }
    }

    volumeItem->setScaling(QVector3D(2.0f, 1.0f, 2.0f));
    volumeItem->setTextureWidth(textureSize);
    volumeItem->setTextureHeight(textureSize / 2);
    volumeItem->setTextureDepth(textureSize);
    volumeItem->setTextureFormat(QImage::Format_Indexed8);
    volumeItem->setTextureData(textureData);

    QVector<QRgb> colorTable(256);

    for (int i = 1; i < 256; i++) {
        if (i < 15)
            colorTable[i] = qRgba(0, 0, 0, 0);
        else if (i < 60)
            colorTable[i] = qRgba((i * 2) + 120, 0, 0, 15);
        else if (i < 120)
            colorTable[i] = qRgba(0, ((i - 60) * 2) + 120, 0, 50);
        else if (i < 180)
            colorTable[i] = qRgba(0, 0, ((i - 120) * 2) + 120, 255);
        else
            colorTable[i] = qRgba(i, i, i, 255);
    }

    volumeItem->setColorTable(colorTable);
}
