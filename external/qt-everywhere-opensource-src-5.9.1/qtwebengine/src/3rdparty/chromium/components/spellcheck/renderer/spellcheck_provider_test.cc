// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_provider_test.h"

#include "base/stl_util.h"
#include "components/spellcheck/common/spellcheck_marker.h"
#include "components/spellcheck/common/spellcheck_messages.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/spellcheck_build_features.h"
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
#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
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
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
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

void TestingSpellCheckProvider::SetLastResults(
    const base::string16 last_request,
    blink::WebVector<blink::WebTextCheckingResult>& last_results) {
  last_request_ = last_request;
  last_results_ = last_results;
}

bool TestingSpellCheckProvider::SatisfyRequestFromCache(
    const base::string16& text,
    blink::WebTextCheckingCompletion* completion) {
  return SpellCheckProvider::SatisfyRequestFromCache(text, completion);
}

SpellCheckProviderTest::SpellCheckProviderTest() {}
SpellCheckProviderTest::~SpellCheckProviderTest() {}

