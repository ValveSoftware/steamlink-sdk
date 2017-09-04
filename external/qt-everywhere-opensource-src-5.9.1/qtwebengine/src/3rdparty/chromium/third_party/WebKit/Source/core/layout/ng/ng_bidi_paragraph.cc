// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_bidi_paragraph.h"

#include "core/style/ComputedStyle.h"

namespace blink {

NGBidiParagraph::~NGBidiParagraph() {
  ubidi_close(ubidi_);
}

bool NGBidiParagraph::SetParagraph(const String& text,
                                   const ComputedStyle* block_style) {
  DCHECK(!ubidi_);
  ubidi_ = ubidi_open();
  UErrorCode error = U_ZERO_ERROR;
  ubidi_setPara(ubidi_, text.characters16(), text.length(),
                block_style->unicodeBidi() == Plaintext
                    ? UBIDI_DEFAULT_LTR
                    : (block_style->direction() == RTL ? UBIDI_RTL : UBIDI_LTR),
                nullptr, &error);
  return U_SUCCESS(error);
}

unsigned NGBidiParagraph::GetLogicalRun(unsigned start,
                                        UBiDiLevel* level) const {
  int32_t end;
  ubidi_getLogicalRun(ubidi_, start, &end, level);
  return end;
}

}  // namespace blink
