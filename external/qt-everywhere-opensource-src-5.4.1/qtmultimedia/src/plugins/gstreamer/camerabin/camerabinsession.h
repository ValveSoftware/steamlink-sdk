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

#ifndef CAMERABINCAPTURESESSION_MAEMO_H
#define CAMERABINCAPTURESESSION_MAEMO_H

#include <qmediarecordercontrol.h>

#include <QtCore/qurl.h>
#include <QtCore/qdir.h>

#include <gst/gst.h>
#ifdef HAVE_GST_PHOTOGRAPHY
#include <gst/interfaces/photography.h>
#endif

#include <private/qgstreamerbushelper_p.h>
#include "qcamera.h"

QT_BEGIN_NAMESPACE

class QGstreamerMessage;
class QGstreamerBusHelper;
class CameraBinControl;
class CameraBinAudioEncoder;
class CameraBinVideoEncoder;
class CameraBinImageEncoder;
class CameraBinRecorder;
class CameraBinContainer;
class CameraBinExposure;
class CameraBinFlash;
class CameraBinFocus;
class CameraBinImageProcessing;
class CameraBinLocks;
class CameraBinZoom;
class CameraBinCaptureDestination;
class CameraBinCaptureBufferFormat;
class QGstreamerVideoRendererInterface;
class CameraBinViewfinderSettings;

class QGstreamerElementFactory
{
public:
    virtual GstElement *buildElement() = 0;
};


class CameraBinSession : public QObject,
                         public QGstreamerBusMessageFilter,
                         public QGstreamerSyncMessageFilter
{
    Q_OBJECT
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_INTERFACES(QGstreamerBusMessageFilter QGstreamerSyncMessageFilter)
public:
    enum CameraRole {
       FrontCamera, // Secondary camera
       BackCamera // Main photo camera
    };

    CameraBinSession(GstElementFactory *sourceFactory, QObject *parent);
    ~CameraBinSession();

#ifdef HAVE_GST_PHOTOGRAPHY
    GstPhotography *photography();
#endif
    GstElement *cameraBin() { return m_camerabin; }
    GstElement *cameraSource() { return m_videoSrc; }
    QGstreamerBusHelper *bus() { return m_busHelper; }

    CameraRole cameraRole() const;

    QList< QPair<int,int> > supportedFrameRates(const QSize &frameSize, bool *continuous) const;
    QList<QSize> supportedResolutions(QPair<int,int> rate, bool *continuous, QCamera::CaptureModes mode) const;

    QCamera::CaptureModes captureMode() { return m_captureMode; }
    void setCaptureMode(QCamera::CaptureModes mode);

    QUrl outputLocation() const;
    bool setOutputLocation(const QUrl& sink);

    QDir defaultDir(QCamera::CaptureModes mode) const;
    QString generateFileName(const QString &prefix, const QDir &dir, const QString &ext) const;

    GstElement *buildCameraSource();
    GstElementFactory *sourceFactory() const { return m_sourceFactory; }

    CameraBinControl *cameraControl() const { return m_cameraControl; }
    CameraBinAudioEncoder *audioEncodeControl() const { return m_audioEncodeControl; }
    CameraBinVideoEncoder *videoEncodeControl() const { return m_videoEncodeControl; }
    CameraBinImageEncoder *imageEncodeControl() const { return m_imageEncodeControl; }

#ifdef HAVE_GST_PHOTOGRAPHY
    CameraBinExposure *cameraExposureControl() const  { return m_cameraExposureControl; }
    CameraBinFlash *cameraFlashControl() const  { return m_cameraFlashControl; }
    CameraBinFocus *cameraFocusControl() const  { return m_cameraFocusControl; }
    CameraBinLocks *cameraLocksControl() const { return m_cameraLocksControl; }
    CameraBinZoom *cameraZoomControl() const { return m_cameraZoomControl; }
#endif

    CameraBinImageProcessing *imageProcessingControl() const { return m_imageProcessingControl; }
    CameraBinCaptureDestination *captureDestinationControl() const { return m_captureDestinationControl; }
    CameraBinCaptureBufferFormat *captureBufferFormatControl() const { return m_captureBufferFormatControl; }
    CameraBinViewfinderSettings *viewfinderSettingsControl() const { return m_viewfinderSettingsControl; }

    CameraBinRecorder *recorderControl() const { return m_recorderControl; }
    CameraBinContainer *mediaContainerControl() const { return m_mediaContainerControl; }

    QGstreamerElementFactory *audioInput() const { return m_audioInputFactory; }
    void setAudioInput(QGstreamerElementFactory *audioInput);

    QGstreamerElementFactory *videoInput() const { return m_videoInputFactory; }
    void setVideoInput(QGstreamerElementFactory *videoInput);
    bool isReady() const;

    QObject *viewfinder() const { return m_viewfinder; }
    void setViewfinder(QObject *viewfinder);

    void captureImage(int requestId, const QString &fileName);

    QCamera::State state() const;
    QCamera::State pendingState() const;
    bool isBusy() const;

    qint64 duration() const;

    void recordVideo();
    void stopVideoRecording();

    bool isMuted() const;

    bool processSyncMessage(const QGstreamerMessage &message);
    bool processBusMessage(const QGstreamerMessage &message);

signals:
    void stateChanged(QCamera::State state);
    void pendingStateChanged(QCamera::State state);
    void durationChanged(qint64 duration);
    void error(int error, const QString &errorString);
    void imageExposed(int requestId);
    void imageCaptured(int requestId, const QImage &img);
    void mutedChanged(bool);
    void viewfinderChanged();
    void readyChanged(bool);
    void busyChanged(bool);

public slots:
    void setDevice(const QString &device);
    void setState(QCamera::State);
    void setCaptureDevice(const QString &deviceName);
    void setMetaData(const QMap<QByteArray, QVariant>&);
    void setMuted(bool);

private slots:
    void handleViewfinderChange();

private:
    bool setupCameraBin();
    void setupCaptureResolution();
    void setAudioCaptureCaps();
    static void updateBusyStatus(GObject *o, GParamSpec *p, gpointer d);

    static void elementAdded(GstBin *bin, GstElement *element, CameraBinSession *session);
    static void elementRemoved(GstBin *bin, GstElement *element, CameraBinSession *session);

    QUrl m_sink;
    QUrl m_actualSink;
    bool m_recordingActive;
    QString m_captureDevice;
    QCamera::State m_state;
    QCamera::State m_pendingState;
    QString m_inputDevice;
    bool m_muted;
    bool m_busy;

    QCamera::CaptureModes m_captureMode;
    QMap<QByteArray, QVariant> m_metaData;

    QGstreamerElementFactory *m_audioInputFactory;
    QGstreamerElementFactory *m_videoInputFactory;
    QObject *m_viewfinder;
    QGstreamerVideoRendererInterface *m_viewfinderInterface;

    CameraBinControl *m_cameraControl;
    CameraBinAudioEncoder *m_audioEncodeControl;
    CameraBinVideoEncoder *m_videoEncodeControl;
    CameraBinImageEncoder *m_imageEncodeControl;
    CameraBinRecorder *m_recorderControl;
    CameraBinContainer *m_mediaContainerControl;
#ifdef HAVE_GST_PHOTOGRAPHY
    CameraBinExposure *m_cameraExposureControl;
    CameraBinFlash *m_cameraFlashControl;
    CameraBinFocus *m_cameraFocusControl;
    CameraBinLocks *m_cameraLocksControl;
    CameraBinZoom *m_cameraZoomControl;
#endif

    CameraBinImageProcessing *m_imageProcessingControl;
    CameraBinCaptureDestination *m_captureDestinationControl;
    CameraBinCaptureBufferFormat *m_captureBufferFormatControl;
    CameraBinViewfinderSettings *m_viewfinderSettingsControl;

    QGstreamerBusHelper *m_busHelper;
    GstBus* m_bus;
    GstElement *m_camerabin;
    GstElement *m_videoSrc;
    GstElement *m_viewfinderElement;
    GstElementFactory *m_sourceFactory;
    bool m_viewfinderHasChanged;
    bool m_videoInputHasChanged;

    GstElement *m_audioSrc;
    GstElement *m_audioConvert;
    GstElement *m_capsFilter;
    GstElement *m_fileSink;
    GstElement *m_audioEncoder;
    GstElement *m_videoEncoder;
    GstElement *m_muxer;

public:
    QString m_imageFileName;
    int m_requestId;
};

QT_END_NAMESPACE

#endif // CAMERABINCAPTURESESSION_MAEMO_H
