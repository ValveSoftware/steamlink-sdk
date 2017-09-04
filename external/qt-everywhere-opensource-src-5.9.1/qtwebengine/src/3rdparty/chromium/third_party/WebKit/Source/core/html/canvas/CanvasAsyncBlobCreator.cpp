// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasAsyncBlobCreator.h"

#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/fileapi/Blob.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/image-encoders/JPEGImageEncoder.h"
#include "platform/image-encoders/PNGImageEncoder.h"
#include "platform/threading/BackgroundTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/CurrentTime.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

const double SlackBeforeDeadline =
    0.001;  // a small slack period between deadline and current time for safety
const int NumChannelsPng = 4;
const int LongTaskImageSizeThreshold =
    1000 *
    1000;  // The max image size we expect to encode in 14ms on Linux in PNG
           // format

// The encoding task is highly likely to switch from idle task to alternative
// code path when the startTimeoutDelay is set to be below 150ms. As we want the
// majority of encoding tasks to take the usual async idle task, we set a
// lenient limit -- 200ms here. This limit still needs to be short enough for
// the latency to be negligible to the user.
const double IdleTaskStartTimeoutDelay = 200.0;
// We should be more lenient on completion timeout delay to ensure that the
// switch from idle to main thread only happens to a minority of toBlob calls
#if !OS(ANDROID)
// Png image encoding on 4k by 4k canvas on Mac HDD takes 5.7+ seconds
const double IdleTaskCompleteTimeoutDelay = 6700.0;
#else
// Png image encoding on 4k by 4k canvas on Android One takes 9.0+ seconds
const double IdleTaskCompleteTimeoutDelay = 10000.0;
#endif

bool isDeadlineNearOrPassed(double deadlineSeconds) {
  return (deadlineSeconds - SlackBeforeDeadline -
              monotonicallyIncreasingTime() <=
          0);
}

String convertMimeTypeEnumToString(
    CanvasAsyncBlobCreator::MimeType mimeTypeEnum) {
  switch (mimeTypeEnum) {
    case CanvasAsyncBlobCreator::MimeTypePng:
      return "image/png";
    case CanvasAsyncBlobCreator::MimeTypeJpeg:
      return "image/jpeg";
    case CanvasAsyncBlobCreator::MimeTypeWebp:
      return "image/webp";
    default:
      return "image/unknown";
  }
}

CanvasAsyncBlobCreator::MimeType convertMimeTypeStringToEnum(
    const String& mimeType) {
  CanvasAsyncBlobCreator::MimeType mimeTypeEnum;
  if (mimeType == "image/png") {
    mimeTypeEnum = CanvasAsyncBlobCreator::MimeTypePng;
  } else if (mimeType == "image/jpeg") {
    mimeTypeEnum = CanvasAsyncBlobCreator::MimeTypeJpeg;
  } else if (mimeType == "image/webp") {
    mimeTypeEnum = CanvasAsyncBlobCreator::MimeTypeWebp;
  } else {
    mimeTypeEnum = CanvasAsyncBlobCreator::NumberOfMimeTypeSupported;
  }
  return mimeTypeEnum;
}

void recordIdleTaskStatusHistogram(
    CanvasAsyncBlobCreator::ToBlobFunctionType functionType,
    CanvasAsyncBlobCreator::IdleTaskStatus status) {
  // TODO(crbug.com/653599): Add histograms for OffscreenCanvas.convertToBlob.
  if (functionType == CanvasAsyncBlobCreator::OffscreenCanvasToBlobPromise)
    return;
  DEFINE_STATIC_LOCAL(EnumerationHistogram, toBlobIdleTaskStatus,
                      ("Blink.Canvas.ToBlob.IdleTaskStatus",
                       CanvasAsyncBlobCreator::IdleTaskCount));
  toBlobIdleTaskStatus.count(status);
}

// This enum is used in histogram and any more types should be appended at the
// end of the list.
enum ElapsedTimeHistogramType {
  InitiateEncodingDelay,
  IdleEncodeDuration,
  ToBlobDuration,
  NumberOfElapsedTimeHistogramTypes
};

void recordElapsedTimeHistogram(
    CanvasAsyncBlobCreator::ToBlobFunctionType functionType,
    ElapsedTimeHistogramType type,
    CanvasAsyncBlobCreator::MimeType mimeType,
    double elapsedTime) {
  // TODO(crbug.com/653599): Add histograms for OffscreenCanvas.convertToBlob.
  if (functionType == CanvasAsyncBlobCreator::OffscreenCanvasToBlobPromise)
    return;

  if (type == InitiateEncodingDelay) {
    if (mimeType == CanvasAsyncBlobCreator::MimeTypePng) {
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, toBlobPNGInitiateEncodingCounter,
          ("Blink.Canvas.ToBlob.InitiateEncodingDelay.PNG", 0, 10000000, 50));
      toBlobPNGInitiateEncodingCounter.count(elapsedTime * 1000000.0);
    } else if (mimeType == CanvasAsyncBlobCreator::MimeTypeJpeg) {
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, toBlobJPEGInitiateEncodingCounter,
          ("Blink.Canvas.ToBlob.InitiateEncodingDelay.JPEG", 0, 10000000, 50));
      toBlobJPEGInitiateEncodingCounter.count(elapsedTime * 1000000.0);
    }
  } else if (type == IdleEncodeDuration) {
    if (mimeType == CanvasAsyncBlobCreator::MimeTypePng) {
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, toBlobPNGIdleEncodeCounter,
          ("Blink.Canvas.ToBlob.IdleEncodeDuration.PNG", 0, 10000000, 50));
      toBlobPNGIdleEncodeCounter.count(elapsedTime * 1000000.0);
    } else if (mimeType == CanvasAsyncBlobCreator::MimeTypeJpeg) {
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, toBlobJPEGIdleEncodeCounter,
          ("Blink.Canvas.ToBlob.IdleEncodeDuration.JPEG", 0, 10000000, 50));
      toBlobJPEGIdleEncodeCounter.count(elapsedTime * 1000000.0);
    }
  } else if (type == ToBlobDuration) {
    if (mimeType == CanvasAsyncBlobCreator::MimeTypePng) {
      DEFINE_STATIC_LOCAL(CustomCountHistogram, toBlobPNGCounter,
                          ("Blink.Canvas.ToBlobDuration.PNG", 0, 10000000, 50));
      toBlobPNGCounter.count(elapsedTime * 1000000.0);
    } else if (mimeType == CanvasAsyncBlobCreator::MimeTypeJpeg) {
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, toBlobJPEGCounter,
          ("Blink.Canvas.ToBlobDuration.JPEG", 0, 10000000, 50));
      toBlobJPEGCounter.count(elapsedTime * 1000000.0);
    } else if (mimeType == CanvasAsyncBlobCreator::MimeTypeWebp) {
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, toBlobWEBPCounter,
          ("Blink.Canvas.ToBlobDuration.WEBP", 0, 10000000, 50));
      toBlobWEBPCounter.count(elapsedTime * 1000000.0);
    }
  }
}

}  // anonymous namespace

CanvasAsyncBlobCreator* CanvasAsyncBlobCreator::create(
    DOMUint8ClampedArray* unpremultipliedRGBAImageData,
    const String& mimeType,
    const IntSize& size,
    BlobCallback* callback,
    double startTime,
    Document* document) {
  return new CanvasAsyncBlobCreator(unpremultipliedRGBAImageData,
                                    convertMimeTypeStringToEnum(mimeType), size,
                                    callback, startTime, document, nullptr);
}

CanvasAsyncBlobCreator* CanvasAsyncBlobCreator::create(
    DOMUint8ClampedArray* unpremultipliedRGBAImageData,
    const String& mimeType,
    const IntSize& size,
    double startTime,
    Document* document,
    ScriptPromiseResolver* resolver) {
  return new CanvasAsyncBlobCreator(unpremultipliedRGBAImageData,
                                    convertMimeTypeStringToEnum(mimeType), size,
                                    nullptr, startTime, document, resolver);
}

CanvasAsyncBlobCreator::CanvasAsyncBlobCreator(DOMUint8ClampedArray* data,
                                               MimeType mimeType,
                                               const IntSize& size,
                                               BlobCallback* callback,
                                               double startTime,
                                               Document* document,
                                               ScriptPromiseResolver* resolver)
    : m_data(data),
      m_document(document),
      m_size(size),
      m_mimeType(mimeType),
      m_startTime(startTime),
      m_elapsedTime(0),
      m_callback(callback),
      m_scriptPromiseResolver(resolver) {
  DCHECK(m_data->length() == (unsigned)(size.height() * size.width() * 4));
  m_encodedImage = wrapUnique(new Vector<unsigned char>());
  m_pixelRowStride = size.width() * NumChannelsPng;
  m_idleTaskStatus = IdleTaskNotSupported;
  m_numRowsCompleted = 0;
  if (document) {
    m_parentFrameTaskRunner = ParentFrameTaskRunners::create(document->frame());
  }
  if (m_scriptPromiseResolver) {
    m_functionType = OffscreenCanvasToBlobPromise;
  } else {
    m_functionType = HTMLCanvasToBlobCallback;
  }
}

CanvasAsyncBlobCreator::~CanvasAsyncBlobCreator() {}

void CanvasAsyncBlobCreator::dispose() {
  // Eagerly let go of references to prevent retention of these
  // resources while any remaining posted tasks are queued.
  m_data.clear();
  m_document.clear();
  m_parentFrameTaskRunner.clear();
  m_callback.clear();
  m_scriptPromiseResolver.clear();
}

void CanvasAsyncBlobCreator::scheduleAsyncBlobCreation(const double& quality) {
  if (m_mimeType == MimeTypeWebp) {
    if (!isMainThread()) {
      DCHECK(m_functionType == OffscreenCanvasToBlobPromise);
      // When OffscreenCanvas.convertToBlob() occurs on worker thread,
      // we do not need to use background task runner to reduce load on main.
      // So we just directly encode images on the worker thread.
      if (!ImageDataBuffer(m_size, m_data->data())
               .encodeImage("image/webp", quality, m_encodedImage.get())) {
        TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, m_document)
            ->postTask(
                BLINK_FROM_HERE,
                WTF::bind(&CanvasAsyncBlobCreator::createNullAndReturnResult,
                          wrapPersistent(this)));

        return;
      }
      TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, m_document)
          ->postTask(
              BLINK_FROM_HERE,
              WTF::bind(&CanvasAsyncBlobCreator::createBlobAndReturnResult,
                        wrapPersistent(this)));

    } else {
      BackgroundTaskRunner::TaskSize taskSize =
          (m_size.height() * m_size.width() >= LongTaskImageSizeThreshold)
              ? BackgroundTaskRunner::TaskSizeLongRunningTask
              : BackgroundTaskRunner::TaskSizeShortRunningTask;
      BackgroundTaskRunner::postOnBackgroundThread(
          BLINK_FROM_HERE,
          crossThreadBind(&CanvasAsyncBlobCreator::encodeImageOnEncoderThread,
                          wrapCrossThreadPersistent(this), quality),
          taskSize);
    }
  } else {
    m_idleTaskStatus = IdleTaskNotStarted;
    if (m_mimeType == MimeTypePng) {
      this->scheduleInitiatePngEncoding();
    } else if (m_mimeType == MimeTypeJpeg) {
      this->scheduleInitiateJpegEncoding(quality);
    } else {
      // Progressive encoding is only applicable to png and jpeg image format,
      // and thus idle tasks scheduling can only be applied to these image
      // formats.
      // TODO(xlai): Progressive encoding on webp image formats
      // (crbug.com/571399)
      NOTREACHED();
    }

    // TODO: Enforce OffscreenCanvas.convertToBlob to finish within deadline.
    // See crbug.com/657102.
    if (m_functionType == HTMLCanvasToBlobCallback) {
      // We post the below task to check if the above idle task isn't late.
      // There's no risk of concurrency as both tasks are on main thread.
      this->postDelayedTaskToMainThread(
          BLINK_FROM_HERE,
          WTF::bind(&CanvasAsyncBlobCreator::idleTaskStartTimeoutEvent,
                    wrapPersistent(this), quality),
          IdleTaskStartTimeoutDelay);
    }
  }
}

void CanvasAsyncBlobCreator::scheduleInitiateJpegEncoding(
    const double& quality) {
  m_scheduleInitiateStartTime = WTF::monotonicallyIncreasingTime();
  Platform::current()->currentThread()->scheduler()->postIdleTask(
      BLINK_FROM_HERE, WTF::bind(&CanvasAsyncBlobCreator::initiateJpegEncoding,
                                 wrapPersistent(this), quality));
}

void CanvasAsyncBlobCreator::initiateJpegEncoding(const double& quality,
                                                  double deadlineSeconds) {
  recordElapsedTimeHistogram(
      m_functionType, InitiateEncodingDelay, MimeTypeJpeg,
      WTF::monotonicallyIncreasingTime() - m_scheduleInitiateStartTime);
  if (m_idleTaskStatus == IdleTaskSwitchedToImmediateTask) {
    return;
  }

  DCHECK(m_idleTaskStatus == IdleTaskNotStarted);
  m_idleTaskStatus = IdleTaskStarted;

  if (!initializeJpegStruct(quality)) {
    m_idleTaskStatus = IdleTaskFailed;
    return;
  }
  this->idleEncodeRowsJpeg(deadlineSeconds);
}

void CanvasAsyncBlobCreator::scheduleInitiatePngEncoding() {
  m_scheduleInitiateStartTime = WTF::monotonicallyIncreasingTime();
  Platform::current()->currentThread()->scheduler()->postIdleTask(
      BLINK_FROM_HERE, WTF::bind(&CanvasAsyncBlobCreator::initiatePngEncoding,
                                 wrapPersistent(this)));
}

void CanvasAsyncBlobCreator::initiatePngEncoding(double deadlineSeconds) {
  recordElapsedTimeHistogram(
      m_functionType, InitiateEncodingDelay, MimeTypePng,
      WTF::monotonicallyIncreasingTime() - m_scheduleInitiateStartTime);
  if (m_idleTaskStatus == IdleTaskSwitchedToImmediateTask) {
    return;
  }

  DCHECK(m_idleTaskStatus == IdleTaskNotStarted);
  m_idleTaskStatus = IdleTaskStarted;

  if (!initializePngStruct()) {
    m_idleTaskStatus = IdleTaskFailed;
    return;
  }
  this->idleEncodeRowsPng(deadlineSeconds);
}

void CanvasAsyncBlobCreator::idleEncodeRowsPng(double deadlineSeconds) {
  if (m_idleTaskStatus == IdleTaskSwitchedToImmediateTask) {
    return;
  }

  double startTime = WTF::monotonicallyIncreasingTime();
  unsigned char* inputPixels =
      m_data->data() + m_pixelRowStride * m_numRowsCompleted;
  for (int y = m_numRowsCompleted; y < m_size.height(); ++y) {
    if (isDeadlineNearOrPassed(deadlineSeconds)) {
      m_numRowsCompleted = y;
      m_elapsedTime += (WTF::monotonicallyIncreasingTime() - startTime);
      Platform::current()->currentThread()->scheduler()->postIdleTask(
          BLINK_FROM_HERE, WTF::bind(&CanvasAsyncBlobCreator::idleEncodeRowsPng,
                                     wrapPersistent(this)));
      return;
    }
    PNGImageEncoder::writeOneRowToPng(inputPixels, m_pngEncoderState.get());
    inputPixels += m_pixelRowStride;
  }
  m_numRowsCompleted = m_size.height();
  PNGImageEncoder::finalizePng(m_pngEncoderState.get());

  m_idleTaskStatus = IdleTaskCompleted;
  m_elapsedTime += (WTF::monotonicallyIncreasingTime() - startTime);
  recordElapsedTimeHistogram(m_functionType, IdleEncodeDuration, MimeTypePng,
                             m_elapsedTime);
  if (isDeadlineNearOrPassed(deadlineSeconds)) {
    TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, m_document)
        ->postTask(BLINK_FROM_HERE,
                   WTF::bind(&CanvasAsyncBlobCreator::createBlobAndReturnResult,
                             wrapPersistent(this)));
  } else {
    this->createBlobAndReturnResult();
  }
}

void CanvasAsyncBlobCreator::idleEncodeRowsJpeg(double deadlineSeconds) {
  if (m_idleTaskStatus == IdleTaskSwitchedToImmediateTask) {
    return;
  }

  double startTime = WTF::monotonicallyIncreasingTime();
  m_numRowsCompleted = JPEGImageEncoder::progressiveEncodeRowsJpegHelper(
      m_jpegEncoderState.get(), m_data->data(), m_numRowsCompleted,
      SlackBeforeDeadline, deadlineSeconds);
  m_elapsedTime += (WTF::monotonicallyIncreasingTime() - startTime);
  if (m_numRowsCompleted == m_size.height()) {
    m_idleTaskStatus = IdleTaskCompleted;
    recordElapsedTimeHistogram(m_functionType, IdleEncodeDuration, MimeTypeJpeg,
                               m_elapsedTime);

    if (isDeadlineNearOrPassed(deadlineSeconds)) {
      TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, m_document)
          ->postTask(
              BLINK_FROM_HERE,
              WTF::bind(&CanvasAsyncBlobCreator::createBlobAndReturnResult,
                        wrapPersistent(this)));
    } else {
      this->createBlobAndReturnResult();
    }
  } else if (m_numRowsCompleted == JPEGImageEncoder::ProgressiveEncodeFailed) {
    m_idleTaskStatus = IdleTaskFailed;
    this->createNullAndReturnResult();
  } else {
    Platform::current()->currentThread()->scheduler()->postIdleTask(
        BLINK_FROM_HERE, WTF::bind(&CanvasAsyncBlobCreator::idleEncodeRowsJpeg,
                                   wrapPersistent(this)));
  }
}

void CanvasAsyncBlobCreator::encodeRowsPngOnMainThread() {
  DCHECK(m_idleTaskStatus == IdleTaskSwitchedToImmediateTask);

  // Continue encoding from the last completed row
  unsigned char* inputPixels =
      m_data->data() + m_pixelRowStride * m_numRowsCompleted;
  for (int y = m_numRowsCompleted; y < m_size.height(); ++y) {
    PNGImageEncoder::writeOneRowToPng(inputPixels, m_pngEncoderState.get());
    inputPixels += m_pixelRowStride;
  }
  PNGImageEncoder::finalizePng(m_pngEncoderState.get());
  this->createBlobAndReturnResult();

  this->signalAlternativeCodePathFinishedForTesting();
}

void CanvasAsyncBlobCreator::encodeRowsJpegOnMainThread() {
  DCHECK(m_idleTaskStatus == IdleTaskSwitchedToImmediateTask);

  // Continue encoding from the last completed row
  if (JPEGImageEncoder::encodeWithPreInitializedState(
          std::move(m_jpegEncoderState), m_data->data(), m_numRowsCompleted)) {
    this->createBlobAndReturnResult();
  } else {
    this->createNullAndReturnResult();
  }

  this->signalAlternativeCodePathFinishedForTesting();
}

void CanvasAsyncBlobCreator::createBlobAndReturnResult() {
  recordIdleTaskStatusHistogram(m_functionType, m_idleTaskStatus);
  recordElapsedTimeHistogram(m_functionType, ToBlobDuration, m_mimeType,
                             WTF::monotonicallyIncreasingTime() - m_startTime);

  Blob* resultBlob =
      Blob::create(m_encodedImage->data(), m_encodedImage->size(),
                   convertMimeTypeEnumToString(m_mimeType));
  if (m_functionType == HTMLCanvasToBlobCallback) {
    TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, m_document)
        ->postTask(BLINK_FROM_HERE, WTF::bind(&BlobCallback::handleEvent,
                                              wrapPersistent(m_callback.get()),
                                              wrapPersistent(resultBlob)));
  } else {
    m_scriptPromiseResolver->resolve(resultBlob);
  }
  // Avoid unwanted retention, see dispose().
  dispose();
}

void CanvasAsyncBlobCreator::createNullAndReturnResult() {
  recordIdleTaskStatusHistogram(m_functionType, m_idleTaskStatus);
  if (m_functionType == HTMLCanvasToBlobCallback) {
    DCHECK(isMainThread());
    recordIdleTaskStatusHistogram(m_functionType, m_idleTaskStatus);
    TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, m_document)
        ->postTask(BLINK_FROM_HERE,
                   WTF::bind(&BlobCallback::handleEvent,
                             wrapPersistent(m_callback.get()), nullptr));
  } else {
    Blob* blob = nullptr;
    m_scriptPromiseResolver->reject(blob);
  }
  // Avoid unwanted retention, see dispose().
  dispose();
}

void CanvasAsyncBlobCreator::encodeImageOnEncoderThread(double quality) {
  DCHECK(!isMainThread());
  DCHECK(m_mimeType == MimeTypeWebp);

  if (!ImageDataBuffer(m_size, m_data->data())
           .encodeImage("image/webp", quality, m_encodedImage.get())) {
    m_parentFrameTaskRunner->get(TaskType::CanvasBlobSerialization)
        ->postTask(BLINK_FROM_HERE,
                   crossThreadBind(&BlobCallback::handleEvent,
                                   wrapCrossThreadPersistent(m_callback.get()),
                                   nullptr));
    return;
  }

  m_parentFrameTaskRunner->get(TaskType::CanvasBlobSerialization)
      ->postTask(
          BLINK_FROM_HERE,
          crossThreadBind(&CanvasAsyncBlobCreator::createBlobAndReturnResult,
                          wrapCrossThreadPersistent(this)));
}

bool CanvasAsyncBlobCreator::initializePngStruct() {
  m_pngEncoderState =
      PNGImageEncoderState::create(m_size, m_encodedImage.get());
  if (!m_pngEncoderState) {
    this->createNullAndReturnResult();
    return false;
  }
  return true;
}

bool CanvasAsyncBlobCreator::initializeJpegStruct(double quality) {
  m_jpegEncoderState =
      JPEGImageEncoderState::create(m_size, quality, m_encodedImage.get());
  if (!m_jpegEncoderState) {
    this->createNullAndReturnResult();
    return false;
  }
  return true;
}

void CanvasAsyncBlobCreator::idleTaskStartTimeoutEvent(double quality) {
  if (m_idleTaskStatus == IdleTaskStarted) {
    // Even if the task started quickly, we still want to ensure completion
    this->postDelayedTaskToMainThread(
        BLINK_FROM_HERE,
        WTF::bind(&CanvasAsyncBlobCreator::idleTaskCompleteTimeoutEvent,
                  wrapPersistent(this)),
        IdleTaskCompleteTimeoutDelay);
  } else if (m_idleTaskStatus == IdleTaskNotStarted) {
    // If the idle task does not start after a delay threshold, we will
    // force it to happen on main thread (even though it may cause more
    // janks) to prevent toBlob being postponed forever in extreme cases.
    m_idleTaskStatus = IdleTaskSwitchedToImmediateTask;
    signalTaskSwitchInStartTimeoutEventForTesting();

    if (m_mimeType == MimeTypePng) {
      if (initializePngStruct()) {
        TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, m_document)
            ->postTask(
                BLINK_FROM_HERE,
                WTF::bind(&CanvasAsyncBlobCreator::encodeRowsPngOnMainThread,
                          wrapPersistent(this)));
      } else {
        // Failing in initialization of png struct
        this->signalAlternativeCodePathFinishedForTesting();
      }
    } else {
      DCHECK(m_mimeType == MimeTypeJpeg);
      if (initializeJpegStruct(quality)) {
        TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, m_document)
            ->postTask(
                BLINK_FROM_HERE,
                WTF::bind(&CanvasAsyncBlobCreator::encodeRowsJpegOnMainThread,
                          wrapPersistent(this)));
      } else {
        // Failing in initialization of jpeg struct
        this->signalAlternativeCodePathFinishedForTesting();
      }
    }
  } else {
    DCHECK(m_idleTaskStatus == IdleTaskFailed ||
           m_idleTaskStatus == IdleTaskCompleted);
    this->signalAlternativeCodePathFinishedForTesting();
  }
}

void CanvasAsyncBlobCreator::idleTaskCompleteTimeoutEvent() {
  DCHECK(m_idleTaskStatus != IdleTaskNotStarted);

  if (m_idleTaskStatus == IdleTaskStarted) {
    // It has taken too long to complete for the idle task.
    m_idleTaskStatus = IdleTaskSwitchedToImmediateTask;
    signalTaskSwitchInCompleteTimeoutEventForTesting();

    if (m_mimeType == MimeTypePng) {
      TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, m_document)
          ->postTask(
              BLINK_FROM_HERE,
              WTF::bind(&CanvasAsyncBlobCreator::encodeRowsPngOnMainThread,
                        wrapPersistent(this)));
    } else {
      DCHECK(m_mimeType == MimeTypeJpeg);
      TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, m_document)
          ->postTask(
              BLINK_FROM_HERE,
              WTF::bind(&CanvasAsyncBlobCreator::encodeRowsJpegOnMainThread,
                        wrapPersistent(this)));
    }
  } else {
    DCHECK(m_idleTaskStatus == IdleTaskFailed ||
           m_idleTaskStatus == IdleTaskCompleted);
    this->signalAlternativeCodePathFinishedForTesting();
  }
}

void CanvasAsyncBlobCreator::postDelayedTaskToMainThread(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::Closure> task,
    double delayMs) {
  DCHECK(isMainThread());
  TaskRunnerHelper::get(TaskType::CanvasBlobSerialization, m_document)
      ->postDelayedTask(location, std::move(task), delayMs);
}

DEFINE_TRACE(CanvasAsyncBlobCreator) {
  visitor->trace(m_document);
  visitor->trace(m_data);
  visitor->trace(m_callback);
  visitor->trace(m_parentFrameTaskRunner);
  visitor->trace(m_scriptPromiseResolver);
}

}  // namespace blink
