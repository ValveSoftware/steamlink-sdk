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

#include "qgstreamervideowidget_p.h"

#include <QtCore/qcoreevent.h>
#include <QtCore/qdebug.h>
#include <QtGui/qpainter.h>

QT_BEGIN_NAMESPACE

class QGstreamerVideoWidget : public QWidget
{
public:
    QGstreamerVideoWidget(QWidget *parent = 0)
        :QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QPalette palette;
        palette.setColor(QPalette::Background, Qt::black);
        setPalette(palette);
    }

    virtual ~QGstreamerVideoWidget() {}

    QSize sizeHint() const
    {
        return m_nativeSize;
    }

    void setNativeSize( const QSize &size)
    {
        if (size != m_nativeSize) {
            m_nativeSize = size;
            if (size.isEmpty())
                setMinimumSize(0,0);
            else
                setMinimumSize(160,120);

            updateGeometry();
        }
    }

    void paint_helper()
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().background());
    }

protected:
    void paintEvent(QPaintEvent *)
    {
        paint_helper();
    }

    QSize m_nativeSize;
};

QGstreamerVideoWidgetControl::QGstreamerVideoWidgetControl(QObject *parent, const QByteArray &elementName)
    : QVideoWidgetControl(parent)
    , m_videoOverlay(this, !elementName.isEmpty() ? elementName : qgetenv("QT_GSTREAMER_WIDGET_VIDEOSINK"))
    , m_widget(0)
    , m_stopped(false)
    , m_windowId(0)
    , m_fullScreen(false)
{
    connect(&m_videoOverlay, &QGstreamerVideoOverlay::activeChanged,
            this, &QGstreamerVideoWidgetControl::onOverlayActiveChanged);
    connect(&m_videoOverlay, &QGstreamerVideoOverlay::nativeVideoSizeChanged,
            this, &QGstreamerVideoWidgetControl::onNativeVideoSizeChanged);
    connect(&m_videoOverlay, &QGstreamerVideoOverlay::brightnessChanged,
            this, &QGstreamerVideoWidgetControl::brightnessChanged);
    connect(&m_videoOverlay, &QGstreamerVideoOverlay::contrastChanged,
            this, &QGstreamerVideoWidgetControl::contrastChanged);
    connect(&m_videoOverlay, &QGstreamerVideoOverlay::hueChanged,
            this, &QGstreamerVideoWidgetControl::hueChanged);
    connect(&m_videoOverlay, &QGstreamerVideoOverlay::saturationChanged,
            this, &QGstreamerVideoWidgetControl::saturationChanged);
}

QGstreamerVideoWidgetControl::~QGstreamerVideoWidgetControl()
{
    delete m_widget;
}

void QGstreamerVideoWidgetControl::createVideoWidget()
{
    if (m_widget)
        return;

    m_widget = new QGstreamerVideoWidget;

    m_widget->installEventFilter(this);
    m_videoOverlay.setWindowHandle(m_windowId = m_widget->winId());
}

GstElement *QGstreamerVideoWidgetControl::videoSink()
{
    return m_videoOverlay.videoSink();
}

void QGstreamerVideoWidgetControl::onOverlayActiveChanged()
{
    updateWidgetAttributes();
}

void QGstreamerVideoWidgetControl::stopRenderer()
{
    m_stopped = true;
    updateWidgetAttributes();
    m_widget->setNativeSize(QSize());
}

void QGstreamerVideoWidgetControl::onNativeVideoSizeChanged()
{
    const QSize &size = m_videoOverlay.nativeVideoSize();

    if (size.isValid())
        m_stopped = false;

    if (m_widget)
        m_widget->setNativeSize(size);
}

bool QGstreamerVideoWidgetControl::eventFilter(QObject *object, QEvent *e)
{
    if (m_widget && object == m_widget) {
        if (e->type() == QEvent::ParentChange || e->type() == QEvent::Show || e->type() == QEvent::WinIdChange) {
            WId newWId = m_widget->winId();
            if (newWId != m_windowId)
                m_videoOverlay.setWindowHandle(m_windowId = newWId);
        }

        if (e->type() == QEvent::Paint) {
            if (m_videoOverlay.isActive())
                m_videoOverlay.expose(); // triggers a repaint of the last frame
            else
                m_widget->paint_helper(); // paints the black background

            return true;
        }
    }

    return false;
}

void QGstreamerVideoWidgetControl::updateWidgetAttributes()
{
    // When frames are being rendered (sink is active), we need the WA_PaintOnScreen attribute to
    // be set in order to avoid flickering when the widget is repainted (for example when resized).
    // We need to clear that flag when the the sink is inactive to allow the widget to paint its
    // background, otherwise some garbage will be displayed.
    if (m_videoOverlay.isActive() && !m_stopped) {
        m_widget->setAttribute(Qt::WA_NoSystemBackground, true);
        m_widget->setAttribute(Qt::WA_PaintOnScreen, true);
    } else {
        m_widget->setAttribute(Qt::WA_NoSystemBackground, false);
        m_widget->setAttribute(Qt::WA_PaintOnScreen, false);
        m_widget->update();
    }
}

bool QGstreamerVideoWidgetControl::processSyncMessage(const QGstreamerMessage &message)
{
    return m_videoOverlay.processSyncMessage(message);
}

bool QGstreamerVideoWidgetControl::processBusMessage(const QGstreamerMessage &message)
{
    return m_videoOverlay.processBusMessage(message);
}

QWidget *QGstreamerVideoWidgetControl::videoWidget()
{
    createVideoWidget();
    return m_widget;
}

Qt::AspectRatioMode QGstreamerVideoWidgetControl::aspectRatioMode() const
{
    return m_videoOverlay.aspectRatioMode();
}

void QGstreamerVideoWidgetControl::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    m_videoOverlay.setAspectRatioMode(mode);
}

bool QGstreamerVideoWidgetControl::isFullScreen() const
{
    return m_fullScreen;
}

void QGstreamerVideoWidgetControl::setFullScreen(bool fullScreen)
{
    emit fullScreenChanged(m_fullScreen =  fullScreen);
}

int QGstreamerVideoWidgetControl::brightness() const
{
    return m_videoOverlay.brightness();
}

void QGstreamerVideoWidgetControl::setBrightness(int brightness)
{
    m_videoOverlay.setBrightness(brightness);
}

int QGstreamerVideoWidgetControl::contrast() const
{
    return m_videoOverlay.contrast();
}

void QGstreamerVideoWidgetControl::setContrast(int contrast)
{
    m_videoOverlay.setContrast(contrast);
}

int QGstreamerVideoWidgetControl::hue() const
{
    return m_videoOverlay.hue();
}

void QGstreamerVideoWidgetControl::setHue(int hue)
{
    m_videoOverlay.setHue(hue);
}

int QGstreamerVideoWidgetControl::saturation() const
{
    return m_videoOverlay.saturation();
}

void QGstreamerVideoWidgetControl::setSaturation(int saturation)
{
    m_videoOverlay.setSaturation(saturation);
}

QT_END_NAMESPACE
