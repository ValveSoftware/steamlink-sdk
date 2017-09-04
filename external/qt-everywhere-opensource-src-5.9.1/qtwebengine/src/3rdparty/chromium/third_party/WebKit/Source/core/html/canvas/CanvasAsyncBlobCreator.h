// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/CoreExport.h"
#include "core/dom/DOMTypedArray.h"
#include "core/fileapi/BlobCallback.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class Document;
class JPEGImageEncoderState;
class PNGImageEncoderState;

class CORE_EXPORT CanvasAsyncBlobCreator
    : public GarbageCollectedFinalized<CanvasAsyncBlobCreator> {
 public:
  static CanvasAsyncBlobCreator* create(
      DOMUint8ClampedArray* unpremultipliedRGBAImageData,
      const String& mimeType,
      const IntSize&,
      BlobCallback*,
      double startTime,
      Document*);
  static CanvasAsyncBlobCreator* create(
      DOMUint8ClampedArray* unpremultipliedRGBAImageData,
      const String& mimeType,
      const IntSize&,
      double startTime,
      Document*,
      ScriptPromiseResolver*);
  void scheduleAsyncBlobCreation(const double& quality);
  virtual ~CanvasAsyncBlobCreator();
  enum MimeType {
    MimeTypePng,
    MimeTypeJpeg,
    MimeTypeWebp,
    NumberOfMimeTypeSupported
  };
  // This enum is used to back an UMA histogram, and should therefore be treated
  // as append-only.
  enum IdleTaskStatus {
    IdleTaskNotStarted,
    IdleTaskStarted,
    IdleTaskCompleted,
    IdleTaskFailed,
    IdleTaskSwitchedToImmediateTask,
    IdleTaskNotSupported,  // Idle tasks are not implemented for some image
                           // types
    IdleTaskCount,         // Should not be seen in production
  };

  enum ToBlobFunctionType {
    HTMLCanvasToBlobCallback,
    OffscreenCanvasToBlobPromise,
    NumberOfToBlobFunctionTypes
  };

  // Methods are virtual for mocking in unit tests
  virtual void signalTaskSwitchInStartTimeoutEventForTesting() {}
  virtual void signalTaskSwitchInCompleteTimeoutEventForTesting() {}

  DECLARE_VIRTUAL_TRACE();

 protected:
  CanvasAsyncBlobCreator(DOMUint8ClampedArray* data,
                         MimeType,
                         const IntSize&,
                         BlobCallback*,
                         double,
                         Document*,
                         ScriptPromiseResolver*);
  // Methods are virtual for unit testing
  virtual void scheduleInitiatePngEncoding();
  virtual void scheduleInitiateJpegEncoding(const double&);
  virtual void idleEncodeRowsPng(double deadlineSeconds);
  virtual void idleEncodeRowsJpeg(double deadlineSeconds);
  virtual void postDelayedTaskToMainThread(const WebTraceLocation&,
                                           std::unique_ptr<WTF::Closure>,
                                           double delayMs);
  virtual void signalAlternativeCodePathFinishedForTesting() {}
  virtual void createBlobAndReturnResult();
  virtual void createNullAndReturnResult();

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
  Member<Document> m_document;

  const IntSize m_size;
  size_t m_pixelRowStride;
  const MimeType m_mimeType;
  double m_startTime;
  double m_scheduleInitiateStartTime;
  double m_elapsedTime;

  ToBlobFunctionType m_functionType;

  // Used when CanvasAsyncBlobCreator runs on main thread only
  Member<ParentFrameTaskRunners> m_parentFrameTaskRunner;

  // Used for HTMLCanvasElement only
  Member<BlobCallback> m_callback;

  // Used for OffscreenCanvas only
  Member<ScriptPromiseResolver> m_scriptPromiseResolver;

  // PNG
  bool initializePngStruct();
  void
  encodeRowsPngOnMainThread();  // Similar to idleEncodeRowsPng without deadline

  // JPEG
  bool initializeJpegStruct(double quality);
  void encodeRowsJpegOnMainThread();  // Similar to idleEncodeRowsJpeg without
                                      // deadline

  // WEBP
  void encodeImageOnEncoderThread(double quality);

  void idleTaskStartTimeoutEvent(double quality);
  void idleTaskCompleteTimeoutEvent();
};

}  // namespace blink
