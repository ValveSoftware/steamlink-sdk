// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_MESSAGE_FILTER_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_MESSAGE_FILTER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "content/public/browser/browser_message_filter.h"

class SpellCheckMarker;
class SpellcheckService;
struct SpellCheckResult;

// A message filter implementation that receives spell checker requests from
// SpellCheckProvider.
class SpellCheckMessageFilter : public content::BrowserMessageFilter {
 public:
  explicit SpellCheckMessageFilter(int render_process_id);

  // content::BrowserMessageFilter implementation.
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  friend class TestingSpellCheckMessageFilter;

  ~SpellCheckMessageFilter() override;

  void OnSpellCheckerRequestDictionary();
  void OnNotifyChecked(const base::string16& word, bool misspelled);
  void OnRespondDocumentMarkers(const std::vector<uint32_t>& markers);
#if !defined(USE_BROWSER_SPELLCHECKER)
  void OnCallSpellingService(int route_id,
                             int identifier,
                             const base::string16& text,
                             std::vector<SpellCheckMarker> markers);

  // A callback function called when the Spelling service finishes checking
  // text. Sends the given results to a renderer.
  void OnTextCheckComplete(
      int route_id,
      int identifier,
      const std::vector<SpellCheckMarker>& markers,
      bool success,
      const base::string16& text,
      const std::vector<SpellCheckResult>& results);

  // Checks the user profile and sends a JSON-RPC request to the Spelling
  // service if a user enables the "Ask Google for suggestions" option. When we
  // receive a response (including an error) from the service, it calls
  // OnTextCheckComplete. When this function is called before we receive a
  // response for the previous request, this function cancels the previous
  // request and sends a new one.
  void CallSpellingService(
      const base::string16& text,
      int route_id,
      int identifier,
      const std::vector<SpellCheckMarker>& markers);
#endif

  // Can be overridden for testing.
  virtual SpellcheckService* GetSpellcheckService() const;

  int render_process_id_;

  // A JSON-RPC client that calls the Spelling service in the background.
  std::unique_ptr<SpellingServiceClient> client_;
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_MESSAGE_FILTER_H_
