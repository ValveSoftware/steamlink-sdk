// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for interacting with frames.
// Multiply-included message file, hence no include guard.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_sequence.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/content_security_policy_header.h"
#include "content/common/frame_message_enums.h"
#include "content/common/frame_replication_state.h"
#include "content/common/navigation_gesture.h"
#include "content/common/navigation_params.h"
#include "content/common/savable_subframe.h"
#include "content/public/common/color_suggestion.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/console_message_level.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/file_chooser_file_info.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/page_importance_signals.h"
#include "content/public/common/page_state.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/transition_element.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/public/platform/WebFocusType.h"
#include "third_party/WebKit/public/platform/WebInsecureRequestPolicy.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "third_party/WebKit/public/web/WebFrameOwnerProperties.h"
#include "third_party/WebKit/public/web/WebFrameSerializerCacheControlPolicy.h"
#include "third_party/WebKit/public/web/WebTreeScopeType.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(ENABLE_PLUGINS)
#include "content/common/pepper_renderer_instance_data.h"
#endif

// Singly-included section for type definitions.
#ifndef CONTENT_COMMON_FRAME_MESSAGES_H_
#define CONTENT_COMMON_FRAME_MESSAGES_H_

using FrameMsg_GetSerializedHtmlWithLocalLinks_UrlMap =
    std::map<GURL, base::FilePath>;
using FrameMsg_GetSerializedHtmlWithLocalLinks_FrameRoutingIdMap =
    std::map<int, base::FilePath>;

using FrameMsg_SerializeAsMHTML_FrameRoutingIdToContentIdMap =
    std::map<int, std::string>;

#endif  // CONTENT_COMMON_FRAME_MESSAGES_H_

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START FrameMsgStart

IPC_ENUM_TRAITS_MIN_MAX_VALUE(AccessibilityMode,
                              AccessibilityModeOff,
                              AccessibilityModeComplete)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(content::JavaScriptMessageType,
                              content::JAVASCRIPT_MESSAGE_TYPE_ALERT,
                              content::JAVASCRIPT_MESSAGE_TYPE_PROMPT)
IPC_ENUM_TRAITS_MAX_VALUE(FrameMsg_Navigate_Type::Value,
                          FrameMsg_Navigate_Type::NAVIGATE_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(FrameMsg_UILoadMetricsReportType::Value,
                          FrameMsg_UILoadMetricsReportType::REPORT_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebContextMenuData::MediaType,
                          blink::WebContextMenuData::MediaTypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebContextMenuData::InputFieldType,
                          blink::WebContextMenuData::InputFieldTypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebFocusType, blink::WebFocusTypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebFrameOwnerProperties::ScrollingMode,
                          blink::WebFrameOwnerProperties::ScrollingMode::Last)
IPC_ENUM_TRAITS_MAX_VALUE(content::StopFindAction,
                          content::STOP_FIND_ACTION_LAST)
IPC_ENUM_TRAITS(blink::WebSandboxFlags)  // Bitmask.
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebTreeScopeType,
                          blink::WebTreeScopeType::Last)
IPC_ENUM_TRAITS_MAX_VALUE(ui::MenuSourceType, ui::MENU_SOURCE_TYPE_LAST)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(content::LoFiState,
                              content::LOFI_UNSPECIFIED,
                              content::LOFI_ON)
IPC_ENUM_TRAITS_MAX_VALUE(content::FileChooserParams::Mode,
                          content::FileChooserParams::Save)

IPC_STRUCT_TRAITS_BEGIN(blink::WebFindOptions)
  IPC_STRUCT_TRAITS_MEMBER(forward)
  IPC_STRUCT_TRAITS_MEMBER(matchCase)
  IPC_STRUCT_TRAITS_MEMBER(findNext)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ColorSuggestion)
  IPC_STRUCT_TRAITS_MEMBER(color)
  IPC_STRUCT_TRAITS_MEMBER(label)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ContextMenuParams)
  IPC_STRUCT_TRAITS_MEMBER(media_type)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(link_url)
  IPC_STRUCT_TRAITS_MEMBER(link_text)
  IPC_STRUCT_TRAITS_MEMBER(unfiltered_link_url)
  IPC_STRUCT_TRAITS_MEMBER(src_url)
  IPC_STRUCT_TRAITS_MEMBER(has_image_contents)
  IPC_STRUCT_TRAITS_MEMBER(properties)
  IPC_STRUCT_TRAITS_MEMBER(page_url)
  IPC_STRUCT_TRAITS_MEMBER(keyword_url)
  IPC_STRUCT_TRAITS_MEMBER(frame_url)
  IPC_STRUCT_TRAITS_MEMBER(frame_page_state)
  IPC_STRUCT_TRAITS_MEMBER(media_flags)
  IPC_STRUCT_TRAITS_MEMBER(selection_text)
  IPC_STRUCT_TRAITS_MEMBER(title_text)
  IPC_STRUCT_TRAITS_MEMBER(suggested_filename)
  IPC_STRUCT_TRAITS_MEMBER(misspelled_word)
  IPC_STRUCT_TRAITS_MEMBER(misspelling_hash)
  IPC_STRUCT_TRAITS_MEMBER(dictionary_suggestions)
  IPC_STRUCT_TRAITS_MEMBER(spellcheck_enabled)
  IPC_STRUCT_TRAITS_MEMBER(is_editable)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_default)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_left_to_right)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_right_to_left)
  IPC_STRUCT_TRAITS_MEMBER(edit_flags)
  IPC_STRUCT_TRAITS_MEMBER(security_info)
  IPC_STRUCT_TRAITS_MEMBER(frame_charset)
  IPC_STRUCT_TRAITS_MEMBER(referrer_policy)
  IPC_STRUCT_TRAITS_MEMBER(custom_context)
  IPC_STRUCT_TRAITS_MEMBER(custom_items)
  IPC_STRUCT_TRAITS_MEMBER(source_type)
#if defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(selection_start)
  IPC_STRUCT_TRAITS_MEMBER(selection_end)
#endif
  IPC_STRUCT_TRAITS_MEMBER(input_field_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::CustomContextMenuContext)
  IPC_STRUCT_TRAITS_MEMBER(is_pepper_menu)
  IPC_STRUCT_TRAITS_MEMBER(request_id)
  IPC_STRUCT_TRAITS_MEMBER(render_widget_id)
  IPC_STRUCT_TRAITS_MEMBER(link_followed)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebFrameOwnerProperties)
  IPC_STRUCT_TRAITS_MEMBER(scrollingMode)
  IPC_STRUCT_TRAITS_MEMBER(marginWidth)
  IPC_STRUCT_TRAITS_MEMBER(marginHeight)
  IPC_STRUCT_TRAITS_MEMBER(allowFullscreen)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::TransitionElement)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(rect)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::PageImportanceSignals)
  IPC_STRUCT_TRAITS_MEMBER(had_form_interaction)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(FrameHostMsg_DidFailProvisionalLoadWithError_Params)
  // Error code as reported in the DidFailProvisionalLoad callback.
  IPC_STRUCT_MEMBER(int, error_code)
  // An error message generated from the error_code. This can be an empty
  // string if we were unable to find a meaningful description.
  IPC_STRUCT_MEMBER(base::string16, error_description)
  // The URL that the error is reported for.
  IPC_STRUCT_MEMBER(GURL, url)
  // True if the failure is the result of navigating to a POST again
  // and we're going to show the POST interstitial.
  IPC_STRUCT_MEMBER(bool, showing_repost_interstitial)
  // True if the navigation was canceled because it was ignored by a handler,
  // e.g. shouldOverrideUrlLoading.
  IPC_STRUCT_MEMBER(bool, was_ignored_by_handler)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(content::FrameNavigateParams)
  IPC_STRUCT_TRAITS_MEMBER(page_id)
  IPC_STRUCT_TRAITS_MEMBER(nav_entry_id)
  IPC_STRUCT_TRAITS_MEMBER(frame_unique_name)
  IPC_STRUCT_TRAITS_MEMBER(item_sequence_number)
  IPC_STRUCT_TRAITS_MEMBER(document_sequence_number)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(base_url)
  IPC_STRUCT_TRAITS_MEMBER(referrer)
  IPC_STRUCT_TRAITS_MEMBER(transition)
  IPC_STRUCT_TRAITS_MEMBER(redirects)
  IPC_STRUCT_TRAITS_MEMBER(should_update_history)
  IPC_STRUCT_TRAITS_MEMBER(searchable_form_url)
  IPC_STRUCT_TRAITS_MEMBER(searchable_form_encoding)
  IPC_STRUCT_TRAITS_MEMBER(contents_mime_type)
  IPC_STRUCT_TRAITS_MEMBER(socket_address)
IPC_STRUCT_TRAITS_END()

// Parameters structure for FrameHostMsg_DidCommitProvisionalLoad, which has
// too many data parameters to be reasonably put in a predefined IPC message.
IPC_STRUCT_BEGIN_WITH_PARENT(FrameHostMsg_DidCommitProvisionalLoad_Params,
                             content::FrameNavigateParams)
  IPC_STRUCT_TRAITS_PARENT(content::FrameNavigateParams)

  // This is the value from the browser (copied from the navigation request)
  // indicating whether it intended to make a new entry. TODO(avi): Remove this
  // when the pending entry situation is made sane and the browser keeps them
  // around long enough to match them via nav_entry_id.
  IPC_STRUCT_MEMBER(bool, intended_as_new_entry)

  // Whether this commit created a new entry.
  IPC_STRUCT_MEMBER(bool, did_create_new_entry)

  // Whether this commit should replace the current entry.
  IPC_STRUCT_MEMBER(bool, should_replace_current_entry)

  // Information regarding the security of the connection (empty if the
  // connection was not secure).
  IPC_STRUCT_MEMBER(std::string, security_info)

  // The gesture that initiated this navigation.
  IPC_STRUCT_MEMBER(content::NavigationGesture, gesture)

  // The HTTP method used by the navigation.
  IPC_STRUCT_MEMBER(std::string, method)

  // The POST body identifier. -1 if it doesn't exist.
  IPC_STRUCT_MEMBER(int64_t, post_id)

  // Whether the frame navigation resulted in no change to the documents within
  // the page. For example, the navigation may have just resulted in scrolling
  // to a named anchor.
  IPC_STRUCT_MEMBER(bool, was_within_same_page)

  // The status code of the HTTP request.
  IPC_STRUCT_MEMBER(int, http_status_code)

  // This flag is used to warn if the renderer is displaying an error page,
  // so that we can set the appropriate page type.
  IPC_STRUCT_MEMBER(bool, url_is_unreachable)

  // True if the connection was proxied.  In this case, socket_address
  // will represent the address of the proxy, rather than the remote host.
  IPC_STRUCT_MEMBER(bool, was_fetched_via_proxy)

  // Serialized history item state to store in the navigation entry.
  IPC_STRUCT_MEMBER(content::PageState, page_state)

  // Original request's URL.
  IPC_STRUCT_MEMBER(GURL, original_request_url)

  // User agent override used to navigate.
  IPC_STRUCT_MEMBER(bool, is_overriding_user_agent)

  // Notifies the browser that for this navigation, the session history was
  // successfully cleared.
  IPC_STRUCT_MEMBER(bool, history_list_was_cleared)

  // The routing_id of the render view associated with the navigation.
  // We need to track the RenderViewHost routing_id because of downstream
  // dependencies (crbug.com/392171 DownloadRequestHandle, SaveFileManager,
  // ResourceDispatcherHostImpl, MediaStreamUIProxy,
  // SpeechRecognitionDispatcherHost and possibly others). They look up the view
  // based on the ID stored in the resource requests. Once those dependencies
  // are unwound or moved to RenderFrameHost (crbug.com/304341) we can move the
  // client to be based on the routing_id of the RenderFrameHost.
  IPC_STRUCT_MEMBER(int, render_view_routing_id)

  // Origin of the frame.  This will be replicated to any associated
  // RenderFrameProxies.
  IPC_STRUCT_MEMBER(url::Origin, origin)

  // How navigation metrics starting on UI action for this load should be
  // reported.
  IPC_STRUCT_MEMBER(FrameMsg_UILoadMetricsReportType::Value, report_type)

  // Timestamp at which the UI action that triggered the navigation originated.
  IPC_STRUCT_MEMBER(base::TimeTicks, ui_timestamp)

  // The insecure request policy the document for the load is enforcing.
  IPC_STRUCT_MEMBER(blink::WebInsecureRequestPolicy, insecure_request_policy)

  // True if the document for the load is a unique origin that should be
  // considered potentially trustworthy.
  IPC_STRUCT_MEMBER(bool, has_potentially_trustworthy_unique_origin)

  // True if the navigation originated as an srcdoc attribute.
  IPC_STRUCT_MEMBER(bool, is_srcdoc)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(FrameMsg_PostMessage_Params)
  // Whether the data format is supplied as serialized script value, or as
  // a simple string. If it is a raw string, must be converted from string to a
  // WebSerializedScriptValue in the renderer process.
  IPC_STRUCT_MEMBER(bool, is_data_raw_string)

  // The serialized script value.
  IPC_STRUCT_MEMBER(base::string16, data)

  // When sent to the browser, this is the routing ID of the source frame in
  // the source process.  The browser replaces it with the routing ID of the
  // equivalent frame proxy in the destination process.
  IPC_STRUCT_MEMBER(int, source_routing_id)

  // The origin of the source frame.
  IPC_STRUCT_MEMBER(base::string16, source_origin)

  // The origin for the message's target.
  IPC_STRUCT_MEMBER(base::string16, target_origin)

  // Information about the MessagePorts this message contains.
  IPC_STRUCT_MEMBER(std::vector<int>, message_ports)
  IPC_STRUCT_MEMBER(std::vector<int>, new_routing_ids)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(content::CommonNavigationParams)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(referrer)
  IPC_STRUCT_TRAITS_MEMBER(transition)
  IPC_STRUCT_TRAITS_MEMBER(navigation_type)
  IPC_STRUCT_TRAITS_MEMBER(allow_download)
  IPC_STRUCT_TRAITS_MEMBER(should_replace_current_entry)
  IPC_STRUCT_TRAITS_MEMBER(ui_timestamp)
  IPC_STRUCT_TRAITS_MEMBER(report_type)
  IPC_STRUCT_TRAITS_MEMBER(base_url_for_data_url)
  IPC_STRUCT_TRAITS_MEMBER(history_url_for_data_url)
  IPC_STRUCT_TRAITS_MEMBER(lofi_state)
  IPC_STRUCT_TRAITS_MEMBER(navigation_start)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(post_data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::BeginNavigationParams)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(load_flags)
  IPC_STRUCT_TRAITS_MEMBER(has_user_gesture)
  IPC_STRUCT_TRAITS_MEMBER(skip_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(request_context_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::StartNavigationParams)
  IPC_STRUCT_TRAITS_MEMBER(extra_headers)
#if defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(has_user_gesture)
#endif
  IPC_STRUCT_TRAITS_MEMBER(transferred_request_child_id)
  IPC_STRUCT_TRAITS_MEMBER(transferred_request_request_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::RequestNavigationParams)
  IPC_STRUCT_TRAITS_MEMBER(is_overriding_user_agent)
  IPC_STRUCT_TRAITS_MEMBER(redirects)
  IPC_STRUCT_TRAITS_MEMBER(can_load_local_resources)
  IPC_STRUCT_TRAITS_MEMBER(request_time)
  IPC_STRUCT_TRAITS_MEMBER(page_state)
  IPC_STRUCT_TRAITS_MEMBER(page_id)
  IPC_STRUCT_TRAITS_MEMBER(nav_entry_id)
  IPC_STRUCT_TRAITS_MEMBER(is_same_document_history_load)
  IPC_STRUCT_TRAITS_MEMBER(has_committed_real_load)
  IPC_STRUCT_TRAITS_MEMBER(intended_as_new_entry)
  IPC_STRUCT_TRAITS_MEMBER(pending_history_list_offset)
  IPC_STRUCT_TRAITS_MEMBER(current_history_list_offset)
  IPC_STRUCT_TRAITS_MEMBER(current_history_list_length)
  IPC_STRUCT_TRAITS_MEMBER(is_view_source)
  IPC_STRUCT_TRAITS_MEMBER(should_clear_history_list)
  IPC_STRUCT_TRAITS_MEMBER(should_create_service_worker)
#if defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(data_url_as_string)
#endif
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::FrameReplicationState)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(sandbox_flags)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(unique_name)
  IPC_STRUCT_TRAITS_MEMBER(accumulated_csp_headers)
  IPC_STRUCT_TRAITS_MEMBER(scope)
  IPC_STRUCT_TRAITS_MEMBER(insecure_request_policy)
  IPC_STRUCT_TRAITS_MEMBER(has_potentially_trustworthy_unique_origin)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(FrameMsg_NewFrame_WidgetParams)
  // Gives the routing ID for the RenderWidget that will be attached to the
  // new RenderFrame. If the RenderFrame does not need a RenderWidget, this
  // is MSG_ROUTING_NONE and the other parameters are not read.
  IPC_STRUCT_MEMBER(int, routing_id)

  // Tells the new RenderWidget whether it is initially hidden.
  IPC_STRUCT_MEMBER(bool, hidden)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(FrameMsg_NewFrame_Params)
  // Specifies the routing ID of the new RenderFrame object.
  IPC_STRUCT_MEMBER(int, routing_id)

  // If a valid |proxy_routing_id| is provided, the new frame will be
  // configured to replace the proxy on commit.
  IPC_STRUCT_MEMBER(int, proxy_routing_id)

  // Specifies the new frame's opener.  The opener will be null if this is
  // MSG_ROUTING_NONE.
  IPC_STRUCT_MEMBER(int, opener_routing_id)

  // The new frame should be created as a child of the object
  // identified by |parent_routing_id| or as top level if that is
  // MSG_ROUTING_NONE.
  IPC_STRUCT_MEMBER(int, parent_routing_id)

  // Identifies the previous sibling of the new frame, so that the new frame is
  // inserted into the correct place in the frame tree.  If this is
  // MSG_ROUTING_NONE, the frame will be created as the leftmost child of its
  // parent frame, in front of any other children.
  IPC_STRUCT_MEMBER(int, previous_sibling_routing_id)

  // When the new frame has a parent, |replication_state| holds the new frame's
  // properties replicated from the process rendering the parent frame, such as
  // the new frame's sandbox flags.
  IPC_STRUCT_MEMBER(content::FrameReplicationState, replication_state)

  // When the new frame has a parent, |frame_owner_properties| holds the
  // properties of the HTMLFrameOwnerElement from the parent process.
  // Note that unlike FrameReplicationState, this is not replicated for remote
  // frames.
  IPC_STRUCT_MEMBER(blink::WebFrameOwnerProperties, frame_owner_properties)

  // Specifies properties for a new RenderWidget that will be attached to the
  // new RenderFrame (if one is needed).
  IPC_STRUCT_MEMBER(FrameMsg_NewFrame_WidgetParams, widget_params)
IPC_STRUCT_END()

// Parameters included with an OpenURL request.  |frame_unique_name| is only
// specified if |is_history_navigation_in_new_child| is true, for the case that
// the browser process should look for an existing history item for the frame.
IPC_STRUCT_BEGIN(FrameHostMsg_OpenURL_Params)
  IPC_STRUCT_MEMBER(GURL, url)
  IPC_STRUCT_MEMBER(bool, uses_post)
  IPC_STRUCT_MEMBER(scoped_refptr<content::ResourceRequestBodyImpl>,
                    resource_request_body)
  IPC_STRUCT_MEMBER(content::Referrer, referrer)
  IPC_STRUCT_MEMBER(WindowOpenDisposition, disposition)
  IPC_STRUCT_MEMBER(bool, should_replace_current_entry)
  IPC_STRUCT_MEMBER(bool, user_gesture)
  IPC_STRUCT_MEMBER(bool, is_history_navigation_in_new_child)
  IPC_STRUCT_MEMBER(std::string, frame_unique_name)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(FrameMsg_TextTrackSettings_Params)
  // Text tracks on/off state
  IPC_STRUCT_MEMBER(bool, text_tracks_enabled)

  // Background color of the text track.
  IPC_STRUCT_MEMBER(std::string, text_track_background_color)

  // Font family of the text track text.
  IPC_STRUCT_MEMBER(std::string, text_track_font_family)

  // Font style of the text track text.
  IPC_STRUCT_MEMBER(std::string, text_track_font_style)

  // Font variant of the text track text.
  IPC_STRUCT_MEMBER(std::string, text_track_font_variant)

  // Color of the text track text.
  IPC_STRUCT_MEMBER(std::string, text_track_text_color)

  // Text shadow (edge style) of the text track text.
  IPC_STRUCT_MEMBER(std::string, text_track_text_shadow)

  // Size of the text track text.
  IPC_STRUCT_MEMBER(std::string, text_track_text_size)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(content::SavableSubframe)
  IPC_STRUCT_TRAITS_MEMBER(original_url)
  IPC_STRUCT_TRAITS_MEMBER(routing_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(FrameMsg_SerializeAsMHTML_Params)
  // Job id - used to match responses to requests.
  IPC_STRUCT_MEMBER(int, job_id)

  // Destination file handle.
  IPC_STRUCT_MEMBER(IPC::PlatformFileForTransit, destination_file)

  // MHTML boundary marker / MIME multipart boundary maker.  The same
  // |mhtml_boundary_marker| should be used for serialization of each frame.
  IPC_STRUCT_MEMBER(std::string, mhtml_boundary_marker)

  // Whether to use binary encoding while serializing.  Binary encoding is not
  // supported outside of Chrome, so this should not be used if the MHTML is
  // intended for sharing.
  IPC_STRUCT_MEMBER(bool, mhtml_binary_encoding)

  IPC_STRUCT_MEMBER(blink::WebFrameSerializerCacheControlPolicy,
                    mhtml_cache_control_policy)

  // Frame to content-id map.
  // Keys are routing ids of either RenderFrames or RenderFrameProxies.
  // Values are MHTML content-ids - see WebFrameSerializer::generateMHTMLParts.
  IPC_STRUCT_MEMBER(FrameMsg_SerializeAsMHTML_FrameRoutingIdToContentIdMap,
                    frame_routing_id_to_content_id)

  // |digests_of_uris_to_skip| contains digests of uris of MHTML parts that
  // should be skipped.  This helps deduplicate mhtml parts across frames.
  // SECURITY NOTE: Sha256 digests (rather than uris) are used to prevent
  // disclosing uris to other renderer processes;  the digests should be
  // generated using SHA256HashString function from crypto/sha2.h and hashing
  // |salt + url.spec()|.
  IPC_STRUCT_MEMBER(std::set<std::string>, digests_of_uris_to_skip)

  // Salt used for |digests_of_uris_to_skip|.
  IPC_STRUCT_MEMBER(std::string, salt)

  // If |is_last_frame| is true, then an MHTML footer will be generated.
  IPC_STRUCT_MEMBER(bool, is_last_frame)
IPC_STRUCT_END()

// This message is used to send hittesting data from the renderer in order
// to perform hittesting on the browser process.
IPC_STRUCT_BEGIN(FrameHostMsg_HittestData_Params)
  // |surface_id| represents the surface used by this remote frame.
  IPC_STRUCT_MEMBER(cc::SurfaceId, surface_id)

  // If |ignored_for_hittest| then this surface should be ignored during
  // hittesting.
  IPC_STRUCT_MEMBER(bool, ignored_for_hittest)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(FrameHostMsg_CreateChildFrame_Params)
  IPC_STRUCT_MEMBER(int32_t, parent_routing_id)
  IPC_STRUCT_MEMBER(blink::WebTreeScopeType, scope)
  IPC_STRUCT_MEMBER(std::string, frame_name)
  IPC_STRUCT_MEMBER(std::string, frame_unique_name)
  IPC_STRUCT_MEMBER(blink::WebSandboxFlags, sandbox_flags)
  IPC_STRUCT_MEMBER(blink::WebFrameOwnerProperties, frame_owner_properties)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(content::ContentSecurityPolicyHeader)
  IPC_STRUCT_TRAITS_MEMBER(header_value)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(source)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::FileChooserFileInfo)
  IPC_STRUCT_TRAITS_MEMBER(file_path)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
  IPC_STRUCT_TRAITS_MEMBER(file_system_url)
  IPC_STRUCT_TRAITS_MEMBER(modification_time)
  IPC_STRUCT_TRAITS_MEMBER(length)
  IPC_STRUCT_TRAITS_MEMBER(is_directory)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::FileChooserParams)
  IPC_STRUCT_TRAITS_MEMBER(mode)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(default_file_name)
  IPC_STRUCT_TRAITS_MEMBER(accept_types)
  IPC_STRUCT_TRAITS_MEMBER(need_local_path)
#if defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(capture)
#endif
  IPC_STRUCT_TRAITS_MEMBER(requestor)
IPC_STRUCT_TRAITS_END()

#if defined(USE_EXTERNAL_POPUP_MENU)
// This message is used for supporting popup menus on Mac OS X and Android using
// native controls. See the FrameHostMsg_ShowPopup message.
IPC_STRUCT_BEGIN(FrameHostMsg_ShowPopup_Params)
  // Position on the screen.
  IPC_STRUCT_MEMBER(gfx::Rect, bounds)

  // The height of each item in the menu.
  IPC_STRUCT_MEMBER(int, item_height)

  // The size of the font to use for those items.
  IPC_STRUCT_MEMBER(double, item_font_size)

  // The currently selected (displayed) item in the menu.
  IPC_STRUCT_MEMBER(int, selected_item)

  // The entire list of items in the popup menu.
  IPC_STRUCT_MEMBER(std::vector<content::MenuItem>, popup_items)

  // Whether items should be right-aligned.
  IPC_STRUCT_MEMBER(bool, right_aligned)

  // Whether this is a multi-select popup.
  IPC_STRUCT_MEMBER(bool, allow_multiple_selection)
IPC_STRUCT_END()
#endif

#if defined(ENABLE_PLUGINS)
IPC_STRUCT_TRAITS_BEGIN(content::PepperRendererInstanceData)
  IPC_STRUCT_TRAITS_MEMBER(render_process_id)
  IPC_STRUCT_TRAITS_MEMBER(render_frame_id)
  IPC_STRUCT_TRAITS_MEMBER(document_url)
  IPC_STRUCT_TRAITS_MEMBER(plugin_url)
  IPC_STRUCT_TRAITS_MEMBER(is_potentially_secure_plugin_context)
IPC_STRUCT_TRAITS_END()
#endif

// -----------------------------------------------------------------------------
// Messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED4(FrameMsg_SetChildFrameSurface,
                    cc::SurfaceId /* surface_id */,
                    gfx::Size /* frame_size */,
                    float /* scale_factor */,
                    cc::SurfaceSequence /* sequence */)

// Notifies the embedding frame that the process rendering the child frame's
// contents has terminated.
IPC_MESSAGE_ROUTED0(FrameMsg_ChildFrameProcessGone)

// Sent in response to a FrameHostMsg_ContextMenu to let the renderer know that
// the menu has been closed.
IPC_MESSAGE_ROUTED1(FrameMsg_ContextMenuClosed,
                    content::CustomContextMenuContext /* custom_context */)

// Reloads all the Lo-Fi images in the RenderFrame. Ignores the cache and
// reloads from the network.
IPC_MESSAGE_ROUTED0(FrameMsg_ReloadLoFiImages)

// Executes custom context menu action that was provided from Blink.
IPC_MESSAGE_ROUTED2(FrameMsg_CustomContextMenuAction,
                    content::CustomContextMenuContext /* custom_context */,
                    unsigned /* action */)

// Requests that the RenderFrame or RenderFrameProxy updates its opener to the
// specified frame.  The routing ID may be MSG_ROUTING_NONE if the opener was
// disowned.
IPC_MESSAGE_ROUTED1(FrameMsg_UpdateOpener, int /* opener_routing_id */)

// Requests that the RenderFrame send back a response after waiting for the
// commit, activation and frame swap of the current DOM tree in blink.
IPC_MESSAGE_ROUTED1(FrameMsg_VisualStateRequest, uint64_t /* id */)

// Instructs the renderer to create a new RenderFrame object.
IPC_MESSAGE_CONTROL1(FrameMsg_NewFrame, FrameMsg_NewFrame_Params /* params */)

// Instructs the renderer to delete the RenderFrame.
IPC_MESSAGE_ROUTED0(FrameMsg_Delete)

// Instructs the renderer to create a new RenderFrameProxy object with
// |routing_id|.  |render_view_routing_id| identifies the
// RenderView to be associated with this proxy.  The new proxy's opener should
// be set to the object identified by |opener_routing_id|, or to null if that
// is MSG_ROUTING_NONE.  The new proxy should be created as a child of the
// object identified by |parent_routing_id| or as top level if that is
// MSG_ROUTING_NONE.
IPC_MESSAGE_CONTROL5(FrameMsg_NewFrameProxy,
                     int /* routing_id */,
                     int /* render_view_routing_id */,
                     int /* opener_routing_id */,
                     int /* parent_routing_id */,
                     content::FrameReplicationState /* replication_state */)

// Tells the renderer to perform the specified navigation, interrupting any
// existing navigation.
IPC_MESSAGE_ROUTED3(FrameMsg_Navigate,
                    content::CommonNavigationParams, /* common_params */
                    content::StartNavigationParams,  /* start_params */
                    content::RequestNavigationParams /* request_params */)

// Instructs the renderer to invoke the frame's beforeunload event handler.
// Expects the result to be returned via FrameHostMsg_BeforeUnload_ACK.
IPC_MESSAGE_ROUTED1(FrameMsg_BeforeUnload, bool /* is_reload */)

// Instructs the frame to swap out for a cross-site transition, including
// running the unload event handler and creating a RenderFrameProxy with the
// given |proxy_routing_id|. Expects a SwapOut_ACK message when finished.
IPC_MESSAGE_ROUTED3(FrameMsg_SwapOut,
                    int /* proxy_routing_id */,
                    bool /* is_loading */,
                    content::FrameReplicationState /* replication_state */)

// Instructs the frame to stop the load in progress, if any.
IPC_MESSAGE_ROUTED0(FrameMsg_Stop)

// A message sent to RenderFrameProxy to indicate that its corresponding
// RenderFrame has started loading a document.
IPC_MESSAGE_ROUTED0(FrameMsg_DidStartLoading)

// A message sent to RenderFrameProxy to indicate that its corresponding
// RenderFrame has completed loading.
IPC_MESSAGE_ROUTED0(FrameMsg_DidStopLoading)

// Request for the renderer to insert CSS into the frame.
IPC_MESSAGE_ROUTED1(FrameMsg_CSSInsertRequest,
                    std::string  /* css */)

// Add message to the frame console.
IPC_MESSAGE_ROUTED2(FrameMsg_AddMessageToConsole,
                    content::ConsoleMessageLevel /* level */,
                    std::string /* message */)

// Request for the renderer to execute JavaScript in the frame's context.
//
// javascript is the string containing the JavaScript to be executed in the
// target frame's context.
//
// If the third parameter is true the result is sent back to the browser using
// the message FrameHostMsg_JavaScriptExecuteResponse.
// FrameHostMsg_JavaScriptExecuteResponse is passed the ID parameter so that the
// host can uniquely identify the request.
IPC_MESSAGE_ROUTED3(FrameMsg_JavaScriptExecuteRequest,
                    base::string16,  /* javascript */
                    int,  /* ID */
                    bool  /* if true, a reply is requested */)

// ONLY FOR TESTS: Same as above but adds a fake UserGestureindicator around
// execution. (crbug.com/408426)
IPC_MESSAGE_ROUTED4(FrameMsg_JavaScriptExecuteRequestForTests,
                    base::string16,  /* javascript */
                    int,  /* ID */
                    bool, /* if true, a reply is requested */
                    bool  /* if true, a user gesture indicator is created */)

// Same as FrameMsg_JavaScriptExecuteRequest above except the script is
// run in the isolated world specified by the fourth parameter.
IPC_MESSAGE_ROUTED4(FrameMsg_JavaScriptExecuteRequestInIsolatedWorld,
                    base::string16, /* javascript */
                    int, /* ID */
                    bool, /* if true, a reply is requested */
                    int /* world_id */)

// Requests a navigation to the supplied markup, in an iframe with sandbox
// attributes.
IPC_MESSAGE_ROUTED1(FrameMsg_SetupTransitionView,
                    std::string /* markup */)

// Tells the renderer to hide the elements specified by the supplied CSS
// selector, and activates any exiting-transition stylesheets.
IPC_MESSAGE_ROUTED2(FrameMsg_BeginExitTransition,
                    std::string /* css_selector */,
                    bool /* exit_to_native_app */)

// Tell the renderer to revert the exit transition done before
IPC_MESSAGE_ROUTED0(FrameMsg_RevertExitTransition)

// Tell the renderer to hide transition elements.
IPC_MESSAGE_ROUTED1(FrameMsg_HideTransitionElements,
                    std::string /* css_selector */)

// Tell the renderer to hide transition elements.
IPC_MESSAGE_ROUTED1(FrameMsg_ShowTransitionElements,
                    std::string /* css_selector */)

// Tells the renderer to reload the frame, optionally bypassing the cache while
// doing so.
IPC_MESSAGE_ROUTED1(FrameMsg_Reload,
                    bool /* bypass_cache */)

// Notifies the color chooser client that the user selected a color.
IPC_MESSAGE_ROUTED2(FrameMsg_DidChooseColorResponse, unsigned, SkColor)

// Notifies the color chooser client that the color chooser has ended.
IPC_MESSAGE_ROUTED1(FrameMsg_DidEndColorChooser, unsigned)

// Notifies the corresponding RenderFrameProxy object to replace itself with the
// RenderFrame object it is associated with.
IPC_MESSAGE_ROUTED0(FrameMsg_DeleteProxy)

// Request the text surrounding the selection with a |max_length|. The response
// will be sent via FrameHostMsg_TextSurroundingSelectionResponse.
IPC_MESSAGE_ROUTED1(FrameMsg_TextSurroundingSelectionRequest,
                    uint32_t /* max_length */)

// Tells the renderer to insert a link to the specified stylesheet. This is
// needed to support navigation transitions.
IPC_MESSAGE_ROUTED1(FrameMsg_AddStyleSheetByURL, std::string)

// Change the accessibility mode in the renderer process.
IPC_MESSAGE_ROUTED1(FrameMsg_SetAccessibilityMode,
                    AccessibilityMode)

// Dispatch a load event in the iframe element containing this frame.
IPC_MESSAGE_ROUTED0(FrameMsg_DispatchLoad)

// Notifies the frame that its parent has changed the frame's sandbox flags.
IPC_MESSAGE_ROUTED1(FrameMsg_DidUpdateSandboxFlags, blink::WebSandboxFlags)

// Update a proxy's window.name property.  Used when the frame's name is
// changed in another process.
IPC_MESSAGE_ROUTED2(FrameMsg_DidUpdateName,
                    std::string /* name */,
                    std::string /* unique_name */)

// Updates replicated ContentSecurityPolicy in a frame proxy.
IPC_MESSAGE_ROUTED1(FrameMsg_AddContentSecurityPolicy,
                    content::ContentSecurityPolicyHeader)

// Resets ContentSecurityPolicy in a frame proxy / in RemoteSecurityContext.
IPC_MESSAGE_ROUTED0(FrameMsg_ResetContentSecurityPolicy)

// Update a proxy's replicated enforcement of insecure request policy.
// Used when the frame's policy is changed in another process.
IPC_MESSAGE_ROUTED1(FrameMsg_EnforceInsecureRequestPolicy,
                    blink::WebInsecureRequestPolicy)

// Update a proxy's replicated origin.  Used when the frame is navigated to a
// new origin.
IPC_MESSAGE_ROUTED2(FrameMsg_DidUpdateOrigin,
                    url::Origin /* origin */,
                    bool /* is potentially trustworthy unique origin */)

// Notifies this frame or proxy that it is now focused.  This is used to
// support cross-process focused frame changes.
IPC_MESSAGE_ROUTED0(FrameMsg_SetFocusedFrame)

// Sent to a frame proxy when its real frame is preparing to enter fullscreen
// in another process.  Actually entering fullscreen will be done separately as
// part of ViewMsg_Resize, once the browser process has resized the tab for
// fullscreen.
IPC_MESSAGE_ROUTED0(FrameMsg_WillEnterFullscreen)

// Send to the RenderFrame to set text tracks state and style settings.
// Sent for top-level frames.
IPC_MESSAGE_ROUTED1(FrameMsg_SetTextTrackSettings,
                    FrameMsg_TextTrackSettings_Params /* params */)

// Posts a message from a frame in another process to the current renderer.
IPC_MESSAGE_ROUTED1(FrameMsg_PostMessageEvent, FrameMsg_PostMessage_Params)

#if defined(OS_ANDROID)
// Request the distance to the nearest find result in a frame from the point at
// (x, y), defined in fractions of the content document's width and height. The
// distance will be returned via FrameHostMsg_GetNearestFindResult_Reply.  Note
// that |nfr_request_id| is a completely seperate ID from the |request_id| used
// in other find-related IPCs. It is specifically used to uniquely identify a
// nearest find result request, rather than a find request.
IPC_MESSAGE_ROUTED3(FrameMsg_GetNearestFindResult,
                    int /* nfr_request_id */,
                    float /* x */,
                    float /* y */)

// Activates a find result. The point (x,y) is in fractions of the content
// document's width and height.
IPC_MESSAGE_ROUTED3(FrameMsg_ActivateNearestFindResult,
                    int /* request_id */,
                    float /* x */,
                    float /* y */)

// Sent when the browser wants the bounding boxes of the current find matches.
//
// If match rects are already cached on the browser side, |current_version|
// should be the version number from the FrameHostMsg_FindMatchRects_Reply
// they came in, so the renderer can tell if it needs to send updated rects.
// Otherwise just pass -1 to always receive the list of rects.
//
// There must be an active search string (it is probably most useful to call
// this immediately after a FrameHostMsg_Find_Reply message arrives with
// final_update set to true).
IPC_MESSAGE_ROUTED1(FrameMsg_FindMatchRects, int /* current_version */)
#endif

#if defined(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)
IPC_MESSAGE_ROUTED1(FrameMsg_SelectPopupMenuItem,
                    int /* selected index, -1 means no selection */)
#else
IPC_MESSAGE_ROUTED2(FrameMsg_SelectPopupMenuItems,
                    bool /* user canceled the popup */,
                    std::vector<int> /* selected indices */)
#endif
#endif

// PlzNavigate
// Tells the renderer that a navigation is ready to commit.  The renderer should
// request |stream_url| to get access to the stream containing the body of the
// response.
IPC_MESSAGE_ROUTED4(FrameMsg_CommitNavigation,
                    content::ResourceResponseHead,    /* response */
                    GURL,                             /* stream_url */
                    content::CommonNavigationParams,  /* common_params */
                    content::RequestNavigationParams) /* request_params */

// PlzNavigate
// Tells the renderer that a navigation failed with the error code |error_code|
// and that the renderer should display an appropriate error page.
IPC_MESSAGE_ROUTED4(FrameMsg_FailedNavigation,
                    content::CommonNavigationParams,  /* common_params */
                    content::RequestNavigationParams, /* request_params */
                    bool,                             /* stale_copy_in_cache */
                    int                               /* error_code */)

// Request to enumerate and return links to all savable resources in the frame
// Note: this covers only the immediate frame / doesn't cover subframes.
IPC_MESSAGE_ROUTED0(FrameMsg_GetSavableResourceLinks)

// Get html data by serializing the target frame and replacing all resource
// links with a path to the local copy passed in the message payload.
IPC_MESSAGE_ROUTED2(FrameMsg_GetSerializedHtmlWithLocalLinks,
                    FrameMsg_GetSerializedHtmlWithLocalLinks_UrlMap,
                    FrameMsg_GetSerializedHtmlWithLocalLinks_FrameRoutingIdMap)

// Serialize target frame and its resources into MHTML and write it into the
// provided destination file handle.  Note that when serializing multiple
// frames, one needs to serialize the *main* frame first (the main frame
// needs to go first according to RFC2557 + the main frame will trigger
// generation of the MHTML header).
IPC_MESSAGE_ROUTED1(FrameMsg_SerializeAsMHTML, FrameMsg_SerializeAsMHTML_Params)

IPC_MESSAGE_ROUTED1(FrameMsg_SetFrameOwnerProperties,
                    blink::WebFrameOwnerProperties /* frame_owner_properties */)

// Request to continue running the sequential focus navigation algorithm in
// this frame.  |source_routing_id| identifies the frame that issued this
// request.  This message is sent when pressing <tab> or <shift-tab> needs to
// find the next focusable element in a cross-process frame.
IPC_MESSAGE_ROUTED2(FrameMsg_AdvanceFocus,
                    blink::WebFocusType /* type */,
                    int32_t /* source_routing_id */)

// Sent when the user wants to search for a word on the page (find-in-page).
IPC_MESSAGE_ROUTED3(FrameMsg_Find,
                    int /* request_id */,
                    base::string16 /* search_text */,
                    blink::WebFindOptions)

// This message notifies the frame that it is no longer the active frame in the
// current find session, and so it should clear its active find match (and no
// longer highlight it with special coloring).
IPC_MESSAGE_ROUTED0(FrameMsg_ClearActiveFindMatch)

// This message notifies the frame that the user has closed the find-in-page
// window (and what action to take regarding the selection).
IPC_MESSAGE_ROUTED1(FrameMsg_StopFinding, content::StopFindAction /* action */)

// Copies the image at location x, y to the clipboard (if there indeed is an
// image at that location).
IPC_MESSAGE_ROUTED2(FrameMsg_CopyImageAt,
                    int /* x */,
                    int /* y */)

// Saves the image at location x, y to the disk (if there indeed is an
// image at that location).
IPC_MESSAGE_ROUTED2(FrameMsg_SaveImageAt,
                    int /* x */,
                    int /* y */)

#if defined(ENABLE_PLUGINS)
// Notifies the renderer of updates to the Plugin Power Saver origin whitelist.
IPC_MESSAGE_ROUTED1(FrameMsg_UpdatePluginContentOriginWhitelist,
                    std::set<url::Origin> /* origin_whitelist */)

// This message notifies that the frame that the volume of the Pepper instance
// for |pp_instance| should be changed to |volume|.
IPC_MESSAGE_ROUTED2(FrameMsg_SetPepperVolume,
                    int32_t /* pp_instance */,
                    double /* volume */)
#endif  // defined(ENABLE_PLUGINS)

// Used to instruct the RenderFrame to go into "view source" mode. This should
// only be sent to the main frame.
IPC_MESSAGE_ROUTED0(FrameMsg_EnableViewSourceMode)

// Tells the frame to suppress any further modal dialogs. This ensures that no
// ScopedPageLoadDeferrer is on the stack for SwapOut.
IPC_MESSAGE_ROUTED0(FrameMsg_SuppressFurtherDialogs)

IPC_MESSAGE_ROUTED1(FrameMsg_RunFileChooserResponse,
                    std::vector<content::FileChooserFileInfo>)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Blink and JavaScript error messages to log to the console
// or debugger UI.
IPC_MESSAGE_ROUTED4(FrameHostMsg_AddMessageToConsole,
                    int32_t,        /* log level */
                    base::string16, /* msg */
                    int32_t,        /* line number */
                    base::string16 /* source id */)

// Sent by the renderer when a child frame is created in the renderer.
//
// Each of these messages will have a corresponding FrameHostMsg_Detach message
// sent when the frame is detached from the DOM.
IPC_SYNC_MESSAGE_CONTROL1_1(FrameHostMsg_CreateChildFrame,
                            FrameHostMsg_CreateChildFrame_Params,
                            int32_t /* new_routing_id */)

// Sent by the renderer to the parent RenderFrameHost when a child frame is
// detached from the DOM.
IPC_MESSAGE_ROUTED0(FrameHostMsg_Detach)

// Indicates the renderer process is gone.  This actually is sent by the
// browser process to itself, but keeps the interface cleaner.
IPC_MESSAGE_ROUTED2(FrameHostMsg_RenderProcessGone,
                    int, /* this really is base::TerminationStatus */
                    int /* exit_code */)

// Sent by the renderer when the frame becomes focused.
IPC_MESSAGE_ROUTED0(FrameHostMsg_FrameFocused)

// Sent when the renderer starts a provisional load for a frame.
IPC_MESSAGE_ROUTED2(FrameHostMsg_DidStartProvisionalLoad,
                    GURL /* url */,
                    base::TimeTicks /* navigation_start */)

// Sent when the renderer fails a provisional load with an error.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidFailProvisionalLoadWithError,
                    FrameHostMsg_DidFailProvisionalLoadWithError_Params)

// Notifies the browser that a frame in the view has changed. This message
// has a lot of parameters and is packed/unpacked by functions defined in
// render_messages.h.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidCommitProvisionalLoad,
                    FrameHostMsg_DidCommitProvisionalLoad_Params)

// Notifies the browser that a document has been loaded.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DidFinishDocumentLoad)

IPC_MESSAGE_ROUTED4(FrameHostMsg_DidFailLoadWithError,
                    GURL /* validated_url */,
                    int /* error_code */,
                    base::string16 /* error_description */,
                    bool /* was_ignored_by_handler */)

// Sent when the renderer starts loading the page. |to_different_document| will
// be true unless the load is a fragment navigation, or triggered by
// history.pushState/replaceState.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidStartLoading,
                    bool /* to_different_document */)

// Sent when the renderer is done loading a page.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DidStopLoading)

// Notifies the browser that this frame has new session history information.
IPC_MESSAGE_ROUTED1(FrameHostMsg_UpdateState, content::PageState /* state */)

// Sent when the frame changes its window.name.
IPC_MESSAGE_ROUTED2(FrameHostMsg_DidChangeName,
                    std::string /* name */,
                    std::string /* unique_name */)

// Notifies the browser process about a new Content Security Policy that needs
// to be applies to the frame.  This message is sent when a frame commits
// navigation to a new location (reporting accumulated policies from HTTP
// headers and/or policies that might have been inherited from the parent frame)
// or when a new policy has been discovered afterwards (i.e. found in a
// dynamically added or a static <meta> element).
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidAddContentSecurityPolicy,
                    content::ContentSecurityPolicyHeader)

// Sent when the frame starts enforcing an insecure request policy. Sending
// this information in DidCommitProvisionalLoad isn't sufficient; this
// message is needed because, for example, a document can dynamically insert
// a <meta> tag that causes strict mixed content checking to be enforced.
IPC_MESSAGE_ROUTED1(FrameHostMsg_EnforceInsecureRequestPolicy,
                    blink::WebInsecureRequestPolicy)

// Sent when the frame is set to a unique origin. TODO(estark): this IPC
// only exists to support dynamic sandboxing via a CSP delivered in a
// <meta> tag. This is not supposed to be allowed per the CSP spec and
// should be ripped out. https://crbug.com/594645
IPC_MESSAGE_ROUTED1(FrameHostMsg_UpdateToUniqueOrigin,
                    bool /* is potentially trustworthy unique origin */)

// Sent when the renderer changed the progress of a load.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidChangeLoadProgress,
                    double /* load_progress */)

// Requests that the given URL be opened in the specified manner.
IPC_MESSAGE_ROUTED1(FrameHostMsg_OpenURL, FrameHostMsg_OpenURL_Params)

// Notifies the browser that a frame finished loading.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidFinishLoad,
                    GURL /* validated_url */)

// Initiates a download based on user actions like 'ALT+click'.
IPC_MESSAGE_CONTROL5(FrameHostMsg_DownloadUrl,
                     int /* render_view_id */,
                     int /* render_frame_id */,
                     GURL /* url */,
                     content::Referrer /* referrer */,
                     base::string16 /* suggested_name */)

// Asks the browser to save a image (for <canvas> or <img>) from a data URL.
// Note: |data_url| is the contents of a data:URL, and that it's represented as
// a string only to work around size limitations for GURLs in IPC messages.
IPC_MESSAGE_CONTROL3(FrameHostMsg_SaveImageFromDataURL,
                     int /* render_view_id */,
                     int /* render_frame_id */,
                     std::string /* data_url */)

// Sent when after the onload handler has been invoked for the document
// in this frame. Sent for top-level frames. |report_type| and |ui_timestamp|
// are used to report navigation metrics starting on the ui input event that
// triggered the navigation timestamp.
IPC_MESSAGE_ROUTED2(FrameHostMsg_DocumentOnLoadCompleted,
                    FrameMsg_UILoadMetricsReportType::Value /* report_type */,
                    base::TimeTicks /* ui_timestamp */)

// Notifies that the initial empty document of a view has been accessed.
// After this, it is no longer safe to show a pending navigation's URL without
// making a URL spoof possible.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DidAccessInitialDocument)

// Sent when the RenderFrame or RenderFrameProxy either updates its opener to
// another frame identified by |opener_routing_id|, or, if |opener_routing_id|
// is MSG_ROUTING_NONE, the frame disowns its opener for the lifetime of the
// window.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidChangeOpener, int /* opener_routing_id */)

// Notifies the browser that a page id was assigned.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidAssignPageId, int32_t /* page_id */)

// Notifies the browser that sandbox flags have changed for a subframe of this
// frame.
IPC_MESSAGE_ROUTED2(FrameHostMsg_DidChangeSandboxFlags,
                    int32_t /* subframe_routing_id */,
                    blink::WebSandboxFlags /* updated_flags */)

// Notifies the browser that frame owner properties have changed for a subframe
// of this frame.
IPC_MESSAGE_ROUTED2(FrameHostMsg_DidChangeFrameOwnerProperties,
                    int32_t /* subframe_routing_id */,
                    blink::WebFrameOwnerProperties /* frame_owner_properties */)

// Changes the title for the page in the UI when the page is navigated or the
// title changes. Sent for top-level frames.
IPC_MESSAGE_ROUTED2(FrameHostMsg_UpdateTitle,
                    base::string16 /* title */,
                    blink::WebTextDirection /* title direction */)

// Change the encoding name of the page in UI when the page has detected
// proper encoding name. Sent for top-level frames.
IPC_MESSAGE_ROUTED1(FrameHostMsg_UpdateEncoding,
                    std::string /* new encoding name */)

// Following message is used to communicate the values received by the
// callback binding the JS to Cpp.
// An instance of browser that has an automation host listening to it can
// have a javascript send a native value (string, number, boolean) to the
// listener in Cpp. (DomAutomationController)
IPC_MESSAGE_ROUTED1(FrameHostMsg_DomOperationResponse,
                    std::string  /* json_string */)

// Used to set a cookie. The cookie is set asynchronously, but will be
// available to a subsequent FrameHostMsg_GetCookies request.
IPC_MESSAGE_CONTROL4(FrameHostMsg_SetCookie,
                     int /* render_frame_id */,
                     GURL /* url */,
                     GURL /* first_party_for_cookies */,
                     std::string /* cookie */)

// Used to get cookies for the given URL. This may block waiting for a
// previous SetCookie message to be processed.
IPC_SYNC_MESSAGE_CONTROL3_1(FrameHostMsg_GetCookies,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* first_party_for_cookies */,
                            std::string /* cookies */)

// Used to check if cookies are enabled for the given URL. This may block
// waiting for a previous SetCookie message to be processed.
IPC_SYNC_MESSAGE_CONTROL3_1(FrameHostMsg_CookiesEnabled,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* first_party_for_cookies */,
                            bool /* cookies_enabled */)

// Sent by the renderer process to check whether client 3D APIs
// (Pepper 3D, WebGL) are explicitly blocked.
IPC_SYNC_MESSAGE_CONTROL3_1(FrameHostMsg_Are3DAPIsBlocked,
                            int /* render_frame_id */,
                            GURL /* top_origin_url */,
                            content::ThreeDAPIType /* requester */,
                            bool /* blocked */)

#if defined(ENABLE_PLUGINS)
// Notification sent from a renderer to the browser that a Pepper plugin
// instance is created in the DOM.
IPC_MESSAGE_ROUTED0(FrameHostMsg_PepperInstanceCreated)

// Notification sent from a renderer to the browser that a Pepper plugin
// instance is deleted from the DOM.
IPC_MESSAGE_ROUTED0(FrameHostMsg_PepperInstanceDeleted)

// Sent to the browser when the renderer detects it is blocked on a pepper
// plugin message for too long. This is also sent when it becomes unhung
// (according to the value of is_hung). The browser can give the user the
// option of killing the plugin.
IPC_MESSAGE_ROUTED3(FrameHostMsg_PepperPluginHung,
                    int /* plugin_child_id */,
                    base::FilePath /* path */,
                    bool /* is_hung */)

// Sent by the renderer process to indicate that a plugin instance has crashed.
// Note: |plugin_pid| should not be trusted. The corresponding process has
// probably died. Moreover, the ID may have been reused by a new process. Any
// usage other than displaying it in a prompt to the user is very likely to be
// wrong.
IPC_MESSAGE_ROUTED2(FrameHostMsg_PluginCrashed,
                    base::FilePath /* plugin_path */,
                    base::ProcessId /* plugin_pid */)

// Used to get the list of plugins
IPC_SYNC_MESSAGE_CONTROL1_1(FrameHostMsg_GetPlugins,
    bool /* refresh*/,
    std::vector<content::WebPluginInfo> /* plugins */)

// Return information about a plugin for the given URL and MIME
// type. If there is no matching plugin, |found| is false.
// |actual_mime_type| is the actual mime type supported by the
// found plugin.
IPC_SYNC_MESSAGE_CONTROL4_3(FrameHostMsg_GetPluginInfo,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* page_url */,
                            std::string /* mime_type */,
                            bool /* found */,
                            content::WebPluginInfo /* plugin info */,
                            std::string /* actual_mime_type */)

// A renderer sends this to the browser process when it wants to temporarily
// whitelist an origin's plugin content as essential. This temporary whitelist
// is specific to a top level frame, and is cleared when the whitelisting
// RenderFrame is destroyed.
IPC_MESSAGE_ROUTED1(FrameHostMsg_PluginContentOriginAllowed,
                    url::Origin /* content_origin */)

// A renderer sends this to the browser process when it wants to create a ppapi
// plugin.  The browser will create the plugin process if necessary, and will
// return a handle to the channel on success.
//
// The plugin_child_id is the ChildProcessHost ID assigned in the browser
// process. This ID is valid only in the context of the browser process and is
// used to identify the proper process when the renderer notifies it that the
// plugin is hung.
//
// On error an empty string and null handles are returned.
IPC_SYNC_MESSAGE_CONTROL1_3(FrameHostMsg_OpenChannelToPepperPlugin,
                            base::FilePath /* path */,
                            IPC::ChannelHandle /* handle to channel */,
                            base::ProcessId /* plugin_pid */,
                            int /* plugin_child_id */)

// Message from the renderer to the browser indicating the in-process instance
// has been created.
IPC_MESSAGE_CONTROL2(FrameHostMsg_DidCreateInProcessInstance,
                     int32_t /* instance */,
                     content::PepperRendererInstanceData /* instance_data */)

// Message from the renderer to the browser indicating the in-process instance
// has been destroyed.
IPC_MESSAGE_CONTROL1(FrameHostMsg_DidDeleteInProcessInstance,
                     int32_t /* instance */)

// Notification that a plugin has created a new plugin instance. The parameters
// indicate:
//  - The plugin process ID that we're creating the instance for.
//  - The instance ID of the instance being created.
//  - A PepperRendererInstanceData struct which contains properties from the
//    renderer which are associated with the plugin instance. This includes the
//    routing ID of the associated RenderFrame and the URL of plugin.
//  - Whether the plugin we're creating an instance for is external or internal.
//
// This message must be sync even though it returns no parameters to avoid
// a race condition with the plugin process. The plugin process sends messages
// to the browser that assume the browser knows about the instance. We need to
// make sure that the browser actually knows about the instance before we tell
// the plugin to run.
IPC_SYNC_MESSAGE_CONTROL4_0(
    FrameHostMsg_DidCreateOutOfProcessPepperInstance,
    int /* plugin_child_id */,
    int32_t /* pp_instance */,
    content::PepperRendererInstanceData /* creation_data */,
    bool /* is_external */)

// Notification that a plugin has destroyed an instance. This is the opposite of
// the "DidCreate" message above.
IPC_MESSAGE_CONTROL3(FrameHostMsg_DidDeleteOutOfProcessPepperInstance,
                     int /* plugin_child_id */,
                     int32_t /* pp_instance */,
                     bool /* is_external */)

// A renderer sends this to the browser process when it wants to
// create a ppapi broker.  The browser will create the broker process
// if necessary, and will return a handle to the channel on success.
// On error an empty string is returned.
// The browser will respond with ViewMsg_PpapiBrokerChannelCreated.
IPC_MESSAGE_CONTROL2(FrameHostMsg_OpenChannelToPpapiBroker,
                     int /* routing_id */,
                     base::FilePath /* path */)

// A renderer sends this to the browser process when it throttles or unthrottles
// a plugin instance for the Plugin Power Saver feature.
IPC_MESSAGE_CONTROL3(FrameHostMsg_PluginInstanceThrottleStateChange,
                     int /* plugin_child_id */,
                     int32_t /* pp_instance */,
                     bool /* is_throttled */)
#endif  // defined(ENABLE_PLUGINS)

// Satisfies a Surface destruction dependency associated with |sequence|.
IPC_MESSAGE_ROUTED1(FrameHostMsg_SatisfySequence,
                    cc::SurfaceSequence /* sequence */)

// Creates a destruction dependency for the Surface specified by the given
// |surface_id|.
IPC_MESSAGE_ROUTED2(FrameHostMsg_RequireSequence,
                    cc::SurfaceId /* surface_id */,
                    cc::SurfaceSequence /* sequence */)

// Provides the result from handling BeforeUnload.  |proceed| matches the return
// value of the frame's beforeunload handler: true if the user decided to
// proceed with leaving the page.
IPC_MESSAGE_ROUTED3(FrameHostMsg_BeforeUnload_ACK,
                    bool /* proceed */,
                    base::TimeTicks /* before_unload_start_time */,
                    base::TimeTicks /* before_unload_end_time */)

// Indicates that the current frame has swapped out, after a SwapOut message.
IPC_MESSAGE_ROUTED0(FrameHostMsg_SwapOut_ACK)

// Forwards an input event to a child.
// TODO(nick): Temporary bridge, revisit once the browser process can route
// input directly to subframes. http://crbug.com/339659
IPC_MESSAGE_ROUTED1(FrameHostMsg_ForwardInputEvent,
                    IPC::WebInputEventPointer /* event */)

// Tells the parent that a child's frame rect has changed (or the rect/scroll
// position of a child's ancestor has changed).
IPC_MESSAGE_ROUTED1(FrameHostMsg_FrameRectChanged, gfx::Rect /* frame_rect */)

// Informs the child that the frame has changed visibility.
IPC_MESSAGE_ROUTED1(FrameHostMsg_VisibilityChanged, bool /* visible */)

// Used to tell the parent that the user right clicked on an area of the
// content area, and a context menu should be shown for it. The params
// object contains information about the node(s) that were selected when the
// user right clicked.
IPC_MESSAGE_ROUTED1(FrameHostMsg_ContextMenu, content::ContextMenuParams)

// Initial drawing parameters for a child frame that has been swapped out to
// another process.
IPC_MESSAGE_ROUTED1(FrameHostMsg_InitializeChildFrame,
                    float /* scale_factor */)

// Response for FrameMsg_JavaScriptExecuteRequest, sent when a reply was
// requested. The ID is the parameter supplied to
// FrameMsg_JavaScriptExecuteRequest. The result has the value returned by the
// script as its only element, one of Null, Boolean, Integer, Real, Date, or
// String.
IPC_MESSAGE_ROUTED2(FrameHostMsg_JavaScriptExecuteResponse,
                    int  /* id */,
                    base::ListValue  /* result */)

// A request to run a JavaScript dialog.
IPC_SYNC_MESSAGE_ROUTED4_2(FrameHostMsg_RunJavaScriptMessage,
                           base::string16     /* in - alert message */,
                           base::string16     /* in - default prompt */,
                           GURL               /* in - originating page URL */,
                           content::JavaScriptMessageType /* in - type */,
                           bool               /* out - success */,
                           base::string16     /* out - user_input field */)

// Displays a dialog to confirm that the user wants to navigate away from the
// page. Replies true if yes, and false otherwise. The reply string is ignored,
// but is included so that we can use OnJavaScriptMessageBoxClosed.
IPC_SYNC_MESSAGE_ROUTED2_2(FrameHostMsg_RunBeforeUnloadConfirm,
                           GURL,           /* in - originating frame URL */
                           bool            /* in - is a reload */,
                           bool            /* out - success */,
                           base::string16  /* out - This is ignored.*/)

// Asks the browser to open the color chooser.
IPC_MESSAGE_ROUTED3(FrameHostMsg_OpenColorChooser,
                    int /* id */,
                    SkColor /* color */,
                    std::vector<content::ColorSuggestion> /* suggestions */)

// Asks the browser to end the color chooser.
IPC_MESSAGE_ROUTED1(FrameHostMsg_EndColorChooser, int /* id */)

// Change the selected color in the color chooser.
IPC_MESSAGE_ROUTED2(FrameHostMsg_SetSelectedColorInColorChooser,
                    int /* id */,
                    SkColor /* color */)

// Notify browser the theme color has been changed.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidChangeThemeColor,
                    SkColor /* theme_color */)

// Response for FrameMsg_TextSurroundingSelectionRequest, |startOffset| and
// |endOffset| are the offsets of the selection in the returned |content|.
IPC_MESSAGE_ROUTED3(FrameHostMsg_TextSurroundingSelectionResponse,
                    base::string16,  /* content */
                    uint32_t, /* startOffset */
                    uint32_t/* endOffset */)

// Register a new handler for URL requests with the given scheme.
IPC_MESSAGE_ROUTED4(FrameHostMsg_RegisterProtocolHandler,
                    std::string /* scheme */,
                    GURL /* url */,
                    base::string16 /* title */,
                    bool /* user_gesture */)

// Unregister the registered handler for URL requests with the given scheme.
IPC_MESSAGE_ROUTED3(FrameHostMsg_UnregisterProtocolHandler,
                    std::string /* scheme */,
                    GURL /* url */,
                    bool /* user_gesture */)

// Sent when the renderer loads a resource from its memory cache.
// The security info is non empty if the resource was originally loaded over
// a secure connection.
// Note: May only be sent once per URL per frame per committed load.
IPC_MESSAGE_ROUTED5(FrameHostMsg_DidLoadResourceFromMemoryCache,
                    GURL /* url */,
                    std::string /* security info */,
                    std::string /* http method */,
                    std::string /* mime type */,
                    content::ResourceType /* resource type */)

// PlzNavigate
// Tells the browser to perform a navigation.
IPC_MESSAGE_ROUTED2(FrameHostMsg_BeginNavigation,
                    content::CommonNavigationParams,
                    content::BeginNavigationParams)

// Sent as a response to FrameMsg_VisualStateRequest.
// The message is delivered using RenderWidget::QueueMessage.
IPC_MESSAGE_ROUTED1(FrameHostMsg_VisualStateResponse, uint64_t /* id */)

// Puts the browser into "tab fullscreen" mode for the sending renderer.
// See the comment in chrome/browser/ui/browser.h for more details.
IPC_MESSAGE_ROUTED1(FrameHostMsg_ToggleFullscreen, bool /* enter_fullscreen */)

// Dispatch a load event for this frame in the iframe element of an
// out-of-process parent frame.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DispatchLoad)

// Sent to the browser from a frame proxy to post a message to the frame's
// active renderer.
IPC_MESSAGE_ROUTED1(FrameHostMsg_RouteMessageEvent,
                    FrameMsg_PostMessage_Params)

// Sent when the renderer displays insecure content in a secure origin.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DidDisplayInsecureContent)

// Sent when the renderer runs insecure content in a secure origin.
IPC_MESSAGE_ROUTED2(FrameHostMsg_DidRunInsecureContent,
                    GURL /* security_origin */,
                    GURL /* target URL */)

// Sent when the renderer displays content that was loaded with
// certificate errors.
IPC_MESSAGE_ROUTED2(FrameHostMsg_DidDisplayContentWithCertificateErrors,
                    GURL /* resource url */,
                    std::string /* serialized security info */)

// Sent when the renderer runs content that was loaded with certificate
// errors.
IPC_MESSAGE_ROUTED2(FrameHostMsg_DidRunContentWithCertificateErrors,
                    GURL /* resource url */,
                    std::string /* serialized security info */)

// Response to FrameMsg_GetSavableResourceLinks.
IPC_MESSAGE_ROUTED3(FrameHostMsg_SavableResourceLinksResponse,
                    std::vector<GURL> /* savable resource links */,
                    content::Referrer /* referrer for all the links above */,
                    std::vector<content::SavableSubframe> /* subframes */)

// Response to FrameMsg_GetSavableResourceLinks in case the frame contains
// non-savable content (i.e. from a non-savable scheme) or if there were
// errors gathering the links.
IPC_MESSAGE_ROUTED0(FrameHostMsg_SavableResourceLinksError)

// Response to FrameMsg_GetSerializedHtmlWithLocalLinks.
IPC_MESSAGE_ROUTED2(FrameHostMsg_SerializedHtmlWithLocalLinksResponse,
                    std::string /* data buffer */,
                    bool /* end of data? */)

// Response to FrameMsg_SerializeAsMHTML.
IPC_MESSAGE_ROUTED3(
    FrameHostMsg_SerializeAsMHTMLResponse,
    int /* job_id (used to match responses to requests) */,
    bool /* true if success, false if error */,
    std::set<std::string> /* digests of uris of serialized resources */)

// Sent when the renderer updates hint for importance of a tab.
IPC_MESSAGE_ROUTED1(FrameHostMsg_UpdatePageImportanceSignals,
                    content::PageImportanceSignals)

// This message is sent from a RenderFrameProxy when sequential focus
// navigation needs to advance into its actual frame.  |source_routing_id|
// identifies the frame that issued this request.  This is used when pressing
// <tab> or <shift-tab> hits an out-of-process iframe when searching for the
// next focusable element.
IPC_MESSAGE_ROUTED2(FrameHostMsg_AdvanceFocus,
                    blink::WebFocusType /* type */,
                    int32_t /* source_routing_id */)

// Result of string search in the document.
// Response to FrameMsg_Find with the results of the requested find-in-page
// search, the number of matches found and the selection rect (in screen
// coordinates) for the string found. If |final_update| is false, it signals
// that this is not the last Find_Reply message - more will be sent as the
// scoping effort continues.
IPC_MESSAGE_ROUTED5(FrameHostMsg_Find_Reply,
                    int /* request_id */,
                    int /* number of matches */,
                    gfx::Rect /* selection_rect */,
                    int /* active_match_ordinal */,
                    bool /* final_update */)

// Sends hittesting data needed to perform hittesting on the browser process.
IPC_MESSAGE_ROUTED1(FrameHostMsg_HittestData, FrameHostMsg_HittestData_Params)

// Asks the browser to display the file chooser.  The result is returned in a
// FrameMsg_RunFileChooserResponse message.
IPC_MESSAGE_ROUTED1(FrameHostMsg_RunFileChooser, content::FileChooserParams)

#if defined(USE_EXTERNAL_POPUP_MENU)

// Message to show/hide a popup menu using native controls.
IPC_MESSAGE_ROUTED1(FrameHostMsg_ShowPopup,
                    FrameHostMsg_ShowPopup_Params)
IPC_MESSAGE_ROUTED0(FrameHostMsg_HidePopup)

#endif

#if defined(OS_ANDROID)
// Response to FrameMsg_FindMatchRects.
//
// |version| will contain the current version number of the renderer's find
// match list (incremented whenever they change), which should be passed in the
// next call to FrameMsg_FindMatchRects.
//
// |rects| will either contain a list of the enclosing rects of all matches
// found by the most recent Find operation, or will be empty if |version| is not
// greater than the |current_version| passed to FrameMsg_FindMatchRects (hence
// your locally cached rects should still be valid). The rect coords will be
// custom normalized fractions of the document size. The rects will be sorted by
// frame traversal order starting in the main frame, then by dom order.
//
// |active_rect| will contain the bounding box of the active find-in-page match
// marker, in similarly normalized coords (or an empty rect if there isn't one).
IPC_MESSAGE_ROUTED3(FrameHostMsg_FindMatchRects_Reply,
                    int /* version */,
                    std::vector<gfx::RectF> /* rects */,
                    gfx::RectF /* active_rect */)

// Response to FrameMsg_GetNearestFindResult. |distance| is the distance to the
// nearest find result in the sending frame.
IPC_MESSAGE_ROUTED2(FrameHostMsg_GetNearestFindResult_Reply,
                    int /* nfr_request_id */,
                    float /* distance */)
#endif

// Adding a new message? Stick to the sort order above: first platform
// independent FrameMsg, then ifdefs for platform specific FrameMsg, then
// platform independent FrameHostMsg, then ifdefs for platform specific
// FrameHostMsg.
