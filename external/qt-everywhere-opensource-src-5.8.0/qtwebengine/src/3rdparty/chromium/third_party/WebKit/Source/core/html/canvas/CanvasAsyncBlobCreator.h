// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/CoreExport.h"
#include "core/dom/DOMTypedArray.h"
#include "core/fileapi/BlobCallback.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class PNGImageEncoderState;
class JPEGImageEncoderState;

class CORE_EXPORT CanvasAsyncBlobCreator : public GarbageCollectedFinalized<CanvasAsyncBlobCreator> {
public:
    static CanvasAsyncBlobCreator* create(DOMUint8ClampedArray* unpremultipliedRGBAImageData, const String& mimeType, const IntSize&, BlobCallback*, double);
    void scheduleAsyncBlobCreation(bool canUseIdlePeriodScheduling, const double& quality = 0.0);
    virtual ~CanvasAsyncBlobCreator();
    enum MimeType {
        MimeTypePng,
        MimeTypeJpeg,
        MimeTypeWebp,
        NumberOfMimeTypeSupported
    };
    // This enum is used to back an UMA histogram, and should therefore be treated as append-only.
    enum IdleTaskStatus {
        IdleTaskNotStarted,
        IdleTaskStarted,
        IdleTaskCompleted,
        IdleTaskFailed,
        IdleTaskSwitchedToMainThreadTask,
        IdleTaskNotSupported, // Idle tasks are not implemented for some image types
        IdleTaskCount, // Should not be seen in production
    };
    // Methods are virtual for mocking in unit tests
    virtual void signalTaskSwitchInStartTimeoutEventForTesting() { }
    virtual void signalTaskSwitchInCompleteTimeoutEventForTesting() { }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_data);
        visitor->trace(m_callback);
    }

protected:
    CanvasAsyncBlobCreator(DOMUint8ClampedArray* data, MimeType, const IntSize&, BlobCallback*, double);
    // Methods are virtual for unit testing
    virtual void scheduleInitiatePngEncoding();
    virtual void scheduleInitiateJpegEncoding(const double&);
    virtual void idleEncodeRowsPng(double deadlineSeconds);
    virtual void idleEncodeRowsJpeg(double deadlineSeconds);
    virtual void postDelayedTaskToMainThread(const WebTraceLocation&, std::unique_ptr<WTF::Closure>, double delayMs);
    virtual void signalAlternativeCodePathFinishedForTesting() { }
    virtual void createBlobAndInvokeCallback();
    virtual void createNullAndInvokeCallback();

    void initiatePngEncoding(double deadlineSeconds);
    void initiateJpegEncoding(const double& quality, double deadlineSeconds);
    IdleTaskStatus m_idleTaskStatus;

private:
    friend class CanvasAsyncBlobCreatorTest;

    void dispose();

    std::unique_ptr<PNGImageEncoderState> m_pngEncoderState;
    std::unique_ptr<JPEGImageEncoderState> m_jpegEncoderState;
    Member<DOMUint8ClampedArray> m_data;
    std::unique_ptr<Vector<unsigned char>> m_encodedImage;
    int m_numRowsCompleted;

    const IntSize m_size;
    size_t m_pixelRowStride;
    const MimeType m_mimeType;
    Member<BlobCallback> m_callback;
    double m_startTime;
    double m_scheduleInitiateStartTime;
    double m_elapsedTime;

    // PNG
    bool initializePngStruct();
    void encodeRowsPngOnMainThread(); // Similar to idleEncodeRowsPng without deadline

    // JPEG
    bool initializeJpegStruct(double quality);
    void encodeRowsJpegOnMainThread(); // Similar to idleEncodeRowsJpeg without deadline

    // WEBP
    void encodeImageOnEncoderThread(double quality);

    void idleTaskStartTimeoutEvent(double quality);
    void idleTaskCompleteTimeoutEvent();
    void recordIdleTaskStatusHistogram();
};

} // namespace blink
