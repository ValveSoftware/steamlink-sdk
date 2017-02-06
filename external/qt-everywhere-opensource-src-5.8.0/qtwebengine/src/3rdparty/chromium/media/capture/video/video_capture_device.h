// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VideoCaptureDevice is the abstract base class for realizing video capture
// device support in Chromium. It provides the interface for OS dependent
// implementations.
// The class is created and functions are invoked on a thread owned by
// VideoCaptureManager. Capturing is done on other threads, depending on the OS
// specific implementation.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/scoped_result_callback.h"
#include "media/mojo/interfaces/image_capture.mojom.h"
#include "mojo/public/cpp/bindings/array.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace media {

class CAPTURE_EXPORT VideoCaptureDevice {
 public:
  // Represents a capture device name and ID.
  // You should not create an instance of this class directly by e.g. setting
  // various properties directly.  Instead use
  // VideoCaptureDevice::GetDeviceNames to do this for you and if you need to
  // cache your own copy of a name, you can do so via the copy constructor.
  // The reason for this is that a device name might contain platform specific
  // settings that are relevant only to the platform specific implementation of
  // VideoCaptureDevice::Create.
  class CAPTURE_EXPORT Name {
   public:
    Name();
    Name(const std::string& name, const std::string& id);

#if defined(OS_LINUX)
    // Linux/CrOS targets Capture Api type: it can only be set on construction.
    enum CaptureApiType {
      V4L2_SINGLE_PLANE,
      API_TYPE_UNKNOWN
    };
#elif defined(OS_WIN)
    // Windows targets Capture Api type: it can only be set on construction.
    enum CaptureApiType { MEDIA_FOUNDATION, DIRECT_SHOW, API_TYPE_UNKNOWN };
#elif defined(OS_MACOSX)
    // Mac targets Capture Api type: it can only be set on construction.
    enum CaptureApiType { AVFOUNDATION, DECKLINK, API_TYPE_UNKNOWN };
    // For AVFoundation Api, identify devices that are built-in or USB.
    enum TransportType { USB_OR_BUILT_IN, OTHER_TRANSPORT };
#elif defined(OS_ANDROID)
    // Android targets Capture Api type: it can only be set on construction.
    // Automatically generated enum to interface with Java world.
    //
    // A Java counterpart will be generated for this enum.
    // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
    enum CaptureApiType {
      API1,
      API2_LEGACY,
      API2_FULL,
      API2_LIMITED,
      TANGO,
      API_TYPE_UNKNOWN
    };
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_ANDROID)
    Name(const std::string& name,
         const std::string& id,
         const CaptureApiType api_type);
#endif
#if defined(OS_MACOSX)
    Name(const std::string& name,
         const std::string& id,
         const CaptureApiType api_type,
         const TransportType transport_type);
#endif
    Name(const Name& other);
    ~Name();

    // Friendly name of a device
    const std::string& name() const { return device_name_; }

    // Unique name of a device. Even if there are multiple devices with the same
    // friendly name connected to the computer this will be unique.
    const std::string& id() const { return unique_id_; }

    // The unique hardware model identifier of the capture device. Returns
    // "[vid]:[pid]" when a USB device is detected, otherwise "".
    // The implementation of this method is platform-dependent.
    const std::string GetModel() const;

    // Friendly name of a device, plus the model identifier in parentheses.
    const std::string GetNameAndModel() const;

    // These operators are needed due to storing the name in an STL container.
    // In the shared build, all methods from the STL container will be exported
    // so even though they're not used, they're still depended upon.
    bool operator==(const Name& other) const {
      return other.id() == unique_id_;
    }
    bool operator<(const Name& other) const { return unique_id_ < other.id(); }

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_ANDROID)
    CaptureApiType capture_api_type() const {
      return capture_api_class_.capture_api_type();
    }
    const char* GetCaptureApiTypeString() const;
#endif
#if defined(OS_WIN)
    // Certain devices need an ID different from the |unique_id_| for
    // capabilities retrieval.
    const std::string& capabilities_id() const { return capabilities_id_; }
    void set_capabilities_id(const std::string& id) { capabilities_id_ = id; }
#endif  // if defined(OS_WIN)
#if defined(OS_MACOSX)
    TransportType transport_type() const { return transport_type_; }
#endif  // if defined(OS_MACOSX)

   private:
    std::string device_name_;
    std::string unique_id_;
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_ANDROID)
    // This class wraps the CaptureApiType to give it a by default value if not
    // initialized.
    class CaptureApiClass {
     public:
      CaptureApiClass() : capture_api_type_(API_TYPE_UNKNOWN) {}
      CaptureApiClass(const CaptureApiType api_type)
          : capture_api_type_(api_type) {}
      CaptureApiType capture_api_type() const {
        DCHECK_NE(capture_api_type_, API_TYPE_UNKNOWN);
        return capture_api_type_;
      }

     private:
      CaptureApiType capture_api_type_;
    };

    CaptureApiClass capture_api_class_;
#endif
#if defined(OS_WIN)
    // ID used for capabilities retrieval. By default is equal to |unique_id|.
    std::string capabilities_id_;
#endif
#if defined(OS_MACOSX)
    TransportType transport_type_;
#endif
    // Allow generated copy constructor and assignment.
  };

  // Manages a list of Name entries.
  typedef std::list<Name> Names;

  // Interface defining the methods that clients of VideoCapture must have. It
  // is actually two-in-one: clients may implement OnIncomingCapturedData() or
  // ReserveOutputBuffer() + OnIncomingCapturedVideoFrame(), or all of them.
  // All clients must implement OnError().
  class CAPTURE_EXPORT Client {
   public:
    // Memory buffer returned by Client::ReserveOutputBuffer().
    class CAPTURE_EXPORT Buffer {
     public:
      virtual ~Buffer() = 0;
      virtual int id() const = 0;
      virtual gfx::Size dimensions() const = 0;
      virtual size_t mapped_size() const = 0;
      virtual void* data(int plane) = 0;
      void* data() { return data(0); }
      virtual ClientBuffer AsClientBuffer(int plane) = 0;
#if defined(OS_POSIX) && !(defined(OS_MACOSX) && !defined(OS_IOS))
      virtual base::FileDescriptor AsPlatformFile() = 0;
#endif
    };

    virtual ~Client() {}

    // Captured a new video frame, data for which is pointed to by |data|.
    //
    // The format of the frame is described by |frame_format|, and is assumed to
    // be tightly packed. This method will try to reserve an output buffer and
    // copy from |data| into the output buffer. If no output buffer is
    // available, the frame will be silently dropped. |reference_time| is
    // system clock time when we detect the capture happens, it is used for
    // Audio/Video sync, not an exact presentation time for playout, because it
    // could contain noise. |timestamp| measures the ideal time span between the
    // first frame in the stream and the current frame; however, the time source
    // is determined by the platform's device driver and is often not the system
    // clock, or even has a drift with respect to system clock.
    virtual void OnIncomingCapturedData(const uint8_t* data,
                                        int length,
                                        const VideoCaptureFormat& frame_format,
                                        int clockwise_rotation,
                                        base::TimeTicks reference_time,
                                        base::TimeDelta timestamp) = 0;

    // Reserve an output buffer into which contents can be captured directly.
    // The returned Buffer will always be allocated with a memory size suitable
    // for holding a packed video frame with pixels of |format| format, of
    // |dimensions| frame dimensions. It is permissible for |dimensions| to be
    // zero; in which case the returned Buffer does not guarantee memory
    // backing, but functions as a reservation for external input for the
    // purposes of buffer throttling.
    //
    // The output buffer stays reserved and mapped for use until the Buffer
    // object is destroyed or returned.
    virtual std::unique_ptr<Buffer> ReserveOutputBuffer(
        const gfx::Size& dimensions,
        VideoPixelFormat format,
        VideoPixelStorage storage) = 0;

    // Captured new video data, held in |frame| or |buffer|, respectively for
    // OnIncomingCapturedVideoFrame() and  OnIncomingCapturedBuffer().
    //
    // In both cases, as the frame is backed by a reservation returned by
    // ReserveOutputBuffer(), delivery is guaranteed and will require no
    // additional copies in the browser process.
    // See OnIncomingCapturedData for details of |reference_time| and
    // |timestamp|.
    virtual void OnIncomingCapturedBuffer(
        std::unique_ptr<Buffer> buffer,
        const VideoCaptureFormat& frame_format,
        base::TimeTicks reference_time,
        base::TimeDelta timestamp) = 0;
    virtual void OnIncomingCapturedVideoFrame(
        std::unique_ptr<Buffer> buffer,
        const scoped_refptr<VideoFrame>& frame) = 0;

    // Attempts to reserve the same Buffer provided in the last call to one of
    // the OnIncomingCapturedXXX() methods. This will fail if the content of the
    // Buffer has not been preserved, or if the |dimensions|, |format|, or
    // |storage| disagree with how it was reserved via ReserveOutputBuffer().
    // When this operation fails, nullptr will be returned.
    virtual std::unique_ptr<Buffer> ResurrectLastOutputBuffer(
        const gfx::Size& dimensions,
        VideoPixelFormat format,
        VideoPixelStorage storage) = 0;

    // An error has occurred that cannot be handled and VideoCaptureDevice must
    // be StopAndDeAllocate()-ed. |reason| is a text description of the error.
    virtual void OnError(const tracked_objects::Location& from_here,
                         const std::string& reason) = 0;

    // VideoCaptureDevice requests the |message| to be logged.
    virtual void OnLog(const std::string& message) {}

    // Returns the current buffer pool utilization, in the range 0.0 (no buffers
    // are in use by producers or consumers) to 1.0 (all buffers are in use).
    virtual double GetBufferPoolUtilization() const = 0;
  };

  virtual ~VideoCaptureDevice();

  // Prepares the video capturer for use. StopAndDeAllocate() must be called
  // before the object is deleted.
  virtual void AllocateAndStart(const VideoCaptureParams& params,
                                std::unique_ptr<Client> client) = 0;

  // In cases where the video capturer self-pauses (e.g., a screen capturer
  // where the screen's content has not changed in a while), consumers may call
  // this to request a "refresh frame" be delivered to the Client.  This is used
  // in a number of circumstances, such as:
  //
  //   1. An additional consumer of video frames is starting up and requires a
  //      first frame (as opposed to not receiving a frame for an indeterminate
  //      amount of time).
  //   2. A few repeats of the same frame would allow a lossy video encoder to
  //      improve the video quality of unchanging content.
  //
  // The default implementation is a no-op. VideoCaptureDevice implementations
  // are not required to honor this request, especially if they do not
  // self-pause and/or if honoring the request would cause them to exceed their
  // configured maximum frame rate. Any VideoCaptureDevice that does self-pause,
  // however, should provide an implementation of this method that makes
  // reasonable attempts to honor these requests.
  virtual void RequestRefreshFrame() {}

  // Deallocates the video capturer, possibly asynchronously.
  //
  // This call requires the device to do the following things, eventually: put
  // hardware into a state where other applications could use it, free the
  // memory associated with capture, and delete the |client| pointer passed into
  // AllocateAndStart.
  //
  // If deallocation is done asynchronously, then the device implementation must
  // ensure that a subsequent AllocateAndStart() operation targeting the same ID
  // would be sequenced through the same task runner, so that deallocation
  // happens first.
  virtual void StopAndDeAllocate() = 0;

  // Retrieve the photo capabilities of the device (e.g. zoom levels etc).
  using GetPhotoCapabilitiesCallback =
      base::Callback<void(mojom::PhotoCapabilitiesPtr)>;
  virtual void GetPhotoCapabilities(
      ScopedResultCallback<GetPhotoCapabilitiesCallback> callback);

  // Asynchronously takes a photo, possibly reconfiguring the capture objects
  // and/or interrupting the capture flow. Runs |callback| on the thread
  // where TakePhoto() is called, if the photo was successfully taken.
  using TakePhotoCallback =
      base::Callback<void(mojo::String, mojo::Array<uint8_t>)>;
  virtual void TakePhoto(ScopedResultCallback<TakePhotoCallback> callback);

  // Gets the power line frequency, either from the params if specified by the
  // user or from the current system time zone.
  PowerLineFrequency GetPowerLineFrequency(
      const VideoCaptureParams& params) const;

 private:
  // Gets the power line frequency from the current system time zone if this is
  // defined, otherwise returns 0.
  PowerLineFrequency GetPowerLineFrequencyForLocation() const;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_H_
