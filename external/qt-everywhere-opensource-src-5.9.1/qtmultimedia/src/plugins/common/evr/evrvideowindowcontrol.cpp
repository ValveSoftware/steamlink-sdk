/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "evrvideowindowcontrol.h"

#ifndef QT_NO_WIDGETS
#include <qwidget.h>
#endif

EvrVideoWindowControl::EvrVideoWindowControl(QObject *parent)
    : QVideoWindowControl(parent)
    , m_windowId(0)
    , m_windowColor(RGB(0, 0, 0))
    , m_dirtyValues(0)
    , m_aspectRatioMode(Qt::KeepAspectRatio)
    , m_brightness(0)
    , m_contrast(0)
    , m_hue(0)
    , m_saturation(0)
    , m_fullScreen(false)
    , m_displayControl(0)
    , m_processor(0)
{
}

EvrVideoWindowControl::~EvrVideoWindowControl()
{
   clear();
}

bool EvrVideoWindowControl::setEvr(IUnknown *evr)
{
    clear();

    if (!evr)
        return true;

    IMFGetService *service = NULL;

    if (SUCCEEDED(evr->QueryInterface(IID_PPV_ARGS(&service)))
            && SUCCEEDED(service->GetService(mr_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&m_displayControl)))) {

        service->GetService(mr_VIDEO_MIXER_SERVICE, IID_PPV_ARGS(&m_processor));

        setWinId(m_windowId);
        setDisplayRect(m_displayRect);
        setAspectRatioMode(m_aspectRatioMode);
        m_dirtyValues = DXVA2_ProcAmp_Brightness | DXVA2_ProcAmp_Contrast | DXVA2_ProcAmp_Hue | DXVA2_ProcAmp_Saturation;
        applyImageControls();
    }

    if (service)
        service->Release();

    return m_displayControl != NULL;
}

void EvrVideoWindowControl::clear()
{
    if (m_displayControl)
        m_displayControl->Release();
    m_displayControl = NULL;

    if (m_processor)
        m_processor->Release();
    m_processor = NULL;
}

WId EvrVideoWindowControl::winId() const
{
    return m_windowId;
}

void EvrVideoWindowControl::setWinId(WId id)
{
    m_windowId = id;

#ifndef QT_NO_WIDGETS
    if (QWidget *widget = QWidget::find(m_windowId)) {
        const QColor color = widget->palette().color(QPalette::Window);

        m_windowColor = RGB(color.red(), color.green(), color.blue());
    }
#endif

    if (m_displayControl)
        m_displayControl->SetVideoWindow(HWND(m_windowId));
}

QRect EvrVideoWindowControl::displayRect() const
{
    return m_displayRect;
}

void EvrVideoWindowControl::setDisplayRect(const QRect &rect)
{
    m_displayRect = rect;

    if (m_displayControl) {
        RECT displayRect = { rect.left(), rect.top(), rect.right() + 1, rect.bottom() + 1 };
        QSize sourceSize = nativeSize();

        RECT sourceRect = { 0, 0, sourceSize.width(), sourceSize.height() };

        if (m_aspectRatioMode == Qt::KeepAspectRatioByExpanding) {
            QSize clippedSize = rect.size();
            clippedSize.scale(sourceRect.right, sourceRect.bottom, Qt::KeepAspectRatio);

            sourceRect.left = (sourceRect.right - clippedSize.width()) / 2;
            sourceRect.top = (sourceRect.bottom - clippedSize.height()) / 2;
            sourceRect.right = sourceRect.left + clippedSize.width();
            sourceRect.bottom = sourceRect.top + clippedSize.height();
        }

        if (sourceSize.width() > 0 && sourceSize.height() > 0) {
            MFVideoNormalizedRect sourceNormRect;
            sourceNormRect.left = float(sourceRect.left) / float(sourceRect.right);
            sourceNormRect.top = float(sourceRect.top) / float(sourceRect.bottom);
            sourceNormRect.right = float(sourceRect.right) / float(sourceRect.right);
            sourceNormRect.bottom = float(sourceRect.bottom) / float(sourceRect.bottom);
            m_displayControl->SetVideoPosition(&sourceNormRect, &displayRect);
        } else {
            m_displayControl->SetVideoPosition(NULL, &displayRect);
        }
    }
}

bool EvrVideoWindowControl::isFullScreen() const
{
    return m_fullScreen;
}

void EvrVideoWindowControl::setFullScreen(bool fullScreen)
{
    if (m_fullScreen == fullScreen)
        return;
    emit fullScreenChanged(m_fullScreen = fullScreen);
}

void EvrVideoWindowControl::repaint()
{
    QSize size = nativeSize();
    if (size.width() > 0 && size.height() > 0
        && m_displayControl
        && SUCCEEDED(m_displayControl->RepaintVideo())) {
        return;
    }

    PAINTSTRUCT paint;
    if (HDC dc = ::BeginPaint(HWND(m_windowId), &paint)) {
        HPEN pen = ::CreatePen(PS_SOLID, 1, m_windowColor);
        HBRUSH brush = ::CreateSolidBrush(m_windowColor);
        ::SelectObject(dc, pen);
        ::SelectObject(dc, brush);

        ::Rectangle(
                dc,
                m_displayRect.left(),
                m_displayRect.top(),
                m_displayRect.right() + 1,
                m_displayRect.bottom() + 1);

        ::DeleteObject(pen);
        ::DeleteObject(brush);
        ::EndPaint(HWND(m_windowId), &paint);
    }
}

QSize EvrVideoWindowControl::nativeSize() const
{
    QSize size;
    if (m_displayControl) {
        SIZE sourceSize;
        if (SUCCEEDED(m_displayControl->GetNativeVideoSize(&sourceSize, 0)))
            size = QSize(sourceSize.cx, sourceSize.cy);
    }
    return size;
}

Qt::AspectRatioMode EvrVideoWindowControl::aspectRatioMode() const
{
    return m_aspectRatioMode;
}

void EvrVideoWindowControl::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    m_aspectRatioMode = mode;

    if (m_displayControl) {
        switch (mode) {
        case Qt::IgnoreAspectRatio:
            //comment from MSDN: Do not maintain the aspect ratio of the video. Stretch the video to fit the output rectangle.
            m_displayControl->SetAspectRatioMode(MFVideoARMode_None);
            break;
        case Qt::KeepAspectRatio:
            //comment from MSDN: Preserve the aspect ratio of the video by letterboxing or within the output rectangle.
            m_displayControl->SetAspectRatioMode(MFVideoARMode_PreservePicture);
            break;
        case Qt::KeepAspectRatioByExpanding:
            //for this mode, more adjustment will be done in setDisplayRect
            m_displayControl->SetAspectRatioMode(MFVideoARMode_PreservePicture);
            break;
        default:
            break;
        }
        setDisplayRect(m_displayRect);
    }
}

int EvrVideoWindowControl::brightness() const
{
    return m_brightness;
}

void EvrVideoWindowControl::setBrightness(int brightness)
{
    if (m_brightness == brightness)
        return;

    m_brightness = brightness;

    m_dirtyValues |= DXVA2_ProcAmp_Brightness;

    applyImageControls();

    emit brightnessChanged(brightness);
}

int EvrVideoWindowControl::contrast() const
{
    return m_contrast;
}

void EvrVideoWindowControl::setContrast(int contrast)
{
    if (m_contrast == contrast)
        return;

    m_contrast = contrast;

    m_dirtyValues |= DXVA2_ProcAmp_Contrast;

    applyImageControls();

    emit contrastChanged(contrast);
}

int EvrVideoWindowControl::hue() const
{
    return m_hue;
}

void EvrVideoWindowControl::setHue(int hue)
{
    if (m_hue == hue)
        return;

    m_hue = hue;

    m_dirtyValues |= DXVA2_ProcAmp_Hue;

    applyImageControls();

    emit hueChanged(hue);
}

int EvrVideoWindowControl::saturation() const
{
    return m_saturation;
}

void EvrVideoWindowControl::setSaturation(int saturation)
{
    if (m_saturation == saturation)
        return;

    m_saturation = saturation;

    m_dirtyValues |= DXVA2_ProcAmp_Saturation;

    applyImageControls();

    emit saturationChanged(saturation);
}

void EvrVideoWindowControl::applyImageControls()
{
    if (m_processor) {
        DXVA2_ProcAmpValues values;
        if (m_dirtyValues & DXVA2_ProcAmp_Brightness) {
            values.Brightness = scaleProcAmpValue(DXVA2_ProcAmp_Brightness, m_brightness);
        }
        if (m_dirtyValues & DXVA2_ProcAmp_Contrast) {
            values.Contrast = scaleProcAmpValue(DXVA2_ProcAmp_Contrast, m_contrast);
        }
        if (m_dirtyValues & DXVA2_ProcAmp_Hue) {
            values.Hue = scaleProcAmpValue(DXVA2_ProcAmp_Hue, m_hue);
        }
        if (m_dirtyValues & DXVA2_ProcAmp_Saturation) {
            values.Saturation = scaleProcAmpValue(DXVA2_ProcAmp_Saturation, m_saturation);
        }

        if (SUCCEEDED(m_processor->SetProcAmpValues(m_dirtyValues, &values))) {
            m_dirtyValues = 0;
        }
    }
}

DXVA2_Fixed32 EvrVideoWindowControl::scaleProcAmpValue(DWORD prop, int value) const
{
    float scaledValue = 0.0;

    DXVA2_ValueRange  range;
    if (SUCCEEDED(m_processor->GetProcAmpRange(prop, &range))) {
        scaledValue = DXVA2FixedToFloat(range.DefaultValue);
        if (value > 0)
            scaledValue += float(value) * (DXVA2FixedToFloat(range.MaxValue) - DXVA2FixedToFloat(range.DefaultValue)) / 100;
        else if (value < 0)
            scaledValue -= float(value) * (DXVA2FixedToFloat(range.MinValue) - DXVA2FixedToFloat(range.DefaultValue)) / 100;
    }

    return DXVA2FloatToFixed(scaledValue);
}
