// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/base/yuv_convert.h"
#include "third_party/libyuv/include/libyuv.h"

using media::VideoCaptureFormat;

namespace content {

namespace {

static const int kInfiniteRatio = 99999;

#define UMA_HISTOGRAM_ASPECT_RATIO(name, width, height) \
    UMA_HISTOGRAM_SPARSE_SLOWLY( \
        name, \
        (height) ? ((width) * 100) / (height) : kInfiniteRatio);

// The number of buffers that VideoCaptureBufferPool should allocate.
const int kNoOfBuffers = 3;

class PoolBuffer : public media::VideoCaptureDevice::Client::Buffer {
 public:
  PoolBuffer(const scoped_refptr<VideoCaptureBufferPool>& pool,
             int buffer_id,
             void* data,
             size_t size)
      : Buffer(buffer_id, data, size), pool_(pool) {
    DCHECK(pool_);
  }

 private:
  virtual ~PoolBuffer() { pool_->RelinquishProducerReservation(id()); }

  const scoped_refptr<VideoCaptureBufferPool> pool_;
};

}  // anonymous namespace

struct VideoCaptureController::ControllerClient {
  ControllerClient(const VideoCaptureControllerID& id,
                   VideoCaptureControllerEventHandler* handler,
                   base::ProcessHandle render_process,
                   media::VideoCaptureSessionId session_id,
                   const media::VideoCaptureParams& params)
      : controller_id(id),
        event_handler(handler),
        render_process_handle(render_process),
        session_id(session_id),
        parameters(params),
        session_closed(false) {}

  ~ControllerClient() {}

  // ID used for identifying this object.
  const VideoCaptureControllerID controller_id;
  VideoCaptureControllerEventHandler* const event_handler;

  // Handle to the render process that will receive the capture buffers.
  const base::ProcessHandle render_process_handle;
  const media::VideoCaptureSessionId session_id;
  const media::VideoCaptureParams parameters;

  // Buffers that are currently known to this client.
  std::set<int> known_buffers;

  // Buffers currently held by this client, and syncpoint callback to call when
  // they are returned from the client.
  typedef std::map<int, scoped_refptr<media::VideoFrame> > ActiveBufferMap;
  ActiveBufferMap active_buffers;

  // State of capture session, controlled by VideoCaptureManager directly. This
  // transitions to true as soon as StopSession() occurs, at which point the
  // client is sent an OnEnded() event. However, because the client retains a
  // VideoCaptureController* pointer, its ControllerClient entry lives on until
  // it unregisters itself via RemoveClient(), which may happen asynchronously.
  //
  // TODO(nick): If we changed the semantics of VideoCaptureHost so that
  // OnEnded() events were processed synchronously (with the RemoveClient() done
  // implicitly), we could avoid tracking this state here in the Controller, and
  // simplify the code in both places.
  bool session_closed;
};

// Receives events from the VideoCaptureDevice and posts them to a
// VideoCaptureController on the IO thread. An instance of this class may safely
// outlive its target VideoCaptureController.
//
// Methods of this class may be called from any thread, and in practice will
// often be called on some auxiliary thread depending on the platform and the
// device type; including, for example, the DirectShow thread on Windows, the
// v4l2_thread on Linux, and the UI thread for tab capture.
class VideoCaptureController::VideoCaptureDeviceClient
    : public media::VideoCaptureDevice::Client {
 public:
  explicit VideoCaptureDeviceClient(
      const base::WeakPtr<VideoCaptureController>& controller,
      const scoped_refptr<VideoCaptureBufferPool>& buffer_pool);
  virtual ~VideoCaptureDeviceClient();

  // VideoCaptureDevice::Client implementation.
  virtual scoped_refptr<Buffer> ReserveOutputBuffer(
      media::VideoFrame::Format format,
      const gfx::Size& size) OVERRIDE;
  virtual void OnIncomingCapturedData(const uint8* data,
                                      int length,
                                      const VideoCaptureFormat& frame_format,
                                      int rotation,
                                      base::TimeTicks timestamp) OVERRIDE;
  virtual void OnIncomingCapturedVideoFrame(
      const scoped_refptr<Buffer>& buffer,
      const VideoCaptureFormat& buffer_format,
      const scoped_refptr<media::VideoFrame>& frame,
      base::TimeTicks timestamp) OVERRIDE;
  virtual void OnError(const std::string& reason) OVERRIDE;
  virtual void OnLog(const std::string& message) OVERRIDE;

 private:
  scoped_refptr<Buffer> DoReserveOutputBuffer(media::VideoFrame::Format format,
                                              const gfx::Size& dimensions);

  // The controller to which we post events.
  const base::WeakPtr<VideoCaptureController> controller_;

  // The pool of shared-memory buffers used for capturing.
  const scoped_refptr<VideoCaptureBufferPool> buffer_pool_;

  bool first_frame_;
};

VideoCaptureController::VideoCaptureController()
    : buffer_pool_(new VideoCaptureBufferPool(kNoOfBuffers)),
      state_(VIDEO_CAPTURE_STATE_STARTED),
      weak_ptr_factory_(this) {
}

VideoCaptureController::VideoCaptureDeviceClient::VideoCaptureDeviceClient(
    const base::WeakPtr<VideoCaptureController>& controller,
    const scoped_refptr<VideoCaptureBufferPool>& buffer_pool)
    : controller_(controller), buffer_pool_(buffer_pool), first_frame_(true) {}

VideoCaptureController::VideoCaptureDeviceClient::~VideoCaptureDeviceClient() {}

base::WeakPtr<VideoCaptureController> VideoCaptureController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

scoped_ptr<media::VideoCaptureDevice::Client>
VideoCaptureController::NewDeviceClient() {
  scoped_ptr<media::VideoCaptureDevice::Client> result(
      new VideoCaptureDeviceClient(this->GetWeakPtr(), buffer_pool_));
  return result.Pass();
}

void VideoCaptureController::AddClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler,
    base::ProcessHandle render_process,
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::AddClient, id " << id.device_id
           << ", " << params.requested_format.frame_size.ToString()
           << ", " << params.requested_format.frame_rate
           << ", " << session_id
           << ")";

  // If this is the first client added to the controller, cache the parameters.
  if (!controller_clients_.size())
    video_capture_format_ = params.requested_format;

  // Signal error in case device is already in error state.
  if (state_ == VIDEO_CAPTURE_STATE_ERROR) {
    event_handler->OnError(id);
    return;
  }

  // Do nothing if this client has called AddClient before.
  if (FindClient(id, event_handler, controller_clients_))
    return;

  ControllerClient* client = new ControllerClient(
      id, event_handler, render_process, session_id, params);
  // If we already have gotten frame_info from the device, repeat it to the new
  // client.
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    controller_clients_.push_back(client);
    return;
  }
}

int VideoCaptureController::RemoveClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::RemoveClient, id " << id.device_id;

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);
  if (!client)
    return kInvalidMediaCaptureSessionId;

  // Take back all buffers held by the |client|.
  for (ControllerClient::ActiveBufferMap::iterator buffer_it =
           client->active_buffers.begin();
       buffer_it != client->active_buffers.end();
       ++buffer_it) {
    buffer_pool_->RelinquishConsumerHold(buffer_it->first, 1);
  }
  client->active_buffers.clear();

  int session_id = client->session_id;
  controller_clients_.remove(client);
  delete client;

  return session_id;
}

void VideoCaptureController::StopSession(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::StopSession, id " << session_id;

  ControllerClient* client = FindClient(session_id, controller_clients_);

  if (client) {
    client->session_closed = true;
    client->event_handler->OnEnded(client->controller_id);
  }
}

void VideoCaptureController::ReturnBuffer(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler,
    int buffer_id,
    const std::vector<uint32>& sync_points) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);

  // If this buffer is not held by this client, or this client doesn't exist
  // in controller, do nothing.
  ControllerClient::ActiveBufferMap::iterator iter;
  if (!client || (iter = client->active_buffers.find(buffer_id)) ==
                     client->active_buffers.end()) {
    NOTREACHED();
    return;
  }
  scoped_refptr<media::VideoFrame> frame = iter->second;
  client->active_buffers.erase(iter);

  if (frame->format() == media::VideoFrame::NATIVE_TEXTURE) {
    for (size_t i = 0; i < sync_points.size(); i++)
      frame->AppendReleaseSyncPoint(sync_points[i]);
  }

  buffer_pool_->RelinquishConsumerHold(buffer_id, 1);
}

const media::VideoCaptureFormat&
VideoCaptureController::GetVideoCaptureFormat() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return video_capture_format_;
}

scoped_refptr<media::VideoCaptureDevice::Client::Buffer>
VideoCaptureController::VideoCaptureDeviceClient::ReserveOutputBuffer(
    media::VideoFrame::Format format,
    const gfx::Size& size) {
  return DoReserveOutputBuffer(format, size);
}

void VideoCaptureController::VideoCaptureDeviceClient::OnIncomingCapturedData(
    const uint8* data,
    int length,
    const VideoCaptureFormat& frame_format,
    int rotation,
    base::TimeTicks timestamp) {
  TRACE_EVENT0("video", "VideoCaptureController::OnIncomingCapturedData");

  if (!frame_format.IsValid())
    return;

  // Chopped pixels in width/height in case video capture device has odd
  // numbers for width/height.
  int chopped_width = 0;
  int chopped_height = 0;
  int new_unrotated_width = frame_format.frame_size.width();
  int new_unrotated_height = frame_format.frame_size.height();

  if (new_unrotated_width & 1) {
    --new_unrotated_width;
    chopped_width = 1;
  }
  if (new_unrotated_height & 1) {
    --new_unrotated_height;
    chopped_height = 1;
  }

  int destination_width = new_unrotated_width;
  int destination_height = new_unrotated_height;
  if (rotation == 90 || rotation == 270) {
    destination_width = new_unrotated_height;
    destination_height = new_unrotated_width;
  }
  const gfx::Size dimensions(destination_width, destination_height);
  if (!media::VideoFrame::IsValidConfig(media::VideoFrame::I420,
                                        dimensions,
                                        gfx::Rect(dimensions),
                                        dimensions)) {
    return;
  }

  scoped_refptr<Buffer> buffer =
      DoReserveOutputBuffer(media::VideoFrame::I420, dimensions);

  if (!buffer)
    return;
  uint8* yplane = NULL;
  bool flip = false;
  yplane = reinterpret_cast<uint8*>(buffer->data());
  uint8* uplane =
      yplane +
      media::VideoFrame::PlaneAllocationSize(
          media::VideoFrame::I420, media::VideoFrame::kYPlane, dimensions);
  uint8* vplane =
      uplane +
      media::VideoFrame::PlaneAllocationSize(
          media::VideoFrame::I420, media::VideoFrame::kUPlane, dimensions);
  int yplane_stride = dimensions.width();
  int uv_plane_stride = yplane_stride / 2;
  int crop_x = 0;
  int crop_y = 0;
  libyuv::FourCC origin_colorspace = libyuv::FOURCC_ANY;

  libyuv::RotationMode rotation_mode = libyuv::kRotate0;
  if (rotation == 90)
    rotation_mode = libyuv::kRotate90;
  else if (rotation == 180)
    rotation_mode = libyuv::kRotate180;
  else if (rotation == 270)
    rotation_mode = libyuv::kRotate270;

  switch (frame_format.pixel_format) {
    case media::PIXEL_FORMAT_UNKNOWN:  // Color format not set.
      break;
    case media::PIXEL_FORMAT_I420:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_I420;
      break;
    case media::PIXEL_FORMAT_YV12:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_YV12;
      break;
    case media::PIXEL_FORMAT_NV21:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_NV21;
      break;
    case media::PIXEL_FORMAT_YUY2:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_YUY2;
      break;
    case media::PIXEL_FORMAT_UYVY:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_UYVY;
      break;
    case media::PIXEL_FORMAT_RGB24:
      origin_colorspace = libyuv::FOURCC_24BG;
#if defined(OS_WIN)
      // TODO(wjia): Currently, for RGB24 on WIN, capture device always
      // passes in positive src_width and src_height. Remove this hardcoded
      // value when nagative src_height is supported. The negative src_height
      // indicates that vertical flipping is needed.
      flip = true;
#endif
      break;
    case media::PIXEL_FORMAT_ARGB:
      origin_colorspace = libyuv::FOURCC_ARGB;
      break;
    case media::PIXEL_FORMAT_MJPEG:
      origin_colorspace = libyuv::FOURCC_MJPG;
      break;
    default:
      NOTREACHED();
  }

  libyuv::ConvertToI420(data,
                        length,
                        yplane,
                        yplane_stride,
                        uplane,
                        uv_plane_stride,
                        vplane,
                        uv_plane_stride,
                        crop_x,
                        crop_y,
                        frame_format.frame_size.width(),
                        (flip ? -frame_format.frame_size.height() :
                                frame_format.frame_size.height()),
                        new_unrotated_width,
                        new_unrotated_height,
                        rotation_mode,
                        origin_colorspace);
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalPackedMemory(
          media::VideoFrame::I420,
          dimensions,
          gfx::Rect(dimensions),
          dimensions,
          yplane,
          media::VideoFrame::AllocationSize(media::VideoFrame::I420,
                                            dimensions),
          base::SharedMemory::NULLHandle(),
          base::TimeDelta(),
          base::Closure());
  DCHECK(frame);

  VideoCaptureFormat format(
      dimensions, frame_format.frame_rate, media::PIXEL_FORMAT_I420);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread,
          controller_,
          buffer,
          format,
          frame,
          timestamp));

  if (first_frame_) {
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.Width",
                         frame_format.frame_size.width());
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.Height",
                         frame_format.frame_size.height());
    UMA_HISTOGRAM_ASPECT_RATIO("Media.VideoCapture.AspectRatio",
                               frame_format.frame_size.width(),
                               frame_format.frame_size.height());
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.FrameRate",
                         frame_format.frame_rate);
    UMA_HISTOGRAM_ENUMERATION("Media.VideoCapture.PixelFormat",
                              frame_format.pixel_format,
                              media::PIXEL_FORMAT_MAX);
    first_frame_ = false;
  }
}

void
VideoCaptureController::VideoCaptureDeviceClient::OnIncomingCapturedVideoFrame(
    const scoped_refptr<Buffer>& buffer,
    const VideoCaptureFormat& buffer_format,
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks timestamp) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread,
          controller_,
          buffer,
          buffer_format,
          frame,
          timestamp));
}

void VideoCaptureController::VideoCaptureDeviceClient::OnError(
    const std::string& reason) {
  MediaStreamManager::SendMessageToNativeLog(
      "Error on video capture: " + reason);
  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoErrorOnIOThread, controller_));
}

void VideoCaptureController::VideoCaptureDeviceClient::OnLog(
    const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog("Video capture: " + message);
}

scoped_refptr<media::VideoCaptureDevice::Client::Buffer>
VideoCaptureController::VideoCaptureDeviceClient::DoReserveOutputBuffer(
    media::VideoFrame::Format format,
    const gfx::Size& dimensions) {
  size_t frame_bytes = 0;
  if (format == media::VideoFrame::NATIVE_TEXTURE) {
    DCHECK_EQ(dimensions.width(), 0);
    DCHECK_EQ(dimensions.height(), 0);
  } else {
    // The capture pipeline expects I420 for now.
    DCHECK_EQ(format, media::VideoFrame::I420)
        << "Non-I420 output buffer format " << format << " requested";
    frame_bytes = media::VideoFrame::AllocationSize(format, dimensions);
  }

  int buffer_id_to_drop = VideoCaptureBufferPool::kInvalidId;
  int buffer_id =
      buffer_pool_->ReserveForProducer(frame_bytes, &buffer_id_to_drop);
  if (buffer_id == VideoCaptureBufferPool::kInvalidId)
    return NULL;
  void* data;
  size_t size;
  buffer_pool_->GetBufferInfo(buffer_id, &data, &size);

  scoped_refptr<media::VideoCaptureDevice::Client::Buffer> output_buffer(
      new PoolBuffer(buffer_pool_, buffer_id, data, size));

  if (buffer_id_to_drop != VideoCaptureBufferPool::kInvalidId) {
    BrowserThread::PostTask(BrowserThread::IO,
        FROM_HERE,
        base::Bind(&VideoCaptureController::DoBufferDestroyedOnIOThread,
                   controller_, buffer_id_to_drop));
  }

  return output_buffer;
}

VideoCaptureController::~VideoCaptureController() {
  STLDeleteContainerPointers(controller_clients_.begin(),
                             controller_clients_.end());
}

void VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread(
    const scoped_refptr<media::VideoCaptureDevice::Client::Buffer>& buffer,
    const media::VideoCaptureFormat& buffer_format,
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(buffer->id(), VideoCaptureBufferPool::kInvalidId);

  int count = 0;
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    for (ControllerClients::iterator client_it = controller_clients_.begin();
         client_it != controller_clients_.end(); ++client_it) {
      ControllerClient* client = *client_it;
      if (client->session_closed)
        continue;

      if (frame->format() == media::VideoFrame::NATIVE_TEXTURE) {
        client->event_handler->OnMailboxBufferReady(client->controller_id,
                                                    buffer->id(),
                                                    *frame->mailbox_holder(),
                                                    buffer_format,
                                                    timestamp);
      } else {
        bool is_new_buffer = client->known_buffers.insert(buffer->id()).second;
        if (is_new_buffer) {
          // On the first use of a buffer on a client, share the memory handle.
          size_t memory_size = 0;
          base::SharedMemoryHandle remote_handle = buffer_pool_->ShareToProcess(
              buffer->id(), client->render_process_handle, &memory_size);
          client->event_handler->OnBufferCreated(
              client->controller_id, remote_handle, memory_size, buffer->id());
        }

        client->event_handler->OnBufferReady(
            client->controller_id, buffer->id(), buffer_format, timestamp);
      }

      bool inserted =
          client->active_buffers.insert(std::make_pair(buffer->id(), frame))
              .second;
      DCHECK(inserted) << "Unexpected duplicate buffer: " << buffer->id();
      count++;
    }
  }

  buffer_pool_->HoldForConsumers(buffer->id(), count);
}

void VideoCaptureController::DoErrorOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  state_ = VIDEO_CAPTURE_STATE_ERROR;

  for (ControllerClients::iterator client_it = controller_clients_.begin();
       client_it != controller_clients_.end(); ++client_it) {
    ControllerClient* client = *client_it;
    if (client->session_closed)
       continue;

    client->event_handler->OnError(client->controller_id);
  }
}

void VideoCaptureController::DoBufferDestroyedOnIOThread(
    int buffer_id_to_drop) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (ControllerClients::iterator client_it = controller_clients_.begin();
       client_it != controller_clients_.end(); ++client_it) {
    ControllerClient* client = *client_it;
    if (client->session_closed)
      continue;

    if (client->known_buffers.erase(buffer_id_to_drop)) {
      client->event_handler->OnBufferDestroyed(client->controller_id,
                                               buffer_id_to_drop);
    }
  }
}

VideoCaptureController::ControllerClient*
VideoCaptureController::FindClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* handler,
    const ControllerClients& clients) {
  for (ControllerClients::const_iterator client_it = clients.begin();
       client_it != clients.end(); ++client_it) {
    if ((*client_it)->controller_id == id &&
        (*client_it)->event_handler == handler) {
      return *client_it;
    }
  }
  return NULL;
}

VideoCaptureController::ControllerClient*
VideoCaptureController::FindClient(
    int session_id,
    const ControllerClients& clients) {
  for (ControllerClients::const_iterator client_it = clients.begin();
       client_it != clients.end(); ++client_it) {
    if ((*client_it)->session_id == session_id) {
      return *client_it;
    }
  }
  return NULL;
}

int VideoCaptureController::GetClientCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return controller_clients_.size();
}

}  // namespace content
