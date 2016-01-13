/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwinrtcameraimagecapturecontrol.h"
#include "qwinrtcameracontrol.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QGlobalStatic>
#include <QtCore/QPointer>
#include <QtCore/QStandardPaths>
#include <QtCore/QVector>
#include <QtCore/qfunctions_winrt.h>
#include <QtMultimedia/private/qmediastoragelocation_p.h>

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

QT_USE_NAMESPACE

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
    Q_D(QWinRTCameraImageCaptureControl);

    ++d->currentCaptureId;
    IMediaCapture *capture = d->cameraControl->handle();
    if (!capture) {
        emit error(d->currentCaptureId, QCameraImageCapture::NotReadyError, tr("Camera not ready"));
        return -1;
    }

    CaptureRequest request = {
        d->currentCaptureId,
        d->location.generateFileName(fileName, QMediaStorageLocation::Pictures, QStringLiteral("IMG_"),
                                     fileName.isEmpty() ? QStringLiteral("jpg") : QFileInfo(fileName).suffix())
    };

    HRESULT hr;
    hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Storage_Streams_InMemoryRandomAccessStream).Get(),
                            &request.stream);
    Q_ASSERT_SUCCEEDED(hr);

    hr = g->encodingPropertiesFactory->CreateBmp(&request.imageFormat);
    Q_ASSERT_SUCCEEDED(hr);

    const QSize imageSize = d->cameraControl->imageSize();
    hr = request.imageFormat->put_Width(imageSize.width());
    Q_ASSERT_SUCCEEDED(hr);
    hr = request.imageFormat->put_Height(imageSize.height());
    Q_ASSERT_SUCCEEDED(hr);

    hr = capture->CapturePhotoToStreamAsync(request.imageFormat.Get(), request.stream.Get(), &request.op);
    Q_ASSERT_SUCCEEDED(hr);
    d->requests.insert(request.op.Get(), request);

    hr = request.op->put_Completed(Callback<IAsyncActionCompletedHandler>(
                                       this, &QWinRTCameraImageCaptureControl::onCaptureCompleted).Get());
    Q_ASSERT_SUCCEEDED(hr);

    return request.id;
}

void QWinRTCameraImageCaptureControl::cancelCapture()
{
    Q_D(QWinRTCameraImageCaptureControl);

    QHash<IAsyncAction *, CaptureRequest>::iterator it = d->requests.begin();
    while (it != d->requests.end()) {
        ComPtr<IAsyncInfo> info;
        it->op.As(&info);
        info->Cancel();
        it = d->requests.erase(it);
    }
}

HRESULT QWinRTCameraImageCaptureControl::onCaptureCompleted(IAsyncAction *asyncInfo, AsyncStatus status)
{
    Q_D(QWinRTCameraImageCaptureControl);

    if (status == Canceled || !d->requests.contains(asyncInfo))
        return S_OK;

    CaptureRequest request = d->requests.take(asyncInfo);

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
    if (image.save(request.fileName))
        emit imageSaved(request.id, request.fileName);
    else
        emit error(request.id, QCameraImageCapture::ResourceError, tr("Image saving failed"));

    return S_OK;
}
