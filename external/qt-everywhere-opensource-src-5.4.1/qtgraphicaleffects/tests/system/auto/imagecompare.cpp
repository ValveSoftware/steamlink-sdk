/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Graphical Effects module.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "imagecompare.h"

ImageCompare::ImageCompare(QObject *parent) : QObject(parent){
}

bool ImageCompare::ComparePixels(QImage actual, QImage expected, int tolerance, QString filename){

    bool result = true;
    bool compareAlpha, compareRed, compareGreen, compareBlue;
    int diffPixelCount = 0;
    QImage diffImage(300, 300, QImage::Format_ARGB32);
    diffImage.fill(Qt::white);

    for(int y = 0; y < 300; y++){
        for(int x = 0; x < 300; x++){

            // Pixel difference is set initially to false.
            bool diff = false;

            // Gets the color of the pixel in the given position.
            QRgb pixelActual = actual.pixel(x,y);
            QRgb pixelExpected = expected.pixel(x,y);

            // Compares separately the alpha, red, green and blue components of an ARGB value within the specified tolerance.
            compareAlpha = (abs(qAlpha(pixelActual) - qAlpha(pixelExpected)) <= tolerance);
            compareRed = (abs(qRed(pixelActual) - qRed(pixelExpected)) <= tolerance);
            compareGreen = (abs(qGreen(pixelActual) - qGreen(pixelExpected)) <= tolerance);
            compareBlue = (abs(qBlue(pixelActual) - qBlue(pixelExpected)) <= tolerance);

            // Result value holds the overall boolean status are the compared images the same or different.
            result &= (compareAlpha && compareRed && compareGreen && compareBlue);

            // When the compared pixels differ, diff value is set to true and the pixel position is marked to an additional image file.
            diff = !(compareAlpha && compareRed && compareGreen && compareBlue);
            if (diff == true){
                diffImage.setPixel(x, y, qRgba(255, 0, 0, 255));
                ++diffPixelCount;
            }
        }
    }

    if(result == false){
        qDebug() << "The percentage difference in" << filename << ":" << diffPixelCount << "/" << 300*300 << "=" << 100.0*diffPixelCount/(300*300);
        QDir wd = QDir().dirName();
        wd.mkdir("diffImages");
        diffImage.save("diffImages/" + filename);
    }

    return result;
}

bool ImageCompare::CompareSizes(QImage actual, QImage expected){
    bool sizesMatch = true;
    if (actual.width() == expected.width() && actual.height() == expected.height()){
        sizesMatch = true;
    }
    else sizesMatch = false;

    return sizesMatch;
}
