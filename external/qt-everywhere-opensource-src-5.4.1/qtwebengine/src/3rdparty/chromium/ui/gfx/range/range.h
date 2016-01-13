// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RANGE_RANGE_H_
#define UI_GFX_RANGE_RANGE_H_

#include <ostream>
#include <string>

#include "base/basictypes.h"
#include "ui/gfx/gfx_export.h"

#if defined(OS_MACOSX)
#if __OBJC__
#import <Foundation/Foundation.h>
#else
typedef struct _NSRange NSRange;
#endif
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include <windows.h>
#include <richedit.h>
#endif

namespace gfx {

// A Range contains two integer values that represent a numeric range, like the
// range of characters in a text selection. A range is made of a start and end
// position; when they are the same, the Range is akin to a caret. Note that
// |start_| can be greater than |end_| to respect the directionality of the
// range.
class GFX_EXPORT Range {
 public:
  // Creates an empty range {0,0}.
  Range();

  // Initializes the range with a start and end.
  Range(size_t start, size_t end);

  // Initializes the range with the same start and end positions.
  explicit Range(size_t position);

  // Platform constructors.
#if defined(OS_MACOSX)
  explicit Range(const NSRange& range);
#elif defined(OS_WIN)
  // The |total_length| paramater should be used if the CHARRANGE is set to
  // {0,-1} to indicate the whole range.
  Range(const CHARRANGE& range, LONG total_length = -1);
#endif

  // Returns a range that is invalid, which is {size_t_max,size_t_max}.
  static const Range InvalidRange();

  // Checks if the range is valid through comparision to InvalidRange().
  bool IsValid() const;

  // Getters and setters.
  size_t start() const { return start_; }
  void set_start(size_t start) { start_ = start; }

  size_t end() const { return end_; }
  void set_end(size_t end) { end_ = end; }

  // Returns the absolute value of the length.
  size_t length() const {
    ptrdiff_t length = end() - start();
    return length >= 0 ? length : -length;
  }

  bool is_reversed() const { return start() > end(); }
  bool is_empty() const { return start() == end(); }

  // Returns the minimum and maximum values.
  size_t GetMin() const;
  size_t GetMax() const;

  bool operator==(const Range& other) const;
  bool operator!=(const Range& other) const;
  bool EqualsIgnoringDirection(const Range& other) const;

  // Returns true if this range intersects the specified |range|.
  bool Intersects(const Range& range) const;

  // Returns true if this range contains the specified |range|.
  bool Contains(const Range& range) const;

  // Computes the intersection of this range with the given |range|.
  // If they don't intersect, it returns an InvalidRange().
  // The returned range is always empty or forward (never reversed).
  Range Intersect(const Range& range) const;

#if defined(OS_MACOSX)
  Range& operator=(const NSRange& range);

  // NSRange does not store the directionality of a range, so if this
  // is_reversed(), the range will get flipped when converted to an NSRange.
  NSRange ToNSRange() const;
#elif defined(OS_WIN)
  CHARRANGE ToCHARRANGE() const;
#endif
  // GTK+ has no concept of a range.

  std::string ToString() const;

 private:
  size_t start_;
  size_t end_;
};

GFX_EXPORT std::ostream& operator<<(std::ostream& os, const Range& range);

}  // namespace gfx

#endif  // UI_GFX_RANGE_RANGE_H_
