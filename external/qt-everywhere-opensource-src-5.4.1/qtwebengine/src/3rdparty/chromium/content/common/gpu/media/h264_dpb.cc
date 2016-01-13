// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/common/gpu/media/h264_dpb.h"

namespace content {

H264DPB::H264DPB() : max_num_pics_(0) {}
H264DPB::~H264DPB() {}

void H264DPB::Clear() {
  pics_.clear();
}

void H264DPB::set_max_num_pics(size_t max_num_pics) {
  DCHECK_LE(max_num_pics, kDPBMaxSize);
  max_num_pics_ = max_num_pics;
  if (pics_.size() > max_num_pics_)
    pics_.resize(max_num_pics_);
}

void H264DPB::DeleteByPOC(int poc) {
  for (Pictures::iterator it = pics_.begin(); it != pics_.end(); ++it) {
    if ((*it)->pic_order_cnt == poc) {
      pics_.erase(it);
      return;
    }
  }
  NOTREACHED() << "Missing POC: " << poc;
}

void H264DPB::DeleteUnused() {
  for (Pictures::iterator it = pics_.begin(); it != pics_.end(); ) {
    if ((*it)->outputted && !(*it)->ref)
      it = pics_.erase(it);
    else
      ++it;
  }
}

void H264DPB::StorePic(H264Picture* pic) {
  DCHECK_LT(pics_.size(), max_num_pics_);
  DVLOG(3) << "Adding PicNum: " << pic->pic_num << " ref: " << (int)pic->ref
           << " longterm: " << (int)pic->long_term << " to DPB";
  pics_.push_back(pic);
}

int H264DPB::CountRefPics() {
  int ret = 0;
  for (size_t i = 0; i < pics_.size(); ++i) {
    if (pics_[i]->ref)
      ++ret;
  }
  return ret;
}

void H264DPB::MarkAllUnusedForRef() {
  for (size_t i = 0; i < pics_.size(); ++i)
    pics_[i]->ref = false;
}

H264Picture* H264DPB::GetShortRefPicByPicNum(int pic_num) {
  for (size_t i = 0; i < pics_.size(); ++i) {
    H264Picture* pic = pics_[i];
    if (pic->ref && !pic->long_term && pic->pic_num == pic_num)
      return pic;
  }

  DVLOG(1) << "Missing short ref pic num: " << pic_num;
  return NULL;
}

H264Picture* H264DPB::GetLongRefPicByLongTermPicNum(int pic_num) {
  for (size_t i = 0; i < pics_.size(); ++i) {
    H264Picture* pic = pics_[i];
    if (pic->ref && pic->long_term && pic->long_term_pic_num == pic_num)
      return pic;
  }

  DVLOG(1) << "Missing long term pic num: " << pic_num;
  return NULL;
}

H264Picture* H264DPB::GetLowestFrameNumWrapShortRefPic() {
  H264Picture* ret = NULL;
  for (size_t i = 0; i < pics_.size(); ++i) {
    H264Picture* pic = pics_[i];
    if (pic->ref && !pic->long_term &&
        (!ret || pic->frame_num_wrap < ret->frame_num_wrap))
      ret = pic;
  }
  return ret;
}

void H264DPB::GetNotOutputtedPicsAppending(H264Picture::PtrVector& out) {
  for (size_t i = 0; i < pics_.size(); ++i) {
    H264Picture* pic = pics_[i];
    if (!pic->outputted)
      out.push_back(pic);
  }
}

void H264DPB::GetShortTermRefPicsAppending(H264Picture::PtrVector& out) {
  for (size_t i = 0; i < pics_.size(); ++i) {
    H264Picture* pic = pics_[i];
    if (pic->ref && !pic->long_term)
      out.push_back(pic);
  }
}

void H264DPB::GetLongTermRefPicsAppending(H264Picture::PtrVector& out) {
  for (size_t i = 0; i < pics_.size(); ++i) {
    H264Picture* pic = pics_[i];
    if (pic->ref && pic->long_term)
      out.push_back(pic);
  }
}

}  // namespace content
