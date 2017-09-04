// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLParserReentryPermit_h
#define HTMLParserReentryPermit_h

#include "base/macros.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

// The HTML spec for parsing controls reentering the parser from
// script with the "parser pause flag" and "script nesting level."
//
// The parser pause flag puts a brake on whether the tokenizer will
// produce more tokens. When the parser is paused, nested invocations
// of the tokenizer unwind.
//
// The script nesting level is incremented and decremented any time
// the parser causes script to run. The script nesting level:
//
// - May prevent document.open from blowing away the document.
//
// - Governs whether a script element becomes the "pending
//   parsing-blocking script." The pending parsing-blocking script in
//   turn affects whether document.write reenters the parser.
//
// Clearing the parser pause flag is simple: Whenever the script
// nesting level hits zero, the parser pause flag is cleared. However
// setting the parser pause flag is subtle.
//
// Processing a typical script end tag, or running a chain of pending
// parser-blocking scripts after that, does not set the parser pause
// flag. However recursively parsing end script tags, or running
// custom element constructors, does set the parser pause flag.
class HTMLParserReentryPermit final
    : public RefCounted<HTMLParserReentryPermit> {
 public:
  static PassRefPtr<HTMLParserReentryPermit> create();
  ~HTMLParserReentryPermit() = default;

  unsigned scriptNestingLevel() const { return m_scriptNestingLevel; }
  bool parserPauseFlag() const { return m_parserPauseFlag; }
  void pause() {
    CHECK(m_scriptNestingLevel);
    m_parserPauseFlag = true;
  }

  class ScriptNestingLevelIncrementer final {
    STACK_ALLOCATED();

   public:
    explicit ScriptNestingLevelIncrementer(HTMLParserReentryPermit* permit)
        : m_permit(permit) {
      m_permit->m_scriptNestingLevel++;
    }

    ScriptNestingLevelIncrementer(ScriptNestingLevelIncrementer&&) = default;

    ~ScriptNestingLevelIncrementer() {
      m_permit->m_scriptNestingLevel--;
      if (!m_permit->m_scriptNestingLevel)
        m_permit->m_parserPauseFlag = false;
    }

   private:
    HTMLParserReentryPermit* m_permit;

    DISALLOW_COPY_AND_ASSIGN(ScriptNestingLevelIncrementer);
  };

  ScriptNestingLevelIncrementer incrementScriptNestingLevel() {
    return ScriptNestingLevelIncrementer(this);
  }

 private:
  HTMLParserReentryPermit();

  // https://html.spec.whatwg.org/#script-nesting-level
  unsigned m_scriptNestingLevel;

  // https://html.spec.whatwg.org/#parser-pause-flag
  bool m_parserPauseFlag;

  DISALLOW_COPY_AND_ASSIGN(HTMLParserReentryPermit);
};

}  // namespace blink

#endif  // HTMLParserReentryPermit_h
