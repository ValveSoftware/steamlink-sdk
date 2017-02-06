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

#ifndef CAMERABINCAPTURESESSION_H
#define CAMERABINCAPTURESESSION_H

#include <qmediarecordercontrol.h>

#include <QtCore/qurl.h>
#include <QtCore/qdir.h>

#include <gst/gst.h>
#ifdef HAVE_GST_PHOTOGRAPHY
#include <gst/interfaces/photography.h>
#endif

#include <private/qgstreamerbushelper_p.h>
#include <private/qgstreamervideoprobecontrol_p.h>
#include <private/qmediastoragelocation_p.h>
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
    CameraBinSession(GstElementFactory *sourceFactory, QObject *parent);
    ~CameraBinSession();

#ifdef HAVE_GST_PHOTOGRAPHY
    GstPhotography *photography();
#endif
    GstElement *cameraBin() { return m_camerabin; }
    GstElement *cameraSource() { return m_cameraSrc; }
    QGstreamerBusHelper *bus() { return m_busHelper; }

    QList< QPair<int,int> > supportedFrameRates(const QSize &frameSize, bool *continuous) const;
    QList<QSize> supportedResolutions(QPair<int,int> rate, bool *continuous, QCamera::CaptureModes mode) const;

    QCamera::CaptureModes captureMode() { return m_captureMode; }
    void setCaptureMode(QCamera::CaptureModes mode);

    QUrl outputLocation() const;
    bool setOutputLocation(const QUrl& sink);

    GstElement *buildCameraSource();
    GstElementFactory *sourceFactory() const { return m_sourceFactory; }

    CameraBinControl *cameraControl() const { return m_cameraControl; }
    CameraBinAudioEncoder *audioEncodeControl() const { return m_audioEncodeControl; }
    CameraBinVideoEncoder *videoEncodeControl() const { return m_videoEncodeControl; }
    CameraBinImageEncoder *imageEncodeControl() const { return m_imageEncodeControl; }

#ifdef HAVE_GST_PHOTOGRAPHY
    CameraBinExposure *cameraExposureControl();
    CameraBinFlash *cameraFlashControl();
    CameraBinFocus *cameraFocusControl();
    CameraBinLocks *cameraLocksControl();
#endif

    CameraBinZoom *cameraZoomControl() const { return m_cameraZoomControl; }
    CameraBinImageProcessing *imageProcessingControl() const { return m_imageProcessingControl; }
    CameraBinCaptureDestination *captureDestinationControl() const { return m_captureDestinationControl; }
    CameraBinCaptureBufferFormat *captureBufferFormatControl() const { return m_captureBufferFormatControl; }

    CameraBinRecorder *recorderControl() const { return m_recorderControl; }
    CameraBinContainer *mediaContainerControl() const { return m_mediaContainerControl; }

    QGstreamerElementFactory *audioInput() const { return m_audioInputFactory; }
    void setAudioInput(QGstreamerElementFactory *audioInput);

    QGstreamerElementFactory *videoInput() const { return m_videoInputFactory; }
    void setVideoInput(QGstreamerElementFactory *videoInput);
    bool isReady() const;

    QObject *viewfinder() const { return m_viewfinder; }
    void setViewfinder(QObject *viewfinder);

    QList<QCameraViewfinderSettings> supportedViewfinderSettings() const;
    QCameraViewfinderSettings viewfinderSettings() const;
    void setViewfinderSettings(const QCameraViewfinderSettings &settings) { m_viewfinderSettings = settings; }

    void captureImage(int requestId, const QString &fileName);

    QCamera::Status status() const;
    QCamera::State pendingState() const;
    bool isBusy() const;

    qint64 duration() const;

    void recordVideo();
    void stopVideoRecording();

    bool isMuted() const;

    QString device() const { return m_inputDevice; }

    bool processSyncMessage(const QGstreamerMessage &message);
    bool processBusMessage(const QGstreamerMessage &message);

    QGstreamerVideoProbeControl *videoProbe();

signals:
    void statusChanged(QCamera::Status status);
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
    void setupCaptureResolution();

private:
    void load();
    void unload();
    void start();
    void stop();

    void setStatus(QCamera::Status status);
    void setStateHelper(QCamera::State state);
    void setError(int error, const QString &errorString);

    bool setupCameraBin();
    void setAudioCaptureCaps();
    GstCaps *supportedCaps(QCamera::CaptureModes mode) const;
    void updateSupportedViewfinderSettings();
    static void updateBusyStatus(GObject *o, GParamSpec *p, gpointer d);

    QString currentContainerFormat() const;

    static void elementAdded(GstBin *bin, GstElement *element, CameraBinSession *session);
    static void elementRemoved(GstBin *bin, GstElement *element, CameraBinSession *session);

    QUrl m_sink;
    QUrl m_actualSink;
    bool m_recordingActive;
    QString m_captureDevice;
    QCamera::Status m_status;
    QCamera::State m_pendingState;
    QString m_inputDevice;
    bool m_muted;
    bool m_busy;
    QMediaStorageLocation m_mediaStorageLocation;

    QCamera::CaptureModes m_captureMode;
    QMap<QByteArray, QVariant> m_metaData;

    QGstreamerElementFactory *m_audioInputFactory;
    QGstreamerElementFactory *m_videoInputFactory;
    QObject *m_viewfinder;
    QGstreamerVideoRendererInterface *m_viewfinderInterface;
    QList<QCameraViewfinderSettings> m_supportedViewfinderSettings;
    QCameraViewfinderSettings m_viewfinderSettings;
    QCameraViewfinderSettings m_actualViewfinderSettings;

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
#endif
    CameraBinZoom *m_cameraZoomControl;
    CameraBinImageProcessing *m_imageProcessingControl;
    CameraBinCaptureDestination *m_captureDestinationControl;
    CameraBinCaptureBufferFormat *m_captureBufferFormatControl;

    QGstreamerBusHelper *m_busHelper;
    GstBus* m_bus;
    GstElement *m_camerabin;
    GstElement *m_cameraSrc;
    GstElement *m_videoSrc;
    GstElement *m_viewfinderElement;
    GstElementFactory *m_sourceFactory;
    bool m_viewfinderHasChanged;
    bool m_inputDeviceHasChanged;
    bool m_usingWrapperCameraBinSrc;

    class ViewfinderProbe : public QGstreamerVideoProbeControl {
    public:
        ViewfinderProbe(CameraBinSession *s)
            : QGstreamerVideoProbeControl(s)
            , session(s)
        {}

        void probeCaps(GstCaps *caps) override;

    private:
        CameraBinSession * const session;
    } m_viewfinderProbe;

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

#endif // CAMERABINCAPTURESESSION_H
