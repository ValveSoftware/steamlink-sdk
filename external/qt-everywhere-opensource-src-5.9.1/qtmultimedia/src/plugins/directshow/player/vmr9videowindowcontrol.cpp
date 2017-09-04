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

#include "vmr9videowindowcontrol.h"

#include "directshowglobal.h"

#ifndef QT_NO_WIDGETS
#include <QtGui/QPalette>
#include <QtWidgets/QWidget>
#endif

Vmr9VideoWindowControl::Vmr9VideoWindowControl(QObject *parent)
    : QVideoWindowControl(parent)
    , m_filter(com_new<IBaseFilter>(CLSID_VideoMixingRenderer9))
    , m_windowId(0)
    , m_windowColor(RGB(0, 0, 0))
    , m_dirtyValues(0)
    , m_aspectRatioMode(Qt::KeepAspectRatio)
    , m_brightness(0)
    , m_contrast(0)
    , m_hue(0)
    , m_saturation(0)
    , m_fullScreen(false)
{
    if (IVMRFilterConfig9 *config = com_cast<IVMRFilterConfig9>(m_filter, IID_IVMRFilterConfig9)) {
        config->SetRenderingMode(VMR9Mode_Windowless);
        config->SetNumberOfStreams(1);
        config->Release();
    }
}

Vmr9VideoWindowControl::~Vmr9VideoWindowControl()
{
    if (m_filter)
        m_filter->Release();
}


WId Vmr9VideoWindowControl::winId() const
{
    return m_windowId;

}

void Vmr9VideoWindowControl::setWinId(WId id)
{
    m_windowId = id;

#ifndef QT_NO_WIDGETS
    if (QWidget *widget = QWidget::find(m_windowId)) {
        const QColor color = widget->palette().color(QPalette::Window);

        m_windowColor = RGB(color.red(), color.green(), color.blue());
    }
#endif

    if (IVMRWindowlessControl9 *control = com_cast<IVMRWindowlessControl9>(
            m_filter, IID_IVMRWindowlessControl9)) {
        control->SetVideoClippingWindow(reinterpret_cast<HWND>(m_windowId));
        control->SetBorderColor(m_windowColor);
        control->Release();
    }
}

QRect Vmr9VideoWindowControl::displayRect() const
{
    return m_displayRect;
}

void Vmr9VideoWindowControl::setDisplayRect(const QRect &rect)
{
    m_displayRect = rect;

    if (IVMRWindowlessControl9 *control = com_cast<IVMRWindowlessControl9>(
            m_filter, IID_IVMRWindowlessControl9)) {
        RECT sourceRect = { 0, 0, 0, 0 };
        RECT displayRect = { rect.left(), rect.top(), rect.right() + 1, rect.bottom() + 1 };

        control->GetNativeVideoSize(&sourceRect.right, &sourceRect.bottom, 0, 0);

        if (m_aspectRatioMode == Qt::KeepAspectRatioByExpanding) {
            QSize clippedSize = rect.size();
            clippedSize.scale(sourceRect.right, sourceRect.bottom, Qt::KeepAspectRatio);

            sourceRect.left = (sourceRect.right - clippedSize.width()) / 2;
            sourceRect.top = (sourceRect.bottom - clippedSize.height()) / 2;
            sourceRect.right = sourceRect.left + clippedSize.width();
            sourceRect.bottom = sourceRect.top + clippedSize.height();
        }

        control->SetVideoPosition(&sourceRect, &displayRect);
        control->Release();
    }
}

bool Vmr9VideoWindowControl::isFullScreen() const
{
    return m_fullScreen;
}

void Vmr9VideoWindowControl::setFullScreen(bool fullScreen)
{
    emit fullScreenChanged(m_fullScreen = fullScreen);
}

void Vmr9VideoWindowControl::repaint()
{
    PAINTSTRUCT paint;

    if (HDC dc = ::BeginPaint(reinterpret_cast<HWND>(m_windowId), &paint)) {
        HRESULT hr = E_FAIL;

        if (IVMRWindowlessControl9 *control = com_cast<IVMRWindowlessControl9>(
                m_filter, IID_IVMRWindowlessControl9)) {
            hr = control->RepaintVideo(reinterpret_cast<HWND>(m_windowId), dc);
            control->Release();
        }

        if (!SUCCEEDED(hr)) {
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
        }
        ::EndPaint(reinterpret_cast<HWND>(m_windowId), &paint);
    }
}

QSize Vmr9VideoWindowControl::nativeSize() const
{
    QSize size;

    if (IVMRWindowlessControl9 *control = com_cast<IVMRWindowlessControl9>(
            m_filter, IID_IVMRWindowlessControl9)) {
        LONG width;
        LONG height;

        if (control->GetNativeVideoSize(&width, &height, 0, 0) == S_OK)
            size = QSize(width, height);
        control->Release();
    }
    return size;
}

Qt::AspectRatioMode Vmr9VideoWindowControl::aspectRatioMode() const
{
    return m_aspectRatioMode;
}

void Vmr9VideoWindowControl::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    m_aspectRatioMode = mode;

    if (IVMRWindowlessControl9 *control = com_cast<IVMRWindowlessControl9>(
            m_filter, IID_IVMRWindowlessControl9)) {
        switch (mode) {
        case Qt::IgnoreAspectRatio:
            control->SetAspectRatioMode(VMR9ARMode_None);
            break;
        case Qt::KeepAspectRatio:
            control->SetAspectRatioMode(VMR9ARMode_LetterBox);
            break;
        case Qt::KeepAspectRatioByExpanding:
            control->SetAspectRatioMode(VMR9ARMode_LetterBox);
            setDisplayRect(m_displayRect);
            break;
        default:
            break;
        }
        control->Release();
    }
}

int Vmr9VideoWindowControl::brightness() const
{
    return m_brightness;
}

void Vmr9VideoWindowControl::setBrightness(int brightness)
{
    m_brightness = brightness;

    m_dirtyValues |= ProcAmpControl9_Brightness;

    setProcAmpValues();

    emit brightnessChanged(brightness);
}

int Vmr9VideoWindowControl::contrast() const
{
    return m_contrast;
}

void Vmr9VideoWindowControl::setContrast(int contrast)
{
    m_contrast = contrast;

    m_dirtyValues |= ProcAmpControl9_Contrast;

    setProcAmpValues();

    emit contrastChanged(contrast);
}

int Vmr9VideoWindowControl::hue() const
{
    return m_hue;
}

void Vmr9VideoWindowControl::setHue(int hue)
{
    m_hue = hue;

    m_dirtyValues |= ProcAmpControl9_Hue;

    setProcAmpValues();

    emit hueChanged(hue);
}

int Vmr9VideoWindowControl::saturation() const
{
    return m_saturation;
}

void Vmr9VideoWindowControl::setSaturation(int saturation)
{
    m_saturation = saturation;

    m_dirtyValues |= ProcAmpControl9_Saturation;

    setProcAmpValues();

    emit saturationChanged(saturation);
}

void Vmr9VideoWindowControl::setProcAmpValues()
{
    if (IVMRMixerControl9 *control = com_cast<IVMRMixerControl9>(m_filter, IID_IVMRMixerControl9)) {
        VMR9ProcAmpControl procAmp;
        procAmp.dwSize = sizeof(VMR9ProcAmpControl);
        procAmp.dwFlags = m_dirtyValues;

        if (m_dirtyValues & ProcAmpControl9_Brightness) {
            procAmp.Brightness = scaleProcAmpValue(
                    control, ProcAmpControl9_Brightness, m_brightness);
        }
        if (m_dirtyValues & ProcAmpControl9_Contrast) {
            procAmp.Contrast = scaleProcAmpValue(
                    control, ProcAmpControl9_Contrast, m_contrast);
        }
        if (m_dirtyValues & ProcAmpControl9_Hue) {
            procAmp.Hue = scaleProcAmpValue(
                    control, ProcAmpControl9_Hue, m_hue);
        }
        if (m_dirtyValues & ProcAmpControl9_Saturation) {
            procAmp.Saturation = scaleProcAmpValue(
                    control, ProcAmpControl9_Saturation, m_saturation);
        }

        if (SUCCEEDED(control->SetProcAmpControl(0, &procAmp))) {
            m_dirtyValues = 0;
        }

        control->Release();
    }
}

float Vmr9VideoWindowControl::scaleProcAmpValue(
        IVMRMixerControl9 *control, VMR9ProcAmpControlFlags property, int value) const
{
    float scaledValue = 0.0;

    VMR9ProcAmpControlRange range;
    range.dwSize = sizeof(VMR9ProcAmpControlRange);
    range.dwProperty = property;

    if (SUCCEEDED(control->GetProcAmpControlRange(0, &range))) {
        scaledValue = range.DefaultValue;
        if (value > 0)
            scaledValue += float(value) * (range.MaxValue - range.DefaultValue) / 100;
        else if (value < 0)
            scaledValue -= float(value) * (range.MinValue - range.DefaultValue) / 100;
    }

    return scaledValue;
}
