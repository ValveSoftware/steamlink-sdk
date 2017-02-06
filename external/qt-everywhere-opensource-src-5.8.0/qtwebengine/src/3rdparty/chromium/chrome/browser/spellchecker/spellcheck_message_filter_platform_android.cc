// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_message_filter_platform.h"

#include "chrome/browser/spellchecker/spellchecker_session_bridge_android.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

SpellCheckMessageFilterPlatform::SpellCheckMessageFilterPlatform(
    int render_process_id)
    : BrowserMessageFilter(SpellCheckMsgStart),
      render_process_id_(render_process_id),
      impl_(new SpellCheckerSessionBridge(render_process_id)) {}

void SpellCheckMessageFilterPlatform::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  if (message.type() == SpellCheckHostMsg_RequestTextCheck::ID)
    *thread = BrowserThread::UI;
}

bool SpellCheckMessageFilterPlatform::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpellCheckMessageFilterPlatform, message)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_RequestTextCheck, OnRequestTextCheck)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_ToggleSpellCheck, OnToggleSpellCheck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// static
void SpellCheckMessageFilterPlatform::CombineResults(
    std::vector<SpellCheckResult>* remote_results,
    const std::vector<SpellCheckResult>& local_results) {
  NOTREACHED();
}

SpellCheckMessageFilterPlatform::~SpellCheckMessageFilterPlatform() {}

void SpellCheckMessageFilterPlatform::OnCheckSpelling(
    const base::string16& word,
    int route_id,
    bool* correct) {
  NOTREACHED();
}

void SpellCheckMessageFilterPlatform::OnFillSuggestionList(
    const base::string16& word,
    std::vector<base::string16>* suggestions) {
  NOTREACHED();
}

void SpellCheckMessageFilterPlatform::OnShowSpellingPanel(bool show) {
  NOTREACHED();
}

void SpellCheckMessageFilterPlatform::OnUpdateSpellingPanelWithMisspelledWord(
    const base::string16& word) {
  NOTREACHED();
}

void SpellCheckMessageFilterPlatform::OnRequestTextCheck(
    int route_id,
    int identifier,
    const base::string16& text,
    std::vector<SpellCheckMarker> markers) {
  DCHECK(!text.empty());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  impl_->RequestTextCheck(route_id, identifier, text);
}

int SpellCheckMessageFilterPlatform::ToDocumentTag(int route_id) {
  NOTREACHED();
  return -1;
}

void SpellCheckMessageFilterPlatform::RetireDocumentTag(int route_id) {
  NOTREACHED();
}

void SpellCheckMessageFilterPlatform::OnToggleSpellCheck(
    bool enabled,
    bool checked) {
  if (!enabled)
    impl_->DisconnectSession();
}
