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

#include "avfaudioinputselectorcontrol.h"
#include "avfmediarecordercontrol_ios.h"
#include "avfcamerarenderercontrol.h"
#include "avfmediaassetwriter.h"
#include "avfcameraservice.h"
#include "avfcamerasession.h"
#include "avfcameradebug.h"
#include "avfmediacontainercontrol.h"

#include <QtCore/qmetaobject.h>
#include <QtCore/qsysinfo.h>

QT_USE_NAMESPACE

namespace {

bool qt_camera_service_isValid(AVFCameraService *service)
{
    if (!service || !service->session())
        return false;

    AVFCameraSession *session = service->session();
    if (!session->captureSession())
        return false;

    if (!session->videoInput())
        return false;

    if (!service->videoOutput()
        || !service->videoOutput()->videoDataOutput()) {
        return false;
    }

    return true;
}

enum WriterState
{
    WriterStateIdle,
    WriterStateActive,
    WriterStateAborted
};

} // unnamed namespace

@interface QT_MANGLE_NAMESPACE(AVFMediaAssetWriter) (PrivateAPI)
- (bool)addAudioCapture;
- (bool)addWriterInputs;
- (void)setQueues;
- (void)updateDuration:(CMTime)newTimeStamp;
@end

@implementation QT_MANGLE_NAMESPACE(AVFMediaAssetWriter)

- (id)initWithDelegate:(AVFMediaRecorderControlIOS *)delegate
{
    Q_ASSERT(delegate);

    if (self = [super init]) {
        m_delegate = delegate;
        m_setStartTime = true;
        m_state.store(WriterStateIdle);
        m_startTime = kCMTimeInvalid;
        m_lastTimeStamp = kCMTimeInvalid;
        m_durationInMs.store(0);
        m_audioSettings = nil;
        m_videoSettings = nil;
    }

    return self;
}

- (bool)setupWithFileURL:(NSURL *)fileURL
        cameraService:(AVFCameraService *)service
        audioSettings:(NSDictionary *)audioSettings
        videoSettings:(NSDictionary *)videoSettings
        transform:(CGAffineTransform)transform
{
    Q_ASSERT(fileURL);

    if (!qt_camera_service_isValid(service)) {
        qDebugCamera() << Q_FUNC_INFO << "invalid camera service";
        return false;
    }

    m_service = service;
    m_audioSettings = audioSettings;
    m_videoSettings = videoSettings;

    m_writerQueue.reset(dispatch_queue_create("asset-writer-queue", DISPATCH_QUEUE_SERIAL));
    if (!m_writerQueue) {
        qDebugCamera() << Q_FUNC_INFO << "failed to create an asset writer's queue";
        return false;
    }

    m_videoQueue.reset(dispatch_queue_create("video-output-queue", DISPATCH_QUEUE_SERIAL));
    if (!m_videoQueue) {
        qDebugCamera() << Q_FUNC_INFO << "failed to create video queue";
        return false;
    }
    dispatch_set_target_queue(m_videoQueue, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
    m_audioQueue.reset(dispatch_queue_create("audio-output-queue", DISPATCH_QUEUE_SERIAL));
    if (!m_audioQueue) {
        qDebugCamera() << Q_FUNC_INFO << "failed to create audio queue";
        // But we still can write video!
    }

    m_assetWriter.reset([[AVAssetWriter alloc] initWithURL:fileURL
                                               fileType:m_service->mediaContainerControl()->fileType()
                                               error:nil]);
    if (!m_assetWriter) {
        qDebugCamera() << Q_FUNC_INFO << "failed to create asset writer";
        return false;
    }

    bool audioCaptureOn = false;

    if (m_audioQueue)
        audioCaptureOn = [self addAudioCapture];

    if (![self addWriterInputs]) {
        if (audioCaptureOn) {
            AVCaptureSession *session = m_service->session()->captureSession();
            [session removeOutput:m_audioOutput];
            [session removeInput:m_audioInput];
            m_audioOutput.reset();
            m_audioInput.reset();
            m_audioCaptureDevice = 0;
        }
        m_assetWriter.reset();
        return false;
    }

    m_cameraWriterInput.data().transform = transform;

    // Ready to start ...
    return true;
}

- (void)start
{
    [self setQueues];

    m_setStartTime = true;

    m_state.storeRelease(WriterStateActive);

    [m_assetWriter startWriting];
    AVCaptureSession *session = m_service->session()->captureSession();
    if (!session.running)
        [session startRunning];
}

- (void)stop
{
    if (m_state.loadAcquire() != WriterStateActive)
        return;

    if ([m_assetWriter status] != AVAssetWriterStatusWriting)
        return;

    // Do this here so that -
    // 1. '-abort' should not try calling finishWriting again and
    // 2. async block (see below) will know if recorder control was deleted
    //    before the block's execution:
    m_state.storeRelease(WriterStateIdle);
    // Now, since we have to ensure no sample buffers are
    // appended after a call to finishWriting, we must
    // ensure writer's queue sees this change in m_state
    // _before_ we call finishWriting:
    dispatch_sync(m_writerQueue, ^{});
    // Done, but now we also want to prevent video queue
    // from updating our viewfinder:
    dispatch_sync(m_videoQueue, ^{});

    // Now we're safe to stop:
    [m_assetWriter finishWritingWithCompletionHandler:^{
        // This block is async, so by the time it's executed,
        // it's possible that render control was deleted already ...
        if (m_state.loadAcquire() == WriterStateAborted)
            return;

        AVCaptureSession *session = m_service->session()->captureSession();
        if (session.running)
            [session stopRunning];
        [session removeOutput:m_audioOutput];
        [session removeInput:m_audioInput];
        QMetaObject::invokeMethod(m_delegate, "assetWriterFinished", Qt::QueuedConnection);
    }];
}

- (void)abort
{
    // -abort is to be called from recorder control's dtor.

    if (m_state.fetchAndStoreRelease(WriterStateAborted) != WriterStateActive) {
        // Not recording, nothing to stop.
        return;
    }

    // From Apple's docs:
    // "To guarantee that all sample buffers are successfully written,
    //  you must ensure that all calls to appendSampleBuffer: and
    //  appendPixelBuffer:withPresentationTime: have returned before
    //  invoking this method."
    //
    // The only way we can ensure this is:
    dispatch_sync(m_writerQueue, ^{});
    // At this point next block (if any) on the writer's queue
    // will see m_state preventing it from any further processing.
    dispatch_sync(m_videoQueue, ^{});
    // After this point video queue will not try to modify our
    // viewfider, so we're safe to delete now.

    [m_assetWriter finishWritingWithCompletionHandler:^{
    }];
}

- (void)setStartTimeFrom:(CMSampleBufferRef)sampleBuffer
{
    // Writer's queue only.
    Q_ASSERT(m_setStartTime);
    Q_ASSERT(sampleBuffer);

    if (m_state.loadAcquire() != WriterStateActive)
        return;

    QMetaObject::invokeMethod(m_delegate, "assetWriterStarted", Qt::QueuedConnection);

    m_durationInMs.storeRelease(0);
    m_startTime = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    m_lastTimeStamp = m_startTime;
    [m_assetWriter startSessionAtSourceTime:m_startTime];
    m_setStartTime = false;
}

- (void)writeVideoSampleBuffer:(CMSampleBufferRef)sampleBuffer
{
    // This code is executed only on a writer's queue.
    Q_ASSERT(sampleBuffer);

    if (m_state.loadAcquire() == WriterStateActive) {
        if (m_setStartTime)
            [self setStartTimeFrom:sampleBuffer];

        if (m_cameraWriterInput.data().readyForMoreMediaData) {
            [self updateDuration:CMSampleBufferGetPresentationTimeStamp(sampleBuffer)];
            [m_cameraWriterInput appendSampleBuffer:sampleBuffer];
        }
    }

    CFRelease(sampleBuffer);
}

- (void)writeAudioSampleBuffer:(CMSampleBufferRef)sampleBuffer
{
    Q_ASSERT(sampleBuffer);

    // This code is executed only on a writer's queue.
    if (m_state.loadAcquire() == WriterStateActive) {
        if (m_setStartTime)
            [self setStartTimeFrom:sampleBuffer];

        if (m_audioWriterInput.data().readyForMoreMediaData) {
            [self updateDuration:CMSampleBufferGetPresentationTimeStamp(sampleBuffer)];
            [m_audioWriterInput appendSampleBuffer:sampleBuffer];
        }
    }

    CFRelease(sampleBuffer);
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
        didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
        fromConnection:(AVCaptureConnection *)connection
{
    Q_UNUSED(connection)

    if (m_state.loadAcquire() != WriterStateActive)
        return;

    if (!CMSampleBufferDataIsReady(sampleBuffer)) {
        qDebugCamera() << Q_FUNC_INFO << "sample buffer is not ready, skipping.";
        return;
    }

    CFRetain(sampleBuffer);

    if (captureOutput != m_audioOutput.data()) {
        if (m_state.load() != WriterStateActive) {
            CFRelease(sampleBuffer);
            return;
        }
        // Find renderercontrol's delegate and invoke its method to
        // show updated viewfinder's frame.
        if (m_service && m_service->videoOutput()) {
            NSObject<AVCaptureVideoDataOutputSampleBufferDelegate> *vfDelegate =
                (NSObject<AVCaptureVideoDataOutputSampleBufferDelegate> *)m_service->videoOutput()->captureDelegate();
            if (vfDelegate)
                [vfDelegate captureOutput:nil didOutputSampleBuffer:sampleBuffer fromConnection:nil];
        }

        dispatch_async(m_writerQueue, ^{
            [self writeVideoSampleBuffer:sampleBuffer];
        });
    } else {
        dispatch_async(m_writerQueue, ^{
            [self writeAudioSampleBuffer:sampleBuffer];
        });
    }
}

- (bool)addAudioCapture
{
    Q_ASSERT(m_service && m_service->session() && m_service->session()->captureSession());

    if (!m_service->audioInputSelectorControl())
        return false;

    AVCaptureSession *captureSession = m_service->session()->captureSession();

    m_audioCaptureDevice = m_service->audioInputSelectorControl()->createCaptureDevice();
    if (!m_audioCaptureDevice) {
        qWarning() << Q_FUNC_INFO << "no audio input device available";
        return false;
    } else {
        NSError *error = nil;
        m_audioInput.reset([[AVCaptureDeviceInput deviceInputWithDevice:m_audioCaptureDevice error:&error] retain]);

        if (!m_audioInput || error) {
            qWarning() << Q_FUNC_INFO << "failed to create audio device input";
            m_audioCaptureDevice = 0;
            m_audioInput.reset();
            return false;
        } else if (![captureSession canAddInput:m_audioInput]) {
            qWarning() << Q_FUNC_INFO << "could not connect the audio input";
            m_audioCaptureDevice = 0;
            m_audioInput.reset();
            return false;
        } else {
            [captureSession addInput:m_audioInput];
        }
    }


    m_audioOutput.reset([[AVCaptureAudioDataOutput alloc] init]);
    if (m_audioOutput && [captureSession canAddOutput:m_audioOutput]) {
        [captureSession addOutput:m_audioOutput];
    } else {
        qDebugCamera() << Q_FUNC_INFO << "failed to add audio output";
        [captureSession removeInput:m_audioInput];
        m_audioCaptureDevice = 0;
        m_audioInput.reset();
        m_audioOutput.reset();
        return false;
    }

    return true;
}

- (bool)addWriterInputs
{
    Q_ASSERT(m_service && m_service->videoOutput()
             && m_service->videoOutput()->videoDataOutput());
    Q_ASSERT(m_assetWriter);

    m_cameraWriterInput.reset([[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo
                                                          outputSettings:m_videoSettings
                                                          sourceFormatHint:m_service->session()->videoCaptureDevice().activeFormat.formatDescription]);
    if (!m_cameraWriterInput) {
        qDebugCamera() << Q_FUNC_INFO << "failed to create camera writer input";
        return false;
    }

    if ([m_assetWriter canAddInput:m_cameraWriterInput]) {
        [m_assetWriter addInput:m_cameraWriterInput];
    } else {
        qDebugCamera() << Q_FUNC_INFO << "failed to add camera writer input";
        m_cameraWriterInput.reset();
        return false;
    }

    m_cameraWriterInput.data().expectsMediaDataInRealTime = YES;

    if (m_audioOutput) {
        CMFormatDescriptionRef sourceFormat = m_audioCaptureDevice ? m_audioCaptureDevice.activeFormat.formatDescription : 0;
        m_audioWriterInput.reset([[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeAudio
                                                             outputSettings:m_audioSettings
                                                             sourceFormatHint:sourceFormat]);
        if (!m_audioWriterInput) {
            qDebugCamera() << Q_FUNC_INFO << "failed to create audio writer input";
            // But we still can record video.
        } else if ([m_assetWriter canAddInput:m_audioWriterInput]) {
            [m_assetWriter addInput:m_audioWriterInput];
            m_audioWriterInput.data().expectsMediaDataInRealTime = YES;
        } else {
            qDebugCamera() << Q_FUNC_INFO << "failed to add audio writer input";
            m_audioWriterInput.reset();
            // We can (still) write video though ...
        }
    }

    return true;
}

- (void)setQueues
{
    Q_ASSERT(m_service && m_service->videoOutput() && m_service->videoOutput()->videoDataOutput());
    Q_ASSERT(m_videoQueue);

    [m_service->videoOutput()->videoDataOutput() setSampleBufferDelegate:self queue:m_videoQueue];

    if (m_audioOutput) {
        Q_ASSERT(m_audioQueue);
        [m_audioOutput setSampleBufferDelegate:self queue:m_audioQueue];
    }
}

- (void)updateDuration:(CMTime)newTimeStamp
{
    Q_ASSERT(CMTimeCompare(m_startTime, kCMTimeInvalid));
    Q_ASSERT(CMTimeCompare(m_lastTimeStamp, kCMTimeInvalid));
    if (CMTimeCompare(newTimeStamp, m_lastTimeStamp) > 0) {

        const CMTime duration = CMTimeSubtract(newTimeStamp, m_startTime);
        if (!CMTimeCompare(duration, kCMTimeInvalid))
            return;

        m_durationInMs.storeRelease(CMTimeGetSeconds(duration) * 1000);
        m_lastTimeStamp = newTimeStamp;
    }
}

@end
