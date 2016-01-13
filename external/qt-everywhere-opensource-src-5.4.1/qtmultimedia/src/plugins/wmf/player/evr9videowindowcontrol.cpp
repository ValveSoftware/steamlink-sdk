/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Mobility Components.
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

#include "evr9videowindowcontrol.h"
#include <QtWidgets/qwidget.h>
#include <QtCore/qdebug.h>
#include <QtCore/qglobal.h>

Evr9VideoWindowControl::Evr9VideoWindowControl(QObject *parent)
    : QVideoWindowControl(parent)
    , m_windowId(0)
    , m_dirtyValues(0)
    , m_aspectRatioMode(Qt::KeepAspectRatio)
    , m_brightness(0)
    , m_contrast(0)
    , m_hue(0)
    , m_saturation(0)
    , m_fullScreen(false)
    , m_currentActivate(0)
    , m_evrSink(0)
    , m_displayControl(0)
    , m_processor(0)
{
}

Evr9VideoWindowControl::~Evr9VideoWindowControl()
{
   clear();
}

void Evr9VideoWindowControl::clear()
{
    if (m_processor)
        m_processor->Release();
    if (m_displayControl)
        m_displayControl->Release();
    if (m_evrSink)
        m_evrSink->Release();
    if (m_currentActivate) {
        m_currentActivate->ShutdownObject();
        m_currentActivate->Release();
    }

    m_processor = NULL;
    m_displayControl = NULL;
    m_evrSink = NULL;
    m_currentActivate = NULL;
}

void Evr9VideoWindowControl::releaseActivate()
{
    clear();
}

WId Evr9VideoWindowControl::winId() const
{
    return m_windowId;
}

void Evr9VideoWindowControl::setWinId(WId id)
{
    m_windowId = id;

    if (QWidget *widget = QWidget::find(m_windowId)) {
        const QColor color = widget->palette().color(QPalette::Window);

        m_windowColor = RGB(color.red(), color.green(), color.blue());
    }

    if (m_displayControl) {
        m_displayControl->SetVideoWindow(HWND(m_windowId));
    }
}

QRect Evr9VideoWindowControl::displayRect() const
{
    return m_displayRect;
}

void Evr9VideoWindowControl::setDisplayRect(const QRect &rect)
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

bool Evr9VideoWindowControl::isFullScreen() const
{
    return m_fullScreen;
}

void Evr9VideoWindowControl::setFullScreen(bool fullScreen)
{
    if (m_fullScreen == fullScreen)
        return;
    emit fullScreenChanged(m_fullScreen = fullScreen);
}

void Evr9VideoWindowControl::repaint()
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

QSize Evr9VideoWindowControl::nativeSize() const
{
    QSize size;
    if (m_displayControl) {
        SIZE sourceSize;
        if (SUCCEEDED(m_displayControl->GetNativeVideoSize(&sourceSize, 0)))
            size = QSize(sourceSize.cx, sourceSize.cy);
    }
    return size;
}

Qt::AspectRatioMode Evr9VideoWindowControl::aspectRatioMode() const
{
    return m_aspectRatioMode;
}

void Evr9VideoWindowControl::setAspectRatioMode(Qt::AspectRatioMode mode)
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

int Evr9VideoWindowControl::brightness() const
{
    return m_brightness;
}

void Evr9VideoWindowControl::setBrightness(int brightness)
{
    if (m_brightness == brightness)
        return;

    m_brightness = brightness;

    m_dirtyValues |= DXVA2_ProcAmp_Brightness;

    setProcAmpValues();

    emit brightnessChanged(brightness);
}

int Evr9VideoWindowControl::contrast() const
{
    return m_contrast;
}

void Evr9VideoWindowControl::setContrast(int contrast)
{
    if (m_contrast == contrast)
        return;

    m_contrast = contrast;

    m_dirtyValues |= DXVA2_ProcAmp_Contrast;

    setProcAmpValues();

    emit contrastChanged(contrast);
}

int Evr9VideoWindowControl::hue() const
{
    return m_hue;
}

void Evr9VideoWindowControl::setHue(int hue)
{
    if (m_hue == hue)
        return;

    m_hue = hue;

    m_dirtyValues |= DXVA2_ProcAmp_Hue;

    setProcAmpValues();

    emit hueChanged(hue);
}

int Evr9VideoWindowControl::saturation() const
{
    return m_saturation;
}

void Evr9VideoWindowControl::setSaturation(int saturation)
{
    if (m_saturation == saturation)
        return;

    m_saturation = saturation;

    m_dirtyValues |= DXVA2_ProcAmp_Saturation;

    setProcAmpValues();

    emit saturationChanged(saturation);
}

IMFActivate* Evr9VideoWindowControl::createActivate()
{
    clear();

    if (FAILED(MFCreateVideoRendererActivate(0, &m_currentActivate))) {
        qWarning() << "Failed to create evr video renderer activate!";
        return 0;
    }
    if (FAILED(m_currentActivate->ActivateObject(IID_IMFMediaSink, (LPVOID*)(&m_evrSink)))) {
        qWarning() << "Failed to activate evr media sink!";
        return 0;
    }
    if (FAILED(MFGetService(m_evrSink, MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&m_displayControl)))) {
        qWarning() << "Failed to get display control from evr media sink!";
        return 0;
    }
    if (FAILED(MFGetService(m_evrSink,  MR_VIDEO_MIXER_SERVICE, IID_PPV_ARGS(&m_processor)))) {
        qWarning() << "Failed to get video processor from evr media sink!";
        return 0;
    }

    setWinId(m_windowId);
    setDisplayRect(m_displayRect);
    setAspectRatioMode(m_aspectRatioMode);
    m_dirtyValues = DXVA2_ProcAmp_Brightness | DXVA2_ProcAmp_Contrast | DXVA2_ProcAmp_Hue | DXVA2_ProcAmp_Saturation;

    return m_currentActivate;
}

void Evr9VideoWindowControl::setProcAmpValues()
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

DXVA2_Fixed32 Evr9VideoWindowControl::scaleProcAmpValue(DWORD prop, int value) const
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
