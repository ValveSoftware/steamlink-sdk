/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderedDocumentMarker_h
#define RenderedDocumentMarker_h

#include "core/editing/markers/DocumentMarker.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

class RenderedDocumentMarker final : public DocumentMarker {
 private:
  enum class State { Invalid, ValidNull, ValidNotNull };

 public:
  static RenderedDocumentMarker* create(const DocumentMarker& marker) {
    return new RenderedDocumentMarker(marker);
  }

  bool isRendered() const { return m_state == State::ValidNotNull; }
  bool contains(const LayoutPoint& point) const {
    DCHECK_EQ(m_state, State::ValidNotNull);
    return m_renderedRect.contains(point);
  }
  void setRenderedRect(const LayoutRect& rect) {
    if (m_state == State::ValidNotNull && rect == m_renderedRect)
      return;
    m_state = State::ValidNotNull;
    m_renderedRect = rect;
  }

  const LayoutRect& renderedRect() const {
    DCHECK_EQ(m_state, State::ValidNotNull);
    return m_renderedRect;
  }

  void nullifyRenderedRect() {
    m_state = State::ValidNull;
    // Now |m_renderedRect| can not be accessed until |setRenderedRect| is
    // called.
  }

  void invalidate() { m_state = State::Invalid; }
  bool isValid() const { return m_state != State::Invalid; }

 private:
  explicit RenderedDocumentMarker(const DocumentMarker& marker)
      : DocumentMarker(marker), m_state(State::Invalid) {}

  LayoutRect m_renderedRect;
  State m_state;
};

DEFINE_TYPE_CASTS(RenderedDocumentMarker, DocumentMarker, marker, true, true);

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::RenderedDocumentMarker);

#endif
