// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of a class that provides H264 decode
// support for use with VAAPI hardware video decode acceleration on Intel
// systems.

#ifndef CONTENT_COMMON_GPU_MEDIA_VAAPI_H264_DECODER_H_
#define CONTENT_COMMON_GPU_MEDIA_VAAPI_H264_DECODER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/media/h264_dpb.h"
#include "content/common/gpu/media/vaapi_wrapper.h"
#include "media/base/limits.h"
#include "media/filters/h264_parser.h"

namespace content {

// An H264 decoder that utilizes VA-API. Provides features not supported by
// the VA-API userspace library (libva), including stream parsing, reference
// picture management and other operations not supported by the HW codec.
//
// Provides functionality to allow plugging VAAPI HW acceleration into the
// VDA framework.
//
// Clients of this class are expected to pass H264 Annex-B byte stream and
// will receive decoded surfaces via client-provided |OutputPicCB|.
//
// This class must be created, called and destroyed on a single thread, and
// does nothing internally on any other thread.
class CONTENT_EXPORT VaapiH264Decoder {
 public:
  // Callback invoked on the client when a surface is to be displayed.
  // Arguments: input buffer id provided at the time of Decode()
  // and VASurface to output.
  typedef base::Callback<
      void(int32, const scoped_refptr<VASurface>&)> OutputPicCB;

  enum VAVDAH264DecoderFailure {
    FRAME_MBS_ONLY_FLAG_NOT_ONE = 0,
    GAPS_IN_FRAME_NUM = 1,
    MID_STREAM_RESOLUTION_CHANGE = 2,
    INTERLACED_STREAM = 3,
    VAAPI_ERROR = 4,
    VAVDA_H264_DECODER_FAILURES_MAX,
  };

  // Callback to report errors for UMA purposes, not used to return errors
  // to clients.
  typedef base::Callback<void(VAVDAH264DecoderFailure error)>
      ReportErrorToUmaCB;

  // Decode result codes.
  enum DecResult {
    kDecodeError,  // Error while decoding.
    // TODO posciak: unsupported streams are currently treated as error
    // in decoding; in future it could perhaps be possible to fall back
    // to software decoding instead.
    // kStreamError,  // Error in stream.
    kAllocateNewSurfaces,  // Need a new set of surfaces to be allocated.
    kRanOutOfStreamData,  // Need more stream data to proceed.
    kRanOutOfSurfaces,  // Waiting for the client to free up output surfaces.
  };

  // |vaapi_wrapper| should be initialized.
  // |output_pic_cb| notifies the client a surface is to be displayed.
  // |report_error_to_uma_cb| called on errors for UMA purposes, not used
  // to report errors to clients.
  VaapiH264Decoder(VaapiWrapper* vaapi_wrapper,
                   const OutputPicCB& output_pic_cb,
                   const ReportErrorToUmaCB& report_error_to_uma_cb);

  ~VaapiH264Decoder();

  // Have the decoder flush its state and trigger output of all previously
  // decoded surfaces via OutputPicCB. Return false on failure.
  bool Flush() WARN_UNUSED_RESULT;

  // To be called during decoding.
  // Stop (pause) decoding, discarding all remaining inputs and outputs,
  // but do not flush decoder state, so that the playback can be resumed later,
  // possibly from a different location.
  void Reset();

  // Set current stream data pointer to |ptr| and |size|. Output surfaces
  // that are decoded from data in this stream chunk are to be returned along
  // with the given |input_id|.
  void SetStream(const uint8* ptr, size_t size, int32 input_id);

  // Try to decode more of the stream, returning decoded frames asynchronously
  // via output_pic_cb_. Return when more stream is needed, when we run out
  // of free surfaces, when we need a new set of them, or when an error occurs.
  DecResult Decode() WARN_UNUSED_RESULT;

  // Return dimensions/required number of output surfaces that client should
  // be ready to provide for the decoder to function properly.
  // To be used after Decode() returns kNeedNewSurfaces.
  gfx::Size GetPicSize() { return pic_size_; }
  size_t GetRequiredNumOfPictures();

  // To be used by the client to feed decoder with output surfaces.
  void ReuseSurface(const scoped_refptr<VASurface>& va_surface);

 private:
  // We need to keep at most kDPBMaxSize pictures in DPB for
  // reference/to display later and an additional one for the one currently
  // being decoded. We also ask for some additional ones since VDA needs
  // to accumulate a few ready-to-output pictures before it actually starts
  // displaying and giving them back. +2 instead of +1 because of subjective
  // smoothness improvement during testing.
  enum {
    kPicsInPipeline = media::limits::kMaxVideoFrames + 2,
    kMaxNumReqPictures = H264DPB::kDPBMaxSize + kPicsInPipeline,
  };

  // Internal state of the decoder.
  enum State {
    kNeedStreamMetadata,  // After initialization, need an SPS.
    kDecoding,  // Ready to decode from any point.
    kAfterReset, // After Reset(), need a resume point.
    kError,  // Error in decode, can't continue.
  };

  // Process H264 stream structures.
  bool ProcessSPS(int sps_id, bool* need_new_buffers);
  bool ProcessPPS(int pps_id);
  bool ProcessSlice(media::H264SliceHeader* slice_hdr);

  // Initialize the current picture according to data in |slice_hdr|.
  bool InitCurrPicture(media::H264SliceHeader* slice_hdr);

  // Calculate picture order counts for the new picture
  // on initialization of a new frame (see spec).
  bool CalculatePicOrderCounts(media::H264SliceHeader* slice_hdr);

  // Update PicNum values in pictures stored in DPB on creation of new
  // frame (see spec).
  void UpdatePicNums();

  bool UpdateMaxNumReorderFrames(const media::H264SPS* sps);

  // Prepare reference picture lists (ref_pic_list[01]_).
  bool PrepareRefPicLists(media::H264SliceHeader* slice_hdr);

  // Construct initial reference picture lists for use in decoding of
  // P and B pictures (see 8.2.4 in spec).
  void ConstructReferencePicListsP(media::H264SliceHeader* slice_hdr);
  void ConstructReferencePicListsB(media::H264SliceHeader* slice_hdr);

  // Helper functions for reference list construction, per spec.
  int PicNumF(H264Picture *pic);
  int LongTermPicNumF(H264Picture *pic);

  // Perform the reference picture lists' modification (reordering), as
  // specified in spec (8.2.4).
  //
  // |list| indicates list number and should be either 0 or 1.
  bool ModifyReferencePicList(media::H264SliceHeader* slice_hdr, int list);

  // Perform reference picture memory management operations (marking/unmarking
  // of reference pictures, long term picture management, discarding, etc.).
  // See 8.2.5 in spec.
  bool HandleMemoryManagementOps();
  void ReferencePictureMarking();

  // Start processing a new frame.
  bool StartNewFrame(media::H264SliceHeader* slice_hdr);

  // All data for a frame received, process it and decode.
  bool FinishPrevFrameIfPresent();

  // Called after decoding, performs all operations to be done after decoding,
  // including DPB management, reference picture marking and memory management
  // operations.
  // This will also output a picture if one is ready for output.
  bool FinishPicture();

  // Clear DPB contents and remove all surfaces in DPB from *in_use_ list.
  // Cleared pictures will be made available for decode, unless they are
  // at client waiting to be displayed.
  void ClearDPB();

  // These queue up data for HW decoder to be committed on running HW decode.
  bool SendPPS();
  bool SendIQMatrix();
  bool SendVASliceParam(media::H264SliceHeader* slice_hdr);
  bool SendSliceData(const uint8* ptr, size_t size);
  bool QueueSlice(media::H264SliceHeader* slice_hdr);

  // Helper methods for filling HW structures.
  void FillVAPicture(VAPictureH264 *va_pic, H264Picture* pic);
  int FillVARefFramesFromDPB(VAPictureH264 *va_pics, int num_pics);

  // Commits all pending data for HW decoder and starts HW decoder.
  bool DecodePicture();

  // Notifies client that a picture is ready for output.
  bool OutputPic(H264Picture* pic);

  // Output all pictures in DPB that have not been outputted yet.
  bool OutputAllRemainingPics();

  // Represents a frame being decoded. Will always have a VASurface
  // assigned to it, which will eventually contain decoded picture data.
  class DecodeSurface;

  // Assign an available surface to the given PicOrderCnt |poc|,
  // removing it from the available surfaces pool. Return true if a surface
  // has been found, false otherwise.
  bool AssignSurfaceToPoC(int32 input_id, int poc);

  // Indicate that a surface is no longer needed by decoder.
  void UnassignSurfaceFromPoC(int poc);

  // Return DecodeSurface assigned to |poc|.
  DecodeSurface* DecodeSurfaceByPoC(int poc);

  // Decoder state.
  State state_;

  // Parser in use.
  media::H264Parser parser_;

  // DPB in use.
  H264DPB dpb_;

  // Picture currently being processed/decoded.
  scoped_ptr<H264Picture> curr_pic_;

  // Reference picture lists, constructed for each picture before decoding.
  // Those lists are not owners of the pointers (DPB is).
  H264Picture::PtrVector ref_pic_list0_;
  H264Picture::PtrVector ref_pic_list1_;

  // Global state values, needed in decoding. See spec.
  int max_pic_order_cnt_lsb_;
  int max_frame_num_;
  int max_pic_num_;
  int max_long_term_frame_idx_;
  size_t max_num_reorder_frames_;

  int frame_num_;
  int prev_frame_num_;
  int prev_frame_num_offset_;
  bool prev_has_memmgmnt5_;

  // Values related to previously decoded reference picture.
  bool prev_ref_has_memmgmnt5_;
  int prev_ref_top_field_order_cnt_;
  int prev_ref_pic_order_cnt_msb_;
  int prev_ref_pic_order_cnt_lsb_;
  H264Picture::Field prev_ref_field_;

  // Currently active SPS and PPS.
  int curr_sps_id_;
  int curr_pps_id_;

  // Output picture size.
  gfx::Size pic_size_;

  // Maps H.264 PicOrderCount to currently used DecodeSurfaces;
  typedef std::map<int, linked_ptr<DecodeSurface> > DecSurfacesInUse;
  DecSurfacesInUse decode_surfaces_in_use_;

  // Unused VA surfaces returned by client, ready to be reused.
  std::vector<scoped_refptr<VASurface> > available_va_surfaces_;

  // The id of current input buffer, which will be associated with an
  // output surface when a frame is successfully decoded.
  int32 curr_input_id_;

  VaapiWrapper* vaapi_wrapper_;

  // Called by decoder when a surface should be outputted.
  OutputPicCB output_pic_cb_;

  // Called to report decoding error to UMA, not used to indicate errors
  // to clients.
  ReportErrorToUmaCB report_error_to_uma_cb_;

  // PicOrderCount of the previously outputted frame.
  int last_output_poc_;

  DISALLOW_COPY_AND_ASSIGN(VaapiH264Decoder);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VAAPI_H264_DECODER_H_
