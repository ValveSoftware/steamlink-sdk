// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLParserReentryPermit.h"

namespace blink {

PassRefPtr<HTMLParserReentryPermit> HTMLParserReentryPermit::create() {
  return adoptRef(new HTMLParserReentryPermit());
}

HTMLParserReentryPermit::HTMLParserReentryPermit()
    : m_scriptNestingLevel(0), m_parserPauseFlag(false) {}

}  // namespace blink
