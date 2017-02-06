/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#ifndef QWINRTCAMERACONTROL_H
#define QWINRTCAMERACONTROL_H

#include <QtMultimedia/QCameraControl>
#include <QtCore/QLoggingCategory>
#include <QtCore/qt_windows.h>

#include <wrl.h>

namespace ABI {
    namespace Windows {
        namespace Media {
            namespace Capture {
                struct IMediaCapture;
                struct IMediaCaptureFailedEventArgs;
            }
        }
        namespace Foundation {
            struct IAsyncAction;
            enum class AsyncStatus;
        }
    }
}

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcMMCamera)

class QVideoRendererControl;
class QVideoDeviceSelectorControl;
class QCameraImageCaptureControl;
class QImageEncoderControl;
class QCameraFlashControl;
class QCameraFocusControl;
class QCameraLocksControl;

class QWinRTCameraControlPrivate;
class QWinRTCameraControl : public QCameraControl
{
    Q_OBJECT
public:
    explicit QWinRTCameraControl(QObject *parent = 0);
    ~QWinRTCameraControl();

    QCamera::State state() const Q_DECL_OVERRIDE;
    void setState(QCamera::State state) Q_DECL_OVERRIDE;

    QCamera::Status status() const Q_DECL_OVERRIDE;

    QCamera::CaptureModes captureMode() const Q_DECL_OVERRIDE;
    void setCaptureMode(QCamera::CaptureModes mode) Q_DECL_OVERRIDE;
    bool isCaptureModeSupported(QCamera::CaptureModes mode) const Q_DECL_OVERRIDE;

    bool canChangeProperty(PropertyChangeType changeType, QCamera::Status status) const Q_DECL_OVERRIDE;

    QVideoRendererControl *videoRenderer() const;
    QVideoDeviceSelectorControl *videoDeviceSelector() const;
    QCameraImageCaptureControl *imageCaptureControl() const;
    QImageEncoderControl *imageEncoderControl() const;
    QCameraFlashControl *cameraFlashControl() const;
    QCameraFocusControl *cameraFocusControl() const;
    QCameraLocksControl *cameraLocksControl() const;

    Microsoft::WRL::ComPtr<ABI::Windows::Media::Capture::IMediaCapture> handle() const;

    bool setFocus(QCameraFocus::FocusModes mode);
    bool setFocusPoint(const QPointF &point);
    bool focus();
    void clearFocusPoint();
    void emitError(int errorCode, const QString &errorString);
    bool lockFocus();
    bool unlockFocus();
    void frameMapped();
    void frameUnmapped();

private slots:
    void onBufferRequested();
    void onApplicationStateChanged(Qt::ApplicationState state);

private:
    HRESULT enumerateDevices();
    HRESULT initialize();
    HRESULT initializeFocus();
    HRESULT onCaptureFailed(ABI::Windows::Media::Capture::IMediaCapture *,
                            ABI::Windows::Media::Capture::IMediaCaptureFailedEventArgs *);
    HRESULT onRecordLimitationExceeded(ABI::Windows::Media::Capture::IMediaCapture *);

    QScopedPointer<QWinRTCameraControlPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QWinRTCameraControl)
};

QT_END_NAMESPACE

#endif // QWINRTCAMERACONTROL_H
