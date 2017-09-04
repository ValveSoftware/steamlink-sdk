// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_MESSAGE_FILTER_PLATFORM_H_
#define COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_MESSAGE_FILTER_PLATFORM_H_

#include <map>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/spellcheck/browser/spelling_service_client.h"
#include "components/spellcheck/common/spellcheck_marker.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/browser/browser_message_filter.h"

class SpellCheckerSessionBridge;

// A message filter implementation that receives
// the platform-specific spell checker requests from SpellCheckProvider.
class SpellCheckMessageFilterPlatform : public content::BrowserMessageFilter {
 public:
  explicit SpellCheckMessageFilterPlatform(int render_process_id);

  // BrowserMessageFilter implementation.
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Adjusts remote_results by examining local_results. Any result that's both
  // local and remote stays type SPELLING, all others are flagged GRAMMAR.
  // (This is needed to force gray underline for remote-only results.)
  static void CombineResults(
      std::vector<SpellCheckResult>* remote_results,
      const std::vector<SpellCheckResult>& local_results);

 private:
  friend class TestingSpellCheckMessageFilter;
  friend class SpellcheckMessageFilterPlatformMacTest;

  ~SpellCheckMessageFilterPlatform() override;

  void OnCheckSpelling(const base::string16& word, int route_id, bool* correct);
  void OnFillSuggestionList(const base::string16& word,
                            std::vector<base::string16>* suggestions);
  void OnShowSpellingPanel(bool show);
  void OnUpdateSpellingPanelWithMisspelledWord(const base::string16& word);
  void OnRequestTextCheck(int route_id,
                          int identifier,
                          const base::string16& text,
                          std::vector<SpellCheckMarker> markers);

  int ToDocumentTag(int route_id);
  void RetireDocumentTag(int route_id);
  std::map<int,int> tag_map_;

  int render_process_id_;

#if defined(OS_ANDROID)
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<SpellCheckMessageFilterPlatform>;

  void OnToggleSpellCheck(bool enabled, bool checked);
  void OnDestruct() const override;

  // Android-specific object used to query the Android spellchecker.
  std::unique_ptr<SpellCheckerSessionBridge> impl_;
#else
  // A JSON-RPC client that calls the Spelling service in the background.
  std::unique_ptr<SpellingServiceClient> client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SpellCheckMessageFilterPlatform);
};

#endif  // COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_MESSAGE_FILTER_PLATFORM_H_
