// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of an H.264 Decoded Picture Buffer
// used in H264 decoders.

#ifndef CONTENT_COMMON_GPU_MEDIA_H264_DPB_H_
#define CONTENT_COMMON_GPU_MEDIA_H264_DPB_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "media/filters/h264_parser.h"

namespace content {

// A picture (a frame or a field) in the H.264 spec sense.
// See spec at http://www.itu.int/rec/T-REC-H.264
struct H264Picture {
  enum Field {
    FIELD_NONE,
    FIELD_TOP,
    FIELD_BOTTOM,
  };

  // Values calculated per H.264 specification or taken from slice header.
  // See spec for more details on each (some names have been converted from
  // CamelCase in spec to Chromium-style names).
  int top_field_order_cnt;
  int bottom_field_order_cnt;
  int pic_order_cnt;
  int pic_order_cnt_msb;
  int pic_order_cnt_lsb;

  int pic_num;
  int long_term_pic_num;
  int frame_num;  // from slice header
  int frame_num_offset;
  int frame_num_wrap;
  int long_term_frame_idx;

  bool idr;  // IDR picture?
  bool ref;  // reference picture?
  bool long_term;  // long term reference picture?
  bool outputted;
  // Does memory management op 5 needs to be executed after this
  // picture has finished decoding?
  bool mem_mgmt_5;

  Field field;

  // Values from slice_hdr to be used during reference marking and
  // memory management after finishing this picture.
  bool long_term_reference_flag;
  bool adaptive_ref_pic_marking_mode_flag;
  media::H264DecRefPicMarking
      ref_pic_marking[media::H264SliceHeader::kRefListSize];

  typedef std::vector<H264Picture*> PtrVector;
};

// DPB - Decoded Picture Buffer.
// Stores decoded pictures that will be used for future display
// and/or reference.
class H264DPB {
 public:
  H264DPB();
  ~H264DPB();

  void set_max_num_pics(size_t max_num_pics);
  size_t max_num_pics() { return max_num_pics_; }

  // Remove unused (not reference and already outputted) pictures from DPB
  // and free it.
  void DeleteUnused();

  // Remove a picture by its pic_order_cnt and free it.
  void DeleteByPOC(int poc);

  // Clear DPB.
  void Clear();

  // Store picture in DPB. DPB takes ownership of its resources.
  void StorePic(H264Picture* pic);

  // Return the number of reference pictures in DPB.
  int CountRefPics();

  // Mark all pictures in DPB as unused for reference.
  void MarkAllUnusedForRef();

  // Return a short-term reference picture by its pic_num.
  H264Picture* GetShortRefPicByPicNum(int pic_num);

  // Return a long-term reference picture by its long_term_pic_num.
  H264Picture* GetLongRefPicByLongTermPicNum(int pic_num);

  // Return the short reference picture with lowest frame_num. Used for sliding
  // window memory management.
  H264Picture* GetLowestFrameNumWrapShortRefPic();

  // Append all pictures that have not been outputted yet to the passed |out|
  // vector, sorted by lowest pic_order_cnt (in output order).
  void GetNotOutputtedPicsAppending(H264Picture::PtrVector& out);

  // Append all short term reference pictures to the passed |out| vector.
  void GetShortTermRefPicsAppending(H264Picture::PtrVector& out);

  // Append all long term reference pictures to the passed |out| vector.
  void GetLongTermRefPicsAppending(H264Picture::PtrVector& out);

  // Iterators for direct access to DPB contents.
  // Will be invalidated after any of Remove* calls.
  typedef ScopedVector<H264Picture> Pictures;
  Pictures::iterator begin() { return pics_.begin(); }
  Pictures::iterator end() { return pics_.end(); }
  Pictures::reverse_iterator rbegin() { return pics_.rbegin(); }
  Pictures::reverse_iterator rend() { return pics_.rend(); }

  size_t size() const { return pics_.size(); }
  bool IsFull() const { return pics_.size() == max_num_pics_; }

  // Per H264 spec, increase to 32 if interlaced video is supported.
  enum { kDPBMaxSize = 16, };

 private:
  Pictures pics_;
  size_t max_num_pics_;

  DISALLOW_COPY_AND_ASSIGN(H264DPB);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_H264_DPB_H_
