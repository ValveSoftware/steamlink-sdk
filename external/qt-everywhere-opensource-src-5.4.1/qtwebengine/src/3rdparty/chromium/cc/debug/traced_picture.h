// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_TRACED_PICTURE_H_
#define CC_DEBUG_TRACED_PICTURE_H_

#include <string>

#include "base/debug/trace_event.h"
#include "base/memory/scoped_ptr.h"
#include "cc/resources/picture.h"

namespace cc {

class TracedPicture : public base::debug::ConvertableToTraceFormat {
 public:
  explicit TracedPicture(scoped_refptr<const Picture>);

  static scoped_refptr<base::debug::ConvertableToTraceFormat>
      AsTraceablePicture(const Picture* picture);

  static scoped_refptr<base::debug::ConvertableToTraceFormat>
      AsTraceablePictureAlias(const Picture* original);

  virtual void AppendAsTraceFormat(std::string* out) const OVERRIDE;

 private:
  virtual ~TracedPicture();

  void AppendPicture(std::string* out) const;
  void AppendPictureAlias(std::string* out) const;

  scoped_refptr<const Picture> picture_;
  bool is_alias_;

  DISALLOW_COPY_AND_ASSIGN(TracedPicture);
};

}  // namespace cc

#endif  // CC_DEBUG_TRACED_PICTURE_H_
