/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CompositionUnderline_h
#define CompositionUnderline_h

#include "core/CoreExport.h"
#include "platform/graphics/Color.h"
#include "wtf/Allocator.h"

namespace blink {

class CORE_EXPORT CompositionUnderline {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  CompositionUnderline(unsigned startOffset,
                       unsigned endOffset,
                       const Color&,
                       bool thick,
                       const Color& backgroundColor);

  unsigned startOffset() const { return m_startOffset; }
  unsigned endOffset() const { return m_endOffset; }
  const Color& color() const { return m_color; }
  bool thick() const { return m_thick; }
  const Color& backgroundColor() const { return m_backgroundColor; }

 private:
  unsigned m_startOffset;
  unsigned m_endOffset;
  Color m_color;
  bool m_thick;
  Color m_backgroundColor;
};

}  // namespace blink

#endif  // CompositionUnderline_h
