// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_id.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

class SearchTermsData;
class TemplateURL;


// TemplateURLRef -------------------------------------------------------------

// A TemplateURLRef represents a single URL within the larger TemplateURL class
// (which represents an entire "search engine", see below).  If
// SupportsReplacement() is true, this URL has placeholders in it, for which
// callers can substitute values to get a "real" URL using ReplaceSearchTerms().
//
// TemplateURLRefs always have a non-NULL |owner_| TemplateURL, which they
// access in order to get at important data like the underlying URL string or
// the associated Profile.
class TemplateURLRef {
 public:
  // Magic numbers to pass to ReplaceSearchTerms() for the |accepted_suggestion|
  // parameter.  Most callers aren't using Suggest capabilities and should just
  // pass NO_SUGGESTIONS_AVAILABLE.
  // NOTE: Because positive values are meaningful, make sure these are negative!
  enum AcceptedSuggestion {
    NO_SUGGESTION_CHOSEN = -1,
    NO_SUGGESTIONS_AVAILABLE = -2,
  };

  // Which kind of URL within our owner we are.  This allows us to get at the
  // correct string field. Use |INDEXED| to indicate that the numerical
  // |index_in_owner_| should be used instead.
  enum Type {
    SEARCH,
    SUGGEST,
    INSTANT,
    IMAGE,
    NEW_TAB,
    CONTEXTUAL_SEARCH,
    INDEXED
  };

  // Type to store <content_type, post_data> pair for POST URLs.
  // The |content_type|(first part of the pair) is the content-type of
  // the |post_data|(second part of the pair) which is encoded in
  // "multipart/form-data" format, it also contains the MIME boundary used in
  // the |post_data|. See http://tools.ietf.org/html/rfc2046 for the details.
  typedef std::pair<std::string, std::string> PostContent;

  // This struct encapsulates arguments passed to
  // TemplateURLRef::ReplaceSearchTerms methods.  By default, only search_terms
  // is required and is passed in the constructor.
  struct SearchTermsArgs {
    explicit SearchTermsArgs(const base::string16& search_terms);
    SearchTermsArgs(const SearchTermsArgs& other);
    ~SearchTermsArgs();

    struct ContextualSearchParams {
      ContextualSearchParams();
      // Used when the content is sent in the HTTP header instead of as CGI
      // parameters.
      // TODO(donnd): Remove base_page_url and selection parameters once
      // they are logged from the HTTP header.
      ContextualSearchParams(const int version,
                             const std::string& selection,
                             const std::string& base_page_url,
                             const bool resolve);
      // TODO(donnd): Delete constructor once Clank, iOS, and tests no
      // longer depend on it.
      ContextualSearchParams(const int version,
                             const size_t start,
                             const size_t end,
                             const std::string& selection,
                             const std::string& content,
                             const std::string& base_page_url,
                             const std::string& encoding,
                             const bool resolve);
      ContextualSearchParams(const ContextualSearchParams& other);
      ~ContextualSearchParams();

      // The version of contextual search.
      int version;

      // Offset into the page content of the start of the user selection.
      size_t start;

      // Offset into the page content of the end of the user selection.
      size_t end;

      // The user selection.
      std::string selection;

      // The text including and surrounding the user selection.
      std::string content;

      // The URL of the page containing the user selection.
      std::string base_page_url;

      // The encoding of content.
      std::string encoding;

      // If true, the server will generate a search term based on the user
      // selection and context.  Otherwise the user selection will be used as-is
      // as the search term.
      bool resolve;
    };

    // The search terms (query).
    base::string16 search_terms;

    // The original (input) query.
    base::string16 original_query;

    // The type the original input query was identified as.
    metrics::OmniboxInputType::Type input_type;

    // The optional assisted query stats, aka AQS, used for logging purposes.
    // This string contains impressions of all autocomplete matches shown
    // at the query submission time.  For privacy reasons, we require the
    // search provider to support HTTPS protocol in order to receive the AQS
    // param.
    // For more details, see http://goto.google.com/binary-clients-logging .
    std::string assisted_query_stats;

    // TODO: Remove along with "aq" CGI param.
    int accepted_suggestion;

    // The 0-based position of the cursor within the query string at the time
    // the request was issued.  Set to base::string16::npos if not used.
    size_t cursor_position;

    // The URL of the current webpage to be used for experimental zero-prefix
    // suggestions.
    std::string current_page_url;

    // Which omnibox the user used to type the prefix.
    metrics::OmniboxEventProto::PageClassification page_classification;

    // Optional session token.
    std::string session_token;

    // Prefetch query and type.
    std::string prefetch_query;
    std::string prefetch_query_type;

    // Additional query params provided by the suggest server.
    std::string suggest_query_params;

    // If set, ReplaceSearchTerms() will automatically append any extra query
    // params specified via the --extra-search-query-params command-line
    // argument.  Generally, this should be set when dealing with the search or
    // instant TemplateURLRefs of the default search engine and the caller cares
    // about the query portion of the URL.  Since neither TemplateURLRef nor
    // indeed TemplateURL know whether a TemplateURL is the default search
    // engine, callers instead must set this manually.
    bool append_extra_query_params;

    // The raw content of an image thumbnail that will be used as a query for
    // search-by-image frontend.
    std::string image_thumbnail_content;

    // When searching for an image, the URL of the original image. Callers
    // should leave this empty for images specified via data: URLs.
    GURL image_url;

    // When searching for an image, the original size of the image.
    gfx::Size image_original_size;

    // If set, ReplaceSearchTerms() will append a param to the TemplateURLRef to
    // update the search results page incrementally even if that is otherwise
    // disabled by google.com preferences. See comments on
    // chrome::ForceInstantResultsParam().
    bool force_instant_results;

    // True if the search was made using the app list search box. Otherwise, the
    // search was made using the omnibox.
    bool from_app_list;

    ContextualSearchParams contextual_search_params;
  };

  TemplateURLRef(const TemplateURL* owner, Type type);
  TemplateURLRef(const TemplateURL* owner, size_t index_in_owner);
  ~TemplateURLRef();

  TemplateURLRef(const TemplateURLRef& source);
  TemplateURLRef& operator=(const TemplateURLRef& source);

  // Returns the raw URL. None of the parameters will have been replaced.
  std::string GetURL() const;

  // Returns the raw string of the post params. Please see comments in
  // prepopulated_engines_schema.json for the format.
  std::string GetPostParamsString() const;

  // Returns true if this URL supports search term replacement.
  bool SupportsReplacement(const SearchTermsData& search_terms_data) const;

  // Returns a string that is the result of replacing the search terms in
  // the url with the specified arguments.  We use our owner's input encoding.
  //
  // If this TemplateURLRef does not support replacement (SupportsReplacement
  // returns false), an empty string is returned.
  // If this TemplateURLRef uses POST, and |post_content| is not NULL, the
  // |post_params_| will be replaced, encoded in "multipart/form-data" format
  // and stored into |post_content|.
  std::string ReplaceSearchTerms(const SearchTermsArgs& search_terms_args,
                                 const SearchTermsData& search_terms_data,
                                 PostContent* post_content) const;

  // TODO(jnd): remove the following ReplaceSearchTerms definition which does
  // not have |post_content| parameter once all reference callers pass
  // |post_content| parameter.
  std::string ReplaceSearchTerms(
      const SearchTermsArgs& search_terms_args,
      const SearchTermsData& search_terms_data) const {
    return ReplaceSearchTerms(search_terms_args, search_terms_data, NULL);
  }

  // Returns true if the TemplateURLRef is valid. An invalid TemplateURLRef is
  // one that contains unknown terms, or invalid characters.
  bool IsValid(const SearchTermsData& search_terms_data) const;

  // Returns a string representation of this TemplateURLRef suitable for
  // display. The display format is the same as the format used by Firefox.
  base::string16 DisplayURL(const SearchTermsData& search_terms_data) const;

  // Converts a string as returned by DisplayURL back into a string as
  // understood by TemplateURLRef.
  static std::string DisplayURLToURLRef(const base::string16& display_url);

  // If this TemplateURLRef is valid and contains one search term, this returns
  // the host/path of the URL, otherwise this returns an empty string.
  const std::string& GetHost(const SearchTermsData& search_terms_data) const;
  const std::string& GetPath(const SearchTermsData& search_terms_data) const;

  // If this TemplateURLRef is valid and contains one search term
  // in its query or ref, this returns the key of the search term,
  // otherwise this returns an empty string.
  const std::string& GetSearchTermKey(
      const SearchTermsData& search_terms_data) const;

  // If this TemplateURLRef is valid and contains one search term
  // in its path, this returns the length of the subpath before the search term,
  // otherwise this returns std::string::npos.
  size_t GetSearchTermPositionInPath(
      const SearchTermsData& search_terms_data) const;

  // If this TemplateURLRef is valid and contains one search term,
  // this returns the location of the search term,
  // otherwise this returns url::Parsed::QUERY.
  url::Parsed::ComponentType GetSearchTermKeyLocation(
      const SearchTermsData& search_terms_data) const;

  // Converts the specified term in our owner's encoding to a base::string16.
  base::string16 SearchTermToString16(const std::string& term) const;

  // Returns true if this TemplateURLRef has a replacement term of
  // {google:baseURL} or {google:baseSuggestURL}.
  bool HasGoogleBaseURLs(const SearchTermsData& search_terms_data) const;

  // Use the pattern referred to by this TemplateURLRef to match the provided
  // |url| and extract |search_terms| from it. Returns true if the pattern
  // matches, even if |search_terms| is empty. In this case
  // |search_term_component|, if not NULL, indicates whether the search terms
  // were found in the query or the ref parameters; and |search_terms_position|,
  // if not NULL, contains the position of the search terms in the query or the
  // ref parameters. Returns false and an empty |search_terms| if the pattern
  // does not match.
  bool ExtractSearchTermsFromURL(
      const GURL& url,
      base::string16* search_terms,
      const SearchTermsData& search_terms_data,
      url::Parsed::ComponentType* search_term_component,
      url::Component* search_terms_position) const;

  // Whether the URL uses POST (as opposed to GET).
  bool UsesPOSTMethod(const SearchTermsData& search_terms_data) const;

 private:
  friend class TemplateURL;
  friend class TemplateURLTest;
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, SetPrepopulatedAndParse);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseParameterKnown);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseParameterUnknown);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLEmpty);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLNoTemplateEnd);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLNoKnownParameters);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLTwoParameters);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLNestedParameter);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, URLRefTestImageURLWithPOST);

  // Enumeration of the known types.
  enum ReplacementType {
    ENCODING,
    GOOGLE_ASSISTED_QUERY_STATS,
    GOOGLE_BASE_URL,
    GOOGLE_BASE_SUGGEST_URL,
    GOOGLE_CURRENT_PAGE_URL,
    GOOGLE_CURSOR_POSITION,
    GOOGLE_FORCE_INSTANT_RESULTS,
    GOOGLE_IMAGE_ORIGINAL_HEIGHT,
    GOOGLE_IMAGE_ORIGINAL_WIDTH,
    GOOGLE_IMAGE_SEARCH_SOURCE,
    GOOGLE_IMAGE_THUMBNAIL,
    GOOGLE_IMAGE_URL,
    GOOGLE_INPUT_TYPE,
    GOOGLE_INSTANT_EXTENDED_ENABLED,
    GOOGLE_IOS_SEARCH_LANGUAGE,
    GOOGLE_NTP_IS_THEMED,
    GOOGLE_CONTEXTUAL_SEARCH_VERSION,
    GOOGLE_CONTEXTUAL_SEARCH_CONTEXT_DATA,
    GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION,
    GOOGLE_PAGE_CLASSIFICATION,
    GOOGLE_PREFETCH_QUERY,
    GOOGLE_RLZ,
    GOOGLE_SEARCH_CLIENT,
    GOOGLE_SEARCH_FIELDTRIAL_GROUP,
    GOOGLE_SEARCH_VERSION,
    GOOGLE_SESSION_TOKEN,
    GOOGLE_SUGGEST_CLIENT,
    GOOGLE_SUGGEST_REQUEST_ID,
    GOOGLE_UNESCAPED_SEARCH_TERMS,
    LANGUAGE,
    SEARCH_TERMS,
  };

  // Used to identify an element of the raw url that can be replaced.
  struct Replacement {
    Replacement(ReplacementType type, size_t index)
        : type(type), index(index), is_post_param(false) {}
    ReplacementType type;
    size_t index;
    // Indicates the location in where the replacement is replaced. If
    // |is_post_param| is false, |index| indicates the byte position in
    // |parsed_url_|. Otherwise, |index| is the index of |post_params_|.
    bool is_post_param;
  };

  // Stores a single parameter for a POST.
  struct PostParam {
    std::string name;
    std::string value;
    std::string content_type;
  };

  // The list of elements to replace.
  typedef std::vector<struct Replacement> Replacements;
  typedef std::vector<PostParam> PostParams;

  // TemplateURLRef internally caches values to make replacement quick. This
  // method invalidates any cached values.
  void InvalidateCachedValues() const;

  // Parses the parameter in url at the specified offset. start/end specify the
  // range of the parameter in the url, including the braces. If the parameter
  // is valid, url is updated to reflect the appropriate parameter. If
  // the parameter is one of the known parameters an element is added to
  // replacements indicating the type and range of the element. The original
  // parameter is erased from the url.
  //
  // If the parameter is not a known parameter, false is returned. If this is a
  // prepopulated URL, the parameter is erased, otherwise it is left alone.
  bool ParseParameter(size_t start,
                      size_t end,
                      std::string* url,
                      Replacements* replacements) const;

  // Parses the specified url, replacing parameters as necessary. If
  // successful, valid is set to true, and the parsed url is returned. For all
  // known parameters that are encountered an entry is added to replacements.
  // If there is an error parsing the url, valid is set to false, and an empty
  // string is returned.  If the URL has the POST parameters, they will be
  // parsed into |post_params| which will be further replaced with real search
  // terms data and encoded in "multipart/form-data" format to generate the
  // POST data.
  std::string ParseURL(const std::string& url,
                       Replacements* replacements,
                       PostParams* post_params,
                       bool* valid) const;

  // If the url has not yet been parsed, ParseURL is invoked.
  // NOTE: While this is const, it modifies parsed_, valid_, parsed_url_ and
  // search_offset_.
  void ParseIfNecessary(const SearchTermsData& search_terms_data) const;

  // Extracts the query key and host from the url.
  void ParseHostAndSearchTermKey(
      const SearchTermsData& search_terms_data) const;

  // Encode post parameters in "multipart/form-data" format and store it
  // inside |post_content|. Returns false if errors are encountered during
  // encoding. This method is called each time ReplaceSearchTerms gets called.
  bool EncodeFormData(const PostParams& post_params,
                      PostContent* post_content) const;

  // Handles a replacement by using real term data. If the replacement
  // belongs to a PostParam, the PostParam will be replaced by the term data.
  // Otherwise, the term data will be inserted at the place that the
  // replacement points to.
  void HandleReplacement(const std::string& name,
                         const std::string& value,
                         const Replacement& replacement,
                         std::string* url) const;

  // Replaces all replacements in |parsed_url_| with their actual values and
  // returns the result.  This is the main functionality of
  // ReplaceSearchTerms().
  std::string HandleReplacements(
      const SearchTermsArgs& search_terms_args,
      const SearchTermsData& search_terms_data,
      PostContent* post_content) const;

  // The TemplateURL that contains us.  This should outlive us.
  const TemplateURL* owner_;

  // What kind of URL we are.
  Type type_;

  // If |type_| is |INDEXED|, this |index_in_owner_| is used instead to refer to
  // a url within our owner.
  size_t index_in_owner_;

  // Whether the URL has been parsed.
  mutable bool parsed_;

  // Whether the url was successfully parsed.
  mutable bool valid_;

  // The parsed URL. All terms have been stripped out of this with
  // replacements_ giving the index of the terms to replace.
  mutable std::string parsed_url_;

  // Do we support search term replacement?
  mutable bool supports_replacements_;

  // The replaceable parts of url (parsed_url_). These are ordered by index
  // into the string, and may be empty.
  mutable Replacements replacements_;

  // Host, port, path, key and location of the search term. These are only set
  // if the url contains one search term.
  mutable std::string host_;
  mutable std::string port_;
  mutable std::string path_;
  mutable std::string search_term_key_;
  mutable size_t search_term_position_in_path_;
  mutable url::Parsed::ComponentType search_term_key_location_;
  mutable std::string search_term_value_prefix_;
  mutable std::string search_term_value_suffix_;

  mutable PostParams post_params_;

  // Whether the contained URL is a pre-populated URL.
  bool prepopulated_;
};


// TemplateURL ----------------------------------------------------------------

// A TemplateURL represents a single "search engine", defined primarily as a
// subset of the Open Search Description Document
// (http://www.opensearch.org/Specifications/OpenSearch) plus some extensions.
// One TemplateURL contains several TemplateURLRefs, which correspond to various
// different capabilities (e.g. doing searches or getting suggestions), as well
// as a TemplateURLData containing other details like the name, keyword, etc.
//
// TemplateURLs are intended to be read-only for most users.
// The TemplateURLService, which handles storing and manipulating TemplateURLs,
// is made a friend so that it can be the exception to this pattern.
class TemplateURL {
 public:
  enum Type {
    // Regular search engine.
    NORMAL,
    // Installed by extension through Override Settings API.
    NORMAL_CONTROLLED_BY_EXTENSION,
    // The keyword associated with an extension that uses the Omnibox API.
    OMNIBOX_API_EXTENSION,
  };

  // An AssociatedExtensionInfo represents information about the extension that
  // added the search engine.
  struct AssociatedExtensionInfo {
    AssociatedExtensionInfo(Type type, const std::string& extension_id);
    ~AssociatedExtensionInfo();

    Type type;

    std::string extension_id;

    // Whether the search engine is supposed to be default.
    bool wants_to_be_default_engine;

    // Used to resolve conflicts when there are multiple extensions specifying
    // the default search engine. The most recently-installed wins.
    base::Time install_time;
  };

  explicit TemplateURL(const TemplateURLData& data);
  ~TemplateURL();

  // Generates a suitable keyword for the specified url, which must be valid.
  // This is guaranteed not to return an empty string, since TemplateURLs should
  // never have an empty keyword.
  static base::string16 GenerateKeyword(const GURL& url);

  // Generates a favicon URL from the specified url.
  static GURL GenerateFaviconURL(const GURL& url);

  // Returns true if |t_url| and |data| are equal in all meaningful respects.
  // Static to allow either or both params to be NULL.
  static bool MatchesData(const TemplateURL* t_url,
                          const TemplateURLData* data,
                          const SearchTermsData& search_terms_data);

  const TemplateURLData& data() const { return data_; }

  const base::string16& short_name() const { return data_.short_name(); }
  // An accessor for the short_name, but adjusted so it can be appropriately
  // displayed even if it is LTR and the UI is RTL.
  base::string16 AdjustedShortNameForLocaleDirection() const;

  const base::string16& keyword() const { return data_.keyword(); }

  const std::string& url() const { return data_.url(); }
  const std::string& suggestions_url() const { return data_.suggestions_url; }
  const std::string& instant_url() const { return data_.instant_url; }
  const std::string& image_url() const { return data_.image_url; }
  const std::string& new_tab_url() const { return data_.new_tab_url; }
  const std::string& contextual_search_url() const {
    return data_.contextual_search_url;
  }
  const std::string& search_url_post_params() const {
    return data_.search_url_post_params;
  }
  const std::string& suggestions_url_post_params() const {
    return data_.suggestions_url_post_params;
  }
  const std::string& instant_url_post_params() const {
    return data_.instant_url_post_params;
  }
  const std::string& image_url_post_params() const {
    return data_.image_url_post_params;
  }
  const std::vector<std::string>& alternate_urls() const {
    return data_.alternate_urls;
  }
  const GURL& favicon_url() const { return data_.favicon_url; }

  const GURL& originating_url() const { return data_.originating_url; }

  bool show_in_default_list() const { return data_.show_in_default_list; }
  // Returns true if show_in_default_list() is true and this TemplateURL has a
  // TemplateURLRef that supports replacement.
  bool ShowInDefaultList(const SearchTermsData& search_terms_data) const;

  bool safe_for_autoreplace() const { return data_.safe_for_autoreplace; }

  const std::vector<std::string>& input_encodings() const {
    return data_.input_encodings;
  }

  TemplateURLID id() const { return data_.id; }

  base::Time date_created() const { return data_.date_created; }
  base::Time last_modified() const { return data_.last_modified; }

  bool created_by_policy() const { return data_.created_by_policy; }

  int usage_count() const { return data_.usage_count; }

  int prepopulate_id() const { return data_.prepopulate_id; }

  const std::string& sync_guid() const { return data_.sync_guid; }

  // TODO(beaudoin): Rename this when renaming HasSearchTermsReplacementKey().
  const std::string& search_terms_replacement_key() const {
    return data_.search_terms_replacement_key;
  }

  const std::vector<TemplateURLRef>& url_refs() const { return url_refs_; }
  const TemplateURLRef& url_ref() const { return *url_ref_; }
  const TemplateURLRef& suggestions_url_ref() const {
    return suggestions_url_ref_;
  }
  const TemplateURLRef& instant_url_ref() const { return instant_url_ref_; }
  const TemplateURLRef& image_url_ref() const { return image_url_ref_; }
  const TemplateURLRef& new_tab_url_ref() const { return new_tab_url_ref_; }
  const TemplateURLRef& contextual_search_url_ref() const {
    return contextual_search_url_ref_;
  }

  // This setter shouldn't be used except by TemplateURLService and
  // TemplateURLServiceClient implementations.
  void set_extension_info(
      std::unique_ptr<AssociatedExtensionInfo> extension_info) {
    extension_info_ = std::move(extension_info);
  }

  // Returns true if |url| supports replacement.
  bool SupportsReplacement(const SearchTermsData& search_terms_data) const;

  // Returns true if any URLRefs use Googe base URLs.
  bool HasGoogleBaseURLs(const SearchTermsData& search_terms_data) const;

  // Returns true if this TemplateURL uses Google base URLs and has a keyword
  // of "google.TLD".  We use this to decide whether we can automatically
  // update the keyword to reflect the current Google base URL TLD.
  bool IsGoogleSearchURLWithReplaceableKeyword(
      const SearchTermsData& search_terms_data) const;

  // Returns true if the keywords match or if
  // IsGoogleSearchURLWithReplaceableKeyword() is true for both |this| and
  // |other|.
  bool HasSameKeywordAs(const TemplateURLData& other,
                        const SearchTermsData& search_terms_data) const;

  Type GetType() const;

  // Returns the id of the extension that added this search engine. Only call
  // this for TemplateURLs of type NORMAL_CONTROLLED_BY_EXTENSION or
  // OMNIBOX_API_EXTENSION.
  std::string GetExtensionId() const;

  // Returns the type of this search engine, or SEARCH_ENGINE_OTHER if no
  // engines match.
  SearchEngineType GetEngineType(
      const SearchTermsData& search_terms_data) const;

  // Use the alternate URLs and the search URL to match the provided |url|
  // and extract |search_terms| from it. Returns false and an empty
  // |search_terms| if no search terms can be matched. The order in which the
  // alternate URLs are listed dictates their priority, the URL at index 0 is
  // treated as the highest priority and the primary search URL is treated as
  // the lowest priority. For example, if a TemplateURL has alternate URL
  // "http://foo/#q={searchTerms}" and search URL "http://foo/?q={searchTerms}",
  // and the URL to be decoded is "http://foo/?q=a#q=b", the alternate URL will
  // match first and the decoded search term will be "b".
  bool ExtractSearchTermsFromURL(const GURL& url,
                                 const SearchTermsData& search_terms_data,
                                 base::string16* search_terms) const;

  // Returns true if non-empty search terms could be extracted from |url| using
  // ExtractSearchTermsFromURL(). In other words, this returns whether |url|
  // could be the result of performing a search with |this|.
  bool IsSearchURL(const GURL& url,
                   const SearchTermsData& search_terms_data) const;

  // Returns true if the specified |url| contains the search terms replacement
  // key in either the query or the ref. This method does not verify anything
  // else about the URL. In particular, it does not check that the domain
  // matches that of this TemplateURL.
  // TODO(beaudoin): Rename this to reflect that it really checks for an
  // InstantExtended capable URL.
  bool HasSearchTermsReplacementKey(const GURL& url) const;

  // Given a |url| corresponding to this TemplateURL, identifies the search
  // terms and replaces them with the ones in |search_terms_args|, leaving the
  // other parameters untouched. If the replacement fails, returns false and
  // leaves |result| untouched. This is used by mobile ports to perform query
  // refinement.
  bool ReplaceSearchTermsInURL(
      const GURL& url,
      const TemplateURLRef::SearchTermsArgs& search_terms_args,
      const SearchTermsData& search_terms_data,
      GURL* result);

  // Encodes the search terms from |search_terms_args| so that we know the
  // |input_encoding|. Returns the |encoded_terms| and the
  // |encoded_original_query|. |encoded_terms| may be escaped as path or query
  // depending on |is_in_query|; |encoded_original_query| is always escaped as
  // query.
  void EncodeSearchTerms(
      const TemplateURLRef::SearchTermsArgs& search_terms_args,
      bool is_in_query,
      std::string* input_encoding,
      base::string16* encoded_terms,
      base::string16* encoded_original_query) const;

  // Returns the search url for this template URL.
  // Returns an empty GURL if this template URL has no url().
  GURL GenerateSearchURL(const SearchTermsData& search_terms_data) const;

  // TemplateURL internally caches values derived from a passed SearchTermsData
  // to make its functions quick. This method invalidates any cached values and
  // it should be called after SearchTermsData has been changed.
  void InvalidateCachedValues() const;

 private:
  friend class TemplateURLService;

  void CopyFrom(const TemplateURL& other);

  void SetURL(const std::string& url);
  void SetPrepopulateId(int id);

  // Resets the keyword if IsGoogleSearchURLWithReplaceableKeyword() or |force|.
  // The |force| parameter is useful when the existing keyword is known to be
  // a placeholder.  The resulting keyword is generated using
  // GenerateSearchURL() and GenerateKeyword().
  void ResetKeywordIfNecessary(const SearchTermsData& search_terms_data,
                               bool force);

  // Resizes the |url_refs_| vector and sets |url_ref_| according to |data_|.
  void ResizeURLRefVector();

  // Uses the alternate URLs and the search URL to match the provided |url|
  // and extract |search_terms| from it as well as the |search_terms_component|
  // (either REF or QUERY) and |search_terms_component| at which the
  // |search_terms| are found in |url|. See also ExtractSearchTermsFromURL().
  bool FindSearchTermsInURL(const GURL& url,
                            const SearchTermsData& search_terms_data,
                            base::string16* search_terms,
                            url::Parsed::ComponentType* search_terms_component,
                            url::Component* search_terms_position) const;

  TemplateURLData data_;

  // Contains TemplateURLRefs corresponding to the alternate URLs and the search
  // URL. This vector must not be resized except by ResizeURLRefVector() to keep
  // the |url_ref_| pointer correct.
  std::vector<TemplateURLRef> url_refs_;

  // Points to the TemplateURLRef in |url_refs_| which corresponds to the search
  // URL.
  TemplateURLRef* url_ref_;

  TemplateURLRef suggestions_url_ref_;
  TemplateURLRef instant_url_ref_;
  TemplateURLRef image_url_ref_;
  TemplateURLRef new_tab_url_ref_;
  TemplateURLRef contextual_search_url_ref_;
  std::unique_ptr<AssociatedExtensionInfo> extension_info_;

  // Caches the computed engine type across successive calls to GetEngineType().
  mutable SearchEngineType engine_type_;

  // TODO(sky): Add date last parsed OSD file.

  DISALLOW_COPY_AND_ASSIGN(TemplateURL);
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_H_
