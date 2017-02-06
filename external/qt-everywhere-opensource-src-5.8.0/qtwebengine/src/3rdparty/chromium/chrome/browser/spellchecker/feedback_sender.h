// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An object to record and send user feedback to spelling service. The spelling
// service uses the feedback to improve its suggestions.
//
// Assigns uint32_t hash identifiers to spelling suggestions from spelling
// service and stores these suggestions. Records user's actions on these
// suggestions. Periodically sends batches of user feedback to the spelling
// service.

#ifndef CHROME_BROWSER_SPELLCHECKER_FEEDBACK_SENDER_H_
#define CHROME_BROWSER_SPELLCHECKER_FEEDBACK_SENDER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <climits>
#include <map>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/spellchecker/feedback.h"
#include "chrome/browser/spellchecker/misspelling.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

class SpellCheckMarker;
struct SpellCheckResult;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace spellcheck {

namespace {

// Constants for the feedback field trial.
const char kFeedbackFieldTrialName[] = "SpellingServiceFeedback";
const char kFeedbackFieldTrialEnabledGroupName[] = "Enabled";

}  // namespace

// Stores and sends user feedback to the spelling service. Sample usage:
//    FeedbackSender sender(profile.GetRequestContext(), language, country);
//    sender.OnSpellcheckResults(spellcheck_results_from_spelling_service,
//                               renderer_process_id,
//                               spellchecked_text,
//                               existing_hashes);
//    sender.SelectedSuggestion(hash, suggestion_index);
class FeedbackSender : public base::SupportsWeakPtr<FeedbackSender>,
                       public net::URLFetcherDelegate {
 public:
  // Constructs a feedback sender. Keeps |request_context| in a scoped_refptr,
  // because URLRequestContextGetter implements RefcountedThreadSafe.
  FeedbackSender(net::URLRequestContextGetter* request_context,
                 const std::string& language,
                 const std::string& country);
  ~FeedbackSender() override;

  // Records that user selected suggestion |suggestion_index| for the
  // misspelling identified by |hash|.
  void SelectedSuggestion(uint32_t hash, int suggestion_index);

  // Records that user added the misspelling identified by |hash| to the
  // dictionary.
  void AddedToDictionary(uint32_t hash);

  // Records that user right-clicked on the misspelling identified by |hash|,
  // but did not select any suggestion.
  void IgnoredSuggestions(uint32_t hash);

  // Records that user did not choose any suggestion but manually corrected the
  // misspelling identified by |hash| to string |correction|, which is not in
  // the list of suggestions.
  void ManuallyCorrected(uint32_t hash, const base::string16& correction);

  // Records that user has the misspelling in the custom dictionary. The user
  // will never see the spellcheck suggestions for the misspelling.
  void RecordInDictionary(uint32_t hash);

  // Receives document markers for renderer with process ID |render_process_id|
  // when the renderer responds to a RequestDocumentMarkers() call. Finalizes
  // feedback for the markers that are gone from the renderer. Sends feedback
  // data for the renderer with process ID |renderer_process_id| to the spelling
  // service. If the current session has expired, then refreshes the session
  // start timestamp and sends out all of the feedback data.
  void OnReceiveDocumentMarkers(int renderer_process_id,
                                const std::vector<uint32_t>& markers);

  // Generates feedback data based on spellcheck results. The new feedback data
  // is pending. Sets hash identifiers for |results|. Called when spelling
  // service client receives results from the spelling service. Does not take
  // ownership of |results|.
  void OnSpellcheckResults(int renderer_process_id,
                           const base::string16& text,
                           const std::vector<SpellCheckMarker>& markers,
                           std::vector<SpellCheckResult>* results);

  // Receives updated language and country code for feedback. Finalizes and
  // sends out all of the feedback data.
  void OnLanguageCountryChange(const std::string& language,
                               const std::string& country);

  // Starts collecting feedback, if it's not already being collected.
  void StartFeedbackCollection();

  // Sends out all previously collected data and stops collecting feedback, if
  // it was being collected.
  void StopFeedbackCollection();

  // 256 bits
  typedef std::array<char, 256 / CHAR_BIT> RandSalt;

 private:
  friend class FeedbackSenderTest;

  // Allow unit tests to override RNG.
  virtual void RandBytes(void* p, size_t len);

  // net::URLFetcherDelegate implementation. Takes ownership of |source|.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Requests the document markers from all of the renderers to determine which
  // feedback can be finalized. Finalizes feedback for renderers that are gone.
  // Called periodically when |timer_| fires.
  void RequestDocumentMarkers();

  // Sends out all feedback data. Resets the session-start timestamp to now.
  // Restarts the timer that requests markers from the renderers.
  void FlushFeedback();

  // Sends out the |feedback_data|.
  void SendFeedback(const std::vector<Misspelling>& feedback_data,
                    bool is_first_feedback_batch);

  // URL request context for the feedback senders.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // The feedback API version.
  const std::string api_version_;

  // The language of text. The string is a BCP 47 language tag.
  std::string language_;

  // The country of origin. The string is the ISO 3166-1 alpha-3 code.
  std::string country_;

  // Misspelling counter used to generate unique hash identifier for each
  // misspelling.
  size_t misspelling_counter_;

  // Feedback data.
  Feedback feedback_;

  // A set of renderer process IDs for renderers that have sent out feedback in
  // this session.
  std::set<int> renderers_sent_feedback_;

  // When the session started.
  base::Time session_start_;

  // The last time that we updated |salt_|.
  base::Time last_salt_update_;

  // A random number updated once a day.
  RandSalt salt_;

  // The URL where the feedback data should be sent.
  GURL feedback_service_url_;

  // A timer to periodically request a list of document spelling markers from
  // all of the renderers. The timer starts in StartFeedbackCollection() and
  // stops in StopFeedbackCollection(). The timer stops and abandons its tasks
  // on destruction.
  base::RepeatingTimer timer_;

  // Feedback senders that need to stay alive for the duration of sending data.
  // If a sender is destroyed before it finishes, then sending feedback will be
  // canceled.
  ScopedVector<net::URLFetcher> senders_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackSender);
};

}  // namespace spellcheck

#endif  // CHROME_BROWSER_SPELLCHECKER_FEEDBACK_SENDER_H_
