/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#include "qgstreamervideowidget_p.h"
#include <private/qgstutils_p.h>

#include <QtCore/qcoreevent.h>
#include <QtCore/qdebug.h>
#include <QtWidgets/qapplication.h>
#include <QtGui/qpainter.h>

#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>
#include <gst/interfaces/propertyprobe.h>

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

protected:
    void paintEvent(QPaintEvent *)
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().background());
    }

    QSize m_nativeSize;
};

QGstreamerVideoWidgetControl::QGstreamerVideoWidgetControl(QObject *parent)
    : QVideoWidgetControl(parent)
    , m_videoSink(0)
    , m_widget(0)
    , m_fullScreen(false)
{
}

QGstreamerVideoWidgetControl::~QGstreamerVideoWidgetControl()
{
    if (m_videoSink)
        gst_object_unref(GST_OBJECT(m_videoSink));

    delete m_widget;
}

void QGstreamerVideoWidgetControl::createVideoWidget()
{
    if (m_widget)
        return;

    m_widget = new QGstreamerVideoWidget;

    m_widget->installEventFilter(this);
    m_windowId = m_widget->winId();

    m_videoSink = gst_element_factory_make ("xvimagesink", NULL);
    if (m_videoSink) {
        // Check if the xv sink is usable
        if (gst_element_set_state(m_videoSink, GST_STATE_READY) != GST_STATE_CHANGE_SUCCESS) {
            gst_object_unref(GST_OBJECT(m_videoSink));
            m_videoSink = 0;
        } else {
            gst_element_set_state(m_videoSink, GST_STATE_NULL);

            g_object_set(G_OBJECT(m_videoSink), "force-aspect-ratio", 1, (const char*)NULL);
        }
    }

    if (!m_videoSink)
        m_videoSink = gst_element_factory_make ("ximagesink", NULL);

    qt_gst_object_ref_sink(GST_OBJECT (m_videoSink)); //Take ownership


}

GstElement *QGstreamerVideoWidgetControl::videoSink()
{
    createVideoWidget();
    return m_videoSink;
}

bool QGstreamerVideoWidgetControl::eventFilter(QObject *object, QEvent *e)
{
    if (m_widget && object == m_widget) {
        if (e->type() == QEvent::ParentChange || e->type() == QEvent::Show) {
            WId newWId = m_widget->winId();
            if (newWId != m_windowId) {
                m_windowId = newWId;
                setOverlay();
            }
        }

        if (e->type() == QEvent::Show) {
            // Setting these values ensures smooth resizing since it
            // will prevent the system from clearing the background
            m_widget->setAttribute(Qt::WA_NoSystemBackground, true);
        } else if (e->type() == QEvent::Resize) {
            // This is a workaround for missing background repaints
            // when reducing window size
            windowExposed();
        }
    }

    return false;
}

bool QGstreamerVideoWidgetControl::processSyncMessage(const QGstreamerMessage &message)
{
    GstMessage* gm = message.rawMessage();

    if (gm && (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_ELEMENT) &&
            gst_structure_has_name(gm->structure, "prepare-xwindow-id")) {

        setOverlay();
        QMetaObject::invokeMethod(this, "updateNativeVideoSize", Qt::QueuedConnection);
        return true;
    }

    return false;
}

bool QGstreamerVideoWidgetControl::processBusMessage(const QGstreamerMessage &message)
{
    GstMessage* gm = message.rawMessage();

    if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_STATE_CHANGED &&
            GST_MESSAGE_SRC(gm) == GST_OBJECT_CAST(m_videoSink)) {
        GstState oldState;
        GstState newState;
        gst_message_parse_state_changed(gm, &oldState, &newState, 0);

        if (oldState == GST_STATE_READY && newState == GST_STATE_PAUSED)
            updateNativeVideoSize();
    }

    return false;
}

void QGstreamerVideoWidgetControl::setOverlay()
{
    if (m_videoSink && GST_IS_X_OVERLAY(m_videoSink)) {
        gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(m_videoSink), m_windowId);
    }
}

void QGstreamerVideoWidgetControl::updateNativeVideoSize()
{
    if (m_videoSink) {
        //find video native size to update video widget size hint
        GstPad *pad = gst_element_get_static_pad(m_videoSink,"sink");
        GstCaps *caps = gst_pad_get_negotiated_caps(pad);
        gst_object_unref(GST_OBJECT(pad));

        if (caps) {
            m_widget->setNativeSize(QGstUtils::capsCorrectedResolution(caps));
            gst_caps_unref(caps);
        }
    } else {
        if (m_widget)
            m_widget->setNativeSize(QSize());
    }
}


void QGstreamerVideoWidgetControl::windowExposed()
{
    if (m_videoSink && GST_IS_X_OVERLAY(m_videoSink))
        gst_x_overlay_expose(GST_X_OVERLAY(m_videoSink));
}

QWidget *QGstreamerVideoWidgetControl::videoWidget()
{
    createVideoWidget();
    return m_widget;
}

Qt::AspectRatioMode QGstreamerVideoWidgetControl::aspectRatioMode() const
{
    return m_aspectRatioMode;
}

void QGstreamerVideoWidgetControl::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    if (m_videoSink) {
        g_object_set(G_OBJECT(m_videoSink),
                     "force-aspect-ratio",
                     (mode == Qt::KeepAspectRatio),
                     (const char*)NULL);
    }

    m_aspectRatioMode = mode;
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
    int brightness = 0;

    if (m_videoSink && g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSink), "brightness"))
        g_object_get(G_OBJECT(m_videoSink), "brightness", &brightness, NULL);

    return brightness / 10;
}

void QGstreamerVideoWidgetControl::setBrightness(int brightness)
{
    if (m_videoSink && g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSink), "brightness")) {
        g_object_set(G_OBJECT(m_videoSink), "brightness", brightness * 10, NULL);

        emit brightnessChanged(brightness);
    }
}

int QGstreamerVideoWidgetControl::contrast() const
{
    int contrast = 0;

    if (m_videoSink && g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSink), "contrast"))
        g_object_get(G_OBJECT(m_videoSink), "contrast", &contrast, NULL);

    return contrast / 10;
}

void QGstreamerVideoWidgetControl::setContrast(int contrast)
{
    if (m_videoSink && g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSink), "contrast")) {
        g_object_set(G_OBJECT(m_videoSink), "contrast", contrast * 10, NULL);

        emit contrastChanged(contrast);
    }
}

int QGstreamerVideoWidgetControl::hue() const
{
    int hue = 0;

    if (m_videoSink && g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSink), "hue"))
        g_object_get(G_OBJECT(m_videoSink), "hue", &hue, NULL);

    return hue / 10;
}

void QGstreamerVideoWidgetControl::setHue(int hue)
{
    if (m_videoSink && g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSink), "hue")) {
        g_object_set(G_OBJECT(m_videoSink), "hue", hue * 10, NULL);

        emit hueChanged(hue);
    }
}

int QGstreamerVideoWidgetControl::saturation() const
{
    int saturation = 0;

    if (m_videoSink && g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSink), "saturation"))
        g_object_get(G_OBJECT(m_videoSink), "saturation", &saturation, NULL);

    return saturation / 10;
}

void QGstreamerVideoWidgetControl::setSaturation(int saturation)
{
    if (m_videoSink && g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSink), "saturation")) {
        g_object_set(G_OBJECT(m_videoSink), "saturation", saturation * 10, NULL);

        emit saturationChanged(saturation);
    }
}

QT_END_NAMESPACE
