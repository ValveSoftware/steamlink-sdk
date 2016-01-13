// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_entry_impl.h"

#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_util.h"
#include "ui/gfx/text_elider.h"

// Use this to get a new unique ID for a NavigationEntry during construction.
// The returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
static int GetUniqueIDInConstructor() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

namespace content {

int NavigationEntryImpl::kInvalidBindings = -1;

NavigationEntry* NavigationEntry::Create() {
  return new NavigationEntryImpl();
}

NavigationEntry* NavigationEntry::Create(const NavigationEntry& copy) {
  return new NavigationEntryImpl(static_cast<const NavigationEntryImpl&>(copy));
}

NavigationEntryImpl* NavigationEntryImpl::FromNavigationEntry(
    NavigationEntry* entry) {
  return static_cast<NavigationEntryImpl*>(entry);
}

NavigationEntryImpl::NavigationEntryImpl()
    : unique_id_(GetUniqueIDInConstructor()),
      site_instance_(NULL),
      bindings_(kInvalidBindings),
      page_type_(PAGE_TYPE_NORMAL),
      update_virtual_url_with_url_(false),
      page_id_(-1),
      transition_type_(PAGE_TRANSITION_LINK),
      has_post_data_(false),
      post_id_(-1),
      restore_type_(RESTORE_NONE),
      is_overriding_user_agent_(false),
      http_status_code_(0),
      is_renderer_initiated_(false),
      should_replace_entry_(false),
      should_clear_history_list_(false),
      can_load_local_resources_(false),
      frame_tree_node_id_(-1) {
}

NavigationEntryImpl::NavigationEntryImpl(SiteInstanceImpl* instance,
                                         int page_id,
                                         const GURL& url,
                                         const Referrer& referrer,
                                         const base::string16& title,
                                         PageTransition transition_type,
                                         bool is_renderer_initiated)
    : unique_id_(GetUniqueIDInConstructor()),
      site_instance_(instance),
      bindings_(kInvalidBindings),
      page_type_(PAGE_TYPE_NORMAL),
      url_(url),
      referrer_(referrer),
      update_virtual_url_with_url_(false),
      title_(title),
      page_id_(page_id),
      transition_type_(transition_type),
      has_post_data_(false),
      post_id_(-1),
      restore_type_(RESTORE_NONE),
      is_overriding_user_agent_(false),
      http_status_code_(0),
      is_renderer_initiated_(is_renderer_initiated),
      should_replace_entry_(false),
      should_clear_history_list_(false),
      can_load_local_resources_(false),
      frame_tree_node_id_(-1) {
}

NavigationEntryImpl::~NavigationEntryImpl() {
}

int NavigationEntryImpl::GetUniqueID() const {
  return unique_id_;
}

PageType NavigationEntryImpl::GetPageType() const {
  return page_type_;
}

void NavigationEntryImpl::SetURL(const GURL& url) {
  url_ = url;
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetURL() const {
  return url_;
}

void NavigationEntryImpl::SetBaseURLForDataURL(const GURL& url) {
  base_url_for_data_url_ = url;
}

const GURL& NavigationEntryImpl::GetBaseURLForDataURL() const {
  return base_url_for_data_url_;
}

void NavigationEntryImpl::SetReferrer(const Referrer& referrer) {
  referrer_ = referrer;
}

const Referrer& NavigationEntryImpl::GetReferrer() const {
  return referrer_;
}

void NavigationEntryImpl::SetVirtualURL(const GURL& url) {
  virtual_url_ = (url == url_) ? GURL() : url;
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetVirtualURL() const {
  return virtual_url_.is_empty() ? url_ : virtual_url_;
}

void NavigationEntryImpl::SetTitle(const base::string16& title) {
  title_ = title;
  cached_display_title_.clear();
}

const base::string16& NavigationEntryImpl::GetTitle() const {
  return title_;
}

void NavigationEntryImpl::SetPageState(const PageState& state) {
  page_state_ = state;
}

const PageState& NavigationEntryImpl::GetPageState() const {
  return page_state_;
}

void NavigationEntryImpl::SetPageID(int page_id) {
  page_id_ = page_id;
}

int32 NavigationEntryImpl::GetPageID() const {
  return page_id_;
}

void NavigationEntryImpl::set_site_instance(SiteInstanceImpl* site_instance) {
  site_instance_ = site_instance;
}

void NavigationEntryImpl::SetBindings(int bindings) {
  // Ensure this is set to a valid value, and that it stays the same once set.
  CHECK_NE(bindings, kInvalidBindings);
  CHECK(bindings_ == kInvalidBindings || bindings_ == bindings);
  bindings_ = bindings;
}

const base::string16& NavigationEntryImpl::GetTitleForDisplay(
    const std::string& languages) const {
  // Most pages have real titles. Don't even bother caching anything if this is
  // the case.
  if (!title_.empty())
    return title_;

  // More complicated cases will use the URLs as the title. This result we will
  // cache since it's more complicated to compute.
  if (!cached_display_title_.empty())
    return cached_display_title_;

  // Use the virtual URL first if any, and fall back on using the real URL.
  base::string16 title;
  if (!virtual_url_.is_empty()) {
    title = net::FormatUrl(virtual_url_, languages);
  } else if (!url_.is_empty()) {
    title = net::FormatUrl(url_, languages);
  }

  // For file:// URLs use the filename as the title, not the full path.
  if (url_.SchemeIsFile()) {
    base::string16::size_type slashpos = title.rfind('/');
    if (slashpos != base::string16::npos)
      title = title.substr(slashpos + 1);
  }

  gfx::ElideString(title, kMaxTitleChars, &cached_display_title_);
  return cached_display_title_;
}

bool NavigationEntryImpl::IsViewSourceMode() const {
  return virtual_url_.SchemeIs(kViewSourceScheme);
}

void NavigationEntryImpl::SetTransitionType(
    PageTransition transition_type) {
  transition_type_ = transition_type;
}

PageTransition NavigationEntryImpl::GetTransitionType() const {
  return transition_type_;
}

const GURL& NavigationEntryImpl::GetUserTypedURL() const {
  return user_typed_url_;
}

void NavigationEntryImpl::SetHasPostData(bool has_post_data) {
  has_post_data_ = has_post_data;
}

bool NavigationEntryImpl::GetHasPostData() const {
  return has_post_data_;
}

void NavigationEntryImpl::SetPostID(int64 post_id) {
  post_id_ = post_id;
}

int64 NavigationEntryImpl::GetPostID() const {
  return post_id_;
}

void NavigationEntryImpl::SetBrowserInitiatedPostData(
    const base::RefCountedMemory* data) {
  browser_initiated_post_data_ = data;
}

const base::RefCountedMemory*
NavigationEntryImpl::GetBrowserInitiatedPostData() const {
  return browser_initiated_post_data_.get();
}


const FaviconStatus& NavigationEntryImpl::GetFavicon() const {
  return favicon_;
}

FaviconStatus& NavigationEntryImpl::GetFavicon() {
  return favicon_;
}

const SSLStatus& NavigationEntryImpl::GetSSL() const {
  return ssl_;
}

SSLStatus& NavigationEntryImpl::GetSSL() {
  return ssl_;
}

void NavigationEntryImpl::SetOriginalRequestURL(const GURL& original_url) {
  original_request_url_ = original_url;
}

const GURL& NavigationEntryImpl::GetOriginalRequestURL() const {
  return original_request_url_;
}

void NavigationEntryImpl::SetIsOverridingUserAgent(bool override) {
  is_overriding_user_agent_ = override;
}

bool NavigationEntryImpl::GetIsOverridingUserAgent() const {
  return is_overriding_user_agent_;
}

void NavigationEntryImpl::SetTimestamp(base::Time timestamp) {
  timestamp_ = timestamp;
}

base::Time NavigationEntryImpl::GetTimestamp() const {
  return timestamp_;
}

void NavigationEntryImpl::SetHttpStatusCode(int http_status_code) {
  http_status_code_ = http_status_code;
}

int NavigationEntryImpl::GetHttpStatusCode() const {
  return http_status_code_;
}

void NavigationEntryImpl::SetRedirectChain(
    const std::vector<GURL>& redirect_chain) {
  redirect_chain_ = redirect_chain;
}

const std::vector<GURL>& NavigationEntryImpl::GetRedirectChain() const {
  return redirect_chain_;
}

bool NavigationEntryImpl::IsRestored() const {
  return restore_type_ != RESTORE_NONE;
}

void NavigationEntryImpl::SetCanLoadLocalResources(bool allow) {
  can_load_local_resources_ = allow;
}

bool NavigationEntryImpl::GetCanLoadLocalResources() const {
  return can_load_local_resources_;
}

void NavigationEntryImpl::SetFrameToNavigate(const std::string& frame_name) {
  frame_to_navigate_ = frame_name;
}

const std::string& NavigationEntryImpl::GetFrameToNavigate() const {
  return frame_to_navigate_;
}

void NavigationEntryImpl::SetExtraData(const std::string& key,
                                       const base::string16& data) {
  extra_data_[key] = data;
}

bool NavigationEntryImpl::GetExtraData(const std::string& key,
                                       base::string16* data) const {
  std::map<std::string, base::string16>::const_iterator iter =
      extra_data_.find(key);
  if (iter == extra_data_.end())
    return false;
  *data = iter->second;
  return true;
}

void NavigationEntryImpl::ClearExtraData(const std::string& key) {
  extra_data_.erase(key);
}

void NavigationEntryImpl::ResetForCommit() {
  // Any state that only matters when a navigation entry is pending should be
  // cleared here.
  SetBrowserInitiatedPostData(NULL);
  set_is_renderer_initiated(false);
  set_transferred_global_request_id(GlobalRequestID());
  set_should_replace_entry(false);

  set_should_clear_history_list(false);
  set_frame_tree_node_id(-1);
}

void NavigationEntryImpl::SetScreenshotPNGData(
    scoped_refptr<base::RefCountedBytes> png_data) {
  screenshot_ = png_data;
  if (screenshot_.get())
    UMA_HISTOGRAM_MEMORY_KB("Overscroll.ScreenshotSize", screenshot_->size());
}

}  // namespace content
