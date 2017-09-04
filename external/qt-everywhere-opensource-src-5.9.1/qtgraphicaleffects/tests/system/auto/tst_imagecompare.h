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

#ifndef TST_IMAGECOMPARE_H
#define TST_IMAGECOMPARE_H

#include <QtTest/QtTest>

class tst_imagecompare : public QObject
{
    Q_OBJECT

public:
    void setDiffTolerance(int tolerance);

private Q_SLOTS:
    void initTestCase();
    void blend_varMode();
    void brightnessContrast_varBrightness();
    void brightnessContrast_varContrast();
    void colorize_varHue();
    void colorize_varSaturation();
    void colorize_varLightness();
    void colorOverlay_varColor();
    void conicalGradient_varAngle();
    void conicalGradient_varHorizontalOffset();
    void conicalGradient_varVerticalOffset();
    void conicalGradient_varGradient();
    void conicalGradient_varMaskSource();
    void desaturate_varDesaturation();
    void displace_varDisplacement();
    void dropShadow_varRadius();
    void dropShadow_varColor();
    void dropShadow_varHorizontalOffset();
    void dropShadow_varVerticalOffset();
    void dropShadow_varSpread();
    void dropShadow_varFast();
    void glow_varRadius();
    void glow_varColor();
    void glow_varSpread();
    void glow_varFast();
    void fastBlur_varBlur();
    void fastBlur_varTransparentBorder();
    void gammaAdjust_varGamma();
    void gaussianBlur_varRadius();
    void gaussianBlur_varDeviation();
    void gaussianBlur_varTransparentBorder();
    void hueSaturation_varHue();
    void hueSaturation_varSaturation();
    void hueSaturation_varLightness();
    void innerShadow_varRadius();
    void innerShadow_varHorizontalOffset();
    void innerShadow_varVerticalOffset();
    void innerShadow_varSpread();
    void innerShadow_varFast();
    void innerShadow_varColor();
    void linearGradient_varGradient();
    void linearGradient_varStart();
    void linearGradient_varEnd();
    void linearGradient_varMaskSource();
    void opacityMask_varMaskSource();
    void radialGradient_varHorizontalOffset();
    void radialGradient_varVerticalOffset();
    void radialGradient_varHorizontalRadius();
    void radialGradient_varVerticalRadius();
    void radialGradient_varGradient();
    void radialGradient_varAngle();
    void radialGradient_varMaskSource();
    void rectangularGlow_varGlowRadius();
    void rectangularGlow_varSpread();
    void rectangularGlow_varColor();
    void rectangularGlow_varCornerRadius();
    void recursiveBlur_varLoops();
    void recursiveBlur_varRadius();
    void recursiveBlur_varTransparentBorder();
    void thresholdMask_varSpread();
    void thresholdMask_varThreshold();
    void radialBlur_varAngle();
    void radialBlur_varHorizontalOffset();
    void radialBlur_varVerticalOffset();
    void directionalBlur_varAngle();
    void directionalBlur_varLength();
    void zoomBlur_varHorizontalOffset();
    void zoomBlur_varVerticalOffset();
    void zoomBlur_varLength();
    void levelAdjust_varMinimumInput();
    void levelAdjust_varMaximumInput();
    void levelAdjust_varMinimumOutput();
    void levelAdjust_varMaximumOutput();
    void maskedBlur_varRadius();
    void maskedBlur_varFast();
    void maskedBlur_varTransparentBorder();
};

#endif // TST_IMAGECOMPARE_H
