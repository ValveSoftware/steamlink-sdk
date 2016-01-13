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

#include "camerabinimagecapture.h"
#include "camerabincontrol.h"
#include "camerabincapturedestination.h"
#include "camerabincapturebufferformat.h"
#include "camerabinsession.h"
#include "camerabinresourcepolicy.h"
#include <private/qgstvideobuffer_p.h>
#include <private/qvideosurfacegstsink_p.h>
#include <private/qgstutils_p.h>
#include <QtMultimedia/qmediametadata.h>
#include <QtCore/qdebug.h>
#include <QtCore/qbuffer.h>
#include <QtGui/qimagereader.h>

//#define DEBUG_CAPTURE

#define IMAGE_DONE_SIGNAL "image-done"

QT_BEGIN_NAMESPACE

CameraBinImageCapture::CameraBinImageCapture(CameraBinSession *session)
    :QCameraImageCaptureControl(session)
    , m_session(session)
    , m_ready(false)
    , m_requestId(0)
    , m_jpegEncoderElement(0)
    , m_metadataMuxerElement(0)
{
    connect(m_session, SIGNAL(stateChanged(QCamera::State)), SLOT(updateState()));
    connect(m_session, SIGNAL(imageExposed(int)), this, SIGNAL(imageExposed(int)));
    connect(m_session, SIGNAL(imageCaptured(int,QImage)), this, SIGNAL(imageCaptured(int,QImage)));
    connect(m_session->cameraControl()->resourcePolicy(), SIGNAL(canCaptureChanged()), this, SLOT(updateState()));

    m_session->bus()->installMessageFilter(this);
}

CameraBinImageCapture::~CameraBinImageCapture()
{
}

bool CameraBinImageCapture::isReadyForCapture() const
{
    return m_ready;
}

int CameraBinImageCapture::capture(const QString &fileName)
{
    m_requestId++;

    if (!m_ready) {
        emit error(m_requestId, QCameraImageCapture::NotReadyError, tr("Camera not ready"));
        return m_requestId;
    }

#ifdef DEBUG_CAPTURE
    qDebug() << Q_FUNC_INFO << m_requestId << fileName;
#endif
    m_session->captureImage(m_requestId, fileName);
    return m_requestId;
}

void CameraBinImageCapture::cancelCapture()
{
}

void CameraBinImageCapture::updateState()
{
    bool ready = m_session->state() == QCamera::ActiveState
            && m_session->cameraControl()->resourcePolicy()->canCapture();
    if (m_ready != ready) {
#ifdef DEBUG_CAPTURE
        qDebug() << "readyForCaptureChanged" << ready;
#endif
        emit readyForCaptureChanged(m_ready = ready);
    }
}

gboolean CameraBinImageCapture::metadataEventProbe(GstPad *pad, GstEvent *event, CameraBinImageCapture *self)
{
    Q_UNUSED(pad);

    if (GST_EVENT_TYPE(event) == GST_EVENT_TAG) {
        GstTagList *gstTags;
        gst_event_parse_tag(event, &gstTags);
        QMap<QByteArray, QVariant> extendedTags = QGstUtils::gstTagListToMap(gstTags);

#ifdef DEBUG_CAPTURE
        qDebug() << QString(gst_structure_to_string(gst_event_get_structure(event))).right(768);
        qDebug() << "Capture event probe" << extendedTags;
#endif

        QVariantMap tags;
        tags[QMediaMetaData::ISOSpeedRatings] = extendedTags.value("capturing-iso-speed");
        tags[QMediaMetaData::DigitalZoomRatio] = extendedTags.value("capturing-digital-zoom-ratio");
        tags[QMediaMetaData::ExposureTime] = extendedTags.value("capturing-shutter-speed");
        tags[QMediaMetaData::WhiteBalance] = extendedTags.value("capturing-white-balance");
        tags[QMediaMetaData::Flash] = extendedTags.value("capturing-flash-fired");
        tags[QMediaMetaData::FocalLengthIn35mmFilm] = extendedTags.value("capturing-focal-length");
        tags[QMediaMetaData::MeteringMode] = extendedTags.value("capturing-metering-mode");
        tags[QMediaMetaData::ExposureMode] = extendedTags.value("capturing-exposure-mode");
        tags[QMediaMetaData::FNumber] = extendedTags.value("capturing-focal-ratio");
        tags[QMediaMetaData::ExposureMode] = extendedTags.value("capturing-exposure-mode");

        QMapIterator<QString, QVariant> i(tags);
        while (i.hasNext()) {
            i.next();
            if (i.value().isValid()) {
                QMetaObject::invokeMethod(self, "imageMetadataAvailable",
                                          Qt::QueuedConnection,
                                          Q_ARG(int, self->m_requestId),
                                          Q_ARG(QString, i.key()),
                                          Q_ARG(QVariant, i.value()));
            }
        }
    }

    return true;
}

gboolean CameraBinImageCapture::uncompressedBufferProbe(GstPad *pad, GstBuffer *buffer, CameraBinImageCapture *self)
{
    Q_UNUSED(pad);
    CameraBinSession *session = self->m_session;

#ifdef DEBUG_CAPTURE
    qDebug() << "Uncompressed buffer probe" << gst_caps_to_string(GST_BUFFER_CAPS(buffer));
#endif

    QCameraImageCapture::CaptureDestinations destination =
            session->captureDestinationControl()->captureDestination();
    QVideoFrame::PixelFormat format = session->captureBufferFormatControl()->bufferFormat();

    if (destination & QCameraImageCapture::CaptureToBuffer) {
        if (format != QVideoFrame::Format_Jpeg) {
            GstCaps *caps = GST_BUFFER_CAPS(buffer);
            int bytesPerLine = -1;
            QVideoSurfaceFormat format = QVideoSurfaceGstSink::formatForCaps(caps, &bytesPerLine);
#ifdef DEBUG_CAPTURE
            qDebug() << "imageAvailable(uncompressed):" << format;
#endif
            QGstVideoBuffer *videoBuffer = new QGstVideoBuffer(buffer, bytesPerLine);

            QVideoFrame frame(videoBuffer,
                              format.frameSize(),
                              format.pixelFormat());

            QMetaObject::invokeMethod(self, "imageAvailable",
                                      Qt::QueuedConnection,
                                      Q_ARG(int, self->m_requestId),
                                      Q_ARG(QVideoFrame, frame));
        }
    }

    //keep the buffer if capture to file or jpeg buffer capture was reuqsted
    bool keepBuffer = (destination & QCameraImageCapture::CaptureToFile) ||
            ((destination & QCameraImageCapture::CaptureToBuffer) &&
              format == QVideoFrame::Format_Jpeg);

    return keepBuffer;
}

gboolean CameraBinImageCapture::jpegBufferProbe(GstPad *pad, GstBuffer *buffer, CameraBinImageCapture *self)
{
    Q_UNUSED(pad);
    CameraBinSession *session = self->m_session;

#ifdef DEBUG_CAPTURE
    qDebug() << "Jpeg buffer probe" << gst_caps_to_string(GST_BUFFER_CAPS(buffer));
#endif

    QCameraImageCapture::CaptureDestinations destination =
            session->captureDestinationControl()->captureDestination();

    if ((destination & QCameraImageCapture::CaptureToBuffer) &&
         session->captureBufferFormatControl()->bufferFormat() == QVideoFrame::Format_Jpeg) {
        QGstVideoBuffer *videoBuffer = new QGstVideoBuffer(buffer,
                                                           -1); //bytesPerLine is not available for jpegs

        QSize resolution = QGstUtils::capsCorrectedResolution(GST_BUFFER_CAPS(buffer));
        //if resolution is not presented in caps, try to find it from encoded jpeg data:
        if (resolution.isEmpty()) {
            QBuffer data;
            data.setData(reinterpret_cast<const char*>(GST_BUFFER_DATA(buffer)), GST_BUFFER_SIZE(buffer));
            QImageReader reader(&data, "JPEG");
            resolution = reader.size();
        }

        QVideoFrame frame(videoBuffer,
                          resolution,
                          QVideoFrame::Format_Jpeg);

        QMetaObject::invokeMethod(self, "imageAvailable",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, self->m_requestId),
                                  Q_ARG(QVideoFrame, frame));
    }

    //drop the buffer if capture to file was disabled
    return destination & QCameraImageCapture::CaptureToFile;
}

bool CameraBinImageCapture::processBusMessage(const QGstreamerMessage &message)
{
    //Install metadata event and buffer probes

    //The image capture pipiline is built dynamically,
    //it's necessary to wait until jpeg encoder is added to pipeline

    GstMessage *gm = message.rawMessage();
    if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_STATE_CHANGED) {
        GstState    oldState;
        GstState    newState;
        GstState    pending;
        gst_message_parse_state_changed(gm, &oldState, &newState, &pending);

        if (newState == GST_STATE_READY) {
            GstElement *element = GST_ELEMENT(GST_MESSAGE_SRC(gm));
            if (!element)
                return false;

            QString elementName = QString::fromLatin1(gst_element_get_name(element));
            GstElementClass *elementClass = GST_ELEMENT_GET_CLASS(element);
            QString elementLongName = elementClass->details.longname;

            if (elementName.contains("jpegenc") && element != m_jpegEncoderElement) {
                m_jpegEncoderElement = element;
                GstPad *sinkpad = gst_element_get_static_pad(element, "sink");

                //metadata event probe is installed before jpeg encoder
                //to emit metadata available signal as soon as possible.
#ifdef DEBUG_CAPTURE
                qDebug() << "install metadata probe";
#endif
                gst_pad_add_event_probe(sinkpad,
                                        G_CALLBACK(CameraBinImageCapture::metadataEventProbe),
                                        this);

#ifdef DEBUG_CAPTURE
                qDebug() << "install uncompressed buffer probe";
#endif
                gst_pad_add_buffer_probe(sinkpad,
                                         G_CALLBACK(CameraBinImageCapture::uncompressedBufferProbe),
                                         this);

                gst_object_unref(sinkpad);
            } else if ((elementName.contains("jifmux") ||
                        elementName.startsWith("metadatamux") ||
                        elementLongName == QLatin1String("JPEG stream muxer"))
                       && element != m_metadataMuxerElement) {
                //Jpeg encoded buffer probe is added after jifmux/metadatamux
                //element to ensure the resulting jpeg buffer contains capture metadata
                m_metadataMuxerElement = element;

                GstPad *srcpad = gst_element_get_static_pad(element, "src");
#ifdef DEBUG_CAPTURE
                qDebug() << "install jpeg buffer probe";
#endif
                gst_pad_add_buffer_probe(srcpad,
                                         G_CALLBACK(CameraBinImageCapture::jpegBufferProbe),
                                         this);
                gst_object_unref(srcpad);
            }
        }
    } else if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_ELEMENT) {
        if (GST_MESSAGE_SRC(gm) == (GstObject *)m_session->cameraBin()) {
            const GstStructure *structure = gst_message_get_structure(gm);

            if (gst_structure_has_name (structure, "image-done")) {
                const gchar *fileName = gst_structure_get_string (structure, "filename");
#ifdef DEBUG_CAPTURE
                qDebug() << "Image saved" << fileName;
#endif

                if (m_session->captureDestinationControl()->captureDestination() & QCameraImageCapture::CaptureToFile) {
                    emit imageSaved(m_requestId, QString::fromUtf8(fileName));
                } else {
#ifdef DEBUG_CAPTURE
                    qDebug() << Q_FUNC_INFO << "Dropped saving file" << fileName;
#endif
                    //camerabin creates an empty file when captured buffer is dropped,
                    //let's remove it
                    QFileInfo info(QString::fromUtf8(fileName));
                    if (info.exists() && info.isFile() && info.size() == 0) {
                        QFile(info.absoluteFilePath()).remove();
                    }
                }
            }
        }
    }

    return false;
}

QT_END_NAMESPACE
