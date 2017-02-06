// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_VAAPI_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <queue>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/threading/thread.h"
#include "media/filters/h264_bitstream_buffer.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/va_surface.h"
#include "media/gpu/vaapi_wrapper.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

// A VideoEncodeAccelerator implementation that uses VA-API
// (http://www.freedesktop.org/wiki/Software/vaapi) for HW-accelerated
// video encode.
class MEDIA_GPU_EXPORT VaapiVideoEncodeAccelerator
    : public VideoEncodeAccelerator {
 public:
  VaapiVideoEncodeAccelerator();
  ~VaapiVideoEncodeAccelerator() override;

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(VideoPixelFormat format,
                  const gfx::Size& input_visible_size,
                  VideoCodecProfile output_profile,
                  uint32_t initial_bitrate,
                  Client* client) override;
  void Encode(const scoped_refptr<VideoFrame>& frame,
              bool force_keyframe) override;
  void UseOutputBitstreamBuffer(const BitstreamBuffer& buffer) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate) override;
  void Destroy() override;

 private:
  // Reference picture list.
  typedef std::list<scoped_refptr<VASurface>> RefPicList;

  // Encode job for one frame. Created when an input frame is awaiting and
  // enough resources are available to proceed. Once the job is prepared and
  // submitted to the hardware, it awaits on the submitted_encode_jobs_ queue
  // for an output bitstream buffer to become available. Once one is ready,
  // the encoded bytes are downloaded to it and job resources are released
  // and become available for reuse.
  struct EncodeJob {
    // Input surface for video frame data.
    scoped_refptr<VASurface> input_surface;
    // Surface for a reconstructed picture, which is used for reference
    // for subsequent frames.
    scoped_refptr<VASurface> recon_surface;
    // Buffer that will contain output bitstream for this frame.
    VABufferID coded_buffer;
    // Reference surfaces required to encode this picture. We keep references
    // to them here, because we may discard some of them from ref_pic_list*
    // before the HW job is done.
    RefPicList reference_surfaces;
    // True if this job will produce a keyframe. Used to report
    // to BitstreamBufferReady().
    bool keyframe;
    // Source timestamp.
    base::TimeDelta timestamp;

    EncodeJob();
    ~EncodeJob();
  };

  // Encoder state.
  enum State {
    kUninitialized,
    kEncoding,
    kError,
  };

  // Holds input frames coming from the client ready to be encoded.
  struct InputFrameRef;
  // Holds output buffers coming from the client ready to be filled.
  struct BitstreamBufferRef;

  // Tasks for each of the VEA interface calls to be executed on the
  // encoder thread.
  void InitializeTask();
  void EncodeTask(const scoped_refptr<VideoFrame>& frame, bool force_keyframe);
  void UseOutputBitstreamBufferTask(
      std::unique_ptr<BitstreamBufferRef> buffer_ref);
  void RequestEncodingParametersChangeTask(uint32_t bitrate,
                                           uint32_t framerate);
  void DestroyTask();

  // Prepare and schedule an encode job if we have an input to encode
  // and enough resources to proceed.
  void EncodeFrameTask();

  // Fill current_sps_/current_pps_ with current values.
  void UpdateSPS();
  void UpdatePPS();
  void UpdateRates(uint32_t bitrate, uint32_t framerate);

  // Generate packed SPS and PPS in packed_sps_/packed_pps_, using
  // values in current_sps_/current_pps_.
  void GeneratePackedSPS();
  void GeneratePackedPPS();

  // Check if we have sufficient resources for a new encode job, claim them and
  // fill current_encode_job_ with them.
  // Return false if we cannot start a new job yet, true otherwise.
  bool PrepareNextJob(base::TimeDelta timestamp);

  // Begin a new frame, making it a keyframe if |force_keyframe| is true,
  // updating current_pic_.
  void BeginFrame(bool force_keyframe);

  // End current frame, updating reference picture lists and storing current
  // job in the jobs awaiting completion on submitted_encode_jobs_.
  void EndFrame();

  // Submit parameters for the current frame to the hardware.
  bool SubmitFrameParameters();
  // Submit keyframe headers to the hardware if the current frame is a keyframe.
  bool SubmitHeadersIfNeeded();

  // Upload image data from |frame| to the input surface for current job.
  bool UploadFrame(const scoped_refptr<VideoFrame>& frame);

  // Execute encode in hardware. This does not block and will return before
  // the job is finished.
  bool ExecuteEncode();

  // Callback that returns a no longer used VASurfaceID to
  // available_va_surface_ids_ for reuse.
  void RecycleVASurfaceID(VASurfaceID va_surface_id);

  // Tries to return a bitstream buffer if both a submitted job awaits to
  // be completed and we have bitstream buffers from the client available
  // to download the encoded data to.
  void TryToReturnBitstreamBuffer();

  // Puts the encoder into en error state and notifies client about the error.
  void NotifyError(Error error);

  // Sets the encoder state on the correct thread.
  void SetState(State state);

  // VaapiWrapper is the owner of all HW resources (surfaces and buffers)
  // and will free them on destruction.
  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  // Input profile and sizes.
  VideoCodecProfile profile_;
  gfx::Size visible_size_;
  gfx::Size coded_size_;  // Macroblock-aligned.
  // Width/height in macroblocks.
  unsigned int mb_width_;
  unsigned int mb_height_;

  // Maximum size of the reference list 0.
  unsigned int max_ref_idx_l0_size_;

  // Initial QP.
  unsigned int qp_;

  // IDR frame period.
  unsigned int idr_period_;
  // I frame period.
  unsigned int i_period_;
  // IP period, i.e. how often do we need to have either an I or a P frame in
  // the stream. Period of 1 means we can have no B frames.
  unsigned int ip_period_;

  // Size in bytes required for input bitstream buffers.
  size_t output_buffer_byte_size_;

  // All of the members below must be accessed on the encoder_thread_,
  // while it is running.

  // Encoder state. Encode tasks will only run in kEncoding state.
  State state_;

  // frame_num to be used for the next frame.
  unsigned int frame_num_;
  // idr_pic_id to be used for the next frame.
  unsigned int idr_pic_id_;

  // Current bitrate in bps.
  unsigned int bitrate_;
  // Current fps.
  unsigned int framerate_;
  // CPB size in bits, i.e. bitrate in kbps * window size in ms/1000.
  unsigned int cpb_size_;
  // True if the parameters have changed and we need to submit a keyframe
  // with updated parameters.
  bool encoding_parameters_changed_;

  // Job currently being prepared for encode.
  std::unique_ptr<EncodeJob> current_encode_job_;

  // Current SPS, PPS and their packed versions. Packed versions are their NALUs
  // in AnnexB format *without* emulation prevention three-byte sequences
  // (those will be added by the driver).
  H264SPS current_sps_;
  H264BitstreamBuffer packed_sps_;
  H264PPS current_pps_;
  H264BitstreamBuffer packed_pps_;

  // Picture currently being prepared for encode.
  scoped_refptr<H264Picture> current_pic_;

  // VA surfaces available for reuse.
  std::vector<VASurfaceID> available_va_surface_ids_;

  // VA buffers for coded frames.
  std::vector<VABufferID> available_va_buffer_ids_;

  // Currently active reference surfaces.
  RefPicList ref_pic_list0_;

  // Callback via which finished VA surfaces are returned to us.
  VASurface::ReleaseCB va_surface_release_cb_;

  // VideoFrames passed from the client, waiting to be encoded.
  std::queue<linked_ptr<InputFrameRef>> encoder_input_queue_;

  // BitstreamBuffers mapped, ready to be filled.
  std::queue<linked_ptr<BitstreamBufferRef>> available_bitstream_buffers_;

  // Jobs submitted for encode, awaiting bitstream buffers to become available.
  std::queue<linked_ptr<EncodeJob>> submitted_encode_jobs_;

  // Encoder thread. All tasks are executed on it.
  base::Thread encoder_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> encoder_thread_task_runner_;

  const scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_task_runner_.
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;
  base::WeakPtr<Client> client_;

  // WeakPtr to post from the encoder thread back to the ChildThread, as it may
  // outlive this. Posting from the ChildThread using base::Unretained(this)
  // to the encoder thread is safe, because |this| always outlives the encoder
  // thread (it's a member of this class).
  base::WeakPtr<VaapiVideoEncodeAccelerator> weak_this_;
  base::WeakPtrFactory<VaapiVideoEncodeAccelerator> weak_this_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VaapiVideoEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VIDEO_ENCODE_ACCELERATOR_H_
