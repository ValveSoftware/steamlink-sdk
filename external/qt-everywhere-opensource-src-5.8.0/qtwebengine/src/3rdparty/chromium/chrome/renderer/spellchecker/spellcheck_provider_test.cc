// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/spellchecker/spellcheck_provider_test.h"

#include "base/stl_util.h"
#include "chrome/common/spellcheck_marker.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "ipc/ipc_message_macros.h"

class MockSpellcheck: public SpellCheck {
};

FakeTextCheckingCompletion::FakeTextCheckingCompletion()
: completion_count_(0),
  cancellation_count_(0) {
}

FakeTextCheckingCompletion::~FakeTextCheckingCompletion() {}

void FakeTextCheckingCompletion::didFinishCheckingText(
    const blink::WebVector<blink::WebTextCheckingResult>& results) {
  ++completion_count_;
}

void FakeTextCheckingCompletion::didCancelCheckingText() {
  ++completion_count_;
  ++cancellation_count_;
}

TestingSpellCheckProvider::TestingSpellCheckProvider()
      : SpellCheckProvider(NULL, new MockSpellcheck),
        spelling_service_call_count_(0) {
}

TestingSpellCheckProvider::TestingSpellCheckProvider(
    SpellCheck* spellcheck)
    : SpellCheckProvider(nullptr, spellcheck),
      spelling_service_call_count_(0) {
}

TestingSpellCheckProvider::~TestingSpellCheckProvider() {
  delete spellcheck_;
}

bool TestingSpellCheckProvider::Send(IPC::Message* message)  {
#if !defined(USE_BROWSER_SPELLCHECKER)
  // Call our mock message handlers.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TestingSpellCheckProvider, *message)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_CallSpellingService,
                        OnCallSpellingService)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (handled) {
    delete message;
    return true;
  }
#endif

  messages_.push_back(message);
  return true;
}

void TestingSpellCheckProvider::OnCallSpellingService(int route_id,
                           int identifier,
                           const base::string16& text,
                           const std::vector<SpellCheckMarker>& markers) {
#if defined (USE_BROWSER_SPELLCHECKER)
  NOTREACHED();
#else
  ++spelling_service_call_count_;
  blink::WebTextCheckingCompletion* completion =
      text_check_completions_.Lookup(identifier);
  if (!completion) {
    ResetResult();
    return;
  }
  text_.assign(text);
  text_check_completions_.Remove(identifier);
  std::vector<blink::WebTextCheckingResult> results;
  results.push_back(blink::WebTextCheckingResult(
      blink::WebTextDecorationTypeSpelling,
      0, 5, blink::WebString("hello")));
  completion->didFinishCheckingText(results);
  last_request_ = text;
  last_results_ = results;
#endif
}

void TestingSpellCheckProvider::ResetResult() {
  text_.clear();
}

SpellCheckProviderTest::SpellCheckProviderTest() {}
SpellCheckProviderTest::~SpellCheckProviderTest() {}


