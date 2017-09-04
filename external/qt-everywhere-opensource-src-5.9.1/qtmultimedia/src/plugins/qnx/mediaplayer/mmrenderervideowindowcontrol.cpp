/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#include "mmrenderervideowindowcontrol.h"
#include "mmrendererutil.h"
#include <QtCore/qdebug.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/qscreen.h>
#include <QtGui/qwindow.h>
#include <mm/renderer.h>

QT_BEGIN_NAMESPACE

static int winIdCounter = 0;

MmRendererVideoWindowControl::MmRendererVideoWindowControl(QObject *parent)
    : QVideoWindowControl(parent),
      m_videoId(-1),
      m_winId(0),
      m_context(0),
      m_fullscreen(false),
      m_aspectRatioMode(Qt::IgnoreAspectRatio),
      m_window(0),
      m_hue(0),
      m_brightness(0),
      m_contrast(0),
      m_saturation(0)
{
}

MmRendererVideoWindowControl::~MmRendererVideoWindowControl()
{
}

WId MmRendererVideoWindowControl::winId() const
{
    return m_winId;
}

void MmRendererVideoWindowControl::setWinId(WId id)
{
    m_winId = id;
}

QRect MmRendererVideoWindowControl::displayRect() const
{
    return m_displayRect ;
}

void MmRendererVideoWindowControl::setDisplayRect(const QRect &rect)
{
    if (m_displayRect != rect) {
        m_displayRect = rect;
        updateVideoPosition();
    }
}

bool MmRendererVideoWindowControl::isFullScreen() const
{
    return m_fullscreen;
}

void MmRendererVideoWindowControl::setFullScreen(bool fullScreen)
{
    if (m_fullscreen != fullScreen) {
        m_fullscreen = fullScreen;
        updateVideoPosition();
        emit fullScreenChanged(m_fullscreen);
    }
}

void MmRendererVideoWindowControl::repaint()
{
    // Nothing we can or should do here
}

QSize MmRendererVideoWindowControl::nativeSize() const
{
    return QSize(m_metaData.width(), m_metaData.height());
}

Qt::AspectRatioMode MmRendererVideoWindowControl::aspectRatioMode() const
{
    return m_aspectRatioMode;
}

void MmRendererVideoWindowControl::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    m_aspectRatioMode = mode;
}

int MmRendererVideoWindowControl::brightness() const
{
    return m_brightness;
}

void MmRendererVideoWindowControl::setBrightness(int brightness)
{
    if (m_brightness != brightness) {
        m_brightness = brightness;
        updateBrightness();
        emit brightnessChanged(m_brightness);
    }
}

int MmRendererVideoWindowControl::contrast() const
{
    return m_contrast;
}

void MmRendererVideoWindowControl::setContrast(int contrast)
{
    if (m_contrast != contrast) {
        m_contrast = contrast;
        updateContrast();
        emit contrastChanged(m_contrast);
    }
}

int MmRendererVideoWindowControl::hue() const
{
    return m_hue;
}

void MmRendererVideoWindowControl::setHue(int hue)
{
    if (m_hue != hue) {
        m_hue = hue;
        updateHue();
        emit hueChanged(m_hue);
    }
}

int MmRendererVideoWindowControl::saturation() const
{
    return m_saturation;
}

void MmRendererVideoWindowControl::setSaturation(int saturation)
{
    if (m_saturation != saturation) {
        m_saturation = saturation;
        updateSaturation();
        emit saturationChanged(m_saturation);
    }
}

void MmRendererVideoWindowControl::attachDisplay(mmr_context_t *context)
{
    if (m_videoId != -1) {
        qDebug() << "MmRendererVideoWindowControl: Video output already attached!";
        return;
    }

    if (!context) {
        qDebug() << "MmRendererVideoWindowControl: No media player context!";
        return;
    }

    QWindow *window = findWindow(m_winId);
    if (!window) {
        qDebug() << "MmRendererVideoWindowControl: No video window!";
        return;
    }

    QPlatformNativeInterface * const nativeInterface = QGuiApplication::platformNativeInterface();
    if (!nativeInterface) {
        qDebug() << "MmRendererVideoWindowControl: Unable to get platform native interface";
        return;
    }

    QWindow *windowForGroup = window;

    //According to mmr_output_attach() documentation, the window group name of the
    //application's top-level window is expected.
    while (windowForGroup->parent())
        windowForGroup = windowForGroup->parent();

    const char * const groupNameData = static_cast<const char *>(
        nativeInterface->nativeResourceForWindow("windowGroup", windowForGroup));
    if (!groupNameData) {
        qDebug() << "MmRendererVideoWindowControl: Unable to find window group for window"
                 << windowForGroup;
        return;
    }

    const QString groupName = QString::fromLatin1(groupNameData);
    m_windowName = QString("MmRendererVideoWindowControl_%1_%2").arg(winIdCounter++)
                                                        .arg(QCoreApplication::applicationPid());

    nativeInterface->setWindowProperty(window->handle(),
            QStringLiteral("mmRendererWindowName"), m_windowName);

    // Start with an invisible window. If it would be visible right away, it would be at the wrong
    // position, and we can only change the position once we get the window handle.
    const QString videoDeviceUrl =
            QString("screen:?winid=%1&wingrp=%2&initflags=invisible&nodstviewport=1").arg(m_windowName).arg(groupName);

    m_videoId = mmr_output_attach(context, videoDeviceUrl.toLatin1(), "video");
    if (m_videoId == -1) {
        qDebug() << mmErrorMessage("mmr_output_attach() for video failed", context);
        return;
    }

    m_context = context;
    updateVideoPosition();
    updateHue();
    updateContrast();
    updateBrightness();
    updateSaturation();
}

void MmRendererVideoWindowControl::updateVideoPosition()
{
    QWindow * const window = findWindow(m_winId);
    if (m_context && m_videoId != -1 && window) {
        QPoint topLeft = m_fullscreen ?
                                   QPoint(0,0) :
                                   window->mapToGlobal(m_displayRect.topLeft());

        QScreen * const screen = window->screen();
        int width = m_fullscreen ?
                        screen->size().width() :
                        m_displayRect.width();
        int height = m_fullscreen ?
                        screen->size().height() :
                        m_displayRect.height();

        if (m_metaData.hasVideo()) { // We need the source size to do aspect ratio scaling
            const qreal sourceRatio = m_metaData.width() / static_cast<float>(m_metaData.height());
            const qreal targetRatio = width / static_cast<float>(height);

            if (m_aspectRatioMode == Qt::KeepAspectRatio) {
                if (targetRatio < sourceRatio) {
                    // Need to make height smaller
                    const int newHeight = width / sourceRatio;
                    const int heightDiff = height - newHeight;
                    topLeft.ry() += heightDiff / 2;
                    height = newHeight;
                } else {
                    // Need to make width smaller
                    const int newWidth = sourceRatio * height;
                    const int widthDiff = width - newWidth;
                    topLeft.rx() += widthDiff / 2;
                    width = newWidth;
                }

            } else if (m_aspectRatioMode == Qt::KeepAspectRatioByExpanding) {
                if (targetRatio < sourceRatio) {
                    // Need to make width larger
                    const int newWidth = sourceRatio * height;
                    const int widthDiff = newWidth - width;
                    topLeft.rx() -= widthDiff / 2;
                    width = newWidth;
                } else {
                    // Need to make height larger
                    const int newHeight = width / sourceRatio;
                    const int heightDiff = newHeight - height;
                    topLeft.ry() -= heightDiff / 2;
                    height = newHeight;
                }
            }
        }

        if (m_window != 0) {
            const int position[2] = { topLeft.x(), topLeft.y() };
            const int size[2] = { width, height };
            const int visible = m_displayRect.isValid();
            if (screen_set_window_property_iv(m_window, SCREEN_PROPERTY_POSITION, position) != 0)
                perror("Setting video position failed");
            if (screen_set_window_property_iv(m_window, SCREEN_PROPERTY_SIZE, size) != 0)
                perror("Setting video size failed");
            if (screen_set_window_property_iv(m_window, SCREEN_PROPERTY_VISIBLE, &visible) != 0)
                perror("Setting video visibility failed");
        }
    }
}

void MmRendererVideoWindowControl::updateBrightness()
{
    if (m_window != 0) {
        const int backendValue = m_brightness * 2.55f;
        if (screen_set_window_property_iv(m_window, SCREEN_PROPERTY_BRIGHTNESS, &backendValue) != 0)
            perror("Setting brightness failed");
    }
}

void MmRendererVideoWindowControl::updateContrast()
{
    if (m_window != 0) {
        const int backendValue = m_contrast * 1.27f;
        if (screen_set_window_property_iv(m_window, SCREEN_PROPERTY_CONTRAST, &backendValue) != 0)
            perror("Setting contrast failed");
    }
}

void MmRendererVideoWindowControl::updateHue()
{
    if (m_window != 0) {
        const int backendValue = m_hue * 1.27f;
        if (screen_set_window_property_iv(m_window, SCREEN_PROPERTY_HUE, &backendValue) != 0)
            perror("Setting hue failed");
    }
}

void MmRendererVideoWindowControl::updateSaturation()
{
    if (m_window != 0) {
        const int backendValue = m_saturation * 1.27f;
        if (screen_set_window_property_iv(m_window, SCREEN_PROPERTY_SATURATION, &backendValue) != 0)
            perror("Setting saturation failed");
    }
}

void MmRendererVideoWindowControl::detachDisplay()
{
    if (m_context && m_videoId != -1)
        mmr_output_detach(m_context, m_videoId);

    m_context = 0;
    m_videoId = -1;
    m_metaData.clear();
    m_windowName.clear();
    m_window = 0;
    m_hue = 0;
    m_brightness = 0;
    m_contrast = 0;
    m_saturation = 0;
}

void MmRendererVideoWindowControl::setMetaData(const MmRendererMetaData &metaData)
{
    m_metaData = metaData;
    emit nativeSizeChanged();

    // To handle the updated source size data
    updateVideoPosition();
}

void MmRendererVideoWindowControl::screenEventHandler(const screen_event_t &screen_event)
{
    int eventType;
    if (screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &eventType) != 0) {
        perror("MmRendererVideoWindowControl: Failed to query screen event type");
        return;
    }

    if (eventType != SCREEN_EVENT_CREATE)
        return;

    screen_window_t window = 0;
    if (screen_get_event_property_pv(screen_event, SCREEN_PROPERTY_WINDOW, (void**)&window) != 0) {
        perror("MmRendererVideoWindowControl: Failed to query window property");
        return;
    }

    const int maxIdStrLength = 128;
    char idString[maxIdStrLength];
    if (screen_get_window_property_cv(window, SCREEN_PROPERTY_ID_STRING, maxIdStrLength, idString) != 0) {
        perror("MmRendererVideoWindowControl: Failed to query window ID string");
        return;
    }

    if (m_windowName == idString) {
        m_window = window;
        updateVideoPosition();

        const int visibleFlag = 1;
        if (screen_set_window_property_iv(m_window, SCREEN_PROPERTY_VISIBLE, &visibleFlag) != 0) {
            perror("MmRendererVideoWindowControl: Failed to make window visible");
            return;
        }
    }
}

QWindow *MmRendererVideoWindowControl::findWindow(WId id) const
{
    const auto allWindows = QGuiApplication::allWindows();
    for (QWindow *window : allWindows)
        if (window->winId() == id)
            return window;
    return 0;
}

QT_END_NAMESPACE
