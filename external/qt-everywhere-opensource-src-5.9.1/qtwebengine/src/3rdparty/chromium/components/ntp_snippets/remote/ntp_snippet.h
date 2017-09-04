// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPET_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPET_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace ntp_snippets {

extern const int kArticlesRemoteId;

class SnippetProto;

struct SnippetSource {
  SnippetSource(const GURL& url,
                const std::string& publisher_name,
                const GURL& amp_url)
      : url(url), publisher_name(publisher_name), amp_url(amp_url) {}
  GURL url;
  std::string publisher_name;
  GURL amp_url;
};

class NTPSnippet {
 public:
  using PtrVector = std::vector<std::unique_ptr<NTPSnippet>>;

  // Creates a new snippet with the given |id|.
  // Public for testing only - create snippets using the Create* methods below.
  // TODO(treib): Make this private and add a CreateSnippetForTest. The
  // constructor can then also take the vector of ids as the handling of unique
  // ids is then completely encapsulated inside the class.
  NTPSnippet(const std::string& id, int remote_category_id);
  ~NTPSnippet();

  // Creates an NTPSnippet from a dictionary, as returned by Chrome Reader.
  // Returns a null pointer if the dictionary doesn't correspond to a valid
  // snippet. The keys in the dictionary are expected to be the same as the
  // property name, with exceptions documented in the property comment.
  static std::unique_ptr<NTPSnippet> CreateFromChromeReaderDictionary(
      const base::DictionaryValue& dict);

  // Creates an NTPSnippet from a dictionary, as returned by Chrome Content
  // Suggestions. Returns a null pointer if the dictionary doesn't correspond to
  // a valid snippet. Maps field names to Chrome Reader field names.
  static std::unique_ptr<NTPSnippet> CreateFromContentSuggestionsDictionary(
      const base::DictionaryValue& dict,
      int remote_category_id);

  // Creates an NTPSnippet from a protocol buffer. Returns a null pointer if the
  // protocol buffer doesn't correspond to a valid snippet.
  static std::unique_ptr<NTPSnippet> CreateFromProto(const SnippetProto& proto);

  // Creates a protocol buffer corresponding to this snippet, for persisting.
  SnippetProto ToProto() const;

  // Returns all ids of the snippet.
  const std::vector<std::string>& GetAllIDs() const { return ids_; }

  // The unique, primary ID for identifying the snippet.
  const std::string& id() const { return ids_.front(); }

  // Title of the snippet.
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  // Summary or relevant extract from the content.
  const std::string& snippet() const { return snippet_; }
  void set_snippet(const std::string& snippet) { snippet_ = snippet; }

  // Link to an image representative of the content. Do not fetch this image
  // directly. If initialized by CreateFromChromeReaderDictionary() the relevant
  // key is 'thumbnailUrl'
  const GURL& salient_image_url() const { return salient_image_url_; }
  void set_salient_image_url(const GURL& salient_image_url) {
    salient_image_url_ = salient_image_url;
  }

  // When the page pointed by this snippet was published.  If initialized by
  // CreateFromChromeReaderDictionary() the relevant key is
  // 'creationTimestampSec'
  const base::Time& publish_date() const { return publish_date_; }
  void set_publish_date(const base::Time& publish_date) {
    publish_date_ = publish_date;
  }

  // After this expiration date this snippet should no longer be presented to
  // the user.
  const base::Time& expiry_date() const { return expiry_date_; }
  void set_expiry_date(const base::Time& expiry_date) {
    expiry_date_ = expiry_date;
  }

  // We should never construct an NTPSnippet object if we don't have any sources
  // so this should never fail
  const SnippetSource& best_source() const {
    return sources_[best_source_index_];
  }

  // Adds a source to the snippet.
  // TODO(tschumann): Remove this from the NTPSnippet interface
  // (NTPSnippetsDatabaseTest is currently using it and should be changed).
  void add_source(const SnippetSource& source) { sources_.push_back(source); }

  // If this snippet has all the data we need to show a full card to the user
  bool is_complete() const {
    return !id().empty() && !sources().empty() && !title().empty() &&
           !snippet().empty() && salient_image_url().is_valid() &&
           !publish_date().is_null() && !expiry_date().is_null() &&
           !best_source().publisher_name.empty();
  }

  float score() const { return score_; }
  void set_score(float score) { score_ = score; }

  bool is_dismissed() const { return is_dismissed_; }
  void set_dismissed(bool dismissed) { is_dismissed_ = dismissed; }

  // The ID of the remote category this snippet belongs to, for use with
  // CategoryFactory::FromRemoteCategory.
  int remote_category_id() const { return remote_category_id_; }

  // Public for testing.
  static base::Time TimeFromJsonString(const std::string& timestamp_str);
  static std::string TimeToJsonString(const base::Time& time);

 private:
  void InitBestSource();
  void AddIDs(const std::vector<std::string>& ids);
  const std::vector<SnippetSource>& sources() const { return sources_; }

  // The first ID in the vector is the primary id.
  std::vector<std::string> ids_;
  std::string title_;
  GURL salient_image_url_;
  std::string snippet_;
  base::Time publish_date_;
  base::Time expiry_date_;
  float score_;
  bool is_dismissed_;
  int remote_category_id_;

  size_t best_source_index_;

  std::vector<SnippetSource> sources_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippet);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPET_H_
