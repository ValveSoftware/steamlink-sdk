/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QDECLARATIVECAMERACAPTURE_H
#define QDECLARATIVECAMERACAPTURE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <qcamera.h>
#include <qcameraimagecapture.h>
#include <qmediaencodersettings.h>

#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QDeclarativeCamera;
class QMetaDataWriterControl;

class QDeclarativeCameraCapture : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ isReadyForCapture NOTIFY readyForCaptureChanged)
    Q_PROPERTY(QString capturedImagePath READ capturedImagePath NOTIFY imageSaved)
    Q_PROPERTY(QSize resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY captureFailed)
    Q_PROPERTY(QVariantList supportedResolutions READ supportedResolutions NOTIFY supportedResolutionsChanged REVISION 1)

public:
    ~QDeclarativeCameraCapture();

    bool isReadyForCapture() const;

    QSize resolution();

    QString capturedImagePath() const;
    QCameraImageCapture::Error error() const;
    QString errorString() const;
    QVariantList supportedResolutions();

public Q_SLOTS:
    int capture();
    int captureToLocation(const QString &location);
    void cancelCapture();

    void setResolution(const QSize &resolution);
    void setMetadata(const QString &key, const QVariant &value);

Q_SIGNALS:
    void readyForCaptureChanged(bool);

    void imageExposed(int requestId);
    void imageCaptured(int requestId, const QString &preview);
    void imageMetadataAvailable(int requestId, const QString &key, const QVariant &value);
    void imageSaved(int requestId, const QString &path);
    void captureFailed(int requestId, const QString &message);

    void resolutionChanged(const QSize &);
    void supportedResolutionsChanged();

private slots:
    void _q_imageCaptured(int, const QImage&);
    void _q_imageSaved(int, const QString&);
    void _q_imageMetadataAvailable(int, const QString &, const QVariant &);
    void _q_captureFailed(int, QCameraImageCapture::Error, const QString&);
    void _q_cameraStatusChanged(QCamera::Status status);

private:
    friend class QDeclarativeCamera;
    QDeclarativeCameraCapture(QCamera *camera, QObject *parent = 0);

    QCamera *m_camera;
    QCameraImageCapture *m_capture;
    QImageEncoderSettings m_imageSettings;
    QString m_capturedImagePath;
    QMetaDataWriterControl *m_metadataWriterControl;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QT_PREPEND_NAMESPACE(QDeclarativeCameraCapture))

#endif
