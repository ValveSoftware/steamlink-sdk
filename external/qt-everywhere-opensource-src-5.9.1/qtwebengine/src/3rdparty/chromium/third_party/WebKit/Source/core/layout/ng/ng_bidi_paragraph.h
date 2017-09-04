// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBidiParagraph_h
#define NGBidiParagraph_h

#include "wtf/Allocator.h"
#include "wtf/Forward.h"

#include <unicode/ubidi.h>

namespace blink {

class ComputedStyle;

// NGBidiParagraph resolves bidirectional runs in a paragraph using ICU BiDi.
// http://userguide.icu-project.org/transforms/bidi
//
// Given a string of a paragraph, it runs Unicode Bidirectional Algorithm in
// UAX#9 and create logical runs.
// http://unicode.org/reports/tr9/
// It can also create visual runs once lines breaks are determined.
class NGBidiParagraph {
  STACK_ALLOCATED();

 public:
  NGBidiParagraph() {}
  ~NGBidiParagraph();

  // Splits the given paragraph to bidi runs and resolves the bidi embedding
  // level of each run.
  // Returns false on failure. Nothing other than the destructor should be
  // called.
  bool SetParagraph(const String&, const ComputedStyle*);

  // Returns whether the paragrpah is LTR, RTL, or MIXED.
  UBiDiDirection Direction() const { return ubidi_getDirection(ubidi_); }

  // Returns the end offset of a logical run that starts from the |start|
  // offset.
  unsigned GetLogicalRun(unsigned start, UBiDiLevel*) const;

 private:
  UBiDi* ubidi_ = nullptr;
};

}  // namespace blink

#endif  // NGBidiParagraph_h
