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

#include "qgstreamercapturesession.h"
#include "qgstreamerrecordercontrol.h"
#include "qgstreamermediacontainercontrol.h"
#include "qgstreameraudioencode.h"
#include "qgstreamervideoencode.h"
#include "qgstreamerimageencode.h"
#include <qmediarecorder.h>
#include <private/qgstreamervideorendererinterface_p.h>
#include <private/qgstreameraudioprobecontrol_p.h>
#include <private/qgstreamerbushelper_p.h>
#include <private/qgstutils_p.h>

#include <gst/gsttagsetter.h>
#include <gst/gstversion.h>
#include <gst/video/video.h>

#include <QtCore/qdebug.h>
#include <QtCore/qurl.h>
#include <QtCore/qset.h>
#include <QCoreApplication>
#include <QtCore/qmetaobject.h>
#include <QtCore/qfile.h>
#include <QtGui/qimage.h>

QT_BEGIN_NAMESPACE

QGstreamerCaptureSession::QGstreamerCaptureSession(QGstreamerCaptureSession::CaptureMode captureMode, QObject *parent)
    :QObject(parent),
     m_state(StoppedState),
     m_pendingState(StoppedState),
     m_waitingForEos(false),
     m_pipelineMode(EmptyPipeline),
     m_captureMode(captureMode),
     m_audioProbe(0),
     m_audioInputFactory(0),
     m_audioPreviewFactory(0),
     m_videoInputFactory(0),
     m_viewfinder(0),
     m_viewfinderInterface(0),
     m_audioSrc(0),
     m_audioTee(0),
     m_audioPreviewQueue(0),
     m_audioPreview(0),
     m_audioVolume(0),
     m_muted(false),
     m_volume(1.0),
     m_videoSrc(0),
     m_videoTee(0),
     m_videoPreviewQueue(0),
     m_videoPreview(0),
     m_imageCaptureBin(0),
     m_encodeBin(0),
     m_passImage(false),
     m_passPrerollImage(false)
{
    m_pipeline = gst_pipeline_new("media-capture-pipeline");
    qt_gst_object_ref_sink(m_pipeline);

    m_bus = gst_element_get_bus(m_pipeline);
    m_busHelper = new QGstreamerBusHelper(m_bus, this);
    m_busHelper->installMessageFilter(this);

    m_audioEncodeControl = new QGstreamerAudioEncode(this);
    m_videoEncodeControl = new QGstreamerVideoEncode(this);
    m_imageEncodeControl = new QGstreamerImageEncode(this);
    m_recorderControl = new QGstreamerRecorderControl(this);
    m_mediaContainerControl = new QGstreamerMediaContainerControl(this);

    setState(StoppedState);
}

QGstreamerCaptureSession::~QGstreamerCaptureSession()
{
    setState(StoppedState);
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(m_bus));
    gst_object_unref(GST_OBJECT(m_pipeline));
}

void QGstreamerCaptureSession::setCaptureMode(CaptureMode mode)
{
    m_captureMode = mode;
}

GstElement *QGstreamerCaptureSession::buildEncodeBin()
{
    GstElement *encodeBin = gst_bin_new("encode-bin");

    GstElement *muxer = gst_element_factory_make( m_mediaContainerControl->formatElementName().constData(), "muxer");
    if (!muxer) {
        qWarning() << "Could not create a media muxer element:" << m_mediaContainerControl->formatElementName();
        gst_object_unref(encodeBin);
        return 0;
    }

    // Output location was rejected in setOutputlocation() if not a local file
    QUrl actualSink = QUrl::fromLocalFile(QDir::currentPath()).resolved(m_sink);
    GstElement *fileSink = gst_element_factory_make("filesink", "filesink");
    g_object_set(G_OBJECT(fileSink), "location", QFile::encodeName(actualSink.toLocalFile()).constData(), NULL);
    gst_bin_add_many(GST_BIN(encodeBin), muxer, fileSink,  NULL);

    if (!gst_element_link(muxer, fileSink)) {
        gst_object_unref(encodeBin);
        return 0;
    }

    if (m_captureMode & Audio) {
        GstElement *audioConvert = gst_element_factory_make("audioconvert", "audioconvert");
        GstElement *audioQueue = gst_element_factory_make("queue", "audio-encode-queue");
        m_audioVolume = gst_element_factory_make("volume", "volume");
        gst_bin_add_many(GST_BIN(encodeBin), audioConvert, audioQueue, m_audioVolume, NULL);

        GstElement *audioEncoder = m_audioEncodeControl->createEncoder();
        if (!audioEncoder) {
            gst_object_unref(encodeBin);
            qWarning() << "Could not create an audio encoder element:" << m_audioEncodeControl->audioSettings().codec();
            return 0;
        }

        gst_bin_add(GST_BIN(encodeBin), audioEncoder);

        if (!gst_element_link_many(audioConvert, audioQueue, m_audioVolume, audioEncoder, muxer, NULL)) {
            m_audioVolume = 0;
            gst_object_unref(encodeBin);
            return 0;
        }

        g_object_set(G_OBJECT(m_audioVolume), "mute", m_muted, NULL);
        g_object_set(G_OBJECT(m_audioVolume), "volume", m_volume, NULL);

        // add ghostpads
        GstPad *pad = gst_element_get_static_pad(audioConvert, "sink");
        gst_element_add_pad(GST_ELEMENT(encodeBin), gst_ghost_pad_new("audiosink", pad));
        gst_object_unref(GST_OBJECT(pad));
    }

    if (m_captureMode & Video) {
        GstElement *videoQueue = gst_element_factory_make("queue", "video-encode-queue");
        GstElement *colorspace = gst_element_factory_make(QT_GSTREAMER_COLORCONVERSION_ELEMENT_NAME, "videoconvert-encoder");
        GstElement *videoscale = gst_element_factory_make("videoscale","videoscale-encoder");
        gst_bin_add_many(GST_BIN(encodeBin), videoQueue, colorspace, videoscale, NULL);

        GstElement *videoEncoder = m_videoEncodeControl->createEncoder();
        if (!videoEncoder) {
            gst_object_unref(encodeBin);
            qWarning() << "Could not create a video encoder element:" << m_videoEncodeControl->videoSettings().codec();
            return 0;
        }

        gst_bin_add(GST_BIN(encodeBin), videoEncoder);

        if (!gst_element_link_many(videoQueue, colorspace, videoscale, videoEncoder, muxer, NULL)) {
            gst_object_unref(encodeBin);
            return 0;
        }

        // add ghostpads
        GstPad *pad = gst_element_get_static_pad(videoQueue, "sink");
        gst_element_add_pad(GST_ELEMENT(encodeBin), gst_ghost_pad_new("videosink", pad));
        gst_object_unref(GST_OBJECT(pad));
    }

    return encodeBin;
}

GstElement *QGstreamerCaptureSession::buildAudioSrc()
{
    GstElement *audioSrc = 0;
    if (m_audioInputFactory)
        audioSrc = m_audioInputFactory->buildElement();
    else {
        QString elementName = "alsasrc";
        QString device;

        if (m_captureDevice.startsWith("alsa:")) {
            device = m_captureDevice.mid(QString("alsa:").length());
        } else if (m_captureDevice.startsWith("oss:")) {
            elementName = "osssrc";
            device = m_captureDevice.mid(QString("oss:").length());
        } else if (m_captureDevice.startsWith("pulseaudio:")) {
            elementName = "pulsesrc";
        } else {
            elementName = "autoaudiosrc";
        }

        audioSrc = gst_element_factory_make(elementName.toLatin1().constData(), "audio_src");
        if (audioSrc && !device.isEmpty())
            g_object_set(G_OBJECT(audioSrc), "device", device.toLocal8Bit().constData(), NULL);
    }

    if (!audioSrc) {
        emit error(int(QMediaRecorder::ResourceError), tr("Could not create an audio source element"));
        audioSrc = gst_element_factory_make("fakesrc", NULL);
    }

    return audioSrc;
}

GstElement *QGstreamerCaptureSession::buildAudioPreview()
{
    GstElement *previewElement = 0;

    if (m_audioPreviewFactory) {
        previewElement = m_audioPreviewFactory->buildElement();
    } else {


#if 1
        previewElement = gst_element_factory_make("fakesink", "audio-preview");
#else
        GstElement *bin = gst_bin_new("audio-preview-bin");
        GstElement *visual = gst_element_factory_make("libvisual_lv_scope", "audio-preview");
        GstElement *sink = gst_element_factory_make("ximagesink", NULL);
        gst_bin_add_many(GST_BIN(bin), visual, sink,  NULL);
        gst_element_link_many(visual,sink, NULL);


        // add ghostpads
        GstPad *pad = gst_element_get_static_pad(visual, "sink");
        Q_ASSERT(pad);
        gst_element_add_pad(GST_ELEMENT(bin), gst_ghost_pad_new("audiosink", pad));
        gst_object_unref(GST_OBJECT(pad));

        previewElement = bin;
#endif
    }

    return previewElement;
}

GstElement *QGstreamerCaptureSession::buildVideoSrc()
{
    GstElement *videoSrc = 0;
    if (m_videoInputFactory) {
        videoSrc = m_videoInputFactory->buildElement();
    } else {
        videoSrc = gst_element_factory_make("videotestsrc", "video_test_src");
        //videoSrc = gst_element_factory_make("v4l2src", "video_test_src");
    }

    return videoSrc;
}

GstElement *QGstreamerCaptureSession::buildVideoPreview()
{
    GstElement *previewElement = 0;

    if (m_viewfinderInterface) {
        GstElement *bin = gst_bin_new("video-preview-bin");
        GstElement *colorspace = gst_element_factory_make(QT_GSTREAMER_COLORCONVERSION_ELEMENT_NAME, "videoconvert-preview");
        GstElement *capsFilter = gst_element_factory_make("capsfilter", "capsfilter-video-preview");
        GstElement *preview = m_viewfinderInterface->videoSink();

        gst_bin_add_many(GST_BIN(bin), colorspace, capsFilter, preview,  NULL);
        gst_element_link(colorspace,capsFilter);
        gst_element_link(capsFilter,preview);

        QSize resolution;
        qreal frameRate = 0;

        if (m_captureMode & Video) {
            QVideoEncoderSettings videoSettings = m_videoEncodeControl->videoSettings();
            resolution = videoSettings.resolution();
            frameRate = videoSettings.frameRate();
        } else if (m_captureMode & Image) {
            resolution = m_imageEncodeControl->imageSettings().resolution();
        }

        GstCaps *caps = QGstUtils::videoFilterCaps();

        if (!resolution.isEmpty()) {
            gst_caps_set_simple(caps, "width", G_TYPE_INT, resolution.width(), NULL);
            gst_caps_set_simple(caps, "height", G_TYPE_INT, resolution.height(), NULL);
        }
        if (frameRate > 0.001) {
            QPair<int,int> rate = m_videoEncodeControl->rateAsRational();

            //qDebug() << "frame rate:" << num << denum;

            gst_caps_set_simple(caps, "framerate", GST_TYPE_FRACTION, rate.first, rate.second, NULL);
        }

        //qDebug() << "set video preview caps filter:" << gst_caps_to_string(caps);

        g_object_set(G_OBJECT(capsFilter), "caps", caps, NULL);

        gst_caps_unref(caps);

        // add ghostpads
        GstPad *pad = gst_element_get_static_pad(colorspace, "sink");
        Q_ASSERT(pad);
        gst_element_add_pad(GST_ELEMENT(bin), gst_ghost_pad_new("videosink", pad));
        gst_object_unref(GST_OBJECT(pad));

        previewElement = bin;
    } else {
#if 1
        previewElement = gst_element_factory_make("fakesink", "video-preview");
#else
        GstElement *bin = gst_bin_new("video-preview-bin");
        GstElement *colorspace = gst_element_factory_make(QT_GSTREAMER_COLORCONVERSION_ELEMENT_NAME, "videoconvert-preview");
        GstElement *preview = gst_element_factory_make("ximagesink", "video-preview");
        gst_bin_add_many(GST_BIN(bin), colorspace, preview,  NULL);
        gst_element_link(colorspace,preview);

        // add ghostpads
        GstPad *pad = gst_element_get_static_pad(colorspace, "sink");
        Q_ASSERT(pad);
        gst_element_add_pad(GST_ELEMENT(bin), gst_ghost_pad_new("videosink", pad));
        gst_object_unref(GST_OBJECT(pad));

        previewElement = bin;
#endif
    }

    return previewElement;
}

void QGstreamerCaptureSession::probeCaps(GstCaps *caps)
{
#if GST_CHECK_VERSION(1,0,0)
    gst_video_info_from_caps(&m_previewInfo, caps);
#else
    Q_UNUSED(caps);
#endif
}

bool QGstreamerCaptureSession::probeBuffer(GstBuffer *buffer)
{
    if (m_passPrerollImage) {
        m_passImage = false;
        m_passPrerollImage = false;

        return true;
    } else  if (!m_passImage) {
        return false;
    }

    m_passImage = false;

#if GST_CHECK_VERSION(1,0,0)
    QImage img = QGstUtils::bufferToImage(buffer, m_previewInfo);
#else
    QImage img = QGstUtils::bufferToImage(buffer);
#endif

    if (img.isNull())
        return true;

    static QMetaMethod exposedSignal = QMetaMethod::fromSignal(&QGstreamerCaptureSession::imageExposed);
    exposedSignal.invoke(this,
                         Qt::QueuedConnection,
                         Q_ARG(int,m_imageRequestId));

    static QMetaMethod capturedSignal = QMetaMethod::fromSignal(&QGstreamerCaptureSession::imageCaptured);
    capturedSignal.invoke(this,
                          Qt::QueuedConnection,
                          Q_ARG(int,m_imageRequestId),
                          Q_ARG(QImage,img));

    return true;
}

static gboolean saveImageFilter(GstElement *element,
                                GstBuffer *buffer,
                                GstPad *pad,
                                void *appdata)
{
    Q_UNUSED(element);
    Q_UNUSED(pad);
    QGstreamerCaptureSession *session = (QGstreamerCaptureSession *)appdata;

    QString fileName = session->m_imageFileName;

    if (!fileName.isEmpty()) {
        QFile f(fileName);
        if (f.open(QFile::WriteOnly)) {
#if GST_CHECK_VERSION(1,0,0)
            GstMapInfo info;
            if (gst_buffer_map(buffer, &info, GST_MAP_READ)) {
                f.write(reinterpret_cast<const char *>(info.data), info.size);
                gst_buffer_unmap(buffer, &info);
            }
#else
            f.write(reinterpret_cast<const char *>(buffer->data), buffer->size);
#endif
            f.close();

            static QMetaMethod savedSignal = QMetaMethod::fromSignal(&QGstreamerCaptureSession::imageSaved);
            savedSignal.invoke(session,
                               Qt::QueuedConnection,
                               Q_ARG(int,session->m_imageRequestId),
                               Q_ARG(QString,fileName));
        }
    }

    return TRUE;
}

GstElement *QGstreamerCaptureSession::buildImageCapture()
{
    GstElement *bin = gst_bin_new("image-capture-bin");
    GstElement *queue = gst_element_factory_make("queue", "queue-image-capture");
    GstElement *colorspace = gst_element_factory_make(QT_GSTREAMER_COLORCONVERSION_ELEMENT_NAME, "videoconvert-image-capture");
    GstElement *encoder = gst_element_factory_make("jpegenc", "image-encoder");
    GstElement *sink = gst_element_factory_make("fakesink","sink-image-capture");

    GstPad *pad = gst_element_get_static_pad(queue, "src");
    Q_ASSERT(pad);

    addProbeToPad(pad, false);

    gst_object_unref(GST_OBJECT(pad));

    g_object_set(G_OBJECT(sink), "signal-handoffs", TRUE, NULL);
    g_signal_connect(G_OBJECT(sink), "handoff", G_CALLBACK(saveImageFilter), this);

    gst_bin_add_many(GST_BIN(bin), queue, colorspace, encoder, sink,  NULL);
    gst_element_link_many(queue, colorspace, encoder, sink, NULL);

    // add ghostpads
    pad = gst_element_get_static_pad(queue, "sink");
    Q_ASSERT(pad);
    gst_element_add_pad(GST_ELEMENT(bin), gst_ghost_pad_new("imagesink", pad));
    gst_object_unref(GST_OBJECT(pad));

    m_passImage = false;
    m_passPrerollImage = true;
    m_imageFileName = QString();

    return bin;
}

void QGstreamerCaptureSession::captureImage(int requestId, const QString &fileName)
{
    m_imageRequestId = requestId;
    m_imageFileName = fileName;
    m_passImage = true;
}


#define REMOVE_ELEMENT(element) { if (element) {gst_bin_remove(GST_BIN(m_pipeline), element); element = 0;} }
#define UNREF_ELEMENT(element) { if (element) { gst_object_unref(GST_OBJECT(element)); element = 0; } }

bool QGstreamerCaptureSession::rebuildGraph(QGstreamerCaptureSession::PipelineMode newMode)
{
    removeAudioBufferProbe();
    REMOVE_ELEMENT(m_audioSrc);
    REMOVE_ELEMENT(m_audioPreview);
    REMOVE_ELEMENT(m_audioPreviewQueue);
    REMOVE_ELEMENT(m_audioTee);
    REMOVE_ELEMENT(m_videoSrc);
    REMOVE_ELEMENT(m_videoPreview);
    REMOVE_ELEMENT(m_videoPreviewQueue);
    REMOVE_ELEMENT(m_videoTee);
    REMOVE_ELEMENT(m_encodeBin);
    REMOVE_ELEMENT(m_imageCaptureBin);
    m_audioVolume = 0;

    bool ok = true;

    switch (newMode) {
        case EmptyPipeline:
            break;
        case PreviewPipeline:
            if (m_captureMode & Audio) {
                m_audioSrc = buildAudioSrc();
                m_audioPreview = buildAudioPreview();

                ok &= m_audioSrc && m_audioPreview;

                if (ok) {
                    gst_bin_add_many(GST_BIN(m_pipeline), m_audioSrc, m_audioPreview, NULL);
                    ok &= gst_element_link(m_audioSrc, m_audioPreview);
                } else {
                    UNREF_ELEMENT(m_audioSrc);
                    UNREF_ELEMENT(m_audioPreview);
                }
            }
            if (m_captureMode & Video || m_captureMode & Image) {
                m_videoSrc = buildVideoSrc();
                m_videoTee = gst_element_factory_make("tee", "video-preview-tee");
                m_videoPreviewQueue = gst_element_factory_make("queue", "video-preview-queue");
                m_videoPreview = buildVideoPreview();
                m_imageCaptureBin = buildImageCapture();

                ok &= m_videoSrc && m_videoTee && m_videoPreviewQueue && m_videoPreview && m_imageCaptureBin;

                if (ok) {
                    gst_bin_add_many(GST_BIN(m_pipeline), m_videoSrc, m_videoTee,
                                     m_videoPreviewQueue, m_videoPreview,
                                     m_imageCaptureBin, NULL);

                    ok &= gst_element_link(m_videoSrc, m_videoTee);
                    ok &= gst_element_link(m_videoTee, m_videoPreviewQueue);
                    ok &= gst_element_link(m_videoPreviewQueue, m_videoPreview);
                    ok &= gst_element_link(m_videoTee, m_imageCaptureBin);
                } else {
                    UNREF_ELEMENT(m_videoSrc);
                    UNREF_ELEMENT(m_videoTee);
                    UNREF_ELEMENT(m_videoPreviewQueue);
                    UNREF_ELEMENT(m_videoPreview);
                    UNREF_ELEMENT(m_imageCaptureBin);
                }
            }
            break;
        case RecordingPipeline:
            m_encodeBin = buildEncodeBin();
            gst_bin_add(GST_BIN(m_pipeline), m_encodeBin);

            if (m_captureMode & Audio) {
                m_audioSrc = buildAudioSrc();
                ok &= m_audioSrc != 0;

                gst_bin_add(GST_BIN(m_pipeline), m_audioSrc);
                ok &= gst_element_link(m_audioSrc, m_encodeBin);
            }

            if (m_captureMode & Video) {
                m_videoSrc = buildVideoSrc();
                ok &= m_videoSrc != 0;

                gst_bin_add(GST_BIN(m_pipeline), m_videoSrc);
                ok &= gst_element_link(m_videoSrc, m_encodeBin);
            }

            if (!m_metaData.isEmpty())
                setMetaData(m_metaData);

            break;
        case PreviewAndRecordingPipeline:
            m_encodeBin = buildEncodeBin();
            if (m_encodeBin)
                gst_bin_add(GST_BIN(m_pipeline), m_encodeBin);

            ok &= m_encodeBin != 0;

            if (ok && m_captureMode & Audio) {
                m_audioSrc = buildAudioSrc();
                m_audioPreview = buildAudioPreview();
                m_audioTee = gst_element_factory_make("tee", NULL);
                m_audioPreviewQueue = gst_element_factory_make("queue", NULL);

                ok &= m_audioSrc && m_audioPreview && m_audioTee && m_audioPreviewQueue;

                if (ok) {
                    gst_bin_add_many(GST_BIN(m_pipeline), m_audioSrc, m_audioTee,
                                     m_audioPreviewQueue, m_audioPreview, NULL);
                    ok &= gst_element_link(m_audioSrc, m_audioTee);
                    ok &= gst_element_link(m_audioTee, m_audioPreviewQueue);
                    ok &= gst_element_link(m_audioPreviewQueue, m_audioPreview);
                    ok &= gst_element_link(m_audioTee, m_encodeBin);
                } else {
                    UNREF_ELEMENT(m_audioSrc);
                    UNREF_ELEMENT(m_audioPreview);
                    UNREF_ELEMENT(m_audioTee);
                    UNREF_ELEMENT(m_audioPreviewQueue);
                }
            }

            if (ok && (m_captureMode & Video || m_captureMode & Image)) {
                m_videoSrc = buildVideoSrc();
                m_videoPreview = buildVideoPreview();
                m_videoTee = gst_element_factory_make("tee", NULL);
                m_videoPreviewQueue = gst_element_factory_make("queue", NULL);

                ok &= m_videoSrc && m_videoPreview && m_videoTee && m_videoPreviewQueue;

                if (ok) {
                    gst_bin_add_many(GST_BIN(m_pipeline), m_videoSrc, m_videoTee,
                                 m_videoPreviewQueue, m_videoPreview, NULL);
                    ok &= gst_element_link(m_videoSrc, m_videoTee);
                    ok &= gst_element_link(m_videoTee, m_videoPreviewQueue);
                    ok &= gst_element_link(m_videoPreviewQueue, m_videoPreview);
                } else {
                    UNREF_ELEMENT(m_videoSrc);
                    UNREF_ELEMENT(m_videoTee);
                    UNREF_ELEMENT(m_videoPreviewQueue);
                    UNREF_ELEMENT(m_videoPreview);
                }

                if (ok && (m_captureMode & Video))
                    ok &= gst_element_link(m_videoTee, m_encodeBin);
            }

            if (!m_metaData.isEmpty())
                setMetaData(m_metaData);


            break;
    }

    if (!ok) {
        emit error(int(QMediaRecorder::FormatError),tr("Failed to build media capture pipeline."));
    }

    dumpGraph( QString("rebuild_graph_%1_%2").arg(m_pipelineMode).arg(newMode) );
#ifdef QT_GST_CAPTURE_DEBUG
    if (m_encodeBin) {
        QString fileName = QString("rebuild_graph_encode_%1_%2").arg(m_pipelineMode).arg(newMode);
        GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(m_encodeBin), GST_DEBUG_GRAPH_SHOW_ALL, fileName.toLatin1());
    }
#endif

    if (ok) {
        addAudioBufferProbe();
        m_pipelineMode = newMode;
    } else {
        m_pipelineMode = EmptyPipeline;

        REMOVE_ELEMENT(m_audioSrc);
        REMOVE_ELEMENT(m_audioPreview);
        REMOVE_ELEMENT(m_audioPreviewQueue);
        REMOVE_ELEMENT(m_audioTee);
        REMOVE_ELEMENT(m_videoSrc);
        REMOVE_ELEMENT(m_videoPreview);
        REMOVE_ELEMENT(m_videoPreviewQueue);
        REMOVE_ELEMENT(m_videoTee);
        REMOVE_ELEMENT(m_encodeBin);
    }

    return ok;
}

void QGstreamerCaptureSession::dumpGraph(const QString &fileName)
{
#ifdef QT_GST_CAPTURE_DEBUG
    GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(m_pipeline),
                              GstDebugGraphDetails(/*GST_DEBUG_GRAPH_SHOW_ALL |*/ GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE | GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS | GST_DEBUG_GRAPH_SHOW_STATES),
                              fileName.toLatin1());
#else
    Q_UNUSED(fileName)
#endif
}

QUrl QGstreamerCaptureSession::outputLocation() const
{
    return m_sink;
}

bool QGstreamerCaptureSession::setOutputLocation(const QUrl& sink)
{
    if (!sink.isRelative() && !sink.isLocalFile()) {
        qWarning("Output location must be a local file");
        return false;
    }

    m_sink = sink;
    return true;
}

void QGstreamerCaptureSession::setAudioInput(QGstreamerElementFactory *audioInput)
{
    m_audioInputFactory = audioInput;
}

void QGstreamerCaptureSession::setAudioPreview(QGstreamerElementFactory *audioPreview)
{
    m_audioPreviewFactory = audioPreview;
}

void QGstreamerCaptureSession::setVideoInput(QGstreamerVideoInput *videoInput)
{
    m_videoInputFactory = videoInput;
}

void QGstreamerCaptureSession::setVideoPreview(QObject *viewfinder)
{
    m_viewfinderInterface = qobject_cast<QGstreamerVideoRendererInterface*>(viewfinder);
    if (!m_viewfinderInterface)
        viewfinder = 0;

    if (m_viewfinder != viewfinder) {
        bool oldReady = isReady();

        if (m_viewfinder) {
            disconnect(m_viewfinder, SIGNAL(sinkChanged()),
                       this, SIGNAL(viewfinderChanged()));
            disconnect(m_viewfinder, SIGNAL(readyChanged(bool)),
                       this, SIGNAL(readyChanged(bool)));

            m_busHelper->removeMessageFilter(m_viewfinder);
        }

        m_viewfinder = viewfinder;
        //m_viewfinderHasChanged = true;

        if (m_viewfinder) {
            connect(m_viewfinder, SIGNAL(sinkChanged()),
                       this, SIGNAL(viewfinderChanged()));
            connect(m_viewfinder, SIGNAL(readyChanged(bool)),
                    this, SIGNAL(readyChanged(bool)));

            m_busHelper->installMessageFilter(m_viewfinder);
        }

        emit viewfinderChanged();
        if (oldReady != isReady())
            emit readyChanged(isReady());
    }
}

bool QGstreamerCaptureSession::isReady() const
{
    //it's possible to use QCamera without any viewfinder attached
    return !m_viewfinderInterface || m_viewfinderInterface->isReady();
}

QGstreamerCaptureSession::State QGstreamerCaptureSession::state() const
{
    return m_state;
}

QGstreamerCaptureSession::State QGstreamerCaptureSession::pendingState() const
{
    return m_pendingState;
}

void QGstreamerCaptureSession::setState(QGstreamerCaptureSession::State newState)
{
    if (newState == m_pendingState && !m_waitingForEos)
        return;

    m_pendingState = newState;

    PipelineMode newMode = EmptyPipeline;

    switch (newState) {
        case PausedState:
        case RecordingState:
            newMode = PreviewAndRecordingPipeline;
            break;
        case PreviewState:
            newMode = PreviewPipeline;
            break;
        case StoppedState:
            newMode = EmptyPipeline;
            break;
    }

    if (newMode != m_pipelineMode) {
        if (m_pipelineMode == PreviewAndRecordingPipeline) {
            if (!m_waitingForEos) {
                m_waitingForEos = true;
                //qDebug() << "Waiting for EOS";
                // Unless gstreamer is in GST_STATE_PLAYING our EOS message will not be received.
                gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
                //with live sources it's necessary to send EOS even to pipeline
                //before going to STOPPED state
                gst_element_send_event(m_pipeline, gst_event_new_eos());

                return;
            } else {
                m_waitingForEos = false;
                //qDebug() << "EOS received";
            }
        }

        //select suitable default codecs/containers, if necessary
        m_recorderControl->applySettings();

        gst_element_set_state(m_pipeline, GST_STATE_NULL);

        if (!rebuildGraph(newMode)) {
            m_pendingState = StoppedState;
            m_state = StoppedState;
            emit stateChanged(StoppedState);

            return;
        }
    }

    switch (newState) {
        case PausedState:
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
            break;
        case RecordingState:
        case PreviewState:
            gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
            break;
        case StoppedState:
            gst_element_set_state(m_pipeline, GST_STATE_NULL);
    }

    //we have to do it here, since gstreamer will not emit bus messages any more
    if (newState == StoppedState) {
        m_state = StoppedState;
        emit stateChanged(StoppedState);
    }
}


qint64 QGstreamerCaptureSession::duration() const
{
    gint64 duration = 0;
    if (m_encodeBin && qt_gst_element_query_position(m_encodeBin, GST_FORMAT_TIME, &duration))
        return duration / 1000000;
    else
        return 0;
}

void QGstreamerCaptureSession::setCaptureDevice(const QString &deviceName)
{
    m_captureDevice = deviceName;
}

void QGstreamerCaptureSession::setMetaData(const QMap<QByteArray, QVariant> &data)
{
    //qDebug() << "QGstreamerCaptureSession::setMetaData" << data;
    m_metaData = data;

    if (m_encodeBin)
        QGstUtils::setMetaData(GST_BIN(m_encodeBin), data);
}

bool QGstreamerCaptureSession::processBusMessage(const QGstreamerMessage &message)
{
    GstMessage* gm = message.rawMessage();

    if (gm) {
        if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_ERROR) {
            GError *err;
            gchar *debug;
            gst_message_parse_error (gm, &err, &debug);
            emit error(int(QMediaRecorder::ResourceError),QString::fromUtf8(err->message));
            g_error_free (err);
            g_free (debug);
        }

        if (GST_MESSAGE_SRC(gm) == GST_OBJECT_CAST(m_pipeline)) {
            switch (GST_MESSAGE_TYPE(gm))  {
            case GST_MESSAGE_DURATION:
                break;

            case GST_MESSAGE_EOS:
                if (m_waitingForEos)
                    setState(m_pendingState);
                break;

            case GST_MESSAGE_STATE_CHANGED:
                {

                    GstState    oldState;
                    GstState    newState;
                    GstState    pending;

                    gst_message_parse_state_changed(gm, &oldState, &newState, &pending);

                    QStringList states;
                    states << "GST_STATE_VOID_PENDING" <<  "GST_STATE_NULL" << "GST_STATE_READY" << "GST_STATE_PAUSED" << "GST_STATE_PLAYING";

                    /*
                    qDebug() << QString("state changed: old: %1  new: %2  pending: %3") \
                            .arg(states[oldState]) \
                           .arg(states[newState]) \
                            .arg(states[pending]);

                    #define ENUM_NAME(c,e,v) (c::staticMetaObject.enumerator(c::staticMetaObject.indexOfEnumerator(e)).valueToKey((v)))

                    qDebug() << "Current session state:" << ENUM_NAME(QGstreamerCaptureSession,"State",m_state);
                    qDebug() << "Pending session state:" << ENUM_NAME(QGstreamerCaptureSession,"State",m_pendingState);
                    */

                    switch (newState) {
                    case GST_STATE_VOID_PENDING:
                    case GST_STATE_NULL:
                    case GST_STATE_READY:
                        if (m_state != StoppedState && m_pendingState == StoppedState) {
                            emit stateChanged(m_state = StoppedState);
                            dumpGraph("stopped");
                        }
                        break;
                    case GST_STATE_PAUSED:
                        if (m_state != PausedState && m_pendingState == PausedState)
                            emit stateChanged(m_state = PausedState);
                        dumpGraph("paused");

                        if (m_pipelineMode == RecordingPipeline && !m_metaData.isEmpty())
                            setMetaData(m_metaData);
                        break;
                    case GST_STATE_PLAYING:
                        {
                            if ((m_pendingState == PreviewState || m_pendingState == RecordingState) &&
                                m_state != m_pendingState)
                            {
                                m_state = m_pendingState;
                                emit stateChanged(m_state);
                            }

                            if (m_pipelineMode == PreviewPipeline)
                                dumpGraph("preview");
                            else
                                dumpGraph("recording");
                        }
                        break;
                    }
                }
                break;
            default:
                break;
            }
            //qDebug() << "New session state:" << ENUM_NAME(QGstreamerCaptureSession,"State",m_state);
        }
    }
    return false;
}

void QGstreamerCaptureSession::setMuted(bool muted)
{
    if (bool(m_muted) != muted) {
        m_muted = muted;
        if (m_audioVolume)
            g_object_set(G_OBJECT(m_audioVolume), "mute", m_muted, NULL);

        emit mutedChanged(muted);
    }
}

void QGstreamerCaptureSession::setVolume(qreal volume)
{
    if (!qFuzzyCompare(double(volume), m_volume)) {
        m_volume = volume;
        if (m_audioVolume)
            g_object_set(G_OBJECT(m_audioVolume), "volume", m_volume, NULL);

        emit volumeChanged(volume);
    }
}

void QGstreamerCaptureSession::addProbe(QGstreamerAudioProbeControl* probe)
{
    Q_ASSERT(!m_audioProbe);
    m_audioProbe = probe;
    addAudioBufferProbe();
}

void QGstreamerCaptureSession::removeProbe(QGstreamerAudioProbeControl* probe)
{
    Q_ASSERT(m_audioProbe == probe);
    removeAudioBufferProbe();
    m_audioProbe = 0;
}

GstPad *QGstreamerCaptureSession::getAudioProbePad()
{
    // first see if preview element is available
    if (m_audioPreview) {
        GstPad *pad = gst_element_get_static_pad(m_audioPreview, "sink");
        if (pad)
            return pad;
    }

    // preview element is not available,
    // try to use sink pin of audio encoder.
    if (m_encodeBin) {
        GstElement *audioEncoder = gst_bin_get_by_name(GST_BIN(m_encodeBin), "audio-encoder-bin");
        if (audioEncoder) {
            GstPad *pad = gst_element_get_static_pad(audioEncoder, "sink");
            gst_object_unref(audioEncoder);
            if (pad)
                return pad;
        }
    }

    return 0;
}

void QGstreamerCaptureSession::removeAudioBufferProbe()
{
    if (!m_audioProbe)
        return;

    GstPad *pad = getAudioProbePad();
    if (pad) {
        m_audioProbe->removeProbeFromPad(pad);
        gst_object_unref(GST_OBJECT(pad));
    }
}

void QGstreamerCaptureSession::addAudioBufferProbe()
{
    if (!m_audioProbe)
        return;

    GstPad *pad = getAudioProbePad();
    if (pad) {
        m_audioProbe->addProbeToPad(pad);
        gst_object_unref(GST_OBJECT(pad));
    }
}

QT_END_NAMESPACE
