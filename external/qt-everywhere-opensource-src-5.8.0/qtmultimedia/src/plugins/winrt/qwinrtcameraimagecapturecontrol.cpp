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

#include "qwinrtcameraimagecapturecontrol.h"
#include "qwinrtcameracontrol.h"
#include "qwinrtimageencodercontrol.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QGlobalStatic>
#include <QtCore/QPointer>
#include <QtCore/QStandardPaths>
#include <QtCore/QVector>
#include <QtCore/qfunctions_winrt.h>
#include <QtCore/private/qeventdispatcher_winrt_p.h>
#include <QtMultimedia/private/qmediastoragelocation_p.h>

#include <functional>
#include <wrl.h>
#include <windows.media.capture.h>
#include <windows.media.devices.h>
#include <windows.media.mediaproperties.h>
#include <windows.storage.streams.h>
#include <windows.graphics.imaging.h>
#include <robuffer.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Media::Capture;
using namespace ABI::Windows::Media::Devices;
using namespace ABI::Windows::Media::MediaProperties;
using namespace ABI::Windows::Storage::Streams;
using namespace ABI::Windows::Graphics::Imaging;

QT_BEGIN_NAMESPACE

#define wchar(str) reinterpret_cast<const wchar_t *>(str.utf16())

struct QWinRTCameraImageCaptureControlGlobal
{
    QWinRTCameraImageCaptureControlGlobal()
    {
        HRESULT hr;
        hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Media_MediaProperties_ImageEncodingProperties).Get(),
                                  &encodingPropertiesFactory);
        Q_ASSERT_SUCCEEDED(hr);

        hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Storage_Streams_Buffer).Get(),
                                  &bufferFactory);
        Q_ASSERT_SUCCEEDED(hr);

        hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Storage_Streams_DataReader).Get(),
                                  &dataReaderFactory);
    }

    ComPtr<IImageEncodingPropertiesStatics2> encodingPropertiesFactory;
    ComPtr<IBufferFactory> bufferFactory;
    ComPtr<IDataReaderFactory> dataReaderFactory;
};
Q_GLOBAL_STATIC(QWinRTCameraImageCaptureControlGlobal, g)

struct CaptureRequest
{
    quint16 id;
    QString fileName;
    ComPtr<IImageEncodingProperties> imageFormat;
    ComPtr<IRandomAccessStream> stream;
    ComPtr<IAsyncAction> op;
};

class QWinRTCameraImageCaptureControlPrivate
{
public:
    QPointer<QWinRTCameraControl> cameraControl;
    QHash<IAsyncAction *, CaptureRequest> requests;
    quint16 currentCaptureId;
    QMediaStorageLocation location;

    void onCameraStateChanged()
    {

    }
};

QWinRTCameraImageCaptureControl::QWinRTCameraImageCaptureControl(QWinRTCameraControl *parent)
    : QCameraImageCaptureControl(parent), d_ptr(new QWinRTCameraImageCaptureControlPrivate)
{
    qCDebug(lcMMCamera) << __FUNCTION__ << parent;
    Q_D(QWinRTCameraImageCaptureControl);

    d->cameraControl = parent;
    connect(d->cameraControl, &QCameraControl::stateChanged,
            this, &QWinRTCameraImageCaptureControl::readyForCaptureChanged);
    d->currentCaptureId = 0;
}

bool QWinRTCameraImageCaptureControl::isReadyForCapture() const
{
    Q_D(const QWinRTCameraImageCaptureControl);
    return d->cameraControl->state() == QCamera::ActiveState;
}

QCameraImageCapture::DriveMode QWinRTCameraImageCaptureControl::driveMode() const
{
    return QCameraImageCapture::SingleImageCapture;
}

void QWinRTCameraImageCaptureControl::setDriveMode(QCameraImageCapture::DriveMode mode)
{
    Q_UNUSED(mode);
}

int QWinRTCameraImageCaptureControl::capture(const QString &fileName)
{
    qCDebug(lcMMCamera) << __FUNCTION__ << fileName;
    Q_D(QWinRTCameraImageCaptureControl);

    ++d->currentCaptureId;
    ComPtr<IMediaCapture> capture = d->cameraControl->handle();
    if (!capture) {
        emit error(d->currentCaptureId, QCameraImageCapture::NotReadyError, tr("Camera not ready"));
        return -1;
    }

    CaptureRequest request = {
        d->currentCaptureId,
        d->location.generateFileName(fileName, QMediaStorageLocation::Pictures, QStringLiteral("IMG_"),
                                     fileName.isEmpty() ? QStringLiteral("jpg") : QFileInfo(fileName).suffix())
    };

    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([this, d, capture, &request]() {
        HRESULT hr;
        hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Storage_Streams_InMemoryRandomAccessStream).Get(),
                                &request.stream);
        Q_ASSERT_SUCCEEDED(hr);

        hr = g->encodingPropertiesFactory->CreateBmp(&request.imageFormat);
        Q_ASSERT_SUCCEEDED(hr);

        const QSize imageSize = static_cast<QWinRTImageEncoderControl*>(d->cameraControl->imageEncoderControl())->imageSettings().resolution();
        hr = request.imageFormat->put_Width(imageSize.width());
        Q_ASSERT_SUCCEEDED(hr);
        hr = request.imageFormat->put_Height(imageSize.height());
        Q_ASSERT_SUCCEEDED(hr);

        hr = capture->CapturePhotoToStreamAsync(request.imageFormat.Get(), request.stream.Get(), &request.op);
        Q_ASSERT_SUCCEEDED(hr);
        if (!request.op) {
            qErrnoWarning("Camera photo capture failed.");
            return E_FAIL;
        }
        emit captureQueueChanged(false);
        d->requests.insert(request.op.Get(), request);

        hr = request.op->put_Completed(Callback<IAsyncActionCompletedHandler>(
                                           this, &QWinRTCameraImageCaptureControl::onCaptureCompleted).Get());
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
    if (FAILED(hr))
        return -1;
    return request.id;
}

void QWinRTCameraImageCaptureControl::cancelCapture()
{
    qCDebug(lcMMCamera) << __FUNCTION__;
    Q_D(QWinRTCameraImageCaptureControl);

    QHash<IAsyncAction *, CaptureRequest>::iterator it = d->requests.begin();
    while (it != d->requests.end()) {
        ComPtr<IAsyncInfo> info;
        it->op.As(&info);
        info->Cancel();
        it = d->requests.erase(it);
    }
    emit captureQueueChanged(true);
}

HRESULT QWinRTCameraImageCaptureControl::onCaptureCompleted(IAsyncAction *asyncInfo, AsyncStatus status)
{
    qCDebug(lcMMCamera) << __FUNCTION__;
    Q_D(QWinRTCameraImageCaptureControl);

    if (status == Canceled || !d->requests.contains(asyncInfo))
        return S_OK;

    CaptureRequest request = d->requests.take(asyncInfo);
    emit captureQueueChanged(d->requests.isEmpty());
    HRESULT hr;
    if (status == Error) {
        hr = asyncInfo->GetResults();
        emit error(request.id, QCameraImageCapture::ResourceError, qt_error_string(hr));
        return S_OK;
    }

    quint64 dataLength;
    hr = request.stream->get_Size(&dataLength);
    Q_ASSERT_SUCCEEDED(hr);
    if (dataLength == 0 || dataLength > INT_MAX) {
        emit error(request.id, QCameraImageCapture::FormatError, tr("Invalid photo data length."));
        return S_OK;
    }

    ComPtr<IBitmapDecoderStatics> bitmapFactory;
    hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Graphics_Imaging_BitmapDecoder).Get(),
                              &bitmapFactory);
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IAsyncOperation<BitmapDecoder *>> op;
    hr = bitmapFactory->CreateAsync(request.stream.Get(), &op);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IBitmapDecoder> decoder;
    hr = QWinRTFunctions::await(op, decoder.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IAsyncOperation<BitmapFrame *>> op2;
    hr = decoder->GetFrameAsync(0, &op2);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IBitmapFrame> frame;
    hr = QWinRTFunctions::await(op2, frame.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IBitmapTransform> transform;
    hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Graphics_Imaging_BitmapTransform).Get(),
                            &transform);
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IAsyncOperation<PixelDataProvider *>> op3;
    hr = frame->GetPixelDataTransformedAsync(BitmapPixelFormat_Rgba8, BitmapAlphaMode_Straight,
                                             transform.Get(), ExifOrientationMode_IgnoreExifOrientation,
                                             ColorManagementMode_DoNotColorManage, &op3);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IPixelDataProvider> pixelDataProvider;
    hr = QWinRTFunctions::await(op3, pixelDataProvider.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);

    UINT32 pixelDataSize;
    BYTE *pixelData;
    hr = pixelDataProvider->DetachPixelData(&pixelDataSize, &pixelData);

    UINT32 pixelHeight;
    hr = frame->get_PixelHeight(&pixelHeight);
    Q_ASSERT_SUCCEEDED(hr);
    UINT32 pixelWidth;
    hr = frame->get_PixelWidth(&pixelWidth);
    Q_ASSERT_SUCCEEDED(hr);
    const QImage image(pixelData, pixelWidth, pixelHeight, QImage::Format_RGBA8888,
                       reinterpret_cast<QImageCleanupFunction>(&CoTaskMemFree), pixelData);
    emit imageCaptured(request.id, image);

    QWinRTImageEncoderControl *imageEncoderControl = static_cast<QWinRTImageEncoderControl*>(d->cameraControl->imageEncoderControl());
    int imageQuality = 100;
    switch (imageEncoderControl->imageSettings().quality()) {
    case QMultimedia::VeryLowQuality:
        imageQuality = 20;
        break;
    case QMultimedia::LowQuality:
        imageQuality = 40;
        break;
    case QMultimedia::NormalQuality:
        imageQuality = 60;
        break;
    case QMultimedia::HighQuality:
        imageQuality = 80;
        break;
    case QMultimedia::VeryHighQuality:
        imageQuality = 100;
        break;
    }

    if (image.save(request.fileName, imageEncoderControl->imageSettings().codec().toLatin1().data(), imageQuality))
        emit imageSaved(request.id, request.fileName);
    else
        emit error(request.id, QCameraImageCapture::ResourceError, tr("Image saving failed"));

    return S_OK;
}

QT_END_NAMESPACE
