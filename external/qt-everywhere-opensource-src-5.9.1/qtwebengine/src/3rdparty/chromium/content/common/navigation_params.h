// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATION_PARAMS_H_
#define CONTENT_COMMON_NAVIGATION_PARAMS_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/frame_message_enums.h"
#include "content/common/resource_request_body_impl.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_response.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {

// The LoFi state which determines whether to add the Lo-Fi header.
enum LoFiState {
  // Let the browser process decide whether or not to request the Lo-Fi version.
  LOFI_UNSPECIFIED = 0,

  // Request a normal (non-Lo-Fi) version of the resource.
  LOFI_OFF,

  // Request a Lo-Fi version of the resource.
  LOFI_ON,
};

// PlzNavigate
// Helper function to determine if the navigation to |url| should make a request
// to the network stack. A request should not be sent for JavaScript URLs or
// about:blank. In these cases, no request needs to be sent.
bool CONTENT_EXPORT ShouldMakeNetworkRequestForURL(const GURL& url);

// The following structures hold parameters used during a navigation. In
// particular they are used by FrameMsg_Navigate, FrameMsg_CommitNavigation and
// FrameHostMsg_BeginNavigation.

// Provided by the browser or the renderer -------------------------------------

// Used by all navigation IPCs.
struct CONTENT_EXPORT CommonNavigationParams {
  CommonNavigationParams();
  CommonNavigationParams(
      const GURL& url,
      const Referrer& referrer,
      ui::PageTransition transition,
      FrameMsg_Navigate_Type::Value navigation_type,
      bool allow_download,
      bool should_replace_current_entry,
      base::TimeTicks ui_timestamp,
      FrameMsg_UILoadMetricsReportType::Value report_type,
      const GURL& base_url_for_data_url,
      const GURL& history_url_for_data_url,
      LoFiState lofi_state,
      const base::TimeTicks& navigation_start,
      std::string method,
      const scoped_refptr<ResourceRequestBodyImpl>& post_data);
  CommonNavigationParams(const CommonNavigationParams& other);
  ~CommonNavigationParams();

  // The URL to navigate to.
  // PlzNavigate: May be modified when the navigation is ready to commit.
  GURL url;

  // The URL to send in the "Referer" header field. Can be empty if there is
  // no referrer.
  Referrer referrer;

  // The type of transition.
  ui::PageTransition transition;

  // Type of navigation.
  FrameMsg_Navigate_Type::Value navigation_type;

  // Allows the URL to be downloaded (true by default).
  // Avoid downloading when in view-source mode.
  bool allow_download;

  // Informs the RenderView the pending navigation should replace the current
  // history entry when it commits. This is used for cross-process redirects so
  // the transferred navigation can recover the navigation state.
  // PlzNavigate: this is used by client-side redirects to indicate that when
  // the navigation commits, it should commit in the existing page.
  bool should_replace_current_entry;

  // Timestamp of the user input event that triggered this navigation. Empty if
  // the navigation was not triggered by clicking on a link or by receiving an
  // intent on Android.
  base::TimeTicks ui_timestamp;

  // The report type to be used when recording the metric using |ui_timestamp|.
  FrameMsg_UILoadMetricsReportType::Value report_type;

  // Base URL for use in Blink's SubstituteData.
  // Is only used with data: URLs.
  GURL base_url_for_data_url;

  // History URL for use in Blink's SubstituteData.
  // Is only used with data: URLs.
  GURL history_url_for_data_url;

  // Whether or not to request a LoFi version of the document or let the browser
  // decide.
  LoFiState lofi_state;

  // The navigationStart time exposed through the Navigation Timing API to JS.
  // If this is for a browser-initiated navigation, this can override the
  // navigation_start value in Blink.
  // PlzNavigate: For renderer initiated navigations, this will be set on the
  // renderer side and sent with FrameHostMsg_BeginNavigation.
  base::TimeTicks navigation_start;

  // The request method: GET, POST, etc.
  std::string method;

  // Body of HTTP POST request.
  scoped_refptr<ResourceRequestBodyImpl> post_data;
};

// Provided by the renderer ----------------------------------------------------
//
// This struct holds parameters sent by the renderer to the browser. It is only
// used in PlzNavigate (since in the current architecture, the renderer does not
// inform the browser of navigations until they commit).

// This struct is not used outside of the PlzNavigate project.
// PlzNavigate: parameters needed to start a navigation on the IO thread,
// following a renderer-initiated navigation request.
struct CONTENT_EXPORT BeginNavigationParams {
  // TODO(clamy): See if it is possible to reuse this in
  // ResourceMsg_Request_Params.
  BeginNavigationParams();
  BeginNavigationParams(std::string headers,
                        int load_flags,
                        bool has_user_gesture,
                        bool skip_service_worker,
                        RequestContextType request_context_type);
  BeginNavigationParams(const BeginNavigationParams& other);

  // Additional HTTP request headers.
  std::string headers;

  // net::URLRequest load flags (net::LOAD_NORMAL) by default).
  int load_flags;

  // True if the request was user initiated.
  bool has_user_gesture;

  // True if the ServiceWorker should be skipped.
  bool skip_service_worker;

  // Indicates the request context type.
  RequestContextType request_context_type;

  // See WebSearchableFormData for a description of these.
  GURL searchable_form_url;
  std::string searchable_form_encoding;
};

// Provided by the browser -----------------------------------------------------
//
// These structs are sent by the browser to the renderer to start/commit a
// navigation depending on whether browser-side navigation is enabled.
// Parameters used both in the current architecture and PlzNavigate should be
// put in RequestNavigationParams.  Parameters only used by the current
// architecture should go in StartNavigationParams.

// Used by FrameMsg_Navigate. Holds the parameters needed by the renderer to
// start a browser-initiated navigation besides those in CommonNavigationParams.
// The difference with the RequestNavigationParams below is that they are only
// used in the current architecture of navigation, and will not be used by
// PlzNavigate.
// PlzNavigate: These are not used.
struct CONTENT_EXPORT StartNavigationParams {
  StartNavigationParams();
  StartNavigationParams(const std::string& extra_headers,
                        int transferred_request_child_id,
                        int transferred_request_request_id);
  StartNavigationParams(const StartNavigationParams& other);
  ~StartNavigationParams();

  // Extra headers (separated by \n) to send during the request.
  std::string extra_headers;

  // The following two members identify a previous request that has been
  // created before this navigation is being transferred to a new process.
  // This serves the purpose of recycling the old request.
  // Unless this refers to a transferred navigation, these values are -1 and -1.
  int transferred_request_child_id;
  int transferred_request_request_id;
};

// PlzNavigate
// Timings collected in the browser during navigation for the
// Navigation Timing API. Sent to Blink in RequestNavigationParams when
// the navigation is ready to be committed.
struct CONTENT_EXPORT NavigationTiming {
  base::TimeTicks redirect_start;
  base::TimeTicks redirect_end;
  base::TimeTicks fetch_start;
};

// Used by FrameMsg_Navigate. Holds the parameters needed by the renderer to
// start a browser-initiated navigation besides those in CommonNavigationParams.
// PlzNavigate: sent to the renderer to make it issue a stream request for a
// navigation that is ready to commit.
struct CONTENT_EXPORT RequestNavigationParams {
  RequestNavigationParams();
  RequestNavigationParams(bool is_overriding_user_agent,
                          const std::vector<GURL>& redirects,
                          bool can_load_local_resources,
                          const PageState& page_state,
                          int nav_entry_id,
                          bool is_same_document_history_load,
                          bool is_history_navigation_in_new_child,
                          std::map<std::string, bool> subframe_unique_names,
                          bool has_committed_real_load,
                          bool intended_as_new_entry,
                          int pending_history_list_offset,
                          int current_history_list_offset,
                          int current_history_list_length,
                          bool is_view_source,
                          bool should_clear_history_list,
                          bool has_user_gesture);
  RequestNavigationParams(const RequestNavigationParams& other);
  ~RequestNavigationParams();

  // Whether or not the user agent override string should be used.
  bool is_overriding_user_agent;

  // Any redirect URLs that occurred before |url|. Useful for cross-process
  // navigations; defaults to empty.
  std::vector<GURL> redirects;

  // The ResourceResponseInfos received during redirects.
  std::vector<ResourceResponseInfo> redirect_response;

  // Whether or not this url should be allowed to access local file://
  // resources.
  bool can_load_local_resources;

  // Opaque history state (received by ViewHostMsg_UpdateState).
  PageState page_state;

  // For browser-initiated navigations, this is the unique id of the
  // NavigationEntry being navigated to. (For renderer-initiated navigations it
  // is 0.) If the load succeeds, then this nav_entry_id will be reflected in
  // the resulting FrameHostMsg_DidCommitProvisionalLoad message.
  int nav_entry_id;

  // For history navigations, this indicates whether the load will stay within
  // the same document.  Defaults to false.
  bool is_same_document_history_load;

  // Whether this is a history navigation in a newly created child frame, in
  // which case the browser process is instructing the renderer process to load
  // a URL from a session history item.  Defaults to false.
  bool is_history_navigation_in_new_child;

  // If this is a history navigation, this contains a map of frame unique names
  // to |is_about_blank| for immediate children of the frame being navigated for
  // which there are history items.  The renderer process only needs to check
  // with the browser process for newly created subframes that have these unique
  // names (and only when not staying on about:blank).
  // TODO(creis): Expand this to a data structure including corresponding
  // same-process PageStates for the whole subtree in https://crbug.com/639842.
  std::map<std::string, bool> subframe_unique_names;

  // Whether the frame being navigated has already committed a real page, which
  // affects how new navigations are classified in the renderer process.
  // This currently is only ever set to true in --site-per-process mode.
  // TODO(creis): Create FrameNavigationEntries by default so this always works.
  bool has_committed_real_load;

  // For browser-initiated navigations, this is true if this is a new entry
  // being navigated to. This is false otherwise. TODO(avi): Remove this when
  // the pending entry situation is made sane and the browser keeps them around
  // long enough to match them via nav_entry_id, above.
  bool intended_as_new_entry;

  // For history navigations, this is the offset in the history list of the
  // pending load. For non-history navigations, this will be ignored.
  int pending_history_list_offset;

  // Where its current page contents reside in session history and the total
  // size of the session history list.
  int current_history_list_offset;
  int current_history_list_length;

  // Indicates whether the navigation is to a view-source:// scheme or not.
  // It is a separate boolean as the view-source scheme is stripped from the
  // URL before it is sent to the renderer process and the RenderFrame needs
  // to be put in special view source mode.
  bool is_view_source;

  // Whether session history should be cleared. In that case, the RenderView
  // needs to notify the browser that the clearing was succesful when the
  // navigation commits.
  bool should_clear_history_list;

  // PlzNavigate
  // Whether a ServiceWorkerProviderHost should be created for the window.
  bool should_create_service_worker;

  // PlzNavigate
  // Timing of navigation events.
  NavigationTiming navigation_timing;

  // PlzNavigate
  // The ServiceWorkerProviderHost ID used for navigations, if it was already
  // created by the browser. Set to kInvalidServiceWorkerProviderId otherwise.
  // This parameter is not used in the current navigation architecture, where
  // it will always be equal to kInvalidServiceWorkerProviderId.
  int service_worker_provider_id;

  // True if the navigation originated due to a user gesture.
  bool has_user_gesture;

#if defined(OS_ANDROID)
  // The real content of the data: URL. Only used in Android WebView for
  // implementing LoadDataWithBaseUrl API method to circumvent the restriction
  // on the GURL max length in the IPC layer. Short data: URLs can still be
  // passed in the |CommonNavigationParams::url| field.
  std::string data_url_as_string;
#endif
};

// Helper struct keeping track in one place of all the parameters the browser
// needs to provide to the renderer.
struct NavigationParams {
  NavigationParams(const CommonNavigationParams& common_params,
                   const StartNavigationParams& start_params,
                   const RequestNavigationParams& request_params);
  ~NavigationParams();

  CommonNavigationParams common_params;
  StartNavigationParams start_params;
  RequestNavigationParams request_params;
};

}  // namespace content

#endif  // CONTENT_COMMON_NAVIGATION_PARAMS_H_
