// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of VideoDecoderAccelerator
// that utilizes hardware video decoder present on Intel CPUs.

#ifndef CONTENT_COMMON_GPU_MEDIA_VAAPI_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_VAAPI_VIDEO_DECODE_ACCELERATOR_H_

#include <map>
#include <queue>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/common/gpu/media/vaapi_h264_decoder.h"
#include "content/common/gpu/media/vaapi_wrapper.h"
#include "media/base/bitstream_buffer.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gl/gl_bindings.h"

namespace content {

// Class to provide video decode acceleration for Intel systems with hardware
// support for it, and on which libva is available.
// Decoding tasks are performed in a separate decoding thread.
//
// Threading/life-cycle: this object is created & destroyed on the GPU
// ChildThread.  A few methods on it are called on the decoder thread which is
// stopped during |this->Destroy()|, so any tasks posted to the decoder thread
// can assume |*this| is still alive.  See |weak_this_| below for more details.
class CONTENT_EXPORT VaapiVideoDecodeAccelerator
    : public media::VideoDecodeAccelerator {
 public:
  VaapiVideoDecodeAccelerator(
      Display* x_display,
      const base::Callback<bool(void)>& make_context_current);
  virtual ~VaapiVideoDecodeAccelerator();

  // media::VideoDecodeAccelerator implementation.
  virtual bool Initialize(media::VideoCodecProfile profile,
                          Client* client) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool CanDecodeOnIOThread() OVERRIDE;

private:
  // Notify the client that an error has occurred and decoding cannot continue.
  void NotifyError(Error error);

  // Map the received input buffer into this process' address space and
  // queue it for decode.
  void MapAndQueueNewInputBuffer(
      const media::BitstreamBuffer& bitstream_buffer);

  // Get a new input buffer from the queue and set it up in decoder. This will
  // sleep if no input buffers are available. Return true if a new buffer has
  // been set up, false if an early exit has been requested (due to initiated
  // reset/flush/destroy).
  bool GetInputBuffer_Locked();

  // Signal the client that the current buffer has been read and can be
  // returned. Will also release the mapping.
  void ReturnCurrInputBuffer_Locked();

  // Pass one or more output buffers to the decoder. This will sleep
  // if no buffers are available. Return true if buffers have been set up or
  // false if an early exit has been requested (due to initiated
  // reset/flush/destroy).
  bool FeedDecoderWithOutputSurfaces_Locked();

  // Continue decoding given input buffers and sleep waiting for input/output
  // as needed. Will exit if a new set of surfaces or reset/flush/destroy
  // is requested.
  void DecodeTask();

  // Scheduled after receiving a flush request and executed after the current
  // decoding task finishes decoding pending inputs. Makes the decoder return
  // all remaining output pictures and puts it in an idle state, ready
  // to resume if needed and schedules a FinishFlush.
  void FlushTask();

  // Scheduled by the FlushTask after decoder is flushed to put VAVDA into idle
  // state and notify the client that flushing has been finished.
  void FinishFlush();

  // Scheduled after receiving a reset request and executed after the current
  // decoding task finishes decoding the current frame. Puts the decoder into
  // an idle state, ready to resume if needed, discarding decoded but not yet
  // outputted pictures (decoder keeps ownership of their associated picture
  // buffers). Schedules a FinishReset afterwards.
  void ResetTask();

  // Scheduled by ResetTask after it's done putting VAVDA into an idle state.
  // Drops remaining input buffers and notifies the client that reset has been
  // finished.
  void FinishReset();

  // Helper for Destroy(), doing all the actual work except for deleting self.
  void Cleanup();

  // Get a usable framebuffer configuration for use in binding textures
  // or return false on failure.
  bool InitializeFBConfig();

  // Callback for the decoder to execute when it wants us to output given
  // |va_surface|.
  void SurfaceReady(int32 input_id, const scoped_refptr<VASurface>& va_surface);

  // Represents a texture bound to an X Pixmap for output purposes.
  class TFPPicture;

  // Callback to be executed once we have a |va_surface| to be output and
  // an available |tfp_picture| to use for output.
  // Puts contents of |va_surface| into given |tfp_picture|, releases the
  // surface and passes the resulting picture to client for output.
  void OutputPicture(const scoped_refptr<VASurface>& va_surface,
                     int32 input_id,
                     TFPPicture* tfp_picture);

  // Try to OutputPicture() if we have both a ready surface and picture.
  void TryOutputSurface();

  // Called when a VASurface is no longer in use by the decoder or is not being
  // synced/waiting to be synced to a picture. Returns it to available surfaces
  // pool.
  void RecycleVASurfaceID(VASurfaceID va_surface_id);

  // Initiate wait cycle for surfaces to be released before we release them
  // and allocate new ones, as requested by the decoder.
  void InitiateSurfaceSetChange(size_t num_pics, gfx::Size size);
  // Check if the surfaces have been released or post ourselves for later.
  void TryFinishSurfaceSetChange();

  // Client-provided X/GLX state.
  Display* x_display_;
  base::Callback<bool(void)> make_context_current_;
  GLXFBConfig fb_config_;

  // VAVDA state.
  enum State {
    // Initialize() not called yet or failed.
    kUninitialized,
    // DecodeTask running.
    kDecoding,
    // Resetting, waiting for decoder to finish current task and cleanup.
    kResetting,
    // Flushing, waiting for decoder to finish current task and cleanup.
    kFlushing,
    // Idle, decoder in state ready to start/resume decoding.
    kIdle,
    // Destroying, waiting for the decoder to finish current task.
    kDestroying,
  };

  // Protects input buffer and surface queues and state_.
  base::Lock lock_;
  State state_;

  // An input buffer awaiting consumption, provided by the client.
  struct InputBuffer {
    InputBuffer();
    ~InputBuffer();

    int32 id;
    size_t size;
    scoped_ptr<base::SharedMemory> shm;
  };

  // Queue for incoming input buffers.
  typedef std::queue<linked_ptr<InputBuffer> > InputBuffers;
  InputBuffers input_buffers_;
  // Signalled when input buffers are queued onto the input_buffers_ queue.
  base::ConditionVariable input_ready_;

  // Current input buffer at decoder.
  linked_ptr<InputBuffer> curr_input_buffer_;

  // Queue for incoming output buffers (texture ids).
  typedef std::queue<int32> OutputBuffers;
  OutputBuffers output_buffers_;

  typedef std::map<int32, linked_ptr<TFPPicture> > TFPPictures;
  // All allocated TFPPictures, regardless of their current state. TFPPictures
  // are allocated once and destroyed at the end of decode.
  TFPPictures tfp_pictures_;

  // Return a TFPPicture associated with given client-provided id.
  TFPPicture* TFPPictureById(int32 picture_buffer_id);

  // VA Surfaces no longer in use that can be passed back to the decoder for
  // reuse, once it requests them.
  std::list<VASurfaceID> available_va_surfaces_;
  // Signalled when output surfaces are queued onto the available_va_surfaces_
  // queue.
  base::ConditionVariable surfaces_available_;

  // Pending output requests from the decoder. When it indicates that we should
  // output a surface and we have an available TFPPicture (i.e. texture) ready
  // to use, we'll execute the callback passing the TFPPicture. The callback
  // will put the contents of the surface into the picture and return it to
  // the client, releasing the surface as well.
  // If we don't have any available TFPPictures at the time when the decoder
  // requests output, we'll store the request on pending_output_cbs_ queue for
  // later and run it once the client gives us more textures
  // via ReusePictureBuffer().
  typedef base::Callback<void(TFPPicture*)> OutputCB;
  std::queue<OutputCB> pending_output_cbs_;

  // ChildThread's message loop
  base::MessageLoop* message_loop_;

  // WeakPtr<> pointing to |this| for use in posting tasks from the decoder
  // thread back to the ChildThread.  Because the decoder thread is a member of
  // this class, any task running on the decoder thread is guaranteed that this
  // object is still alive.  As a result, tasks posted from ChildThread to
  // decoder thread should use base::Unretained(this), and tasks posted from the
  // decoder thread to the ChildThread should use |weak_this_|.
  base::WeakPtr<VaapiVideoDecodeAccelerator> weak_this_;

  // Callback used when creating VASurface objects.
  VASurface::ReleaseCB va_surface_release_cb_;

  // To expose client callbacks from VideoDecodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on message_loop_.
  scoped_ptr<base::WeakPtrFactory<Client> > client_ptr_factory_;
  base::WeakPtr<Client> client_;

  scoped_ptr<VaapiWrapper> vaapi_wrapper_;

  // Comes after vaapi_wrapper_ to ensure its destructor is executed before
  // vaapi_wrapper_ is destroyed.
  scoped_ptr<VaapiH264Decoder> decoder_;
  base::Thread decoder_thread_;
  // Use this to post tasks to |decoder_thread_| instead of
  // |decoder_thread_.message_loop()| because the latter will be NULL once
  // |decoder_thread_.Stop()| returns.
  scoped_refptr<base::MessageLoopProxy> decoder_thread_proxy_;

  int num_frames_at_client_;
  int num_stream_bufs_at_decoder_;

  // Whether we are waiting for any pending_output_cbs_ to be run before
  // NotifyingFlushDone.
  bool finish_flush_pending_;

  // Decoder requested a new surface set and we are waiting for all the surfaces
  // to be returned before we can free them.
  bool awaiting_va_surfaces_recycle_;

  // Last requested number/resolution of output picture buffers.
  size_t requested_num_pics_;
  gfx::Size requested_pic_size_;

  // The WeakPtrFactory for |weak_this_|.
  base::WeakPtrFactory<VaapiVideoDecodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(VaapiVideoDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VAAPI_VIDEO_DECODE_ACCELERATOR_H_
