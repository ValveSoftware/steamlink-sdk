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
#ifndef MMRENDERERVIDEOWINDOWCONTROL_H
#define MMRENDERERVIDEOWINDOWCONTROL_H

#include "mmrenderermetadata.h"
#include <qvideowindowcontrol.h>
#include <screen/screen.h>

typedef struct mmr_context mmr_context_t;

QT_BEGIN_NAMESPACE

class MmRendererVideoWindowControl : public QVideoWindowControl
{
    Q_OBJECT
public:
    explicit MmRendererVideoWindowControl(QObject *parent = 0);
    ~MmRendererVideoWindowControl();

    WId winId() const Q_DECL_OVERRIDE;
    void setWinId(WId id) Q_DECL_OVERRIDE;

    QRect displayRect() const Q_DECL_OVERRIDE;
    void setDisplayRect(const QRect &rect) Q_DECL_OVERRIDE;

    bool isFullScreen() const Q_DECL_OVERRIDE;
    void setFullScreen(bool fullScreen) Q_DECL_OVERRIDE;

    void repaint() Q_DECL_OVERRIDE;

    QSize nativeSize() const Q_DECL_OVERRIDE;

    Qt::AspectRatioMode aspectRatioMode() const Q_DECL_OVERRIDE;
    void setAspectRatioMode(Qt::AspectRatioMode mode) Q_DECL_OVERRIDE;

    int brightness() const Q_DECL_OVERRIDE;
    void setBrightness(int brightness) Q_DECL_OVERRIDE;

    int contrast() const Q_DECL_OVERRIDE;
    void setContrast(int contrast) Q_DECL_OVERRIDE;

    int hue() const Q_DECL_OVERRIDE;
    void setHue(int hue) Q_DECL_OVERRIDE;

    int saturation() const Q_DECL_OVERRIDE;
    void setSaturation(int saturation) Q_DECL_OVERRIDE;

    //
    // Called by media control
    //
    void detachDisplay();
    void attachDisplay(mmr_context_t *context);
    void setMetaData(const MmRendererMetaData &metaData);
    void screenEventHandler(const screen_event_t &event);

private:
    QWindow *findWindow(WId id) const;
    void updateVideoPosition();
    void updateBrightness();
    void updateContrast();
    void updateHue();
    void updateSaturation();

    int m_videoId;
    WId m_winId;
    QRect m_displayRect;
    mmr_context_t *m_context;
    bool m_fullscreen;
    MmRendererMetaData m_metaData;
    Qt::AspectRatioMode m_aspectRatioMode;
    QString m_windowName;
    screen_window_t m_window;
    int m_hue;
    int m_brightness;
    int m_contrast;
    int m_saturation;
};

QT_END_NAMESPACE

#endif
