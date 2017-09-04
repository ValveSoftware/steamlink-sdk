// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutRubyRun_h
#define LineLayoutRubyRun_h

#include "core/layout/LayoutRubyRun.h"
#include "core/layout/api/LineLayoutBlockFlow.h"
#include "core/layout/api/LineLayoutRubyBase.h"
#include "core/layout/api/LineLayoutRubyText.h"

namespace blink {

class LineLayoutRubyRun : public LineLayoutBlockFlow {
 public:
  explicit LineLayoutRubyRun(LayoutRubyRun* layoutRubyRun)
      : LineLayoutBlockFlow(layoutRubyRun) {}

  explicit LineLayoutRubyRun(const LineLayoutItem& item)
      : LineLayoutBlockFlow(item) {
    SECURITY_DCHECK(!item || item.isRubyRun());
  }

  explicit LineLayoutRubyRun(std::nullptr_t) : LineLayoutBlockFlow(nullptr) {}

  LineLayoutRubyRun() {}

  void getOverhang(bool firstLine,
                   LineLayoutItem startLayoutItem,
                   LineLayoutItem endLayoutItem,
                   int& startOverhang,
                   int& endOverhang) const {
    toRubyRun()->getOverhang(firstLine, startLayoutItem.layoutObject(),
                             endLayoutItem.layoutObject(), startOverhang,
                             endOverhang);
  }

  LineLayoutRubyText rubyText() const {
    return LineLayoutRubyText(toRubyRun()->rubyText());
  }

  LineLayoutRubyBase rubyBase() const {
    return LineLayoutRubyBase(toRubyRun()->rubyBase());
  }

  bool canBreakBefore(const LazyLineBreakIterator& iterator) const {
    return toRubyRun()->canBreakBefore(iterator);
  }

 private:
  LayoutRubyRun* toRubyRun() { return toLayoutRubyRun(layoutObject()); }

  const LayoutRubyRun* toRubyRun() const {
    return toLayoutRubyRun(layoutObject());
  }
};

}  // namespace blink

#endif  // LineLayoutRubyRun_h
