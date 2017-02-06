// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_entry_impl.h"

#include <stddef.h>

#include <queue>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "content/common/content_constants_internal.h"
#include "content/common/navigation_params.h"
#include "content/common/page_state_serialization.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "ui/gfx/text_elider.h"

using base::UTF16ToUTF8;

namespace content {

namespace {

// Use this to get a new unique ID for a NavigationEntry during construction.
// The returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
int GetUniqueIDInConstructor() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

void RecursivelyGenerateFrameEntries(const ExplodedFrameState& state,
                                     NavigationEntryImpl::TreeNode* node) {
  node->frame_entry = new FrameNavigationEntry(
      UTF16ToUTF8(state.target.string()), state.item_sequence_number,
      state.document_sequence_number, nullptr, nullptr,
      GURL(state.url_string.string()),
      Referrer(GURL(state.referrer.string()), state.referrer_policy), "GET",
      -1);

  // Set a single-frame PageState on the entry.
  ExplodedPageState page_state;
  page_state.top = state;
  std::string data;
  if (EncodePageState(page_state, &data))
    node->frame_entry->set_page_state(PageState::CreateFromEncodedData(data));

  for (const ExplodedFrameState& child_state : state.children) {
    NavigationEntryImpl::TreeNode* child_node =
        new NavigationEntryImpl::TreeNode(nullptr);
    node->children.push_back(child_node);
    RecursivelyGenerateFrameEntries(child_state, child_node);
  }
}

void RecursivelyGenerateFrameState(
    NavigationEntryImpl::TreeNode* node,
    ExplodedFrameState* state,
    std::vector<base::NullableString16>* referenced_files) {
  // The FrameNavigationEntry's PageState contains just the ExplodedFrameState
  // for that particular frame.
  ExplodedPageState exploded_page_state;
  if (!DecodePageState(node->frame_entry->page_state().ToEncodedData(),
                       &exploded_page_state)) {
    NOTREACHED();
    return;
  }
  ExplodedFrameState frame_state = exploded_page_state.top;

  // Copy the FrameNavigationEntry's frame state into the destination state.
  *state = frame_state;

  // Copy the frame's files into the PageState's |referenced_files|.
  referenced_files->reserve(referenced_files->size() +
                            exploded_page_state.referenced_files.size());
  for (auto& file : exploded_page_state.referenced_files)
    referenced_files->push_back(file);

  state->children.resize(node->children.size());
  for (size_t i = 0; i < node->children.size(); ++i) {
    RecursivelyGenerateFrameState(node->children[i], &state->children[i],
                                  referenced_files);
  }
}

}  // namespace

int NavigationEntryImpl::kInvalidBindings = -1;

NavigationEntryImpl::TreeNode::TreeNode(FrameNavigationEntry* frame_entry)
    : frame_entry(frame_entry) {
}

NavigationEntryImpl::TreeNode::~TreeNode() {
}

bool NavigationEntryImpl::TreeNode::MatchesFrame(FrameTreeNode* frame_tree_node,
                                                 bool is_root_tree_node) const {
  // The root node is for the main frame whether the unique name matches or not.
  if (is_root_tree_node)
    return frame_tree_node->IsMainFrame();

  // Otherwise check the unique name for subframes.
  return !frame_tree_node->IsMainFrame() &&
         frame_tree_node->unique_name() == frame_entry->frame_unique_name();
}

std::unique_ptr<NavigationEntryImpl::TreeNode>
NavigationEntryImpl::TreeNode::CloneAndReplace(
    FrameTreeNode* frame_tree_node,
    FrameNavigationEntry* frame_navigation_entry,
    bool is_root_tree_node) const {
  if (frame_tree_node && MatchesFrame(frame_tree_node, is_root_tree_node)) {
    // Replace this node in the cloned tree and prune its children.
    return base::WrapUnique(
        new NavigationEntryImpl::TreeNode(frame_navigation_entry));
  }

  // Clone the tree using a copy of the FrameNavigationEntry, without sharing.
  // TODO(creis): Share FNEs unless it's for another tab.
  std::unique_ptr<NavigationEntryImpl::TreeNode> copy(
      new NavigationEntryImpl::TreeNode(frame_entry->Clone()));

  // Recursively clone the children.
  for (auto& child : children) {
    copy->children.push_back(
        child->CloneAndReplace(frame_tree_node, frame_navigation_entry, false));
  }

  return copy;
}

std::unique_ptr<NavigationEntry> NavigationEntry::Create() {
  return base::WrapUnique(new NavigationEntryImpl());
}

NavigationEntryImpl* NavigationEntryImpl::FromNavigationEntry(
    NavigationEntry* entry) {
  return static_cast<NavigationEntryImpl*>(entry);
}

const NavigationEntryImpl* NavigationEntryImpl::FromNavigationEntry(
    const NavigationEntry* entry) {
  return static_cast<const NavigationEntryImpl*>(entry);
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::FromNavigationEntry(
    std::unique_ptr<NavigationEntry> entry) {
  return base::WrapUnique(FromNavigationEntry(entry.release()));
}

NavigationEntryImpl::NavigationEntryImpl()
    : NavigationEntryImpl(nullptr, -1, GURL(), Referrer(), base::string16(),
                          ui::PAGE_TRANSITION_LINK, false) {
}

NavigationEntryImpl::NavigationEntryImpl(
    scoped_refptr<SiteInstanceImpl> instance,
    int page_id,
    const GURL& url,
    const Referrer& referrer,
    const base::string16& title,
    ui::PageTransition transition_type,
    bool is_renderer_initiated)
    : frame_tree_(new TreeNode(new FrameNavigationEntry("",
                                                        -1,
                                                        -1,
                                                        std::move(instance),
                                                        nullptr,
                                                        url,
                                                        referrer,
                                                        "GET",
                                                        -1))),
      unique_id_(GetUniqueIDInConstructor()),
      bindings_(kInvalidBindings),
      page_type_(PAGE_TYPE_NORMAL),
      update_virtual_url_with_url_(false),
      title_(title),
      page_id_(page_id),
      transition_type_(transition_type),
      restore_type_(RESTORE_NONE),
      is_overriding_user_agent_(false),
      http_status_code_(0),
      is_renderer_initiated_(is_renderer_initiated),
      should_replace_entry_(false),
      should_clear_history_list_(false),
      can_load_local_resources_(false),
      frame_tree_node_id_(-1) {
#if defined(OS_ANDROID)
  has_user_gesture_ = false;
#endif
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
  frame_tree_->frame_entry->set_url(url);
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetURL() const {
  return frame_tree_->frame_entry->url();
}

void NavigationEntryImpl::SetBaseURLForDataURL(const GURL& url) {
  base_url_for_data_url_ = url;
}

const GURL& NavigationEntryImpl::GetBaseURLForDataURL() const {
  return base_url_for_data_url_;
}

#if defined(OS_ANDROID)
void NavigationEntryImpl::SetDataURLAsString(
    scoped_refptr<base::RefCountedString> data_url) {
  if (data_url) {
    // A quick check that it's actually a data URL.
    DCHECK(base::StartsWith(data_url->front_as<char>(), url::kDataScheme,
                            base::CompareCase::SENSITIVE));
  }
  data_url_as_string_ = data_url;
}

const scoped_refptr<const base::RefCountedString>
NavigationEntryImpl::GetDataURLAsString() const {
  return data_url_as_string_;
}
#endif

void NavigationEntryImpl::SetReferrer(const Referrer& referrer) {
  frame_tree_->frame_entry->set_referrer(referrer);
}

const Referrer& NavigationEntryImpl::GetReferrer() const {
  return frame_tree_->frame_entry->referrer();
}

void NavigationEntryImpl::SetVirtualURL(const GURL& url) {
  virtual_url_ = (url == GetURL()) ? GURL() : url;
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetVirtualURL() const {
  return virtual_url_.is_empty() ? GetURL() : virtual_url_;
}

void NavigationEntryImpl::SetTitle(const base::string16& title) {
  title_ = title;
  cached_display_title_.clear();
}

const base::string16& NavigationEntryImpl::GetTitle() const {
  return title_;
}

void NavigationEntryImpl::SetPageState(const PageState& state) {
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    frame_tree_->frame_entry->set_page_state(state);
    return;
  }

  // SetPageState should only be called before the NavigationEntry has been
  // loaded, such as for restore (when there are no subframe
  // FrameNavigationEntries yet).  However, some callers expect to call this
  // after a Clone but before loading the page.  Clone will copy over the
  // subframe entries, and we should reset them before setting the state again.
  //
  // TODO(creis): It would be good to verify that this NavigationEntry hasn't
  // been loaded yet in cases that SetPageState is called while subframe
  // entries exist, but there's currently no way to check that.
  if (!frame_tree_->children.empty())
    frame_tree_->children.clear();

  // If the PageState can't be parsed or has no children, just store it on the
  // main frame's FrameNavigationEntry without recursively creating subframe
  // entries.
  ExplodedPageState exploded_state;
  if (!DecodePageState(state.ToEncodedData(), &exploded_state) ||
      exploded_state.top.children.size() == 0U) {
    frame_tree_->frame_entry->set_page_state(state);
    return;
  }

  RecursivelyGenerateFrameEntries(exploded_state.top, frame_tree_.get());
}

PageState NavigationEntryImpl::GetPageState() const {
  // Just return the main frame's PageState in default Chrome, or if there are
  // no subframe FrameNavigationEntries.
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries() ||
      frame_tree_->children.size() == 0U)
    return frame_tree_->frame_entry->page_state();

  // When we're using subframe entries, each FrameNavigationEntry has a
  // frame-specific PageState.  We combine these into an ExplodedPageState tree
  // and generate a full PageState from it.
  ExplodedPageState exploded_state;
  RecursivelyGenerateFrameState(frame_tree_.get(), &exploded_state.top,
                                &exploded_state.referenced_files);

  std::string encoded_data;
  if (!EncodePageState(exploded_state, &encoded_data))
    return frame_tree_->frame_entry->page_state();

  return PageState::CreateFromEncodedData(encoded_data);
}

void NavigationEntryImpl::SetPageID(int page_id) {
  page_id_ = page_id;
}

int32_t NavigationEntryImpl::GetPageID() const {
  return page_id_;
}

void NavigationEntryImpl::set_site_instance(
    scoped_refptr<SiteInstanceImpl> site_instance) {
  // TODO(creis): Update all callers and remove this method.
  frame_tree_->frame_entry->set_site_instance(std::move(site_instance));
}

void NavigationEntryImpl::SetBindings(int bindings) {
  // Ensure this is set to a valid value, and that it stays the same once set.
  CHECK_NE(bindings, kInvalidBindings);
  CHECK(bindings_ == kInvalidBindings || bindings_ == bindings);
  bindings_ = bindings;
}

const base::string16& NavigationEntryImpl::GetTitleForDisplay() const {
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
    title = url_formatter::FormatUrl(virtual_url_);
  } else if (!GetURL().is_empty()) {
    title = url_formatter::FormatUrl(GetURL());
  }

  // For file:// URLs use the filename as the title, not the full path.
  if (GetURL().SchemeIsFile()) {
    // It is necessary to ignore the reference and query parameters or else
    // looking for slashes might accidentally return one of those values. See
    // https://crbug.com/503003.
    base::string16::size_type refpos = title.find('#');
    base::string16::size_type querypos = title.find('?');
    base::string16::size_type lastpos;
    if (refpos == base::string16::npos)
      lastpos = querypos;
    else if (querypos == base::string16::npos)
      lastpos = refpos;
    else
      lastpos = (refpos < querypos) ? refpos : querypos;
    base::string16::size_type slashpos = title.rfind('/', lastpos);
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
    ui::PageTransition transition_type) {
  transition_type_ = transition_type;
}

ui::PageTransition NavigationEntryImpl::GetTransitionType() const {
  return transition_type_;
}

const GURL& NavigationEntryImpl::GetUserTypedURL() const {
  return user_typed_url_;
}

void NavigationEntryImpl::SetHasPostData(bool has_post_data) {
  frame_tree_->frame_entry->set_method(has_post_data ? "POST" : "GET");
}

bool NavigationEntryImpl::GetHasPostData() const {
  return frame_tree_->frame_entry->method() == "POST";
}

void NavigationEntryImpl::SetPostID(int64_t post_id) {
  frame_tree_->frame_entry->set_post_id(post_id);
}

int64_t NavigationEntryImpl::GetPostID() const {
  return frame_tree_->frame_entry->post_id();
}

void NavigationEntryImpl::SetPostData(
    const scoped_refptr<ResourceRequestBody>& data) {
  post_data_ = static_cast<ResourceRequestBodyImpl*>(data.get());
}

scoped_refptr<ResourceRequestBody> NavigationEntryImpl::GetPostData() const {
  return post_data_.get();
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

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::Clone() const {
  return NavigationEntryImpl::CloneAndReplace(nullptr, nullptr);
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::CloneAndReplace(
    FrameTreeNode* frame_tree_node,
    FrameNavigationEntry* frame_navigation_entry) const {
  std::unique_ptr<NavigationEntryImpl> copy =
      base::WrapUnique(new NavigationEntryImpl());

  // TODO(creis): Only share the same FrameNavigationEntries if cloning within
  // the same tab.
  copy->frame_tree_ = frame_tree_->CloneAndReplace(
      frame_tree_node, frame_navigation_entry, true);

  // Copy most state over, unless cleared in ResetForCommit.
  // Don't copy unique_id_, otherwise it won't be unique.
  copy->bindings_ = bindings_;
  copy->page_type_ = page_type_;
  copy->virtual_url_ = virtual_url_;
  copy->update_virtual_url_with_url_ = update_virtual_url_with_url_;
  copy->title_ = title_;
  copy->favicon_ = favicon_;
  copy->page_id_ = page_id_;
  copy->ssl_ = ssl_;
  copy->transition_type_ = transition_type_;
  copy->user_typed_url_ = user_typed_url_;
  copy->restore_type_ = restore_type_;
  copy->original_request_url_ = original_request_url_;
  copy->is_overriding_user_agent_ = is_overriding_user_agent_;
  copy->timestamp_ = timestamp_;
  copy->http_status_code_ = http_status_code_;
  // ResetForCommit: post_data_
  copy->screenshot_ = screenshot_;
  copy->extra_headers_ = extra_headers_;
  copy->base_url_for_data_url_ = base_url_for_data_url_;
#if defined(OS_ANDROID)
  copy->data_url_as_string_ = data_url_as_string_;
#endif
  // ResetForCommit: is_renderer_initiated_
  copy->cached_display_title_ = cached_display_title_;
  // ResetForCommit: transferred_global_request_id_
  // ResetForCommit: should_replace_entry_
  copy->redirect_chain_ = redirect_chain_;
  // ResetForCommit: should_clear_history_list_
  // ResetForCommit: frame_tree_node_id_
  // ResetForCommit: intent_received_timestamp_
  copy->extra_data_ = extra_data_;

  return copy;
}

CommonNavigationParams NavigationEntryImpl::ConstructCommonNavigationParams(
    const FrameNavigationEntry& frame_entry,
    const scoped_refptr<ResourceRequestBodyImpl>& post_body,
    const GURL& dest_url,
    const Referrer& dest_referrer,
    FrameMsg_Navigate_Type::Value navigation_type,
    LoFiState lofi_state,
    const base::TimeTicks& navigation_start) const {
  FrameMsg_UILoadMetricsReportType::Value report_type =
      FrameMsg_UILoadMetricsReportType::NO_REPORT;
  base::TimeTicks ui_timestamp = base::TimeTicks();
#if defined(OS_ANDROID)
  if (!intent_received_timestamp().is_null())
    report_type = FrameMsg_UILoadMetricsReportType::REPORT_INTENT;
  ui_timestamp = intent_received_timestamp();
#endif

  std::string method;

  // TODO(clamy): Consult the FrameNavigationEntry in all modes that use
  // subframe navigation entries.
  if (IsBrowserSideNavigationEnabled())
    method = frame_entry.method();
  else
    method = (post_body.get() || GetHasPostData()) ? "POST" : "GET";

  return CommonNavigationParams(
      dest_url, dest_referrer, GetTransitionType(), navigation_type,
      !IsViewSourceMode(), should_replace_entry(), ui_timestamp, report_type,
      GetBaseURLForDataURL(), GetHistoryURLForDataURL(), lofi_state,
      navigation_start, method, post_body ? post_body : post_data_);
}

StartNavigationParams NavigationEntryImpl::ConstructStartNavigationParams()
    const {
  return StartNavigationParams(extra_headers(),
#if defined(OS_ANDROID)
                               has_user_gesture(),
#endif
                               transferred_global_request_id().child_id,
                               transferred_global_request_id().request_id);
}

RequestNavigationParams NavigationEntryImpl::ConstructRequestNavigationParams(
    const FrameNavigationEntry& frame_entry,
    bool is_same_document_history_load,
    bool has_committed_real_load,
    bool intended_as_new_entry,
    int pending_history_list_offset,
    int current_history_list_offset,
    int current_history_list_length) const {
  // Set the redirect chain to the navigation's redirects, unless returning to a
  // completed navigation (whose previous redirects don't apply).
  std::vector<GURL> redirects;
  if (ui::PageTransitionIsNewNavigation(GetTransitionType())) {
    redirects = GetRedirectChain();
  }

  int pending_offset_to_send = pending_history_list_offset;
  int current_offset_to_send = current_history_list_offset;
  int current_length_to_send = current_history_list_length;
  if (should_clear_history_list()) {
    // Set the history list related parameters to the same values a
    // NavigationController would return before its first navigation. This will
    // fully clear the RenderView's view of the session history.
    pending_offset_to_send = -1;
    current_offset_to_send = -1;
    current_length_to_send = 0;
  }

  RequestNavigationParams request_params(
      GetIsOverridingUserAgent(), redirects, GetCanLoadLocalResources(),
      base::Time::Now(), frame_entry.page_state(), GetPageID(), GetUniqueID(),
      is_same_document_history_load, has_committed_real_load,
      intended_as_new_entry, pending_offset_to_send, current_offset_to_send,
      current_length_to_send, IsViewSourceMode(), should_clear_history_list());
#if defined(OS_ANDROID)
  if (GetDataURLAsString() &&
      GetDataURLAsString()->size() <= kMaxLengthOfDataURLString) {
    // The number of characters that is enough for validating a data: URI.  From
    // the GURL's POV, the only important part here is scheme, it doesn't check
    // the actual content. Thus we can take only the prefix of the url, to avoid
    // unneeded copying of a potentially long string.
    const size_t kDataUriPrefixMaxLen = 64;
    GURL data_url(std::string(
        GetDataURLAsString()->front_as<char>(),
        std::min(GetDataURLAsString()->size(), kDataUriPrefixMaxLen)));
    if (data_url.is_valid() && data_url.SchemeIs(url::kDataScheme))
      request_params.data_url_as_string = GetDataURLAsString()->data();
  }
#endif
  return request_params;
}

void NavigationEntryImpl::ResetForCommit(FrameNavigationEntry* frame_entry) {
  // Any state that only matters when a navigation entry is pending should be
  // cleared here.
  // TODO(creis): This state should be moved to NavigationRequest once
  // PlzNavigate is enabled.
  SetPostData(nullptr);
  set_is_renderer_initiated(false);
  set_transferred_global_request_id(GlobalRequestID());
  set_should_replace_entry(false);

  set_should_clear_history_list(false);
  set_frame_tree_node_id(-1);

  if (frame_entry)
    frame_entry->set_source_site_instance(nullptr);

#if defined(OS_ANDROID)
  // Reset the time stamp so that the metrics are not reported if this entry is
  // loaded again in the future.
  set_intent_received_timestamp(base::TimeTicks());
#endif
}

void NavigationEntryImpl::AddOrUpdateFrameEntry(
    FrameTreeNode* frame_tree_node,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    SiteInstanceImpl* site_instance,
    scoped_refptr<SiteInstanceImpl> source_site_instance,
    const GURL& url,
    const Referrer& referrer,
    const PageState& page_state,
    const std::string& method,
    int64_t post_id) {
  // If this is called for the main frame, the FrameNavigationEntry is
  // guaranteed to exist, so just update it directly and return.
  if (frame_tree_node->IsMainFrame()) {
    // If the document of the FrameNavigationEntry is changing, we must clear
    // any child FrameNavigationEntries.
    if (root_node()->frame_entry->document_sequence_number() !=
        document_sequence_number)
      root_node()->children.clear();

    root_node()->frame_entry->UpdateEntry(
        frame_tree_node->unique_name(), item_sequence_number,
        document_sequence_number, site_instance,
        std::move(source_site_instance), url, referrer, page_state, method,
        post_id);
    return;
  }

  // We should already have a TreeNode for the parent node by the time this node
  // commits.  Find it first.
  NavigationEntryImpl::TreeNode* parent_node =
      FindFrameEntry(frame_tree_node->parent());
  if (!parent_node) {
    // The renderer should not send a commit for a subframe before its parent.
    // TODO(creis): Kill the renderer if we get here.
    return;
  }

  // Now check whether we have a TreeNode for the node itself.
  const std::string& unique_name = frame_tree_node->unique_name();
  for (TreeNode* child : parent_node->children) {
    if (child->frame_entry->frame_unique_name() == unique_name) {
      // Update the existing FrameNavigationEntry (e.g., for replaceState).
      child->frame_entry->UpdateEntry(unique_name, item_sequence_number,
                                      document_sequence_number, site_instance,
                                      std::move(source_site_instance), url,
                                      referrer, page_state, method, post_id);
      return;
    }
  }

  // No entry exists yet, so create a new one.
  // Unordered list, since we expect to look up entries by frame sequence number
  // or unique name.
  FrameNavigationEntry* frame_entry = new FrameNavigationEntry(
      unique_name, item_sequence_number, document_sequence_number,
      site_instance, std::move(source_site_instance), url, referrer, method,
      post_id);
  frame_entry->set_page_state(page_state);
  parent_node->children.push_back(
      new NavigationEntryImpl::TreeNode(frame_entry));
}

FrameNavigationEntry* NavigationEntryImpl::GetFrameEntry(
    FrameTreeNode* frame_tree_node) const {
  NavigationEntryImpl::TreeNode* tree_node = FindFrameEntry(frame_tree_node);
  return tree_node ? tree_node->frame_entry.get() : nullptr;
}

void NavigationEntryImpl::SetScreenshotPNGData(
    scoped_refptr<base::RefCountedBytes> png_data) {
  screenshot_ = png_data;
  if (screenshot_.get())
    UMA_HISTOGRAM_MEMORY_KB("Overscroll.ScreenshotSize", screenshot_->size());
}

GURL NavigationEntryImpl::GetHistoryURLForDataURL() const {
  return GetBaseURLForDataURL().is_empty() ? GURL() : GetVirtualURL();
}

NavigationEntryImpl::TreeNode* NavigationEntryImpl::FindFrameEntry(
    FrameTreeNode* frame_tree_node) const {
  NavigationEntryImpl::TreeNode* node = nullptr;
  std::queue<NavigationEntryImpl::TreeNode*> work_queue;
  work_queue.push(root_node());
  while (!work_queue.empty()) {
    node = work_queue.front();
    work_queue.pop();
    if (node->MatchesFrame(frame_tree_node, node == root_node()))
      return node;

    // Enqueue any children and keep looking.
    for (auto& child : node->children)
      work_queue.push(child);
  }
  return nullptr;
}

}  // namespace content
