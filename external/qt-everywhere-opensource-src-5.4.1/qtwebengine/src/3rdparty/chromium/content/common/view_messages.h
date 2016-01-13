// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for page rendering.
// Multiply-included message file, hence no include guard.

#include "base/memory/shared_memory.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/cookie_data.h"
#include "content/common/navigation_gesture.h"
#include "content/common/pepper_renderer_instance_data.h"
#include "content/common/view_message_enums.h"
#include "content/common/webplugin_geometry.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/favicon_url.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/menu_item.h"
#include "content/public/common/page_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/referrer.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/window_container_type.h"
#include "content/common/date_time_suggestion.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "media/audio/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_log_event.h"
#include "net/base/network_change_notifier.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "third_party/WebKit/public/web/WebMediaPlayerAction.h"
#include "third_party/WebKit/public/web/WebPluginAction.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/vector2d.h"
#include "ui/gfx/vector2d_f.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if defined(OS_MACOSX)
#include "content/common/mac/font_descriptor.h"
#include "third_party/WebKit/public/web/mac/WebScrollbarTheme.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ViewMsgStart

IPC_ENUM_TRAITS(AccessibilityMode)
IPC_ENUM_TRAITS(blink::WebMediaPlayerAction::Type)
IPC_ENUM_TRAITS(blink::WebPluginAction::Type)
IPC_ENUM_TRAITS(blink::WebPopupType)
IPC_ENUM_TRAITS(blink::WebTextDirection)
IPC_ENUM_TRAITS(WindowContainerType)
IPC_ENUM_TRAITS(content::FaviconURL::IconType)
IPC_ENUM_TRAITS(content::FileChooserParams::Mode)
IPC_ENUM_TRAITS(content::MenuItem::Type)
IPC_ENUM_TRAITS(content::NavigationGesture)
IPC_ENUM_TRAITS(content::PageZoom)
IPC_ENUM_TRAITS(content::RendererPreferencesHintingEnum)
IPC_ENUM_TRAITS(content::RendererPreferencesSubpixelRenderingEnum)
IPC_ENUM_TRAITS_MAX_VALUE(content::TapMultipleTargetsStrategy,
                          content::TAP_MULTIPLE_TARGETS_STRATEGY_MAX)
IPC_ENUM_TRAITS(content::StopFindAction)
IPC_ENUM_TRAITS(content::ThreeDAPIType)
IPC_ENUM_TRAITS(media::ChannelLayout)
IPC_ENUM_TRAITS(media::MediaLogEvent::Type)
IPC_ENUM_TRAITS_MAX_VALUE(ui::TextInputMode, ui::TEXT_INPUT_MODE_MAX)
IPC_ENUM_TRAITS(ui::TextInputType)

#if defined(OS_MACOSX)
IPC_STRUCT_TRAITS_BEGIN(FontDescriptor)
  IPC_STRUCT_TRAITS_MEMBER(font_name)
  IPC_STRUCT_TRAITS_MEMBER(font_point_size)
IPC_STRUCT_TRAITS_END()
#endif

IPC_STRUCT_TRAITS_BEGIN(blink::WebFindOptions)
  IPC_STRUCT_TRAITS_MEMBER(forward)
  IPC_STRUCT_TRAITS_MEMBER(matchCase)
  IPC_STRUCT_TRAITS_MEMBER(findNext)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebMediaPlayerAction)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(enable)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebPluginAction)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(enable)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebFloatPoint)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebFloatRect)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebScreenInfo)
  IPC_STRUCT_TRAITS_MEMBER(deviceScaleFactor)
  IPC_STRUCT_TRAITS_MEMBER(depth)
  IPC_STRUCT_TRAITS_MEMBER(depthPerComponent)
  IPC_STRUCT_TRAITS_MEMBER(isMonochrome)
  IPC_STRUCT_TRAITS_MEMBER(rect)
  IPC_STRUCT_TRAITS_MEMBER(availableRect)
  IPC_STRUCT_TRAITS_MEMBER(orientationType)
  IPC_STRUCT_TRAITS_MEMBER(orientationAngle)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::MenuItem)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(tool_tip)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(rtl)
  IPC_STRUCT_TRAITS_MEMBER(has_directional_override)
  IPC_STRUCT_TRAITS_MEMBER(enabled)
  IPC_STRUCT_TRAITS_MEMBER(checked)
  IPC_STRUCT_TRAITS_MEMBER(submenu)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::DateTimeSuggestion)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(localized_value)
  IPC_STRUCT_TRAITS_MEMBER(label)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::FaviconURL)
  IPC_STRUCT_TRAITS_MEMBER(icon_url)
  IPC_STRUCT_TRAITS_MEMBER(icon_type)
  IPC_STRUCT_TRAITS_MEMBER(icon_sizes)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::FileChooserParams)
  IPC_STRUCT_TRAITS_MEMBER(mode)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(default_file_name)
  IPC_STRUCT_TRAITS_MEMBER(accept_types)
#if defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(capture)
#endif
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::PepperRendererInstanceData)
  IPC_STRUCT_TRAITS_MEMBER(render_process_id)
  IPC_STRUCT_TRAITS_MEMBER(render_frame_id)
  IPC_STRUCT_TRAITS_MEMBER(document_url)
  IPC_STRUCT_TRAITS_MEMBER(plugin_url)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::RendererPreferences)
  IPC_STRUCT_TRAITS_MEMBER(can_accept_load_drops)
  IPC_STRUCT_TRAITS_MEMBER(should_antialias_text)
  IPC_STRUCT_TRAITS_MEMBER(hinting)
  IPC_STRUCT_TRAITS_MEMBER(use_autohinter)
  IPC_STRUCT_TRAITS_MEMBER(use_bitmaps)
  IPC_STRUCT_TRAITS_MEMBER(subpixel_rendering)
  IPC_STRUCT_TRAITS_MEMBER(use_subpixel_positioning)
  IPC_STRUCT_TRAITS_MEMBER(focus_ring_color)
  IPC_STRUCT_TRAITS_MEMBER(thumb_active_color)
  IPC_STRUCT_TRAITS_MEMBER(thumb_inactive_color)
  IPC_STRUCT_TRAITS_MEMBER(track_color)
  IPC_STRUCT_TRAITS_MEMBER(active_selection_bg_color)
  IPC_STRUCT_TRAITS_MEMBER(active_selection_fg_color)
  IPC_STRUCT_TRAITS_MEMBER(inactive_selection_bg_color)
  IPC_STRUCT_TRAITS_MEMBER(inactive_selection_fg_color)
  IPC_STRUCT_TRAITS_MEMBER(browser_handles_non_local_top_level_requests)
  IPC_STRUCT_TRAITS_MEMBER(browser_handles_all_top_level_requests)
  IPC_STRUCT_TRAITS_MEMBER(caret_blink_interval)
  IPC_STRUCT_TRAITS_MEMBER(use_custom_colors)
  IPC_STRUCT_TRAITS_MEMBER(enable_referrers)
  IPC_STRUCT_TRAITS_MEMBER(enable_do_not_track)
  IPC_STRUCT_TRAITS_MEMBER(default_zoom_level)
  IPC_STRUCT_TRAITS_MEMBER(user_agent_override)
  IPC_STRUCT_TRAITS_MEMBER(accept_languages)
  IPC_STRUCT_TRAITS_MEMBER(report_frame_name_changes)
  IPC_STRUCT_TRAITS_MEMBER(touchpad_fling_profile)
  IPC_STRUCT_TRAITS_MEMBER(touchscreen_fling_profile)
  IPC_STRUCT_TRAITS_MEMBER(tap_multiple_targets_strategy)
  IPC_STRUCT_TRAITS_MEMBER(disable_client_blocked_error_page)
  IPC_STRUCT_TRAITS_MEMBER(plugin_fullscreen_allowed)
  IPC_STRUCT_TRAITS_MEMBER(use_video_overlay_for_embedded_encrypted_video)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::CookieData)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(domain)
  IPC_STRUCT_TRAITS_MEMBER(path)
  IPC_STRUCT_TRAITS_MEMBER(expires)
  IPC_STRUCT_TRAITS_MEMBER(http_only)
  IPC_STRUCT_TRAITS_MEMBER(secure)
  IPC_STRUCT_TRAITS_MEMBER(session)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::WebPluginGeometry)
  IPC_STRUCT_TRAITS_MEMBER(window)
  IPC_STRUCT_TRAITS_MEMBER(window_rect)
  IPC_STRUCT_TRAITS_MEMBER(clip_rect)
  IPC_STRUCT_TRAITS_MEMBER(cutout_rects)
  IPC_STRUCT_TRAITS_MEMBER(rects_valid)
  IPC_STRUCT_TRAITS_MEMBER(visible)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::MediaLogEvent)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(params)
  IPC_STRUCT_TRAITS_MEMBER(time)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::SelectedFileInfo)
  IPC_STRUCT_TRAITS_MEMBER(file_path)
  IPC_STRUCT_TRAITS_MEMBER(local_path)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(ViewHostMsg_CreateWindow_Params)
  // Routing ID of the view initiating the open.
  IPC_STRUCT_MEMBER(int, opener_id)

  // True if this open request came in the context of a user gesture.
  IPC_STRUCT_MEMBER(bool, user_gesture)

  // Type of window requested.
  IPC_STRUCT_MEMBER(WindowContainerType, window_container_type)

  // The session storage namespace ID this view should use.
  IPC_STRUCT_MEMBER(int64, session_storage_namespace_id)

  // The name of the resulting frame that should be created (empty if none
  // has been specified).
  IPC_STRUCT_MEMBER(base::string16, frame_name)

  // The routing id of the frame initiating the open.
  IPC_STRUCT_MEMBER(int, opener_render_frame_id)

  // The URL of the frame initiating the open.
  IPC_STRUCT_MEMBER(GURL, opener_url)

  // The URL of the top frame containing the opener.
  IPC_STRUCT_MEMBER(GURL, opener_top_level_frame_url)

  // The security origin of the frame initiating the open.
  IPC_STRUCT_MEMBER(GURL, opener_security_origin)

  // Whether the opener will be suppressed in the new window, in which case
  // scripting the new window is not allowed.
  IPC_STRUCT_MEMBER(bool, opener_suppressed)

  // Whether the window should be opened in the foreground, background, etc.
  IPC_STRUCT_MEMBER(WindowOpenDisposition, disposition)

  // The URL that will be loaded in the new window (empty if none has been
  // sepcified).
  IPC_STRUCT_MEMBER(GURL, target_url)

  // The referrer that will be used to load |target_url| (empty if none has
  // been specified).
  IPC_STRUCT_MEMBER(content::Referrer, referrer)

  // The window features to use for the new view.
  IPC_STRUCT_MEMBER(blink::WebWindowFeatures, features)

  // The additional window features to use for the new view. We pass these
  // separately from |features| above because we cannot serialize WebStrings
  // over IPC.
  IPC_STRUCT_MEMBER(std::vector<base::string16>, additional_features)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_CreateWorker_Params)
  // URL for the worker script.
  IPC_STRUCT_MEMBER(GURL, url)

  // Name for a SharedWorker, otherwise empty string.
  IPC_STRUCT_MEMBER(base::string16, name)

  // Security policy used in the worker.
  IPC_STRUCT_MEMBER(base::string16, content_security_policy)

  // Security policy type used in the worker.
  IPC_STRUCT_MEMBER(blink::WebContentSecurityPolicyType, security_policy_type)

  // The ID of the parent document (unique within parent renderer).
  IPC_STRUCT_MEMBER(unsigned long long, document_id)

  // RenderFrame routing id used to send messages back to the parent.
  IPC_STRUCT_MEMBER(int, render_frame_route_id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_DateTimeDialogValue_Params)
  IPC_STRUCT_MEMBER(ui::TextInputType, dialog_type)
  IPC_STRUCT_MEMBER(double, dialog_value)
  IPC_STRUCT_MEMBER(double, minimum)
  IPC_STRUCT_MEMBER(double, maximum)
  IPC_STRUCT_MEMBER(double, step)
  IPC_STRUCT_MEMBER(std::vector<content::DateTimeSuggestion>, suggestions)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_SelectionBounds_Params)
  IPC_STRUCT_MEMBER(gfx::Rect, anchor_rect)
  IPC_STRUCT_MEMBER(blink::WebTextDirection, anchor_dir)
  IPC_STRUCT_MEMBER(gfx::Rect, focus_rect)
  IPC_STRUCT_MEMBER(blink::WebTextDirection, focus_dir)
  IPC_STRUCT_MEMBER(bool, is_anchor_first)
IPC_STRUCT_END()

// This message is used for supporting popup menus on Mac OS X using native
// Cocoa controls. The renderer sends us this message which we use to populate
// the popup menu.
IPC_STRUCT_BEGIN(ViewHostMsg_ShowPopup_Params)
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

IPC_STRUCT_BEGIN(ViewHostMsg_TextInputState_Params)
  // The type of input field
  IPC_STRUCT_MEMBER(ui::TextInputType, type)

  // The mode of input field
  IPC_STRUCT_MEMBER(ui::TextInputMode, mode)

  // The value of the input field
  IPC_STRUCT_MEMBER(std::string, value)

  // The cursor position of the current selection start, or the caret position
  // if nothing is selected
  IPC_STRUCT_MEMBER(int, selection_start)

  // The cursor position of the current selection end, or the caret position
  // if nothing is selected
  IPC_STRUCT_MEMBER(int, selection_end)

  // The start position of the current composition, or -1 if there is none
  IPC_STRUCT_MEMBER(int, composition_start)

  // The end position of the current composition, or -1 if there is none
  IPC_STRUCT_MEMBER(int, composition_end)

  // Whether or not inline composition can be performed for the current input.
  IPC_STRUCT_MEMBER(bool, can_compose_inline)

  // Whether or not the IME should be shown as a result of this update. Even if
  // true, the IME will only be shown if the type is appropriate (e.g. not
  // TEXT_INPUT_TYPE_NONE).
  IPC_STRUCT_MEMBER(bool, show_ime_if_needed)

  // Whether this change is originated from non-IME (e.g. Javascript, Autofill).
  IPC_STRUCT_MEMBER(bool, is_non_ime_change)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_UpdateRect_Params)
  // The scroll offset of the render view.
  IPC_STRUCT_MEMBER(gfx::Vector2d, scroll_offset)

  // The size of the RenderView when this message was generated.  This is
  // included so the host knows how large the view is from the perspective of
  // the renderer process.  This is necessary in case a resize operation is in
  // progress. If auto-resize is enabled, this should update the corresponding
  // view size.
  IPC_STRUCT_MEMBER(gfx::Size, view_size)

  // New window locations for plugin child windows.
  IPC_STRUCT_MEMBER(std::vector<content::WebPluginGeometry>,
                    plugin_window_moves)

  // The following describes the various bits that may be set in flags:
  //
  //   ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK
  //     Indicates that this is a response to a ViewMsg_Resize message.
  //
  //   ViewHostMsg_UpdateRect_Flags::IS_REPAINT_ACK
  //     Indicates that this is a response to a ViewMsg_Repaint message.
  //
  // If flags is zero, then this message corresponds to an unsolicited paint
  // request by the render view.  Any of the above bits may be set in flags,
  // which would indicate that this paint message is an ACK for multiple
  // request messages.
  IPC_STRUCT_MEMBER(int, flags)

  // All the above coordinates are in DIP. This is the scale factor needed
  // to convert them to pixels.
  IPC_STRUCT_MEMBER(float, scale_factor)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewMsg_New_Params)
  // Renderer-wide preferences.
  IPC_STRUCT_MEMBER(content::RendererPreferences, renderer_preferences)

  // Preferences for this view.
  IPC_STRUCT_MEMBER(WebPreferences, web_preferences)

  // The ID of the view to be created.
  IPC_STRUCT_MEMBER(int32, view_id)

  // The ID of the main frame hosted in the view.
  IPC_STRUCT_MEMBER(int32, main_frame_routing_id)

  // The ID of the rendering surface.
  IPC_STRUCT_MEMBER(int32, surface_id)

  // The session storage namespace ID this view should use.
  IPC_STRUCT_MEMBER(int64, session_storage_namespace_id)

  // The name of the frame associated with this view (or empty if none).
  IPC_STRUCT_MEMBER(base::string16, frame_name)

  // The route ID of the opener RenderView if we need to set one
  // (MSG_ROUTING_NONE otherwise).
  IPC_STRUCT_MEMBER(int, opener_route_id)

  // Whether the RenderView should initially be swapped out.
  IPC_STRUCT_MEMBER(bool, swapped_out)

  // The ID of the proxy object for the main frame in this view. It is only
  // used if |swapped_out| is true.
  IPC_STRUCT_MEMBER(int32, proxy_routing_id)

  // Whether the RenderView should initially be hidden.
  IPC_STRUCT_MEMBER(bool, hidden)

  // Whether the RenderView will never be visible.
  IPC_STRUCT_MEMBER(bool, never_visible)

  // Whether the window associated with this view was created with an opener.
  IPC_STRUCT_MEMBER(bool, window_was_created_with_opener)

  // The initial page ID to use for this view, which must be larger than any
  // existing navigation that might be loaded in the view.  Page IDs are unique
  // to a view and are only updated by the renderer after this initial value.
  IPC_STRUCT_MEMBER(int32, next_page_id)

  // The properties of the screen associated with the view.
  IPC_STRUCT_MEMBER(blink::WebScreenInfo, screen_info)

  // The accessibility mode of the renderer.
  IPC_STRUCT_MEMBER(AccessibilityMode, accessibility_mode)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewMsg_PostMessage_Params)
  // The serialized script value.
  IPC_STRUCT_MEMBER(base::string16, data)

  // When sent to the browser, this is the routing ID of the source frame in
  // the source process.  The browser replaces it with the routing ID of the
  // equivalent (swapped out) frame in the destination process.
  IPC_STRUCT_MEMBER(int, source_routing_id)

  // The origin of the source frame.
  IPC_STRUCT_MEMBER(base::string16, source_origin)

  // The origin for the message's target.
  IPC_STRUCT_MEMBER(base::string16, target_origin)

  // Information about the MessagePorts this message contains.
  IPC_STRUCT_MEMBER(std::vector<int>, message_port_ids)
  IPC_STRUCT_MEMBER(std::vector<int>, new_routing_ids)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Tells the renderer to cancel an opened date/time dialog.
IPC_MESSAGE_ROUTED0(ViewMsg_CancelDateTimeDialog)

// Get all savable resource links from current webpage, include main
// frame and sub-frame.
IPC_MESSAGE_ROUTED1(ViewMsg_GetAllSavableResourceLinksForCurrentPage,
                    GURL /* url of page which is needed to save */)

// Get html data by serializing all frames of current page with lists
// which contain all resource links that have local copy.
IPC_MESSAGE_ROUTED3(ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks,
                    std::vector<GURL> /* urls that have local copy */,
                    std::vector<base::FilePath> /* paths of local copy */,
                    base::FilePath /* local directory path */)

// Tells the render side that a ViewHostMsg_LockMouse message has been
// processed. |succeeded| indicates whether the mouse has been successfully
// locked or not.
IPC_MESSAGE_ROUTED1(ViewMsg_LockMouse_ACK,
                    bool /* succeeded */)
// Tells the render side that the mouse has been unlocked.
IPC_MESSAGE_ROUTED0(ViewMsg_MouseLockLost)

// Sent by the browser when the parameters for vsync alignment have changed.
IPC_MESSAGE_ROUTED2(ViewMsg_UpdateVSyncParameters,
                    base::TimeTicks /* timebase */,
                    base::TimeDelta /* interval */)

// Sent to the RenderView when a new tab is swapped into an existing
// tab and the histories need to be merged. The existing tab has a history of
// |merged_history_length| which precedes the history of the new tab. All
// page_ids >= |minimum_page_id| in the new tab are appended to the history.
//
// For example, suppose the history of page_ids in the new tab's RenderView
// is [4 7 8]. This is merged into an existing tab with 3 history items, and
// all pages in the new tab with page_id >= 7 are to be preserved.
// The resulting page history is [-1 -1 -1 7 8].
IPC_MESSAGE_ROUTED2(ViewMsg_SetHistoryLengthAndPrune,
                    int, /* merge_history_length */
                    int32 /* minimum_page_id */)

// Tells the renderer to create a new view.
// This message is slightly different, the view it takes (via
// ViewMsg_New_Params) is the view to create, the message itself is sent as a
// non-view control message.
IPC_MESSAGE_CONTROL1(ViewMsg_New,
                     ViewMsg_New_Params)

// Reply in response to ViewHostMsg_ShowView or ViewHostMsg_ShowWidget.
// similar to the new command, but used when the renderer created a view
// first, and we need to update it.
IPC_MESSAGE_ROUTED0(ViewMsg_CreatingNew_ACK)

// Sends updated preferences to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_SetRendererPrefs,
                    content::RendererPreferences)

// This passes a set of webkit preferences down to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_UpdateWebPreferences,
                    WebPreferences)

// Informs the renderer that the timezone has changed.
IPC_MESSAGE_CONTROL0(ViewMsg_TimezoneChange)

// Tells the render view to close.
// Expects a Close_ACK message when finished.
IPC_MESSAGE_ROUTED0(ViewMsg_Close)

IPC_STRUCT_BEGIN(ViewMsg_Resize_Params)
  IPC_STRUCT_MEMBER(blink::WebScreenInfo, screen_info)
  IPC_STRUCT_MEMBER(gfx::Size, new_size)
  IPC_STRUCT_MEMBER(gfx::Size, physical_backing_size)
  IPC_STRUCT_MEMBER(float, overdraw_bottom_height)
  IPC_STRUCT_MEMBER(gfx::Size, visible_viewport_size)
  IPC_STRUCT_MEMBER(gfx::Rect, resizer_rect)
  IPC_STRUCT_MEMBER(bool, is_fullscreen)
IPC_STRUCT_END()

// Tells the render view to change its size.  A ViewHostMsg_UpdateRect message
// is generated in response provided new_size is not empty and not equal to
// the view's current size.  The generated ViewHostMsg_UpdateRect message will
// have the IS_RESIZE_ACK flag set. It also receives the resizer rect so that
// we don't have to fetch it every time WebKit asks for it.
IPC_MESSAGE_ROUTED1(ViewMsg_Resize,
                    ViewMsg_Resize_Params /* params */)

// Tells the render view that the resize rect has changed.
IPC_MESSAGE_ROUTED1(ViewMsg_ChangeResizeRect,
                    gfx::Rect /* resizer_rect */)

// Sent to inform the view that it was hidden.  This allows it to reduce its
// resource utilization.
IPC_MESSAGE_ROUTED0(ViewMsg_WasHidden)

// Tells the render view that it is no longer hidden (see WasHidden), and the
// render view is expected to respond with a full repaint if needs_repainting
// is true. If needs_repainting is false, then this message does not trigger a
// message in response.
IPC_MESSAGE_ROUTED1(ViewMsg_WasShown,
                    bool /* needs_repainting */)

// Sent to inform the view that it was swapped out.  This allows the process to
// exit if no other views are using it.
IPC_MESSAGE_ROUTED0(ViewMsg_WasSwappedOut)

// Tells the renderer to focus the first (last if reverse is true) focusable
// node.
IPC_MESSAGE_ROUTED1(ViewMsg_SetInitialFocus,
                    bool /* reverse */)

// Sent to inform the renderer to invoke a context menu.
// The parameter specifies the location in the render view's coordinates.
IPC_MESSAGE_ROUTED2(ViewMsg_ShowContextMenu,
                    ui::MenuSourceType,
                    gfx::Point /* location where menu should be shown */)

IPC_MESSAGE_ROUTED0(ViewMsg_Stop)

// Sent when the user wants to search for a word on the page (find in page).
IPC_MESSAGE_ROUTED3(ViewMsg_Find,
                    int /* request_id */,
                    base::string16 /* search_text */,
                    blink::WebFindOptions)

// This message notifies the renderer that the user has closed the FindInPage
// window (and what action to take regarding the selection).
IPC_MESSAGE_ROUTED1(ViewMsg_StopFinding,
                    content::StopFindAction /* action */)

// Replaces a date time input field.
IPC_MESSAGE_ROUTED1(ViewMsg_ReplaceDateTime,
                    double /* dialog_value */)

// Copies the image at location x, y to the clipboard (if there indeed is an
// image at that location).
IPC_MESSAGE_ROUTED2(ViewMsg_CopyImageAt,
                    int /* x */,
                    int /* y */)

// Saves the image at location x, y to the disk (if there indeed is an
// image at that location).
IPC_MESSAGE_ROUTED2(ViewMsg_SaveImageAt,
                    int /* x */,
                    int /* y */)

// Tells the renderer to perform the given action on the media player
// located at the given point.
IPC_MESSAGE_ROUTED2(ViewMsg_MediaPlayerActionAt,
                    gfx::Point, /* location */
                    blink::WebMediaPlayerAction)

// Tells the renderer to perform the given action on the plugin located at
// the given point.
IPC_MESSAGE_ROUTED2(ViewMsg_PluginActionAt,
                    gfx::Point, /* location */
                    blink::WebPluginAction)

// Posts a message from a frame in another process to the current renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_PostMessageEvent,
                    ViewMsg_PostMessage_Params)

// Requests that the RenderView's main frame sets its opener to null.
IPC_MESSAGE_ROUTED0(ViewMsg_DisownOpener)

// Change the zoom level for the current main frame.  If the level actually
// changes, a ViewHostMsg_DidZoomURL message will be sent back to the browser
// telling it what url got zoomed and what its current zoom level is.
IPC_MESSAGE_ROUTED1(ViewMsg_Zoom,
                    content::PageZoom /* function */)

// Set the zoom level for a particular url that the renderer is in the
// process of loading.  This will be stored, to be used if the load commits
// and ignored otherwise.
IPC_MESSAGE_ROUTED2(ViewMsg_SetZoomLevelForLoadingURL,
                    GURL /* url */,
                    double /* zoom_level */)

// Set the zoom level for a particular url, so all render views
// displaying this url can update their zoom levels to match.
// If scheme is empty, then only host is used for matching.
IPC_MESSAGE_CONTROL3(ViewMsg_SetZoomLevelForCurrentURL,
                     std::string /* scheme */,
                     std::string /* host */,
                     double /* zoom_level */)

// Set the zoom level for a particular render view.
IPC_MESSAGE_ROUTED2(ViewMsg_SetZoomLevelForView,
                    bool /* uses_temporary_zoom_level */,
                    double /* zoom_level */)

// Change encoding of page in the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_SetPageEncoding,
                    std::string /*new encoding name*/)

// Reset encoding of page in the renderer back to default.
IPC_MESSAGE_ROUTED0(ViewMsg_ResetPageEncodingToDefault)

// Used to tell a render view whether it should expose various bindings
// that allow JS content extended privileges.  See BindingsPolicy for valid
// flag values.
IPC_MESSAGE_ROUTED1(ViewMsg_AllowBindings,
                    int /* enabled_bindings_flags */)

// Tell the renderer to add a property to the WebUI binding object.  This
// only works if we allowed WebUI bindings.
IPC_MESSAGE_ROUTED2(ViewMsg_SetWebUIProperty,
                    std::string /* property_name */,
                    std::string /* property_value_json */)

// This message starts/stop monitoring the input method status of the focused
// edit control of a renderer process.
// Parameters
// * is_active (bool)
//   Indicates if an input method is active in the browser process.
//   The possible actions when a renderer process receives this message are
//   listed below:
//     Value Action
//     true  Start sending IPC message ViewHostMsg_ImeUpdateTextInputState
//           to notify the input method status of the focused edit control.
//     false Stop sending IPC message ViewHostMsg_ImeUpdateTextInputState.
IPC_MESSAGE_ROUTED1(ViewMsg_SetInputMethodActive,
                    bool /* is_active */)

// IME API oncandidatewindow* events for InputMethodContext.
IPC_MESSAGE_ROUTED0(ViewMsg_CandidateWindowShown)
IPC_MESSAGE_ROUTED0(ViewMsg_CandidateWindowUpdated)
IPC_MESSAGE_ROUTED0(ViewMsg_CandidateWindowHidden)

// This message sends a string being composed with an input method.
IPC_MESSAGE_ROUTED4(
    ViewMsg_ImeSetComposition,
    base::string16, /* text */
    std::vector<blink::WebCompositionUnderline>, /* underlines */
    int, /* selectiont_start */
    int /* selection_end */)

// This message confirms an ongoing composition.
IPC_MESSAGE_ROUTED3(ViewMsg_ImeConfirmComposition,
                    base::string16 /* text */,
                    gfx::Range /* replacement_range */,
                    bool /* keep_selection */)

// Used to notify the render-view that we have received a target URL. Used
// to prevent target URLs spamming the browser.
IPC_MESSAGE_ROUTED0(ViewMsg_UpdateTargetURL_ACK)

IPC_MESSAGE_ROUTED1(ViewMsg_RunFileChooserResponse,
                    std::vector<ui::SelectedFileInfo>)

// Provides the results of directory enumeration.
IPC_MESSAGE_ROUTED2(ViewMsg_EnumerateDirectoryResponse,
                    int /* request_id */,
                    std::vector<base::FilePath> /* files_in_directory */)

// When a renderer sends a ViewHostMsg_Focus to the browser process,
// the browser has the option of sending a ViewMsg_CantFocus back to
// the renderer.
IPC_MESSAGE_ROUTED0(ViewMsg_CantFocus)

// Tells the renderer to suppress any further modal dialogs until it receives a
// corresponding ViewMsg_SwapOut message.  This ensures that no
// PageGroupLoadDeferrer is on the stack for SwapOut.
IPC_MESSAGE_ROUTED0(ViewMsg_SuppressDialogsUntilSwapOut)

// Instructs the renderer to close the current page, including running the
// onunload event handler.
//
// Expects a ClosePage_ACK message when finished.
IPC_MESSAGE_ROUTED0(ViewMsg_ClosePage)

// Notifies the renderer about ui theme changes
IPC_MESSAGE_ROUTED0(ViewMsg_ThemeChanged)

// Notifies the renderer that a paint is to be generated for the rectangle
// passed in.
IPC_MESSAGE_ROUTED1(ViewMsg_Repaint,
                    gfx::Size /* The view size to be repainted */)

// Notification that a move or resize renderer's containing window has
// started.
IPC_MESSAGE_ROUTED0(ViewMsg_MoveOrResizeStarted)

IPC_MESSAGE_ROUTED2(ViewMsg_UpdateScreenRects,
                    gfx::Rect /* view_screen_rect */,
                    gfx::Rect /* window_screen_rect */)

// Reply to ViewHostMsg_RequestMove, ViewHostMsg_ShowView, and
// ViewHostMsg_ShowWidget to inform the renderer that the browser has
// processed the move.  The browser may have ignored the move, but it finished
// processing.  This is used because the renderer keeps a temporary cache of
// the widget position while these asynchronous operations are in progress.
IPC_MESSAGE_ROUTED0(ViewMsg_Move_ACK)

// Used to instruct the RenderView to send back updates to the preferred size.
IPC_MESSAGE_ROUTED0(ViewMsg_EnablePreferredSizeChangedMode)

// Used to instruct the RenderView to automatically resize and send back
// updates for the new size.
IPC_MESSAGE_ROUTED2(ViewMsg_EnableAutoResize,
                    gfx::Size /* min_size */,
                    gfx::Size /* max_size */)

// Used to instruct the RenderView to disalbe automatically resize.
IPC_MESSAGE_ROUTED1(ViewMsg_DisableAutoResize,
                    gfx::Size /* new_size */)

// Changes the text direction of the currently selected input field (if any).
IPC_MESSAGE_ROUTED1(ViewMsg_SetTextDirection,
                    blink::WebTextDirection /* direction */)

// Tells the renderer to clear the focused element (if any).
IPC_MESSAGE_ROUTED0(ViewMsg_ClearFocusedElement)

// Make the RenderView background transparent or opaque.
IPC_MESSAGE_ROUTED1(ViewMsg_SetBackgroundOpaque, bool /* opaque */)

// Used to tell the renderer not to add scrollbars with height and
// width below a threshold.
IPC_MESSAGE_ROUTED1(ViewMsg_DisableScrollbarsForSmallWindows,
                    gfx::Size /* disable_scrollbar_size_limit */)

// Activate/deactivate the RenderView (i.e., set its controls' tint
// accordingly, etc.).
IPC_MESSAGE_ROUTED1(ViewMsg_SetActive,
                    bool /* active */)

// Response message to ViewHostMsg_CreateWorker.
// Sent when the worker has started.
IPC_MESSAGE_ROUTED0(ViewMsg_WorkerCreated)

// Sent when the worker failed to load the worker script.
// In normal cases, this message is sent after ViewMsg_WorkerCreated is sent.
// But if the shared worker of the same URL already exists and it has failed
// to load the script, when the renderer send ViewHostMsg_CreateWorker before
// the shared worker is killed only ViewMsg_WorkerScriptLoadFailed is sent.
IPC_MESSAGE_ROUTED0(ViewMsg_WorkerScriptLoadFailed)

// Sent when the worker has connected.
// This message is sent only if the worker successfully loaded the script.
IPC_MESSAGE_ROUTED0(ViewMsg_WorkerConnected)

// Tells the renderer that the network type has changed so that navigator.onLine
// and navigator.connection can be updated.
IPC_MESSAGE_CONTROL1(ViewMsg_NetworkTypeChanged,
                     net::NetworkChangeNotifier::ConnectionType /* type */)

// Reply to ViewHostMsg_OpenChannelToPpapiBroker
// Tells the renderer that the channel to the broker has been created.
IPC_MESSAGE_ROUTED2(ViewMsg_PpapiBrokerChannelCreated,
                    base::ProcessId /* broker_pid */,
                    IPC::ChannelHandle /* handle */)

// Reply to ViewHostMsg_RequestPpapiBrokerPermission.
// Tells the renderer whether permission to access to PPAPI broker was granted
// or not.
IPC_MESSAGE_ROUTED1(ViewMsg_PpapiBrokerPermissionResult,
                    bool /* result */)

// Tells the renderer to empty its plugin list cache, optional reloading
// pages containing plugins.
IPC_MESSAGE_CONTROL1(ViewMsg_PurgePluginListCache,
                     bool /* reload_pages */)

// Used to instruct the RenderView to go into "view source" mode.
IPC_MESSAGE_ROUTED0(ViewMsg_EnableViewSourceMode)

// Instructs the renderer to save the current page to MHTML.
IPC_MESSAGE_ROUTED2(ViewMsg_SavePageAsMHTML,
                    int /* job_id */,
                    IPC::PlatformFileForTransit /* file handle */)

// Temporary message to diagnose an unexpected condition in WebContentsImpl.
IPC_MESSAGE_CONTROL1(ViewMsg_TempCrashWithData,
                     GURL /* data */)

// Change the accessibility mode in the renderer process.
IPC_MESSAGE_ROUTED1(ViewMsg_SetAccessibilityMode,
                    AccessibilityMode)

// An acknowledge to ViewHostMsg_MultipleTargetsTouched to notify the renderer
// process to release the magnified image.
IPC_MESSAGE_ROUTED1(ViewMsg_ReleaseDisambiguationPopupBitmap,
                    cc::SharedBitmapId /* id */)

// Notifies the renderer that a snapshot has been retrieved.
IPC_MESSAGE_ROUTED3(ViewMsg_WindowSnapshotCompleted,
                    int /* snapshot_id */,
                    gfx::Size /* size */,
                    std::vector<unsigned char> /* png */)

#if defined(OS_MACOSX)
IPC_ENUM_TRAITS_MAX_VALUE(blink::ScrollerStyle, blink::ScrollerStyleOverlay);

// Notification of a change in scrollbar appearance and/or behavior.
IPC_MESSAGE_CONTROL5(ViewMsg_UpdateScrollbarTheme,
                     float /* initial_button_delay */,
                     float /* autoscroll_button_delay */,
                     bool /* jump_on_track_click */,
                     blink::ScrollerStyle /* preferred_scroller_style */,
                     bool /* redraw */)
#endif

#if defined(OS_ANDROID)
// Tells the renderer to suspend/resume the webkit timers.
IPC_MESSAGE_CONTROL1(ViewMsg_SetWebKitSharedTimersSuspended,
                     bool /* suspend */)

// Sent when the browser wants the bounding boxes of the current find matches.
//
// If match rects are already cached on the browser side, |current_version|
// should be the version number from the ViewHostMsg_FindMatchRects_Reply
// they came in, so the renderer can tell if it needs to send updated rects.
// Otherwise just pass -1 to always receive the list of rects.
//
// There must be an active search string (it is probably most useful to call
// this immediately after a ViewHostMsg_Find_Reply message arrives with
// final_update set to true).
IPC_MESSAGE_ROUTED1(ViewMsg_FindMatchRects,
                    int /* current_version */)

// External popup menus.
IPC_MESSAGE_ROUTED2(ViewMsg_SelectPopupMenuItems,
                    bool /* user canceled the popup */,
                    std::vector<int> /* selected indices */)

// Notifies the renderer whether hiding/showing the top controls is enabled
// and whether or not to animate to the proper state.
IPC_MESSAGE_ROUTED3(ViewMsg_UpdateTopControlsState,
                    bool /* enable_hiding */,
                    bool /* enable_showing */,
                    bool /* animate */)

IPC_MESSAGE_ROUTED0(ViewMsg_ShowImeIfNeeded)

// Sent by the browser when the renderer should generate a new frame.
IPC_MESSAGE_ROUTED1(ViewMsg_BeginFrame,
                    cc::BeginFrameArgs /* args */)

// Sent by the browser when an IME update that requires acknowledgement has been
// processed on the browser side.
IPC_MESSAGE_ROUTED0(ViewMsg_ImeEventAck)

// Extracts the data at the given rect, returning it through the
// ViewHostMsg_SmartClipDataExtracted IPC.
IPC_MESSAGE_ROUTED1(ViewMsg_ExtractSmartClipData,
                    gfx::Rect /* rect */)

#elif defined(OS_MACOSX)
// Let the RenderView know its window has changed visibility.
IPC_MESSAGE_ROUTED1(ViewMsg_SetWindowVisibility,
                    bool /* visibile */)

// Let the RenderView know its window's frame has changed.
IPC_MESSAGE_ROUTED2(ViewMsg_WindowFrameChanged,
                    gfx::Rect /* window frame */,
                    gfx::Rect /* content view frame */)

// Message sent from the browser to the renderer when the user starts or stops
// resizing the view.
IPC_MESSAGE_ROUTED1(ViewMsg_SetInLiveResize,
                    bool /* enable */)

// Tell the renderer that plugin IME has completed.
IPC_MESSAGE_ROUTED2(ViewMsg_PluginImeCompositionCompleted,
                    base::string16 /* text */,
                    int /* plugin_id */)

// External popup menus.
IPC_MESSAGE_ROUTED1(ViewMsg_SelectPopupMenuItem,
                    int /* selected index, -1 means no selection */)
#endif

// Sent by the browser as a reply to ViewHostMsg_SwapCompositorFrame.
IPC_MESSAGE_ROUTED2(ViewMsg_SwapCompositorFrameAck,
                    uint32 /* output_surface_id */,
                    cc::CompositorFrameAck /* ack */)

// Sent by browser to tell renderer compositor that some resources that were
// given to the browser in a swap are not being used anymore.
IPC_MESSAGE_ROUTED2(ViewMsg_ReclaimCompositorResources,
                    uint32 /* output_surface_id */,
                    cc::CompositorFrameAck /* ack */)

IPC_MESSAGE_ROUTED0(ViewMsg_SelectWordAroundCaret)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Sent by the renderer when it is creating a new window.  The browser creates
// a tab for it and responds with a ViewMsg_CreatingNew_ACK.  If route_id is
// MSG_ROUTING_NONE, the view couldn't be created.
IPC_SYNC_MESSAGE_CONTROL1_4(ViewHostMsg_CreateWindow,
                            ViewHostMsg_CreateWindow_Params,
                            int /* route_id */,
                            int /* main_frame_route_id */,
                            int32 /* surface_id */,
                            int64 /* cloned_session_storage_namespace_id */)

// Similar to ViewHostMsg_CreateWindow, except used for sub-widgets, like
// <select> dropdowns.  This message is sent to the WebContentsImpl that
// contains the widget being created.
IPC_SYNC_MESSAGE_CONTROL2_2(ViewHostMsg_CreateWidget,
                            int /* opener_id */,
                            blink::WebPopupType /* popup type */,
                            int /* route_id */,
                            int32 /* surface_id */)

// Similar to ViewHostMsg_CreateWidget except the widget is a full screen
// window.
IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_CreateFullscreenWidget,
                            int /* opener_id */,
                            int /* route_id */,
                            int32 /* surface_id */)

// Asks the browser for a unique routing ID.
IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GenerateRoutingID,
                            int /* routing_id */)

// Asks the browser for the default audio hardware configuration.
IPC_SYNC_MESSAGE_CONTROL0_2(ViewHostMsg_GetAudioHardwareConfig,
                            media::AudioParameters /* input parameters */,
                            media::AudioParameters /* output parameters */)

// Asks the browser for CPU usage of the renderer process in percents.
IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GetCPUUsage,
                            int /* CPU usage in percents */)

// Asks the browser for the renderer process memory size stats.
IPC_SYNC_MESSAGE_CONTROL0_2(ViewHostMsg_GetProcessMemorySizes,
                            size_t /* private_bytes */,
                            size_t /* shared_bytes */)

// These three messages are sent to the parent RenderViewHost to display the
// page/widget that was created by
// CreateWindow/CreateWidget/CreateFullscreenWidget. routing_id
// refers to the id that was returned from the Create message above.
// The initial_position parameter is a rectangle in screen coordinates.
//
// FUTURE: there will probably be flags here to control if the result is
// in a new window.
IPC_MESSAGE_ROUTED4(ViewHostMsg_ShowView,
                    int /* route_id */,
                    WindowOpenDisposition /* disposition */,
                    gfx::Rect /* initial_pos */,
                    bool /* opened_by_user_gesture */)

IPC_MESSAGE_ROUTED2(ViewHostMsg_ShowWidget,
                    int /* route_id */,
                    gfx::Rect /* initial_pos */)

// Message to show a full screen widget.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowFullscreenWidget,
                    int /* route_id */)

// This message is sent after ViewHostMsg_ShowView to cause the RenderView
// to run in a modal fashion until it is closed.
IPC_SYNC_MESSAGE_ROUTED1_0(ViewHostMsg_RunModal,
                           int /* opener_id */)

// Indicates the renderer is ready in response to a ViewMsg_New or
// a ViewMsg_CreatingNew_ACK.
IPC_MESSAGE_ROUTED0(ViewHostMsg_RenderViewReady)

// Indicates the renderer process is gone.  This actually is sent by the
// browser process to itself, but keeps the interface cleaner.
IPC_MESSAGE_ROUTED2(ViewHostMsg_RenderProcessGone,
                    int, /* this really is base::TerminationStatus */
                    int /* exit_code */)

// Sent by the renderer process to request that the browser close the view.
// This corresponds to the window.close() API, and the browser may ignore
// this message.  Otherwise, the browser will generates a ViewMsg_Close
// message to close the view.
IPC_MESSAGE_ROUTED0(ViewHostMsg_Close)

// Send in response to a ViewMsg_UpdateScreenRects so that the renderer can
// throttle these messages.
IPC_MESSAGE_ROUTED0(ViewHostMsg_UpdateScreenRects_ACK)

// Sent by the renderer process to request that the browser move the view.
// This corresponds to the window.resizeTo() and window.moveTo() APIs, and
// the browser may ignore this message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_RequestMove,
                    gfx::Rect /* position */)

#if defined(OS_MACOSX) || defined(OS_ANDROID)
// Message to show/hide a popup menu using native controls.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowPopup,
                    ViewHostMsg_ShowPopup_Params)
IPC_MESSAGE_ROUTED0(ViewHostMsg_HidePopup)
#endif

// Result of string search in the page.
// Response to ViewMsg_Find with the results of the requested find-in-page
// search, the number of matches found and the selection rect (in screen
// coordinates) for the string found. If |final_update| is false, it signals
// that this is not the last Find_Reply message - more will be sent as the
// scoping effort continues.
IPC_MESSAGE_ROUTED5(ViewHostMsg_Find_Reply,
                    int /* request_id */,
                    int /* number of matches */,
                    gfx::Rect /* selection_rect */,
                    int /* active_match_ordinal */,
                    bool /* final_update */)

// Indicates that the render view has been closed in respose to a
// Close message.
IPC_MESSAGE_CONTROL1(ViewHostMsg_Close_ACK,
                     int /* old_route_id */);

// Indicates that the current page has been closed, after a ClosePage
// message.
IPC_MESSAGE_ROUTED0(ViewHostMsg_ClosePage_ACK)

// Notifies the browser that we have session history information.
// page_id: unique ID that allows us to distinguish between history entries.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateState,
                    int32 /* page_id */,
                    content::PageState /* state */)

// Notifies the browser that we want to show a destination url for a potential
// action (e.g. when the user is hovering over a link).
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateTargetURL,
                    int32,
                    GURL)

// Sent when the document element is available for the top-level frame.  This
// happens after the page starts loading, but before all resources are
// finished.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DocumentAvailableInMainFrame,
                    bool /* uses_temporary_zoom_level */)

// Sent when the renderer loads a resource from its memory cache.
// The security info is non empty if the resource was originally loaded over
// a secure connection.
// Note: May only be sent once per URL per frame per committed load.
IPC_MESSAGE_ROUTED5(ViewHostMsg_DidLoadResourceFromMemoryCache,
                    GURL /* url */,
                    std::string  /* security info */,
                    std::string  /* http method */,
                    std::string  /* mime type */,
                    ResourceType::Type /* resource type */)

// Sent when the renderer displays insecure content in a secure page.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidDisplayInsecureContent)

// Sent when the renderer runs insecure content in a secure origin.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DidRunInsecureContent,
                    std::string  /* security_origin */,
                    GURL         /* target URL */)

// Sent to update part of the view.  In response to this message, the host
// generates a ViewMsg_UpdateRect_ACK message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateRect,
                    ViewHostMsg_UpdateRect_Params)

IPC_MESSAGE_ROUTED0(ViewHostMsg_Focus)
IPC_MESSAGE_ROUTED0(ViewHostMsg_Blur)

// Message sent from renderer to the browser when focus changes inside the
// webpage. The parameter says whether the newly focused element needs
// keyboard input (true for textfields, text areas and content editable divs).
IPC_MESSAGE_ROUTED1(ViewHostMsg_FocusedNodeChanged,
    bool /* is_editable_node */)

IPC_MESSAGE_ROUTED1(ViewHostMsg_SetCursor, content::WebCursor)

// Message sent from renderer requesting touch emulation using mouse.
// Shift-scrolling should be converted to pinch, if |allow_pinch| is true.
IPC_MESSAGE_ROUTED2(ViewHostMsg_SetTouchEventEmulationEnabled,
                    bool /* enabled */,
                    bool /* allow_pinch */)

// Used to set a cookie. The cookie is set asynchronously, but will be
// available to a subsequent ViewHostMsg_GetCookies request.
IPC_MESSAGE_CONTROL4(ViewHostMsg_SetCookie,
                     int /* render_frame_id */,
                     GURL /* url */,
                     GURL /* first_party_for_cookies */,
                     std::string /* cookie */)

// Used to get cookies for the given URL. This may block waiting for a
// previous SetCookie message to be processed.
IPC_SYNC_MESSAGE_CONTROL3_1(ViewHostMsg_GetCookies,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* first_party_for_cookies */,
                            std::string /* cookies */)

// Used to get raw cookie information for the given URL. This may block
// waiting for a previous SetCookie message to be processed.
IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_GetRawCookies,
                            GURL /* url */,
                            GURL /* first_party_for_cookies */,
                            std::vector<content::CookieData>
                                /* raw_cookies */)

// Used to delete cookie for the given URL and name
IPC_SYNC_MESSAGE_CONTROL2_0(ViewHostMsg_DeleteCookie,
                            GURL /* url */,
                            std::string /* cookie_name */)

// Used to check if cookies are enabled for the given URL. This may block
// waiting for a previous SetCookie message to be processed.
IPC_SYNC_MESSAGE_CONTROL3_1(ViewHostMsg_CookiesEnabled,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* first_party_for_cookies */,
                            bool /* cookies_enabled */)

// Used to get the list of plugins
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetPlugins,
    bool /* refresh*/,
    std::vector<content::WebPluginInfo> /* plugins */)

#if defined(OS_WIN)
IPC_MESSAGE_ROUTED1(ViewHostMsg_WindowlessPluginDummyWindowCreated,
                    gfx::NativeViewId /* dummy_activation_window */)

IPC_MESSAGE_ROUTED1(ViewHostMsg_WindowlessPluginDummyWindowDestroyed,
                    gfx::NativeViewId /* dummy_activation_window */)

// Asks the browser for the user's monitor profile.
IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GetMonitorColorProfile,
                            std::vector<char> /* profile */)
#endif

// Get the list of proxies to use for |url|, as a semicolon delimited list
// of "<TYPE> <HOST>:<PORT>" | "DIRECT".
IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_ResolveProxy,
                            GURL /* url */,
                            bool /* result */,
                            std::string /* proxy list */)

// A renderer sends this to the browser process when it wants to create a
// worker.  The browser will create the worker process if necessary, and
// will return the route id on success.  On error returns MSG_ROUTING_NONE.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_CreateWorker,
                            ViewHostMsg_CreateWorker_Params,
                            int /* route_id */)

// A renderer sends this to the browser process when a document has been
// detached. The browser will use this to constrain the lifecycle of worker
// processes (SharedWorkers are shut down when their last associated document
// is detached).
IPC_MESSAGE_CONTROL1(ViewHostMsg_DocumentDetached,
                     uint64 /* document_id */)

// Wraps an IPC message that's destined to the worker on the renderer->browser
// hop.
IPC_MESSAGE_CONTROL1(ViewHostMsg_ForwardToWorker,
                     IPC::Message /* message */)

// Tells the browser that a specific Appcache manifest in the current page
// was accessed.
IPC_MESSAGE_ROUTED2(ViewHostMsg_AppCacheAccessed,
                    GURL /* manifest url */,
                    bool /* blocked by policy */)

// Initiates a download based on user actions like 'ALT+click'.
IPC_MESSAGE_CONTROL5(ViewHostMsg_DownloadUrl,
                     int /* render_view_id */,
                     GURL /* url */,
                     content::Referrer /* referrer */,
                     base::string16 /* suggested_name */,
                     bool /* use prompt for save location */)

// Used to go to the session history entry at the given offset (ie, -1 will
// return the "back" item).
IPC_MESSAGE_ROUTED1(ViewHostMsg_GoToEntryAtOffset,
                    int /* offset (from current) of history item to get */)

// Sent from an inactive renderer for the browser to route to the active
// renderer, instructing it to close.
IPC_MESSAGE_ROUTED0(ViewHostMsg_RouteCloseEvent)

// Sent to the browser from an inactive renderer to post a message to the
// active renderer.
IPC_MESSAGE_ROUTED1(ViewHostMsg_RouteMessageEvent,
                    ViewMsg_PostMessage_Params)

// Notifies that the preferred size of the content changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidContentsPreferredSizeChange,
                    gfx::Size /* pref_size */)

// Notifies that the scroll offset changed.
// This is different from ViewHostMsg_UpdateRect in that ViewHostMsg_UpdateRect
// is not sent at all when threaded compositing is enabled while
// ViewHostMsg_DidChangeScrollOffset works properly in this case.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidChangeScrollOffset)

// Notifies whether there are JavaScript touch event handlers or not.
IPC_MESSAGE_ROUTED1(ViewHostMsg_HasTouchEventHandlers,
                    bool /* has_handlers */)

// A message from HTML-based UI.  When (trusted) Javascript calls
// send(message, args), this message is sent to the browser.
IPC_MESSAGE_ROUTED3(ViewHostMsg_WebUISend,
                    GURL /* source_url */,
                    std::string  /* message */,
                    base::ListValue /* args */)

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
IPC_SYNC_MESSAGE_CONTROL1_3(ViewHostMsg_OpenChannelToPepperPlugin,
                            base::FilePath /* path */,
                            IPC::ChannelHandle /* handle to channel */,
                            base::ProcessId /* plugin_pid */,
                            int /* plugin_child_id */)

// Notification that a plugin has created a new plugin instance. The parameters
// indicate:
// -The plugin process ID that we're creating the instance for.
// -The instance ID of the instance being created.
// -A PepperRendererInstanceData struct which contains properties from the
// renderer which are associated with the plugin instance. This includes the
// routing ID of the associated render view and the URL of plugin.
// -Whether the plugin we're creating an instance for is external or internal.
//
// This message must be sync even though it returns no parameters to avoid
// a race condition with the plugin process. The plugin process sends messages
// to the browser that assume the browser knows about the instance. We need to
// make sure that the browser actually knows about the instance before we tell
// the plugin to run.
IPC_SYNC_MESSAGE_CONTROL4_0(
    ViewHostMsg_DidCreateOutOfProcessPepperInstance,
    int /* plugin_child_id */,
    int32 /* pp_instance */,
    content::PepperRendererInstanceData /* creation_data */,
    bool /* is_external */)

// Notification that a plugin has destroyed an instance. This is the opposite of
// the "DidCreate" message above.
IPC_MESSAGE_CONTROL3(ViewHostMsg_DidDeleteOutOfProcessPepperInstance,
                     int /* plugin_child_id */,
                     int32 /* pp_instance */,
                     bool /* is_external */)

// Message from the renderer to the browser indicating the in-process instance
// has been created.
IPC_MESSAGE_CONTROL2(ViewHostMsg_DidCreateInProcessInstance,
                     int32 /* instance */,
                     content::PepperRendererInstanceData /* instance_data */)

// Message from the renderer to the browser indicating the in-process instance
// has been destroyed.
IPC_MESSAGE_CONTROL1(ViewHostMsg_DidDeleteInProcessInstance,
                     int32 /* instance */)

// A renderer sends this to the browser process when it wants to
// create a ppapi broker.  The browser will create the broker process
// if necessary, and will return a handle to the channel on success.
// On error an empty string is returned.
// The browser will respond with ViewMsg_PpapiBrokerChannelCreated.
IPC_MESSAGE_CONTROL2(ViewHostMsg_OpenChannelToPpapiBroker,
                     int /* routing_id */,
                     base::FilePath /* path */)

// A renderer sends this to the browser process when it wants to access a PPAPI
// broker. In contrast to ViewHostMsg_OpenChannelToPpapiBroker, this is called
// for every connection.
// The browser will respond with ViewMsg_PpapiBrokerPermissionResult.
IPC_MESSAGE_ROUTED3(ViewHostMsg_RequestPpapiBrokerPermission,
                    int /* routing_id */,
                    GURL /* document_url */,
                    base::FilePath /* plugin_path */)

// Send the tooltip text for the current mouse position to the browser.
IPC_MESSAGE_ROUTED2(ViewHostMsg_SetTooltipText,
                    base::string16 /* tooltip text string */,
                    blink::WebTextDirection /* text direction hint */)

IPC_MESSAGE_ROUTED0(ViewHostMsg_SelectRange_ACK)
IPC_MESSAGE_ROUTED0(ViewHostMsg_MoveCaret_ACK)

// Notification that the text selection has changed.
// Note: The secound parameter is the character based offset of the
// base::string16
// text in the document.
IPC_MESSAGE_ROUTED3(ViewHostMsg_SelectionChanged,
                    base::string16 /* text covers the selection range */,
                    size_t /* the offset of the text in the document */,
                    gfx::Range /* selection range in the document */)

// Notification that the selection bounds have changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_SelectionBoundsChanged,
                    ViewHostMsg_SelectionBounds_Params)

// Asks the browser to display the file chooser.  The result is returned in a
// ViewMsg_RunFileChooserResponse message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_RunFileChooser,
                    content::FileChooserParams)

// Asks the browser to enumerate a directory.  This is equivalent to running
// the file chooser in directory-enumeration mode and having the user select
// the given directory.  The result is returned in a
// ViewMsg_EnumerateDirectoryResponse message.
IPC_MESSAGE_ROUTED2(ViewHostMsg_EnumerateDirectory,
                    int /* request_id */,
                    base::FilePath /* file_path */)

// Tells the browser to move the focus to the next (previous if reverse is
// true) focusable element.
IPC_MESSAGE_ROUTED1(ViewHostMsg_TakeFocus,
                    bool /* reverse */)

// Required for opening a date/time dialog
IPC_MESSAGE_ROUTED1(ViewHostMsg_OpenDateTimeDialog,
                    ViewHostMsg_DateTimeDialogValue_Params /* value */)

// Required for updating text input state.
IPC_MESSAGE_ROUTED1(ViewHostMsg_TextInputStateChanged,
                    ViewHostMsg_TextInputState_Params /* input state params */)

// Required for cancelling an ongoing input method composition.
IPC_MESSAGE_ROUTED0(ViewHostMsg_ImeCancelComposition)

// Sent when the renderer changes the zoom level for a particular url, so the
// browser can update its records.  If the view is a plugin doc, then url is
// used to update the zoom level for all pages in that site.  Otherwise, the
// render view's id is used so that only the menu is updated.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DidZoomURL,
                    double /* zoom_level */,
                    GURL /* url */)

// Updates the minimum/maximum allowed zoom percent for this tab from the
// default values.  If |remember| is true, then the zoom setting is applied to
// other pages in the site and is saved, otherwise it only applies to this
// tab.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateZoomLimits,
                    int /* minimum_percent */,
                    int /* maximum_percent */)

// Notify the browser that this render process can or can't be suddenly
// terminated.
IPC_MESSAGE_CONTROL1(ViewHostMsg_SuddenTerminationChanged,
                     bool /* enabled */)

IPC_STRUCT_BEGIN(ViewHostMsg_CompositorSurfaceBuffersSwapped_Params)
  IPC_STRUCT_MEMBER(int32, surface_id)
  IPC_STRUCT_MEMBER(uint64, surface_handle)
  IPC_STRUCT_MEMBER(int32, route_id)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(float, scale_factor)
  IPC_STRUCT_MEMBER(int32, gpu_process_host_id)
  IPC_STRUCT_MEMBER(std::vector<ui::LatencyInfo>, latency_info)
IPC_STRUCT_END()

// This message is synthesized by GpuProcessHost to pass through a swap message
// to the RenderWidgetHelper. This allows GetBackingStore to block for either a
// software or GPU frame.
IPC_MESSAGE_ROUTED1(
    ViewHostMsg_CompositorSurfaceBuffersSwapped,
    ViewHostMsg_CompositorSurfaceBuffersSwapped_Params /* params */)

IPC_MESSAGE_ROUTED2(ViewHostMsg_SwapCompositorFrame,
                    uint32 /* output_surface_id */,
                    cc::CompositorFrame /* frame */)

// Sent by the compositor when a flinging animation is stopped.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidStopFlinging)

//---------------------------------------------------------------------------
// Request for cryptographic operation messages:
// These are messages from the renderer to the browser to perform a
// cryptographic operation.

// Asks the browser process to generate a keypair for grabbing a client
// certificate from a CA (<keygen> tag), and returns the signed public
// key and challenge string.
IPC_SYNC_MESSAGE_CONTROL3_1(ViewHostMsg_Keygen,
                            uint32 /* key size index */,
                            std::string /* challenge string */,
                            GURL /* URL of requestor */,
                            std::string /* signed public key and challenge */)

// Message sent from the renderer to the browser to request that the browser
// cache |data| associated with |url|.
IPC_MESSAGE_CONTROL3(ViewHostMsg_DidGenerateCacheableMetadata,
                     GURL /* url */,
                     double /* expected_response_time */,
                     std::vector<char> /* data */)

// Register a new handler for URL requests with the given scheme.
IPC_MESSAGE_ROUTED4(ViewHostMsg_RegisterProtocolHandler,
                    std::string /* scheme */,
                    GURL /* url */,
                    base::string16 /* title */,
                    bool /* user_gesture */)

// Stores new inspector setting in the profile.
// TODO(jam): this should be in the chrome module
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateInspectorSetting,
                    std::string,  /* key */
                    std::string /* value */)

// Puts the browser into "tab fullscreen" mode for the sending renderer.
// See the comment in chrome/browser/ui/browser.h for more details.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ToggleFullscreen,
                    bool /* enter_fullscreen */)

// Send back a string to be recorded by UserMetrics.
IPC_MESSAGE_CONTROL1(ViewHostMsg_UserMetricsRecordAction,
                     std::string /* action */)

// Notifies the browser that the page was or was not saved as MHTML.
IPC_MESSAGE_CONTROL2(ViewHostMsg_SavedPageAsMHTML,
                     int /* job_id */,
                     int64 /* size of the MHTML file, -1 if error */)

IPC_MESSAGE_ROUTED3(ViewHostMsg_SendCurrentPageAllSavableResourceLinks,
                    std::vector<GURL> /* all savable resource links */,
                    std::vector<content::Referrer> /* all referrers */,
                    std::vector<GURL> /* all frame links */)

IPC_MESSAGE_ROUTED3(ViewHostMsg_SendSerializedHtmlData,
                    GURL /* frame's url */,
                    std::string /* data buffer */,
                    int32 /* complete status */)

// Notifies the browser of an event occurring in the media pipeline.
IPC_MESSAGE_CONTROL1(ViewHostMsg_MediaLogEvents,
                     std::vector<media::MediaLogEvent> /* events */)

// Requests to lock the mouse. Will result in a ViewMsg_LockMouse_ACK message
// being sent back.
// |privileged| is used by Pepper Flash. If this flag is set to true, we won't
// pop up a bubble to ask for user permission or take mouse lock content into
// account.
IPC_MESSAGE_ROUTED3(ViewHostMsg_LockMouse,
                    bool /* user_gesture */,
                    bool /* last_unlocked_by_target */,
                    bool /* privileged */)

// Requests to unlock the mouse. A ViewMsg_MouseLockLost message will be sent
// whenever the mouse is unlocked (which may or may not be caused by
// ViewHostMsg_UnlockMouse).
IPC_MESSAGE_ROUTED0(ViewHostMsg_UnlockMouse)

// Notifies that multiple touch targets may have been pressed, and to show
// the disambiguation popup.
IPC_MESSAGE_ROUTED3(ViewHostMsg_ShowDisambiguationPopup,
                    gfx::Rect, /* Border of touched targets */
                    gfx::Size, /* Size of zoomed image */
                    cc::SharedBitmapId /* id */)

// Sent by the renderer process to check whether client 3D APIs
// (Pepper 3D, WebGL) are explicitly blocked.
IPC_SYNC_MESSAGE_CONTROL3_1(ViewHostMsg_Are3DAPIsBlocked,
                            int /* render_view_id */,
                            GURL /* top_origin_url */,
                            content::ThreeDAPIType /* requester */,
                            bool /* blocked */)

// Sent by the renderer process to indicate that a context was lost by
// client 3D content (Pepper 3D, WebGL) running on the page at the
// given URL.
IPC_MESSAGE_CONTROL3(ViewHostMsg_DidLose3DContext,
                     GURL /* top_origin_url */,
                     content::ThreeDAPIType /* context_type */,
                     int /* arb_robustness_status_code */)

// Notifies the browser that document has parsed the body. This is used by the
// ResourceScheduler as an indication that bandwidth contention won't block
// first paint.
IPC_MESSAGE_ROUTED0(ViewHostMsg_WillInsertBody)

// Notification that the urls for the favicon of a site has been determined.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateFaviconURL,
                    std::vector<content::FaviconURL> /* candidates */)

// Sent once a paint happens after the first non empty layout. In other words
// after the page has painted something.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidFirstVisuallyNonEmptyPaint)

// Sent by the renderer to the browser to start a vibration with the given
// duration.
IPC_MESSAGE_CONTROL1(ViewHostMsg_Vibrate,
                     int64 /* milliseconds */)

// Sent by the renderer to the browser to cancel the currently running
// vibration, if there is one.
IPC_MESSAGE_CONTROL0(ViewHostMsg_CancelVibration)

// Message sent from renderer to the browser when the element that is focused
// has been touched. A bool is passed in this message which indicates if the
// node is editable.
IPC_MESSAGE_ROUTED1(ViewHostMsg_FocusedNodeTouched,
                    bool /* editable */)

// Message sent from the renderer to the browser when an HTML form has failed
// validation constraints.
IPC_MESSAGE_ROUTED3(ViewHostMsg_ShowValidationMessage,
                    gfx::Rect /* anchor rectangle in root view coordinate */,
                    base::string16 /* validation message */,
                    base::string16 /* supplemental text */)

// Message sent from the renderer to the browser when a HTML form validation
// message should be hidden from view.
IPC_MESSAGE_ROUTED0(ViewHostMsg_HideValidationMessage)

// Message sent from the renderer to the browser when the suggested co-ordinates
// of the anchor for a HTML form validation message have changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_MoveValidationMessage,
                    gfx::Rect /* anchor rectangle in root view coordinate */)

#if defined(OS_ANDROID)
// Response to ViewMsg_FindMatchRects.
//
// |version| will contain the current version number of the renderer's find
// match list (incremented whenever they change), which should be passed in the
// next call to ViewMsg_FindMatchRects.
//
// |rects| will either contain a list of the enclosing rects of all matches
// found by the most recent Find operation, or will be empty if |version| is not
// greater than the |current_version| passed to ViewMsg_FindMatchRects (hence
// your locally cached rects should still be valid). The rect coords will be
// custom normalized fractions of the document size. The rects will be sorted by
// frame traversal order starting in the main frame, then by dom order.
//
// |active_rect| will contain the bounding box of the active find-in-page match
// marker, in similarly normalized coords (or an empty rect if there isn't one).
IPC_MESSAGE_ROUTED3(ViewHostMsg_FindMatchRects_Reply,
                    int /* version */,
                    std::vector<gfx::RectF> /* rects */,
                    gfx::RectF /* active_rect */)

// Start an android intent with the given URI.
IPC_MESSAGE_ROUTED1(ViewHostMsg_StartContentIntent,
                    GURL /* content_url */)

// Message sent when the renderer changed the background color for the view.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidChangeBodyBackgroundColor,
                    uint32  /* bg_color */)

// This message runs the MediaCodec for decoding audio for webaudio.
IPC_MESSAGE_CONTROL3(ViewHostMsg_RunWebAudioMediaCodec,
                     base::SharedMemoryHandle /* encoded_data_handle */,
                     base::FileDescriptor /* pcm_output */,
                     uint32_t /* data_size*/)

// Sent by renderer to request a ViewMsg_BeginFrame message for upcoming
// display events. If |enabled| is true, the BeginFrame message will continue
// to be be delivered until the notification is disabled.
IPC_MESSAGE_ROUTED1(ViewHostMsg_SetNeedsBeginFrame,
                    bool /* enabled */)

// Reply to the ViewMsg_ExtractSmartClipData message.
IPC_MESSAGE_ROUTED3(ViewHostMsg_SmartClipDataExtracted,
                    base::string16 /* text */,
                    base::string16 /* html */,
                    gfx::Rect /* rect */)

#elif defined(OS_MACOSX)
// Request that the browser load a font into shared memory for us.
IPC_SYNC_MESSAGE_CONTROL1_3(ViewHostMsg_LoadFont,
                           FontDescriptor /* font to load */,
                           uint32 /* buffer size */,
                           base::SharedMemoryHandle /* font data */,
                           uint32 /* font id */)

// Informs the browser that a plugin has gained or lost focus.
IPC_MESSAGE_ROUTED2(ViewHostMsg_PluginFocusChanged,
                    bool, /* focused */
                    int /* plugin_id */)

// Instructs the browser to start plugin IME.
IPC_MESSAGE_ROUTED0(ViewHostMsg_StartPluginIme)

#elif defined(OS_WIN)
// Request that the given font characters be loaded by the browser so it's
// cached by the OS. Please see RenderMessageFilter::OnPreCacheFontCharacters
// for details.
IPC_SYNC_MESSAGE_CONTROL2_0(ViewHostMsg_PreCacheFontCharacters,
                            LOGFONT /* font_data */,
                            base::string16 /* characters */)
#endif

#if defined(OS_POSIX)
// On POSIX, we cannot allocated shared memory from within the sandbox, so
// this call exists for the renderer to ask the browser to allocate memory
// on its behalf. We return a file descriptor to the POSIX shared memory.
// If the |cache_in_browser| flag is |true|, then a copy of the shmem is kept
// by the browser, and it is the caller's repsonsibility to send a
// ViewHostMsg_FreeTransportDIB message in order to release the cached shmem.
// In all cases, the caller is responsible for deleting the resulting
// TransportDIB.
IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_AllocTransportDIB,
                            uint32_t, /* bytes requested */
                            bool, /* cache in the browser */
                            TransportDIB::Handle /* DIB */)

// Since the browser keeps handles to the allocated transport DIBs, this
// message is sent to tell the browser that it may release them when the
// renderer is finished with them.
IPC_MESSAGE_CONTROL1(ViewHostMsg_FreeTransportDIB,
                     TransportDIB::Id /* DIB id */)
#endif

#if defined(OS_MACOSX) || defined(USE_AURA)
// On Mac and Aura IME can request composition character bounds
// synchronously (see crbug.com/120597). This IPC message sends the character
// bounds after every composition change to always have correct bound info.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ImeCompositionRangeChanged,
                    gfx::Range /* composition range */,
                    std::vector<gfx::Rect> /* character bounds */)
#endif

// Adding a new message? Stick to the sort order above: first platform
// independent ViewMsg, then ifdefs for platform specific ViewMsg, then platform
// independent ViewHostMsg, then ifdefs for platform specific ViewHostMsg.
