/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
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
