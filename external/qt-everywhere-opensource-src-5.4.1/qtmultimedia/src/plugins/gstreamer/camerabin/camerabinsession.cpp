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
#include "camerabinsession.h"
#include "camerabincontrol.h"
#include "camerabinrecorder.h"
#include "camerabincontainer.h"
#include "camerabinaudioencoder.h"
#include "camerabinvideoencoder.h"
#include "camerabinimageencoder.h"

#ifdef HAVE_GST_PHOTOGRAPHY
#include "camerabinexposure.h"
#include "camerabinflash.h"
#include "camerabinfocus.h"
#include "camerabinlocks.h"
#include "camerabinzoom.h"
#endif

#include "camerabinimageprocessing.h"
#include "camerabinviewfindersettings.h"

#include "camerabincapturedestination.h"
#include "camerabincapturebufferformat.h"
#include <private/qgstreamerbushelper_p.h>
#include <private/qgstreamervideorendererinterface_p.h>
#include <private/qgstutils_p.h>
#include <qmediarecorder.h>

#ifdef HAVE_GST_PHOTOGRAPHY
#include <gst/interfaces/photography.h>
#endif

#include <gst/gsttagsetter.h>
#include <gst/gstversion.h>

#include <QtCore/qdebug.h>
#include <QCoreApplication>
#include <QtCore/qmetaobject.h>
#include <QtGui/qdesktopservices.h>

#include <QtGui/qimage.h>
#include <QtCore/qdatetime.h>

//#define CAMERABIN_DEBUG 1
//#define CAMERABIN_DEBUG_DUMP_BIN 1
#define ENUM_NAME(c,e,v) (c::staticMetaObject.enumerator(c::staticMetaObject.indexOfEnumerator(e)).valueToKey((v)))

#define FILENAME_PROPERTY "location"
#define MODE_PROPERTY "mode"
#define MUTE_PROPERTY "mute"
#define IMAGE_PP_PROPERTY "image-post-processing"
#define IMAGE_ENCODER_PROPERTY "image-encoder"
#define VIDEO_PP_PROPERTY "video-post-processing"
#define VIEWFINDER_SINK_PROPERTY "viewfinder-sink"
#define CAMERA_SOURCE_PROPERTY "camera-source"
#define AUDIO_SOURCE_PROPERTY "audio-source"
#define SUPPORTED_IMAGE_CAPTURE_CAPS_PROPERTY "image-capture-supported-caps"
#define SUPPORTED_VIDEO_CAPTURE_CAPS_PROPERTY "video-capture-supported-caps"
#define SUPPORTED_VIEWFINDER_CAPS_PROPERTY "viewfinder-supported-caps"
#define AUDIO_CAPTURE_CAPS_PROPERTY "audio-capture-caps"
#define IMAGE_CAPTURE_CAPS_PROPERTY "image-capture-caps"
#define VIDEO_CAPTURE_CAPS_PROPERTY "video-capture-caps"
#define VIEWFINDER_CAPS_PROPERTY "viewfinder-caps"
#define PREVIEW_CAPS_PROPERTY "preview-caps"
#define POST_PREVIEWS_PROPERTY "post-previews"


#define CAPTURE_START "start-capture"
#define CAPTURE_STOP "stop-capture"

#define FILESINK_BIN_NAME "videobin-filesink"

#define CAMERABIN_IMAGE_MODE 1
#define CAMERABIN_VIDEO_MODE 2

#define PREVIEW_CAPS_4_3 \
    "video/x-raw-rgb, width = (int) 640, height = (int) 480"

//using GST_STATE_READY for QCamera::LoadedState
//may not work reliably at least with some webcams.

//#define USE_READY_STATE_ON_LOADED

QT_BEGIN_NAMESPACE

CameraBinSession::CameraBinSession(GstElementFactory *sourceFactory, QObject *parent)
    :QObject(parent),
     m_recordingActive(false),
     m_state(QCamera::UnloadedState),
     m_pendingState(QCamera::UnloadedState),
     m_muted(false),
     m_busy(false),
     m_captureMode(QCamera::CaptureStillImage),
     m_audioInputFactory(0),
     m_videoInputFactory(0),
     m_viewfinder(0),
     m_viewfinderInterface(0),
     m_videoSrc(0),
     m_viewfinderElement(0),
     m_sourceFactory(sourceFactory),
     m_viewfinderHasChanged(true),
     m_videoInputHasChanged(true),
     m_audioSrc(0),
     m_audioConvert(0),
     m_capsFilter(0),
     m_fileSink(0),
     m_audioEncoder(0),
     m_videoEncoder(0),
     m_muxer(0)
{
    if (m_sourceFactory)
        gst_object_ref(GST_OBJECT(m_sourceFactory));

    m_camerabin = gst_element_factory_make("camerabin2", "camerabin2");
    g_signal_connect(G_OBJECT(m_camerabin), "notify::idle", G_CALLBACK(updateBusyStatus), this);
    g_signal_connect(G_OBJECT(m_camerabin), "element-added",  G_CALLBACK(elementAdded), this);
    g_signal_connect(G_OBJECT(m_camerabin), "element-removed",  G_CALLBACK(elementRemoved), this);
    qt_gst_object_ref_sink(m_camerabin);

    m_bus = gst_element_get_bus(m_camerabin);

    m_busHelper = new QGstreamerBusHelper(m_bus, this);
    m_busHelper->installMessageFilter(this);

    m_cameraControl = new CameraBinControl(this);
    m_audioEncodeControl = new CameraBinAudioEncoder(this);
    m_videoEncodeControl = new CameraBinVideoEncoder(this);
    m_imageEncodeControl = new CameraBinImageEncoder(this);
    m_recorderControl = new CameraBinRecorder(this);
    m_mediaContainerControl = new CameraBinContainer(this);

#ifdef HAVE_GST_PHOTOGRAPHY
    m_cameraExposureControl = new CameraBinExposure(this);
    m_cameraFlashControl = new CameraBinFlash(this);
    m_cameraFocusControl = new CameraBinFocus(this);
    m_cameraLocksControl = new CameraBinLocks(this);
    m_cameraZoomControl = new CameraBinZoom(this);
#endif
    m_imageProcessingControl = new CameraBinImageProcessing(this);
    m_captureDestinationControl = new CameraBinCaptureDestination(this);
    m_captureBufferFormatControl = new CameraBinCaptureBufferFormat(this);
    m_viewfinderSettingsControl = new CameraBinViewfinderSettings(this);

    QByteArray envFlags = qgetenv("QT_GSTREAMER_CAMERABIN_FLAGS");
    if (!envFlags.isEmpty())
        g_object_set(G_OBJECT(m_camerabin), "flags", envFlags.toInt(), NULL);

    //post image preview in RGB format
    g_object_set(G_OBJECT(m_camerabin), POST_PREVIEWS_PROPERTY, TRUE, NULL);

    GstCaps *previewCaps = gst_caps_from_string("video/x-raw-rgb");
    g_object_set(G_OBJECT(m_camerabin), PREVIEW_CAPS_PROPERTY, previewCaps, NULL);
    gst_caps_unref(previewCaps);
}

CameraBinSession::~CameraBinSession()
{
    if (m_camerabin) {
        if (m_viewfinderInterface)
            m_viewfinderInterface->stopRenderer();

        gst_element_set_state(m_camerabin, GST_STATE_NULL);
        gst_element_get_state(m_camerabin, NULL, NULL, GST_CLOCK_TIME_NONE);
        gst_object_unref(GST_OBJECT(m_bus));
        gst_object_unref(GST_OBJECT(m_camerabin));
    }
    if (m_viewfinderElement)
        gst_object_unref(GST_OBJECT(m_viewfinderElement));

    if (m_sourceFactory)
        gst_object_unref(GST_OBJECT(m_sourceFactory));
}

#ifdef HAVE_GST_PHOTOGRAPHY
GstPhotography *CameraBinSession::photography()
{
    if (GST_IS_PHOTOGRAPHY(m_camerabin)) {
        return GST_PHOTOGRAPHY(m_camerabin);
    }

    GstElement * const source = buildCameraSource();

    if (source && GST_IS_PHOTOGRAPHY(source))
        return GST_PHOTOGRAPHY(source);

    return 0;
}
#endif

CameraBinSession::CameraRole CameraBinSession::cameraRole() const
{
    return BackCamera;
}

/*
  Configure camera during Loaded->Active states stansition.
*/
bool CameraBinSession::setupCameraBin()
{
    if (!buildCameraSource())
        return false;

    if (m_viewfinderHasChanged) {
        if (m_viewfinderElement)
            gst_object_unref(GST_OBJECT(m_viewfinderElement));

        m_viewfinderElement = m_viewfinderInterface ? m_viewfinderInterface->videoSink() : 0;
#if CAMERABIN_DEBUG
        qDebug() << Q_FUNC_INFO << "Viewfinder changed, reconfigure.";
#endif
        m_viewfinderHasChanged = false;
        if (!m_viewfinderElement) {
            qWarning() << "Staring camera without viewfinder available";
            m_viewfinderElement = gst_element_factory_make("fakesink", NULL);
        }
        qt_gst_object_ref_sink(GST_OBJECT(m_viewfinderElement));
        gst_element_set_state(m_camerabin, GST_STATE_NULL);
        g_object_set(G_OBJECT(m_camerabin), VIEWFINDER_SINK_PROPERTY, m_viewfinderElement, NULL);
    }

    return true;
}

static GstCaps *resolutionToCaps(const QSize &resolution, const QPair<int, int> &rate = qMakePair<int,int>(0,0))
{
    if (resolution.isEmpty())
        return gst_caps_new_any();

    GstCaps *caps = 0;
    if (rate.second > 0) {
        caps = gst_caps_new_full(gst_structure_new("video/x-raw-yuv",
                                                   "width", G_TYPE_INT, resolution.width(),
                                                   "height", G_TYPE_INT, resolution.height(),
                                                   "framerate", GST_TYPE_FRACTION, rate.first, rate.second,
                                                   NULL),
                                 gst_structure_new("video/x-raw-rgb",
                                                   "width", G_TYPE_INT, resolution.width(),
                                                   "height", G_TYPE_INT, resolution.height(),
                                                   "framerate", GST_TYPE_FRACTION, rate.first, rate.second,
                                                   NULL),
                                 gst_structure_new("video/x-raw-data",
                                                   "width", G_TYPE_INT, resolution.width(),
                                                   "height", G_TYPE_INT, resolution.height(),
                                                   "framerate", GST_TYPE_FRACTION, rate.first, rate.second,
                                                   NULL),
                                gst_structure_new("video/x-android-buffer",
                                                   "width", G_TYPE_INT, resolution.width(),
                                                                                    "height", G_TYPE_INT, resolution.height(),
                                                                                    "framerate", GST_TYPE_FRACTION, rate.first, rate.second,
                                                                                    NULL),
                                 gst_structure_new("image/jpeg",
                                                   "width", G_TYPE_INT, resolution.width(),
                                                   "height", G_TYPE_INT, resolution.height(),
                                                   "framerate", GST_TYPE_FRACTION, rate.first, rate.second,
                                                   NULL),
                                 NULL);
    } else {
        caps = gst_caps_new_full (gst_structure_new ("video/x-raw-yuv",
                                                     "width", G_TYPE_INT, resolution.width(),
                                                     "height", G_TYPE_INT, resolution.height(),
                                                     NULL),
                                  gst_structure_new ("video/x-raw-rgb",
                                                     "width", G_TYPE_INT, resolution.width(),
                                                     "height", G_TYPE_INT, resolution.height(),
                                                     NULL),
                                  gst_structure_new("video/x-raw-data",
                                                    "width", G_TYPE_INT, resolution.width(),
                                                    "height", G_TYPE_INT, resolution.height(),
                                                    NULL),
                                  gst_structure_new ("video/x-android-buffer",
                                                     "width", G_TYPE_INT, resolution.width(),
                                                     "height", G_TYPE_INT, resolution.height(),
                                                     NULL),
                                  gst_structure_new ("image/jpeg",
                                                     "width", G_TYPE_INT, resolution.width(),
                                                     "height", G_TYPE_INT, resolution.height(),
                                                     NULL),
                                  NULL);
    }

    return caps;
}

void CameraBinSession::setupCaptureResolution()
{
    QSize resolution = m_imageEncodeControl->imageSettings().resolution();
    if (!resolution.isEmpty()) {
        GstCaps *caps = resolutionToCaps(resolution);
#if CAMERABIN_DEBUG
        qDebug() << Q_FUNC_INFO << "set image resolution" << resolution << gst_caps_to_string(caps);
#endif
        g_object_set(m_camerabin, IMAGE_CAPTURE_CAPS_PROPERTY, caps, NULL);
        gst_caps_unref(caps);
    } else {
        g_object_set(m_camerabin, IMAGE_CAPTURE_CAPS_PROPERTY, NULL, NULL);
    }

    resolution = m_videoEncodeControl->actualVideoSettings().resolution();
    //qreal framerate = m_videoEncodeControl->videoSettings().frameRate();
    if (!resolution.isEmpty()) {
        GstCaps *caps = resolutionToCaps(resolution /*, framerate*/); //convert to rational
#if CAMERABIN_DEBUG
        qDebug() << Q_FUNC_INFO << "set video resolution" << resolution << gst_caps_to_string(caps);
#endif
        g_object_set(m_camerabin, VIDEO_CAPTURE_CAPS_PROPERTY, caps, NULL);
        gst_caps_unref(caps);
    } else {
        g_object_set(m_camerabin, VIDEO_CAPTURE_CAPS_PROPERTY, NULL, NULL);
    }

    GstElement *mfw_v4lsrc = 0;
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSrc), "video-source")) {
        GstElement *videoSrc = 0;
        g_object_get(G_OBJECT(m_videoSrc), "video-source", &videoSrc, NULL);
        if (videoSrc) {
            const char *name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(gst_element_get_factory(videoSrc)));
            if (!qstrcmp(name, "mfw_v4lsrc"))
                mfw_v4lsrc = videoSrc;
        }
    }

    resolution = m_viewfinderSettingsControl->resolution();
    if (!resolution.isEmpty()) {
        GstCaps *caps = resolutionToCaps(resolution);
#if CAMERABIN_DEBUG
        qDebug() << Q_FUNC_INFO << "set viewfinder resolution" << resolution << gst_caps_to_string(caps);
#endif
        g_object_set(m_camerabin, VIEWFINDER_CAPS_PROPERTY, caps, NULL);
        gst_caps_unref(caps);

        if (mfw_v4lsrc) {
            int capMode = 0;
            if (resolution == QSize(320, 240))
                capMode = 1;
            else if (resolution == QSize(720, 480))
                capMode = 2;
            else if (resolution == QSize(720, 576))
                capMode = 3;
            else if (resolution == QSize(1280, 720))
                capMode = 4;
            else if (resolution == QSize(1920, 1080))
                capMode = 5;
            g_object_set(G_OBJECT(mfw_v4lsrc), "capture-mode", capMode, NULL);
        }
    } else {
        g_object_set(m_camerabin, VIEWFINDER_CAPS_PROPERTY, NULL, NULL);
    }

    const qreal maxFps = m_viewfinderSettingsControl->maximumFrameRate();
    if (!qFuzzyIsNull(maxFps)) {
        if (mfw_v4lsrc) {
            int n, d;
            gst_util_double_to_fraction(maxFps, &n, &d);
            g_object_set(G_OBJECT(mfw_v4lsrc), "fps-n", n, NULL);
            g_object_set(G_OBJECT(mfw_v4lsrc), "fps-d", d, NULL);
        }
    }

    if (m_videoEncoder)
        m_videoEncodeControl->applySettings(m_videoEncoder);
}

void CameraBinSession::setAudioCaptureCaps()
{
    QAudioEncoderSettings settings = m_audioEncodeControl->audioSettings();
    const int sampleRate = settings.sampleRate();
    const int channelCount = settings.channelCount();

    if (sampleRate == -1 && channelCount == -1)
        return;

    GstStructure *structure = gst_structure_new(
                "audio/x-raw-int",
                "endianness", G_TYPE_INT, 1234,
                "signed", G_TYPE_BOOLEAN, TRUE,
                "width", G_TYPE_INT, 16,
                "depth", G_TYPE_INT, 16,
                NULL);
    if (sampleRate != -1)
        gst_structure_set(structure, "rate", G_TYPE_INT, sampleRate, NULL);
    if (channelCount != -1)
        gst_structure_set(structure, "channels", G_TYPE_INT, channelCount, NULL);

    GstCaps *caps = gst_caps_new_full(structure, NULL);
    g_object_set(G_OBJECT(m_camerabin), AUDIO_CAPTURE_CAPS_PROPERTY, caps, NULL);
    gst_caps_unref(caps);

    if (m_audioEncoder)
        m_audioEncodeControl->applySettings(m_audioEncoder);
}

GstElement *CameraBinSession::buildCameraSource()
{
#if CAMERABIN_DEBUG
    qDebug() << Q_FUNC_INFO;
#endif
    if (!m_videoInputHasChanged)
        return m_videoSrc;
    m_videoInputHasChanged = false;

    GstElement *videoSrc = 0;

    if (!videoSrc)
    g_object_get(G_OBJECT(m_camerabin), CAMERA_SOURCE_PROPERTY, &videoSrc, NULL);

    if (m_sourceFactory)
        m_videoSrc = gst_element_factory_create(m_sourceFactory, "camera_source");

    // If gstreamer has set a default source use it.
    if (!m_videoSrc)
        m_videoSrc = videoSrc;

    if (m_videoSrc && !m_inputDevice.isEmpty()) {
#if CAMERABIN_DEBUG
        qDebug() << "set camera device" << m_inputDevice;
#endif

        if (g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSrc), "video-source")) {
            GstElement *src = 0;

            /* QT_GSTREAMER_CAMERABIN_VIDEOSRC can be used to set the video source element.

               --- Usage

                 QT_GSTREAMER_CAMERABIN_VIDEOSRC=[drivername=elementname[,drivername2=elementname2 ...],][elementname]

               --- Examples

                 Always use 'somevideosrc':
                 QT_GSTREAMER_CAMERABIN_VIDEOSRC="somevideosrc"

                 Use 'somevideosrc' when the device driver is 'somedriver', otherwise use default:
                 QT_GSTREAMER_CAMERABIN_VIDEOSRC="somedriver=somevideosrc"

                 Use 'somevideosrc' when the device driver is 'somedriver', otherwise use 'somevideosrc2'
                 QT_GSTREAMER_CAMERABIN_VIDEOSRC="somedriver=somevideosrc,somevideosrc2"
            */
            const QByteArray envVideoSource = qgetenv("QT_GSTREAMER_CAMERABIN_VIDEOSRC");
            if (!envVideoSource.isEmpty()) {
                QList<QByteArray> sources = envVideoSource.split(',');
                foreach (const QByteArray &source, sources) {
                    QList<QByteArray> keyValue = source.split('=');
                    if (keyValue.count() == 1) {
                        src = gst_element_factory_make(keyValue.at(0), "camera_source");
                        break;
                    } else if (keyValue.at(0) == QGstUtils::cameraDriver(m_inputDevice, m_sourceFactory)) {
                        src = gst_element_factory_make(keyValue.at(1), "camera_source");
                        break;
                    }
                }
            } else if (m_videoInputFactory) {
                src = m_videoInputFactory->buildElement();
            }

            if (!src)
                src = gst_element_factory_make("v4l2src", "camera_source");

            if (src) {
                g_object_set(G_OBJECT(src), "device", m_inputDevice.toUtf8().constData(), NULL);
                g_object_set(G_OBJECT(m_videoSrc), "video-source", src, NULL);
            }
        } else if (g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSrc), "camera-device")) {
            if (m_inputDevice == QLatin1String("secondary")) {
                g_object_set(G_OBJECT(m_videoSrc), "camera-device", 1, NULL);
            } else {
                g_object_set(G_OBJECT(m_videoSrc), "camera-device", 0, NULL);
            }
        }
    }

    if (m_videoSrc != videoSrc)
        g_object_set(G_OBJECT(m_camerabin), CAMERA_SOURCE_PROPERTY, m_videoSrc, NULL);

    if (videoSrc)
        gst_object_unref(GST_OBJECT(videoSrc));

    return m_videoSrc;
}

void CameraBinSession::captureImage(int requestId, const QString &fileName)
{
    QString actualFileName = fileName;
    if (actualFileName.isEmpty())
        actualFileName = generateFileName("img_", defaultDir(QCamera::CaptureStillImage), "jpg");

    m_requestId = requestId;

#if CAMERABIN_DEBUG
    qDebug() << Q_FUNC_INFO << m_requestId << fileName << "actual file name:" << actualFileName;
#endif

    g_object_set(G_OBJECT(m_camerabin), FILENAME_PROPERTY, actualFileName.toLocal8Bit().constData(), NULL);

    g_signal_emit_by_name(G_OBJECT(m_camerabin), CAPTURE_START, NULL);

    m_imageFileName = actualFileName;
}

void CameraBinSession::setCaptureMode(QCamera::CaptureModes mode)
{
    m_captureMode = mode;

    switch (m_captureMode) {
    case QCamera::CaptureStillImage:
        g_object_set(m_camerabin, MODE_PROPERTY, CAMERABIN_IMAGE_MODE, NULL);
        break;
    case QCamera::CaptureVideo:
        g_object_set(m_camerabin, MODE_PROPERTY, CAMERABIN_VIDEO_MODE, NULL);
        break;
    }

    m_recorderControl->updateStatus();
}

QUrl CameraBinSession::outputLocation() const
{
    //return the location service wrote data to, not one set by user, it can be empty.
    return m_actualSink;
}

bool CameraBinSession::setOutputLocation(const QUrl& sink)
{
    if (!sink.isRelative() && !sink.isLocalFile()) {
        qWarning("Output location must be a local file");
        return false;
    }

    m_sink = m_actualSink = sink;
    return true;
}

QDir CameraBinSession::defaultDir(QCamera::CaptureModes mode) const
{
    QStringList dirCandidates;

#if defined(Q_WS_MAEMO_6)
    dirCandidates << QLatin1String("/home/user/MyDocs/DCIM");
    dirCandidates << QLatin1String("/home/user/MyDocs/");
#endif

    if (mode == QCamera::CaptureVideo) {
        dirCandidates << QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        dirCandidates << QDir::home().filePath("Documents/Video");
        dirCandidates << QDir::home().filePath("Documents/Videos");
    } else {
        dirCandidates << QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        dirCandidates << QDir::home().filePath("Documents/Photo");
        dirCandidates << QDir::home().filePath("Documents/Photos");
        dirCandidates << QDir::home().filePath("Documents/photo");
        dirCandidates << QDir::home().filePath("Documents/photos");
        dirCandidates << QDir::home().filePath("Documents/Images");
    }

    dirCandidates << QDir::home().filePath("Documents");
    dirCandidates << QDir::home().filePath("My Documents");
    dirCandidates << QDir::homePath();
    dirCandidates << QDir::currentPath();
    dirCandidates << QDir::tempPath();

    foreach (const QString &path, dirCandidates) {
        if (QFileInfo(path).isWritable())
            return QDir(path);
    }

    return QDir();
}

QString CameraBinSession::generateFileName(const QString &prefix, const QDir &dir, const QString &ext) const
{
    int lastClip = 0;
    foreach(QString fileName, dir.entryList(QStringList() << QString("%1*.%2").arg(prefix).arg(ext))) {
        int imgNumber = fileName.midRef(prefix.length(), fileName.size()-prefix.length()-ext.length()-1).toInt();
        lastClip = qMax(lastClip, imgNumber);
    }

    QString name = QString("%1%2.%3").arg(prefix)
                                     .arg(lastClip+1,
                                     4, //fieldWidth
                                     10,
                                     QLatin1Char('0'))
                                     .arg(ext);

    return dir.absoluteFilePath(name);
}

void CameraBinSession::setDevice(const QString &device)
{
    if (m_inputDevice != device) {
        m_inputDevice = device;
        m_videoInputHasChanged = true;
    }
}

void CameraBinSession::setAudioInput(QGstreamerElementFactory *audioInput)
{
    m_audioInputFactory = audioInput;
}

void CameraBinSession::setVideoInput(QGstreamerElementFactory *videoInput)
{
    m_videoInputFactory = videoInput;
    m_videoInputHasChanged = true;
}

bool CameraBinSession::isReady() const
{
    //it's possible to use QCamera without any viewfinder attached
    return !m_viewfinderInterface || m_viewfinderInterface->isReady();
}

void CameraBinSession::setViewfinder(QObject *viewfinder)
{
    if (m_viewfinderInterface)
        m_viewfinderInterface->stopRenderer();

    m_viewfinderInterface = qobject_cast<QGstreamerVideoRendererInterface*>(viewfinder);
    if (!m_viewfinderInterface)
        viewfinder = 0;

    if (m_viewfinder != viewfinder) {
        bool oldReady = isReady();

        if (m_viewfinder) {
            disconnect(m_viewfinder, SIGNAL(sinkChanged()),
                       this, SLOT(handleViewfinderChange()));
            disconnect(m_viewfinder, SIGNAL(readyChanged(bool)),
                       this, SIGNAL(readyChanged(bool)));

            m_busHelper->removeMessageFilter(m_viewfinder);
        }

        m_viewfinder = viewfinder;
        m_viewfinderHasChanged = true;

        if (m_viewfinder) {
            connect(m_viewfinder, SIGNAL(sinkChanged()),
                       this, SLOT(handleViewfinderChange()));
            connect(m_viewfinder, SIGNAL(readyChanged(bool)),
                    this, SIGNAL(readyChanged(bool)));

            m_busHelper->installMessageFilter(m_viewfinder);
        }

        emit viewfinderChanged();
        if (oldReady != isReady())
            emit readyChanged(isReady());
    }
}

void CameraBinSession::handleViewfinderChange()
{
    //the viewfinder will be reloaded
    //shortly when the pipeline is started
    m_viewfinderHasChanged = true;
    emit viewfinderChanged();
}

QCamera::State CameraBinSession::state() const
{
    return m_state;
}

QCamera::State CameraBinSession::pendingState() const
{
    return m_pendingState;
}

void CameraBinSession::setState(QCamera::State newState)
{
    if (newState == m_pendingState)
        return;

    m_pendingState = newState;
    emit pendingStateChanged(m_pendingState);

#if CAMERABIN_DEBUG
    qDebug() << Q_FUNC_INFO << newState;
#endif

    switch (newState) {
    case QCamera::UnloadedState:
        if (m_recordingActive)
            stopVideoRecording();

        if (m_viewfinderInterface)
            m_viewfinderInterface->stopRenderer();

        gst_element_set_state(m_camerabin, GST_STATE_NULL);
        m_state = newState;
        if (m_busy)
            emit busyChanged(m_busy = false);

        emit stateChanged(m_state);
        break;
    case QCamera::LoadedState:
        if (m_recordingActive)
            stopVideoRecording();

        if (m_videoInputHasChanged) {
            if (m_viewfinderInterface)
                m_viewfinderInterface->stopRenderer();

            gst_element_set_state(m_camerabin, GST_STATE_NULL);
            buildCameraSource();
        }
#ifdef USE_READY_STATE_ON_LOADED
        gst_element_set_state(m_camerabin, GST_STATE_READY);
#else
        m_state = QCamera::LoadedState;
        if (m_viewfinderInterface)
            m_viewfinderInterface->stopRenderer();
        gst_element_set_state(m_camerabin, GST_STATE_NULL);
        emit stateChanged(m_state);
#endif
        break;
    case QCamera::ActiveState:
        if (setupCameraBin()) {
            GstState binState = GST_STATE_NULL;
            GstState pending = GST_STATE_NULL;
            gst_element_get_state(m_camerabin, &binState, &pending, 0);

            m_recorderControl->applySettings();

            GstEncodingContainerProfile *profile = m_recorderControl->videoProfile();
            g_object_set (G_OBJECT(m_camerabin),
                          "video-profile",
                          profile,
                          NULL);
            gst_encoding_profile_unref(profile);

            setAudioCaptureCaps();

            setupCaptureResolution();

            gst_element_set_state(m_camerabin, GST_STATE_PLAYING);
        }
    }
}

bool CameraBinSession::isBusy() const
{
    return m_busy;
}

void CameraBinSession::updateBusyStatus(GObject *o, GParamSpec *p, gpointer d)
{
    Q_UNUSED(p);
    CameraBinSession *session = reinterpret_cast<CameraBinSession *>(d);

    gboolean idle = false;
    g_object_get(o, "idle", &idle, NULL);
    bool busy = !idle;

    if (session->m_busy != busy) {
        session->m_busy = busy;
        QMetaObject::invokeMethod(session, "busyChanged",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, busy));
    }
}

qint64 CameraBinSession::duration() const
{
    if (m_camerabin) {
        GstElement *fileSink = gst_bin_get_by_name(GST_BIN(m_camerabin), FILESINK_BIN_NAME);
        if (fileSink) {
            GstFormat format = GST_FORMAT_TIME;
            gint64 duration = 0;
            bool ret = gst_element_query_position(fileSink, &format, &duration);
            gst_object_unref(GST_OBJECT(fileSink));
            if (ret)
                return duration / 1000000;
        }
    }

    return 0;
}

bool CameraBinSession::isMuted() const
{
    return m_muted;
}

void CameraBinSession::setMuted(bool muted)
{
    if (m_muted != muted) {
        m_muted = muted;

        if (m_camerabin)
            g_object_set(G_OBJECT(m_camerabin), MUTE_PROPERTY, m_muted, NULL);
        emit mutedChanged(m_muted);
    }
}

void CameraBinSession::setCaptureDevice(const QString &deviceName)
{
    m_captureDevice = deviceName;
}

void CameraBinSession::setMetaData(const QMap<QByteArray, QVariant> &data)
{
    m_metaData = data;

    if (m_camerabin) {
        GstIterator *elements = gst_bin_iterate_all_by_interface(GST_BIN(m_camerabin), GST_TYPE_TAG_SETTER);
        GstElement *element = 0;
        while (gst_iterator_next(elements, (void**)&element) == GST_ITERATOR_OK) {
            gst_tag_setter_reset_tags(GST_TAG_SETTER(element));

            QMapIterator<QByteArray, QVariant> it(data);
            while (it.hasNext()) {
                it.next();
                const QString tagName = it.key();
                const QVariant tagValue = it.value();

                switch(tagValue.type()) {
                    case QVariant::String:
                        gst_tag_setter_add_tags(GST_TAG_SETTER(element),
                            GST_TAG_MERGE_REPLACE,
                            tagName.toUtf8().constData(),
                            tagValue.toString().toUtf8().constData(),
                            NULL);
                        break;
                    case QVariant::Int:
                    case QVariant::LongLong:
                        gst_tag_setter_add_tags(GST_TAG_SETTER(element),
                            GST_TAG_MERGE_REPLACE,
                            tagName.toUtf8().constData(),
                            tagValue.toInt(),
                            NULL);
                        break;
                    case QVariant::Double:
                        gst_tag_setter_add_tags(GST_TAG_SETTER(element),
                            GST_TAG_MERGE_REPLACE,
                            tagName.toUtf8().constData(),
                            tagValue.toDouble(),
                            NULL);
                        break;
                    case QVariant::DateTime: {
                        QDateTime date = tagValue.toDateTime().toLocalTime();
                        gst_tag_setter_add_tags(GST_TAG_SETTER(element),
                            GST_TAG_MERGE_REPLACE,
                            tagName.toUtf8().constData(),
                            gst_date_time_new_local_time(
                                        date.date().year(), date.date().month(), date.date().day(),
                                        date.time().hour(), date.time().minute(), date.time().second()),
                            NULL);
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        gst_iterator_free(elements);
    }
}

bool CameraBinSession::processSyncMessage(const QGstreamerMessage &message)
{
    GstMessage* gm = message.rawMessage();
    const GstStructure *st;
    const GValue *image;
    GstBuffer *buffer = NULL;

    if (gm && GST_MESSAGE_TYPE(gm) == GST_MESSAGE_ELEMENT) {
        if (m_captureMode == QCamera::CaptureStillImage &&
            gst_structure_has_name(gm->structure, "preview-image")) {
            st = gst_message_get_structure(gm);

            if (gst_structure_has_field_typed(st, "buffer", GST_TYPE_BUFFER)) {
                image = gst_structure_get_value(st, "buffer");
                if (image) {
                    buffer = gst_value_get_buffer(image);

                    QImage img;

                    GstCaps *caps = gst_buffer_get_caps(buffer);
                    if (caps) {
                        GstStructure *structure = gst_caps_get_structure(caps, 0);
                        gint width = 0;
                        gint height = 0;
#if CAMERABIN_DEBUG
                        qDebug() << "Preview caps:" << gst_structure_to_string(structure);
#endif

                        if (structure &&
                            gst_structure_get_int(structure, "width", &width) &&
                            gst_structure_get_int(structure, "height", &height) &&
                            width > 0 && height > 0) {
                            if (qstrcmp(gst_structure_get_name(structure), "video/x-raw-rgb") == 0) {
                                QImage::Format format = QImage::Format_Invalid;
                                int bpp = 0;
                                gst_structure_get_int(structure, "bpp", &bpp);

                                if (bpp == 24)
                                    format = QImage::Format_RGB888;
                                else if (bpp == 32)
                                    format = QImage::Format_RGB32;

                                if (format != QImage::Format_Invalid) {
                                    img = QImage((const uchar *)buffer->data, width, height, format);
                                    img.bits(); //detach
                                 }
                            }
                        }
                        gst_caps_unref(caps);

                        static QMetaMethod exposedSignal = QMetaMethod::fromSignal(&CameraBinSession::imageExposed);
                        exposedSignal.invoke(this,
                                             Qt::QueuedConnection,
                                             Q_ARG(int,m_requestId));

                        static QMetaMethod capturedSignal = QMetaMethod::fromSignal(&CameraBinSession::imageCaptured);
                        capturedSignal.invoke(this,
                                              Qt::QueuedConnection,
                                              Q_ARG(int,m_requestId),
                                              Q_ARG(QImage,img));
                    }

                }
                return true;
            }
        }
#ifdef HAVE_GST_PHOTOGRAPHY
        if (gst_structure_has_name(gm->structure, GST_PHOTOGRAPHY_AUTOFOCUS_DONE))
            m_cameraFocusControl->handleFocusMessage(gm);
#endif
    }

    return false;
}

bool CameraBinSession::processBusMessage(const QGstreamerMessage &message)
{
    GstMessage* gm = message.rawMessage();

    if (gm) {
        if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_ERROR) {
            GError *err;
            gchar *debug;
            gst_message_parse_error (gm, &err, &debug);

            QString message;

            if (err && err->message) {
                message = QString::fromUtf8(err->message);
                qWarning() << "CameraBin error:" << message;
            }

            //only report error messager from camerabin
            if (GST_MESSAGE_SRC(gm) == GST_OBJECT_CAST(m_camerabin)) {
                if (message.isEmpty())
                    message = tr("Camera error");

                emit error(int(QMediaRecorder::ResourceError), message);
            }

#ifdef CAMERABIN_DEBUG_DUMP_BIN
            _gst_debug_bin_to_dot_file_with_ts(GST_BIN(m_camerabin),
                                  GstDebugGraphDetails(GST_DEBUG_GRAPH_SHOW_ALL /* GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE | GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS | GST_DEBUG_GRAPH_SHOW_STATES*/),
                                  "camerabin_error");
#endif


            if (err)
                g_error_free (err);

            if (debug)
                g_free (debug);
        }

        if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_WARNING) {
            GError *err;
            gchar *debug;
            gst_message_parse_warning (gm, &err, &debug);

            if (err && err->message)
                qWarning() << "CameraBin warning:" << QString::fromUtf8(err->message);

            if (err)
                g_error_free (err);
            if (debug)
                g_free (debug);
        }

        if (GST_MESSAGE_SRC(gm) == GST_OBJECT_CAST(m_camerabin)) {
            switch (GST_MESSAGE_TYPE(gm))  {
            case GST_MESSAGE_DURATION:
                break;

            case GST_MESSAGE_STATE_CHANGED:
                {

                    GstState    oldState;
                    GstState    newState;
                    GstState    pending;

                    gst_message_parse_state_changed(gm, &oldState, &newState, &pending);


#if CAMERABIN_DEBUG
                    QStringList states;
                    states << "GST_STATE_VOID_PENDING" <<  "GST_STATE_NULL" << "GST_STATE_READY" << "GST_STATE_PAUSED" << "GST_STATE_PLAYING";


                    qDebug() << QString("state changed: old: %1  new: %2  pending: %3") \
                            .arg(states[oldState]) \
                           .arg(states[newState]) \
                            .arg(states[pending]);
#endif

#ifdef CAMERABIN_DEBUG_DUMP_BIN
                    _gst_debug_bin_to_dot_file_with_ts(GST_BIN(m_camerabin),
                                  GstDebugGraphDetails(GST_DEBUG_GRAPH_SHOW_ALL /*GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE | GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS | GST_DEBUG_GRAPH_SHOW_STATES*/),
                                  "camerabin");
#endif

                    switch (newState) {
                    case GST_STATE_VOID_PENDING:
                    case GST_STATE_NULL:
                        if (m_state != QCamera::UnloadedState)
                            emit stateChanged(m_state = QCamera::UnloadedState);
                        break;
                    case GST_STATE_READY:
                        setMetaData(m_metaData);
                        if (m_state != QCamera::LoadedState)
                            emit stateChanged(m_state = QCamera::LoadedState);
                        break;
                    case GST_STATE_PAUSED:
                    case GST_STATE_PLAYING:
                        emit stateChanged(m_state = QCamera::ActiveState);
                        break;
                    }
                }
                break;
            default:
                break;
            }
            //qDebug() << "New session state:" << ENUM_NAME(CameraBinSession,"State",m_state);
        }
    }

    return false;
}

void CameraBinSession::recordVideo()
{
    m_recordingActive = true;
    m_actualSink = m_sink;
    if (m_actualSink.isEmpty()) {
        QString ext = m_mediaContainerControl->suggestedFileExtension(m_mediaContainerControl->actualContainerFormat());
        m_actualSink = QUrl::fromLocalFile(generateFileName("clip_", defaultDir(QCamera::CaptureVideo), ext));
    } else {
        // Output location was rejected in setOutputlocation() if not a local file
        m_actualSink = QUrl::fromLocalFile(QDir::currentPath()).resolved(m_actualSink);
    }

    QString fileName = m_actualSink.toLocalFile();
    g_object_set(G_OBJECT(m_camerabin), FILENAME_PROPERTY, QFile::encodeName(fileName).constData(), NULL);

    g_signal_emit_by_name(G_OBJECT(m_camerabin), CAPTURE_START, NULL);
}

void CameraBinSession::stopVideoRecording()
{
    m_recordingActive = false;
    g_signal_emit_by_name(G_OBJECT(m_camerabin), CAPTURE_STOP, NULL);
}

//internal, only used by CameraBinSession::supportedFrameRates.
//recursively fills the list of framerates res from value data.
static void readValue(const GValue *value, QList< QPair<int,int> > *res, bool *continuous)
{
    if (GST_VALUE_HOLDS_FRACTION(value)) {
        int num = gst_value_get_fraction_numerator(value);
        int denum = gst_value_get_fraction_denominator(value);

        *res << QPair<int,int>(num, denum);
    } else if (GST_VALUE_HOLDS_FRACTION_RANGE(value)) {
        const GValue *rateValueMin = gst_value_get_fraction_range_min(value);
        const GValue *rateValueMax = gst_value_get_fraction_range_max(value);

        if (continuous)
            *continuous = true;

        readValue(rateValueMin, res, continuous);
        readValue(rateValueMax, res, continuous);
    } else if (GST_VALUE_HOLDS_LIST(value)) {
        for (uint i=0; i<gst_value_list_get_size(value); i++) {
            readValue(gst_value_list_get_value(value, i), res, continuous);
        }
    }
}

static bool rateLessThan(const QPair<int,int> &r1, const QPair<int,int> &r2)
{
     return r1.first*r2.second < r2.first*r1.second;
}

QList< QPair<int,int> > CameraBinSession::supportedFrameRates(const QSize &frameSize, bool *continuous) const
{
    QList< QPair<int,int> > res;

    GstCaps *supportedCaps = 0;
    g_object_get(G_OBJECT(m_camerabin),
                 SUPPORTED_VIDEO_CAPTURE_CAPS_PROPERTY,
                 &supportedCaps, NULL);

    if (!supportedCaps)
        return res;

    GstCaps *caps = 0;

    if (frameSize.isEmpty()) {
        caps = gst_caps_copy(supportedCaps);
    } else {
        GstCaps *filter = gst_caps_new_full(
                gst_structure_new(
                        "video/x-raw-rgb",
                        "width"     , G_TYPE_INT , frameSize.width(),
                        "height"    , G_TYPE_INT, frameSize.height(), NULL),
                gst_structure_new(
                        "video/x-raw-yuv",
                        "width"     , G_TYPE_INT, frameSize.width(),
                        "height"    , G_TYPE_INT, frameSize.height(), NULL),
                gst_structure_new(
                        "image/jpeg",
                        "width"     , G_TYPE_INT, frameSize.width(),
                        "height"    , G_TYPE_INT, frameSize.height(), NULL),
                NULL);

        caps = gst_caps_intersect(supportedCaps, filter);
        gst_caps_unref(filter);
    }
    gst_caps_unref(supportedCaps);

    //simplify to the list of rates only:
    caps = gst_caps_make_writable(caps);
    for (uint i=0; i<gst_caps_get_size(caps); i++) {
        GstStructure *structure = gst_caps_get_structure(caps, i);
        gst_structure_set_name(structure, "video/x-raw-yuv");
        const GValue *oldRate = gst_structure_get_value(structure, "framerate");
        GValue rate;
        memset(&rate, 0, sizeof(rate));
        g_value_init(&rate, G_VALUE_TYPE(oldRate));
        g_value_copy(oldRate, &rate);
        gst_structure_remove_all_fields(structure);
        gst_structure_set_value(structure, "framerate", &rate);
    }
    gst_caps_do_simplify(caps);


    for (uint i=0; i<gst_caps_get_size(caps); i++) {
        GstStructure *structure = gst_caps_get_structure(caps, i);
        const GValue *rateValue = gst_structure_get_value(structure, "framerate");
        readValue(rateValue, &res, continuous);
    }

    qSort(res.begin(), res.end(), rateLessThan);

#if CAMERABIN_DEBUG
    qDebug() << "Supported rates:" << gst_caps_to_string(caps);
    qDebug() << res;
#endif

    gst_caps_unref(caps);

    return res;
}

//internal, only used by CameraBinSession::supportedResolutions
//recursively find the supported resolutions range.
static QPair<int,int> valueRange(const GValue *value, bool *continuous)
{
    int minValue = 0;
    int maxValue = 0;

    if (g_value_type_compatible(G_VALUE_TYPE(value), G_TYPE_INT)) {
        minValue = maxValue = g_value_get_int(value);
    } else if (GST_VALUE_HOLDS_INT_RANGE(value)) {
        minValue = gst_value_get_int_range_min(value);
        maxValue = gst_value_get_int_range_max(value);
        *continuous = true;
    } else if (GST_VALUE_HOLDS_LIST(value)) {
        for (uint i=0; i<gst_value_list_get_size(value); i++) {
            QPair<int,int> res = valueRange(gst_value_list_get_value(value, i), continuous);

            if (res.first > 0 && minValue > 0)
                minValue = qMin(minValue, res.first);
            else //select non 0 valid value
                minValue = qMax(minValue, res.first);

            maxValue = qMax(maxValue, res.second);
        }
    }

    return QPair<int,int>(minValue, maxValue);
}

static bool resolutionLessThan(const QSize &r1, const QSize &r2)
{
     return r1.width()*r1.height() < r2.width()*r2.height();
}


QList<QSize> CameraBinSession::supportedResolutions(QPair<int,int> rate,
                                                    bool *continuous,
                                                    QCamera::CaptureModes mode) const
{
    QList<QSize> res;

    if (continuous)
        *continuous = false;

    GstCaps *supportedCaps = 0;
    g_object_get(G_OBJECT(m_camerabin),
                 (mode == QCamera::CaptureStillImage) ?
                     SUPPORTED_IMAGE_CAPTURE_CAPS_PROPERTY : SUPPORTED_VIDEO_CAPTURE_CAPS_PROPERTY,
                 &supportedCaps, NULL);

    if (!supportedCaps)
        return res;

#if CAMERABIN_DEBUG
    qDebug() << "Source caps:" << gst_caps_to_string(supportedCaps);
#endif

    GstCaps *caps = 0;
    bool isContinuous = false;

    if (rate.first <= 0 || rate.second <= 0) {
        caps = gst_caps_copy(supportedCaps);
    } else {
        GstCaps *filter = gst_caps_new_full(
                gst_structure_new(
                        "video/x-raw-rgb",
                        "framerate"     , GST_TYPE_FRACTION , rate.first, rate.second, NULL),
                gst_structure_new(
                        "video/x-raw-yuv",
                        "framerate"     , GST_TYPE_FRACTION , rate.first, rate.second, NULL),
                gst_structure_new(
                        "image/jpeg",
                        "framerate"     , GST_TYPE_FRACTION , rate.first, rate.second, NULL),
                NULL);

        caps = gst_caps_intersect(supportedCaps, filter);
        gst_caps_unref(filter);
    }
    gst_caps_unref(supportedCaps);

    //simplify to the list of resolutions only:
    caps = gst_caps_make_writable(caps);
    for (uint i=0; i<gst_caps_get_size(caps); i++) {
        GstStructure *structure = gst_caps_get_structure(caps, i);
        gst_structure_set_name(structure, "video/x-raw-yuv");
        const GValue *oldW = gst_structure_get_value(structure, "width");
        const GValue *oldH = gst_structure_get_value(structure, "height");
        GValue w;
        memset(&w, 0, sizeof(GValue));
        GValue h;
        memset(&h, 0, sizeof(GValue));
        g_value_init(&w, G_VALUE_TYPE(oldW));
        g_value_init(&h, G_VALUE_TYPE(oldH));
        g_value_copy(oldW, &w);
        g_value_copy(oldH, &h);
        gst_structure_remove_all_fields(structure);
        gst_structure_set_value(structure, "width", &w);
        gst_structure_set_value(structure, "height", &h);
    }
    gst_caps_do_simplify(caps);

    for (uint i=0; i<gst_caps_get_size(caps); i++) {
        GstStructure *structure = gst_caps_get_structure(caps, i);
        const GValue *wValue = gst_structure_get_value(structure, "width");
        const GValue *hValue = gst_structure_get_value(structure, "height");

        QPair<int,int> wRange = valueRange(wValue, &isContinuous);
        QPair<int,int> hRange = valueRange(hValue, &isContinuous);

        QSize minSize(wRange.first, hRange.first);
        QSize maxSize(wRange.second, hRange.second);

        if (!minSize.isEmpty())
            res << minSize;

        if (minSize != maxSize && !maxSize.isEmpty())
            res << maxSize;
    }


    qSort(res.begin(), res.end(), resolutionLessThan);

    //if the range is continuos, populate is with the common rates
    if (isContinuous && res.size() >= 2) {
        //fill the ragne with common value
        static QList<QSize> commonSizes =
                QList<QSize>() << QSize(128, 96)
                               << QSize(160,120)
                               << QSize(176, 144)
                               << QSize(320, 240)
                               << QSize(352, 288)
                               << QSize(640, 480)
                               << QSize(848, 480)
                               << QSize(854, 480)
                               << QSize(1024, 768)
                               << QSize(1280, 720) // HD 720
                               << QSize(1280, 1024)
                               << QSize(1600, 1200)
                               << QSize(1920, 1080) // HD
                               << QSize(1920, 1200)
                               << QSize(2048, 1536)
                               << QSize(2560, 1600)
                               << QSize(2580, 1936);
        QSize minSize = res.first();
        QSize maxSize = res.last();
        res.clear();

        foreach (const QSize &candidate, commonSizes) {
            int w = candidate.width();
            int h = candidate.height();

            if (w > maxSize.width() && h > maxSize.height())
                break;

            if (w >= minSize.width() && h >= minSize.height() &&
                w <= maxSize.width() && h <= maxSize.height())
                res << candidate;
        }

        if (res.isEmpty() || res.first() != minSize)
            res.prepend(minSize);

        if (res.last() != maxSize)
            res.append(maxSize);
    }

#if CAMERABIN_DEBUG
    qDebug() << "Supported resolutions:" << gst_caps_to_string(caps);
    qDebug() << res;
#endif

    gst_caps_unref(caps);

    if (continuous)
        *continuous = isContinuous;

    return res;
}

void CameraBinSession::elementAdded(GstBin *, GstElement *element, CameraBinSession *session)
{
    GstElementFactory *factory = gst_element_get_factory(element);

    if (GST_IS_BIN(element)) {
        g_signal_connect(G_OBJECT(element), "element-added",  G_CALLBACK(elementAdded), session);
        g_signal_connect(G_OBJECT(element), "element-removed",  G_CALLBACK(elementRemoved), session);
    } else if (!factory) {
        // no-op
    } else if (gst_element_factory_list_is_type(factory, GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER)) {
        session->m_audioEncoder = element;

        session->m_audioEncodeControl->applySettings(element);
    } else if (gst_element_factory_list_is_type(factory, GST_ELEMENT_FACTORY_TYPE_VIDEO_ENCODER)) {
        session->m_videoEncoder = element;

        session->m_videoEncodeControl->applySettings(element);
    }
}

void CameraBinSession::elementRemoved(GstBin *, GstElement *element, CameraBinSession *session)
{
    if (element == session->m_audioEncoder)
        session->m_audioEncoder = 0;
    else if (element == session->m_videoEncoder)
        session->m_videoEncoder = 0;
}

QT_END_NAMESPACE
