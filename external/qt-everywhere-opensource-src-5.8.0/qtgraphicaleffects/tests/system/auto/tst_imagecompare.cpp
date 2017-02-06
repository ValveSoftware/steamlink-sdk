/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include <QtCore/QString>
#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QStringList>
#include <QtGui/QImage>
#include <QDebug>
#include "imagecompare.h"
#include "tst_imagecompare.h"

QDir expectedDir("../images");
QDir actualDir("./output");
int diffTolerance = 0;

void tst_imagecompare::setDiffTolerance(int tolerance){
    diffTolerance = tolerance;
}

void tst_imagecompare::initTestCase(){
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString qtDir = env.value("QTDIR");
    QStringList dumperpath;
    dumperpath << "../../../tools/pngdumper/pngdumper.qml";

    // Verifies that QTDIR environment variable is set
    QCOMPARE(qtDir == "", false);

    // Verifies does the expected images folder exist
    QCOMPARE(expectedDir.exists(), true);

    QProcess *myProcess = new QProcess();
    myProcess->start(qtDir + "/bin/qmlscene", dumperpath);
    myProcess->waitForFinished(300000);

    // Verifies does the output folder exist
    QCOMPARE(actualDir.exists(), true);

    // Verifies that the output folder includes dumped png images
    QStringList filters;
    filters << "*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();
    QCOMPARE(list.size() == 0, false);
}

void tst_imagecompare::blend_varMode(){
    QStringList filters;
    filters << "Blend_mode*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::brightnessContrast_varBrightness(){
    QStringList filters;
    filters << "BrightnessContrast_brightness*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::brightnessContrast_varContrast(){
    QStringList filters;
    filters << "BrightnessContrast_contrast*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::colorize_varHue(){
    QStringList filters;
    filters << "Colorize_hue*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::colorize_varSaturation(){
    QStringList filters;
    filters << "Colorize_saturation*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::colorize_varLightness(){
    QStringList filters;
    filters << "Colorize_lightness*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::colorOverlay_varColor(){
    QStringList filters;
    filters << "ColorOverlay_color*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::conicalGradient_varAngle(){
    QStringList filters;
    filters << "ConicalGradient_angle*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::conicalGradient_varHorizontalOffset(){
    QStringList filters;
    filters << "ConicalGradient_horizontalOffset*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::conicalGradient_varVerticalOffset(){
    QStringList filters;
    filters << "ConicalGradient_verticalOffset*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::conicalGradient_varGradient(){
    QStringList filters;
    filters << "ConicalGradient_gradient*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::conicalGradient_varMaskSource(){
    QStringList filters;
    filters << "ConicalGradient_maskSource*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::desaturate_varDesaturation(){
    QStringList filters;
    filters << "Desaturate_desaturation*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::displace_varDisplacement(){
    QStringList filters;
    filters << "Displace_displacement*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::dropShadow_varRadius(){
    QStringList filters;
    filters << "DropShadow_radius*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::dropShadow_varColor(){
    QStringList filters;
    filters << "DropShadow_color*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::dropShadow_varHorizontalOffset(){
    QStringList filters;
    filters << "DropShadow_horizontalOffset*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::dropShadow_varVerticalOffset(){
    QStringList filters;
    filters << "DropShadow_verticalOffset*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::dropShadow_varSpread(){
    QStringList filters;
    filters << "DropShadow_spread*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::dropShadow_varFast(){
    QStringList filters;
    filters << "DropShadow_fast*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::glow_varRadius(){
    QStringList filters;
    filters << "Glow_radius*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::glow_varColor(){
    QStringList filters;
    filters << "Glow_color*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::glow_varSpread(){
    QStringList filters;
    filters << "Glow_spread*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::glow_varFast(){
    QStringList filters;
    filters << "Glow_fast*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::fastBlur_varBlur(){
    QStringList filters;
    filters << "FastBlur_blur*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::fastBlur_varTransparentBorder(){
    QStringList filters;
    filters << "FastBlur_transparentBorder*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::gammaAdjust_varGamma(){
    QStringList filters;
    filters << "GammaAdjust_gamma*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::gaussianBlur_varRadius(){
    QStringList filters;
    filters << "GaussianBlur_radius*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::gaussianBlur_varDeviation(){
    QStringList filters;
    filters << "GaussianBlur_deviation*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::gaussianBlur_varTransparentBorder(){
    QStringList filters;
    filters << "GaussianBlur_transparentBorder*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::hueSaturation_varHue(){
    QStringList filters;
    filters << "HueSaturation_hue*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::hueSaturation_varSaturation(){
    QStringList filters;
    filters << "HueSaturation_saturation*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::hueSaturation_varLightness(){
    QStringList filters;
    filters << "HueSaturation_lightness*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::innerShadow_varRadius(){
    QStringList filters;
    filters << "InnerShadow_radius*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::innerShadow_varHorizontalOffset(){
    QStringList filters;
    filters << "InnerShadow_horizontalOffset*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::innerShadow_varVerticalOffset(){
    QStringList filters;
    filters << "InnerShadow_verticalOffset*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::innerShadow_varSpread(){
    QStringList filters;
    filters << "InnerShadow_spread*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::innerShadow_varFast(){
    QStringList filters;
    filters << "InnerShadow_fast*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::innerShadow_varColor(){
    QStringList filters;
    filters << "InnerShadow_color*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::linearGradient_varGradient(){
    QStringList filters;
    filters << "LinearGradient_gradient*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::linearGradient_varStart(){
    QStringList filters;
    filters << "LinearGradient_start*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::linearGradient_varEnd(){
    QStringList filters;
    filters << "LinearGradient_end*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::linearGradient_varMaskSource(){
    QStringList filters;
    filters << "LinearGradient_maskSource*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::opacityMask_varMaskSource(){
    QStringList filters;
    filters << "OpacityMask_maskSource*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::radialGradient_varHorizontalOffset(){
    QStringList filters;
    filters << "RadialGradient_horizontalOffset*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::radialGradient_varVerticalOffset(){
    QStringList filters;
    filters << "RadialGradient_verticalOffset*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::radialGradient_varHorizontalRadius(){
    QStringList filters;
    filters << "RadialGradient_horizontalRadius*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::radialGradient_varVerticalRadius(){
    QStringList filters;
    filters << "RadialGradient_verticalRadius*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::radialGradient_varGradient(){
    QStringList filters;
    filters << "RadialGradient_gradient*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::radialGradient_varAngle(){
    QStringList filters;
    filters << "RadialGradient_angle*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::radialGradient_varMaskSource(){
    QStringList filters;
    filters << "RadialGradient_maskSource*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::rectangularGlow_varGlowRadius(){
    QStringList filters;
    filters << "RectangularGlow_glowRadius*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::rectangularGlow_varSpread(){
    QStringList filters;
    filters << "RectangularGlow_spread*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::rectangularGlow_varColor(){
    QStringList filters;
    filters << "RectangularGlow_color*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::rectangularGlow_varCornerRadius(){
    QStringList filters;
    filters << "RectangularGlow_cornerRadius*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::recursiveBlur_varLoops(){
    QStringList filters;
    filters << "RecursiveBlur_loops*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::recursiveBlur_varRadius(){
    QStringList filters;
    filters << "RecursiveBlur_radius*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::recursiveBlur_varTransparentBorder(){
    QStringList filters;
    filters << "RecursiveBlur_transparentBorder*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::thresholdMask_varSpread(){
    QStringList filters;
    filters << "ThresholdMask_spread*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::thresholdMask_varThreshold(){
    QStringList filters;
    filters << "ThresholdMask_threshold*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::radialBlur_varAngle(){
    QStringList filters;
    filters << "RadialBlur_angle*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::radialBlur_varHorizontalOffset(){
    QStringList filters;
    filters << "RadialBlur_horizontalOffset*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::radialBlur_varVerticalOffset(){
    QStringList filters;
    filters << "RadialBlur_verticalOffset*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::directionalBlur_varAngle(){
    QStringList filters;
    filters << "DirectionalBlur_angle*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::directionalBlur_varLength(){
    QStringList filters;
    filters << "DirectionalBlur_length*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::zoomBlur_varHorizontalOffset(){
    QStringList filters;
    filters << "ZoomBlur_horizontalOffset*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::zoomBlur_varVerticalOffset(){
    QStringList filters;
    filters << "ZoomBlur_verticalOffset*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::zoomBlur_varLength(){
    QStringList filters;
    filters << "ZoomBlur_length*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::levelAdjust_varMinimumInput(){
    QStringList filters;
    filters << "LevelAdjust_minimumInput*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::levelAdjust_varMaximumInput(){
    QStringList filters;
    filters << "LevelAdjust_maximumInput*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::levelAdjust_varMinimumOutput(){
    QStringList filters;
    filters << "LevelAdjust_minimumOutput*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::levelAdjust_varMaximumOutput(){
    QStringList filters;
    filters << "LevelAdjust_maximumOutput*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}

void tst_imagecompare::maskedBlur_varRadius(){
    QStringList filters;
    filters << "MaskedBlur_radius*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::maskedBlur_varFast(){
    QStringList filters;
    filters << "MaskedBlur_fast*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
void tst_imagecompare::maskedBlur_varTransparentBorder(){
    QStringList filters;
    filters << "MaskedBlur_transparentBorder*.png";
    actualDir.setNameFilters(filters);
    QStringList list = actualDir.entryList();

    for (int i = 0; i < list.size(); ++i){
        QString filename = list.at(i).toLocal8Bit().constData();
        qDebug() << "Testing shader image " + filename;
        QString actualFile = actualDir.absolutePath() + "/" + filename;
        QString expectedFile = expectedDir.absolutePath() + "/" + filename;

        QImage actual(actualFile);
        QImage expected(expectedFile);

        // Verifies that pngdumper generated image is not a null size image
        QCOMPARE((actual.width() != 0 || actual.height() != 0), true);

        ImageCompare compare;

        // Verifies that actual and expected images have the same size
        QCOMPARE(compare.CompareSizes(actual, expected), true);

        // Verifies that actual and expected images are pixel-wise the same
        QCOMPARE(compare.ComparePixels(actual, expected, diffTolerance, filename), true);
    }
}
