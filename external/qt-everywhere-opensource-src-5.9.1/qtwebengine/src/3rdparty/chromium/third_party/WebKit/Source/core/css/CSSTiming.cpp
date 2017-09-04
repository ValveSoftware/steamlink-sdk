// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSTiming.h"

#include "core/paint/PaintTiming.h"

namespace blink {

static const char kSupplementName[] = "CSSTiming";

CSSTiming& CSSTiming::from(Document& document) {
  CSSTiming* timing = static_cast<CSSTiming*>(
      Supplement<Document>::from(document, kSupplementName));
  if (!timing) {
    timing = new CSSTiming(document);
    Supplement<Document>::provideTo(document, kSupplementName, timing);
  }
  return *timing;
}

void CSSTiming::recordAuthorStyleSheetParseTime(double seconds) {
  if (!m_paintTiming->firstContentfulPaint())
    m_parseTimeBeforeFCP += seconds;
}

DEFINE_TRACE(CSSTiming) {
  visitor->trace(m_document);
  visitor->trace(m_paintTiming);
  Supplement<Document>::trace(visitor);
}

CSSTiming::CSSTiming(Document& document)
    : m_document(document), m_paintTiming(PaintTiming::from(document)) {}

}  // namespace blink
