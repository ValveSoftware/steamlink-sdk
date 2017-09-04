// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollbarManager_h
#define ScrollbarManager_h

#include "core/CoreExport.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

class CORE_EXPORT ScrollbarManager {
  DISALLOW_NEW();

  // Helper class to manage the life cycle of Scrollbar objects.
 public:
  ScrollbarManager(ScrollableArea&);

  void dispose();

  Scrollbar* horizontalScrollbar() const {
    return m_hBarIsAttached ? m_hBar.get() : nullptr;
  }
  Scrollbar* verticalScrollbar() const {
    return m_vBarIsAttached ? m_vBar.get() : nullptr;
  }
  bool hasHorizontalScrollbar() const { return horizontalScrollbar(); }
  bool hasVerticalScrollbar() const { return verticalScrollbar(); }

  // These functions are used to create/destroy scrollbars.
  virtual void setHasHorizontalScrollbar(bool hasScrollbar) = 0;
  virtual void setHasVerticalScrollbar(bool hasScrollbar) = 0;

  DECLARE_VIRTUAL_TRACE();

 protected:
  // TODO(ymalik): This can be made non-virtual since there's a lot of
  // common code in subclasses.
  virtual Scrollbar* createScrollbar(ScrollbarOrientation) = 0;
  virtual void destroyScrollbar(ScrollbarOrientation) = 0;

 protected:
  Member<ScrollableArea> m_scrollableArea;

  // The scrollbars associated with m_scrollableArea. Both can nullptr.
  Member<Scrollbar> m_hBar;
  Member<Scrollbar> m_vBar;

  unsigned m_hBarIsAttached : 1;
  unsigned m_vBarIsAttached : 1;
};

}  // namespace blink

#endif  // ScrollbarManager_h
