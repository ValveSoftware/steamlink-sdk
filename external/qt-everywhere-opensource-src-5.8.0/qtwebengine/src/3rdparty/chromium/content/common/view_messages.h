// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for page rendering.
// Multiply-included message file, hence no include guard.

#include <stddef.h>
#include <stdint.h>

#include "base/memory/shared_memory.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "cc/ipc/cc_param_traits.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/resources/shared_bitmap.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/date_time_suggestion.h"
#include "content/common/frame_replication_state.h"
#include "content/common/navigation_gesture.h"
#include "content/common/resize_params.h"
#include "content/common/text_input_state.h"
#include "content/common/view_message_enums.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/favicon_url.h"
#include "content/public/common/menu_item.h"
#include "content/public/common/page_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/referrer.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/window_container_type.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/base/media_log_event.h"
#include "net/base/network_change_notifier.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationType.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"
#include "third_party/WebKit/public/web/WebMediaPlayerAction.h"
#include "third_party/WebKit/public/web/WebPluginAction.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/WebKit/public/web/WebSharedWorkerCreationContextType.h"
#include "third_party/WebKit/public/web/WebSharedWorkerCreationErrors.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"
#include "ui/gfx/range/range.h"

#if defined(OS_MACOSX)
#include "third_party/WebKit/public/platform/WebScrollbarButtonsPlacement.h"
#include "third_party/WebKit/public/web/mac/WebScrollbarTheme.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ViewMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebDeviceEmulationParams::ScreenPosition,
                          blink::WebDeviceEmulationParams::ScreenPositionLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebMediaPlayerAction::Type,
                          blink::WebMediaPlayerAction::Type::TypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebPluginAction::Type,
                          blink::WebPluginAction::Type::TypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebPopupType,
                          blink::WebPopupType::WebPopupTypeLast)
// TODO(dcheng): Update WebScreenOrientationType to have a "Last" enum member.
IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::WebScreenOrientationType,
                              blink::WebScreenOrientationUndefined,
                              blink::WebScreenOrientationLandscapeSecondary)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebWorkerCreationError,
                          blink::WebWorkerCreationErrorLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebTextDirection,
                          blink::WebTextDirection::WebTextDirectionLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebDisplayMode,
                          blink::WebDisplayMode::WebDisplayModeLast)
IPC_ENUM_TRAITS_MAX_VALUE(WindowContainerType, WINDOW_CONTAINER_TYPE_MAX_VALUE)
IPC_ENUM_TRAITS(content::FaviconURL::IconType)
IPC_ENUM_TRAITS(content::MenuItem::Type)
IPC_ENUM_TRAITS_MAX_VALUE(content::NavigationGesture,
                          content::NavigationGestureLast)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(content::PageZoom,
                              content::PageZoom::PAGE_ZOOM_OUT,
                              content::PageZoom::PAGE_ZOOM_IN)
IPC_ENUM_TRAITS_MAX_VALUE(gfx::FontRenderParams::Hinting,
                          gfx::FontRenderParams::HINTING_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(gfx::FontRenderParams::SubpixelRendering,
                          gfx::FontRenderParams::SUBPIXEL_RENDERING_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(content::TapMultipleTargetsStrategy,
                          content::TAP_MULTIPLE_TARGETS_STRATEGY_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(content::ThreeDAPIType,
                          content::THREE_D_API_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(media::MediaLogEvent::Type,
                          media::MediaLogEvent::TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ui::TextInputMode, ui::TEXT_INPUT_MODE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(ui::TextInputType, ui::TEXT_INPUT_TYPE_MAX)

#if defined(OS_MACOSX)
IPC_ENUM_TRAITS_MAX_VALUE(
    blink::WebScrollbarButtonsPlacement,
    blink::WebScrollbarButtonsPlacement::WebScrollbarButtonsPlacementLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::ScrollerStyle, blink::ScrollerStyleOverlay)
#endif

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

IPC_STRUCT_TRAITS_BEGIN(blink::WebSize)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebDeviceEmulationParams)
  IPC_STRUCT_TRAITS_MEMBER(screenPosition)
  IPC_STRUCT_TRAITS_MEMBER(screenSize)
  IPC_STRUCT_TRAITS_MEMBER(viewPosition)
  IPC_STRUCT_TRAITS_MEMBER(deviceScaleFactor)
  IPC_STRUCT_TRAITS_MEMBER(viewSize)
  IPC_STRUCT_TRAITS_MEMBER(fitToView)
  IPC_STRUCT_TRAITS_MEMBER(offset)
  IPC_STRUCT_TRAITS_MEMBER(scale)
  IPC_STRUCT_TRAITS_MEMBER(screenOrientationAngle)
  IPC_STRUCT_TRAITS_MEMBER(screenOrientationType)
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

IPC_STRUCT_TRAITS_BEGIN(content::ResizeParams)
  IPC_STRUCT_TRAITS_MEMBER(screen_info)
  IPC_STRUCT_TRAITS_MEMBER(new_size)
  IPC_STRUCT_TRAITS_MEMBER(physical_backing_size)
  IPC_STRUCT_TRAITS_MEMBER(top_controls_shrink_blink_size)
  IPC_STRUCT_TRAITS_MEMBER(top_controls_height)
  IPC_STRUCT_TRAITS_MEMBER(visible_viewport_size)
  IPC_STRUCT_TRAITS_MEMBER(resizer_rect)
  IPC_STRUCT_TRAITS_MEMBER(is_fullscreen_granted)
  IPC_STRUCT_TRAITS_MEMBER(display_mode)
  IPC_STRUCT_TRAITS_MEMBER(needs_resize_ack)
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
  IPC_STRUCT_TRAITS_MEMBER(browser_handles_all_top_level_requests)
  IPC_STRUCT_TRAITS_MEMBER(caret_blink_interval)
  IPC_STRUCT_TRAITS_MEMBER(use_custom_colors)
  IPC_STRUCT_TRAITS_MEMBER(enable_referrers)
  IPC_STRUCT_TRAITS_MEMBER(enable_do_not_track)
  IPC_STRUCT_TRAITS_MEMBER(webrtc_ip_handling_policy)
  IPC_STRUCT_TRAITS_MEMBER(user_agent_override)
  IPC_STRUCT_TRAITS_MEMBER(accept_languages)
  IPC_STRUCT_TRAITS_MEMBER(report_frame_name_changes)
  IPC_STRUCT_TRAITS_MEMBER(tap_multiple_targets_strategy)
  IPC_STRUCT_TRAITS_MEMBER(disable_client_blocked_error_page)
  IPC_STRUCT_TRAITS_MEMBER(plugin_fullscreen_allowed)
  IPC_STRUCT_TRAITS_MEMBER(use_video_overlay_for_embedded_encrypted_video)
  IPC_STRUCT_TRAITS_MEMBER(use_view_overlay_for_all_video)
  IPC_STRUCT_TRAITS_MEMBER(network_contry_iso)
#if defined(OS_WIN)
  IPC_STRUCT_TRAITS_MEMBER(caption_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(caption_font_height)
  IPC_STRUCT_TRAITS_MEMBER(small_caption_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(small_caption_font_height)
  IPC_STRUCT_TRAITS_MEMBER(menu_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(menu_font_height)
  IPC_STRUCT_TRAITS_MEMBER(status_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(status_font_height)
  IPC_STRUCT_TRAITS_MEMBER(message_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(message_font_height)
  IPC_STRUCT_TRAITS_MEMBER(vertical_scroll_bar_width_in_dips)
  IPC_STRUCT_TRAITS_MEMBER(horizontal_scroll_bar_height_in_dips)
  IPC_STRUCT_TRAITS_MEMBER(arrow_bitmap_height_vertical_scroll_bar_in_dips)
  IPC_STRUCT_TRAITS_MEMBER(arrow_bitmap_width_horizontal_scroll_bar_in_dips)
#endif
  IPC_STRUCT_TRAITS_MEMBER(default_font_size)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::MediaLogEvent)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(params)
  IPC_STRUCT_TRAITS_MEMBER(time)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::TextInputState)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(mode)
  IPC_STRUCT_TRAITS_MEMBER(flags)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(selection_start)
  IPC_STRUCT_TRAITS_MEMBER(selection_end)
  IPC_STRUCT_TRAITS_MEMBER(composition_start)
  IPC_STRUCT_TRAITS_MEMBER(composition_end)
  IPC_STRUCT_TRAITS_MEMBER(can_compose_inline)
  IPC_STRUCT_TRAITS_MEMBER(show_ime_if_needed)
  IPC_STRUCT_TRAITS_MEMBER(is_non_ime_change)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(ViewHostMsg_CreateWindow_Params)
  // Routing ID of the view initiating the open.
  IPC_STRUCT_MEMBER(int, opener_id)

  // True if this open request came in the context of a user gesture.
  IPC_STRUCT_MEMBER(bool, user_gesture)

  // Type of window requested.
  IPC_STRUCT_MEMBER(WindowContainerType, window_container_type)

  // The session storage namespace ID this view should use.
  IPC_STRUCT_MEMBER(int64_t, session_storage_namespace_id)

  // The name of the resulting frame that should be created (empty if none
  // has been specified). UTF8 encoded string.
  IPC_STRUCT_MEMBER(std::string, frame_name)

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

IPC_STRUCT_BEGIN(ViewHostMsg_CreateWindow_Reply)
  // The ID of the view to be created. If the ID is MSG_ROUTING_NONE, then the
  // view couldn't be created.
  IPC_STRUCT_MEMBER(int32_t, route_id, MSG_ROUTING_NONE)

  // The ID of the main frame hosted in the view.
  IPC_STRUCT_MEMBER(int32_t, main_frame_route_id, MSG_ROUTING_NONE)

  // The ID of the widget for the main frame.
  IPC_STRUCT_MEMBER(int32_t, main_frame_widget_route_id, MSG_ROUTING_NONE)

  // TODO(dcheng): No clue. This is kind of duplicated from ViewMsg_New_Params.
  IPC_STRUCT_MEMBER(int64_t, cloned_session_storage_namespace_id)
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

  // Address space of the context that created the worker.
  IPC_STRUCT_MEMBER(blink::WebAddressSpace, creation_address_space)

  // The type (secure or nonsecure) of the context that created the worker.
  IPC_STRUCT_MEMBER(blink::WebSharedWorkerCreationContextType,
                    creation_context_type)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_CreateWorker_Reply)
  // The route id for the created worker.
  IPC_STRUCT_MEMBER(int, route_id)

  // The error that occurred, if the browser failed to create the
  // worker.
  IPC_STRUCT_MEMBER(blink::WebWorkerCreationError, error)
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

IPC_STRUCT_BEGIN(ViewHostMsg_UpdateRect_Params)
  // The size of the RenderView when this message was generated.  This is
  // included so the host knows how large the view is from the perspective of
  // the renderer process.  This is necessary in case a resize operation is in
  // progress. If auto-resize is enabled, this should update the corresponding
  // view size.
  IPC_STRUCT_MEMBER(gfx::Size, view_size)

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
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewMsg_New_Params)
  // Renderer-wide preferences.
  IPC_STRUCT_MEMBER(content::RendererPreferences, renderer_preferences)

  // Preferences for this view.
  IPC_STRUCT_MEMBER(content::WebPreferences, web_preferences)

  // The ID of the view to be created.
  IPC_STRUCT_MEMBER(int32_t, view_id, MSG_ROUTING_NONE)

  // The ID of the main frame hosted in the view.
  IPC_STRUCT_MEMBER(int32_t, main_frame_routing_id, MSG_ROUTING_NONE)

  // The ID of the widget for the main frame.
  IPC_STRUCT_MEMBER(int32_t, main_frame_widget_routing_id, MSG_ROUTING_NONE)

  // The session storage namespace ID this view should use.
  IPC_STRUCT_MEMBER(int64_t, session_storage_namespace_id)

  // The route ID of the opener RenderFrame or RenderFrameProxy, if we need to
  // set one (MSG_ROUTING_NONE otherwise).
  IPC_STRUCT_MEMBER(int, opener_frame_route_id, MSG_ROUTING_NONE)

  // Whether the RenderView should initially be swapped out.
  IPC_STRUCT_MEMBER(bool, swapped_out)

  // Carries replicated information, such as frame name and sandbox flags, for
  // this view's main frame, which will be a proxy in |swapped_out|
  // views when in --site-per-process mode, or a RenderFrame in all other
  // cases.
  IPC_STRUCT_MEMBER(content::FrameReplicationState, replicated_frame_state)

  // The ID of the proxy object for the main frame in this view. It is only
  // used if |swapped_out| is true.
  IPC_STRUCT_MEMBER(int32_t, proxy_routing_id, MSG_ROUTING_NONE)

  // Whether the RenderView should initially be hidden.
  IPC_STRUCT_MEMBER(bool, hidden)

  // Whether the RenderView will never be visible.
  IPC_STRUCT_MEMBER(bool, never_visible)

  // Whether the window associated with this view was created with an opener.
  IPC_STRUCT_MEMBER(bool, window_was_created_with_opener)

  // The initial page ID to use for this view, which must be larger than any
  // existing navigation that might be loaded in the view.  Page IDs are unique
  // to a view and are only updated by the renderer after this initial value.
  IPC_STRUCT_MEMBER(int32_t, next_page_id)

  // The initial renderer size.
  IPC_STRUCT_MEMBER(content::ResizeParams, initial_size)

  // Whether to enable auto-resize.
  IPC_STRUCT_MEMBER(bool, enable_auto_resize)

  // The minimum size to layout the page if auto-resize is enabled.
  IPC_STRUCT_MEMBER(gfx::Size, min_size)

  // The maximum size to layout the page if auto-resize is enabled.
  IPC_STRUCT_MEMBER(gfx::Size, max_size)

  // The page zoom level.
  IPC_STRUCT_MEMBER(double, page_zoom_level)

  // The color profile to use for image decode.
  IPC_STRUCT_MEMBER(std::vector<char>, image_decode_color_profile)
IPC_STRUCT_END()

#if defined(OS_MACOSX)
IPC_STRUCT_BEGIN(ViewMsg_UpdateScrollbarTheme_Params)
  IPC_STRUCT_MEMBER(float, initial_button_delay)
  IPC_STRUCT_MEMBER(float, autoscroll_button_delay)
  IPC_STRUCT_MEMBER(bool, jump_on_track_click)
  IPC_STRUCT_MEMBER(blink::ScrollerStyle, preferred_scroller_style)
  IPC_STRUCT_MEMBER(bool, redraw)
  IPC_STRUCT_MEMBER(blink::WebScrollbarButtonsPlacement, button_placement)
IPC_STRUCT_END()
#endif

// Messages sent from the browser to the renderer.

#if defined(OS_ANDROID)
// Tells the renderer to cancel an opened date/time dialog.
IPC_MESSAGE_ROUTED0(ViewMsg_CancelDateTimeDialog)

// Replaces a date time input field.
IPC_MESSAGE_ROUTED1(ViewMsg_ReplaceDateTime,
                    double /* dialog_value */)

#endif

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

// Sent when the history is altered outside of navigation. The history list was
// reset to |history_length| length, and the offset was reset to be
// |history_offset|.
IPC_MESSAGE_ROUTED2(ViewMsg_SetHistoryOffsetAndLength,
                    int /* history_offset */,
                    int /* history_length */)

// Tells the renderer to create a new view.
// This message is slightly different, the view it takes (via
// ViewMsg_New_Params) is the view to create, the message itself is sent as a
// non-view control message.
IPC_MESSAGE_CONTROL1(ViewMsg_New,
                     ViewMsg_New_Params)

// Sends updated preferences to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_SetRendererPrefs,
                    content::RendererPreferences)

// This passes a set of webkit preferences down to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_UpdateWebPreferences,
                    content::WebPreferences)

// Informs the renderer that the timezone has changed along with a new
// timezone ID.
IPC_MESSAGE_CONTROL1(ViewMsg_TimezoneChange, std::string)

// Tells the render view to close.
// Expects a Close_ACK message when finished.
IPC_MESSAGE_ROUTED0(ViewMsg_Close)

// Tells the render view to change its size.  A ViewHostMsg_UpdateRect message
// is generated in response provided new_size is not empty and not equal to
// the view's current size.  The generated ViewHostMsg_UpdateRect message will
// have the IS_RESIZE_ACK flag set. It also receives the resizer rect so that
// we don't have to fetch it every time WebKit asks for it.
IPC_MESSAGE_ROUTED1(ViewMsg_Resize, content::ResizeParams /* params */)

// Enables device emulation. See WebDeviceEmulationParams for description.
IPC_MESSAGE_ROUTED1(ViewMsg_EnableDeviceEmulation,
                    blink::WebDeviceEmulationParams /* params */)

// Disables device emulation, enabled previously by EnableDeviceEmulation.
IPC_MESSAGE_ROUTED0(ViewMsg_DisableDeviceEmulation)

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
IPC_MESSAGE_ROUTED2(ViewMsg_WasShown,
                    bool /* needs_repainting */,
                    ui::LatencyInfo /* latency_info */)

// Tells the renderer to focus the first (last if reverse is true) focusable
// node.
IPC_MESSAGE_ROUTED1(ViewMsg_SetInitialFocus,
                    bool /* reverse */)

// Sent to inform the renderer to invoke a context menu.
// The parameter specifies the location in the render view's coordinates.
IPC_MESSAGE_ROUTED2(ViewMsg_ShowContextMenu,
                    ui::MenuSourceType,
                    gfx::Point /* location where menu should be shown */)

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

// Sets the page scale for the current main frame to the given page scale.
IPC_MESSAGE_ROUTED1(ViewMsg_SetPageScale, float /* page_scale_factor */)

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

// Used to notify the render-view that we have received a target URL. Used
// to prevent target URLs spamming the browser.
IPC_MESSAGE_ROUTED0(ViewMsg_UpdateTargetURL_ACK)

// Provides the results of directory enumeration.
IPC_MESSAGE_ROUTED2(ViewMsg_EnumerateDirectoryResponse,
                    int /* request_id */,
                    std::vector<base::FilePath> /* files_in_directory */)

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
IPC_MESSAGE_CONTROL2(ViewMsg_NetworkConnectionChanged,
                     net::NetworkChangeNotifier::ConnectionType /* type */,
                     double /* max bandwidth mbps */)

// Sent by the browser to synchronize with the next compositor frame. Used only
// for tests.
IPC_MESSAGE_ROUTED1(ViewMsg_WaitForNextFrameForTests, int /* routing_id */)

#if defined(ENABLE_PLUGINS)
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
#endif

// An acknowledge to ViewHostMsg_MultipleTargetsTouched to notify the renderer
// process to release the magnified image.
IPC_MESSAGE_ROUTED1(ViewMsg_ReleaseDisambiguationPopupBitmap,
                    cc::SharedBitmapId /* id */)

// Fetches complete rendered content of a web page as plain text.
IPC_MESSAGE_ROUTED0(ViewMsg_GetRenderedText)

#if defined(OS_MACOSX)
// Notification of a change in scrollbar appearance and/or behavior.
IPC_MESSAGE_CONTROL1(ViewMsg_UpdateScrollbarTheme,
                     ViewMsg_UpdateScrollbarTheme_Params /* params */)

// Notification that the OS X Aqua color preferences changed.
IPC_MESSAGE_CONTROL3(ViewMsg_SystemColorsChanged,
                     int /* AppleAquaColorVariant */,
                     std::string /* AppleHighlightedTextColor */,
                     std::string /* AppleHighlightColor */)
#endif

#if defined(OS_ANDROID)
// Tells the renderer to suspend/resume the webkit timers.
IPC_MESSAGE_CONTROL1(ViewMsg_SetWebKitSharedTimersSuspended,
                     bool /* suspend */)

// Notifies the renderer whether hiding/showing the top controls is enabled
// and whether or not to animate to the proper state.
IPC_MESSAGE_ROUTED3(ViewMsg_UpdateTopControlsState,
                    bool /* enable_hiding */,
                    bool /* enable_showing */,
                    bool /* animate */)

IPC_MESSAGE_ROUTED0(ViewMsg_ShowImeIfNeeded)

// Extracts the data at the given rect, returning it through the
// ViewHostMsg_SmartClipDataExtracted IPC.
IPC_MESSAGE_ROUTED1(ViewMsg_ExtractSmartClipData,
                    gfx::Rect /* rect */)
#endif

// Sent by the browser as a reply to ViewHostMsg_SwapCompositorFrame.
IPC_MESSAGE_ROUTED2(ViewMsg_SwapCompositorFrameAck,
                    uint32_t /* output_surface_id */,
                    cc::CompositorFrameAck /* ack */)

// Sent by browser to tell renderer compositor that some resources that were
// given to the browser in a swap are not being used anymore.
IPC_MESSAGE_ROUTED2(ViewMsg_ReclaimCompositorResources,
                    uint32_t /* output_surface_id */,
                    cc::CompositorFrameAck /* ack */)

// Sent by browser to give renderer compositor a new namespace ID for any
// SurfaceSequences it has to create.
IPC_MESSAGE_ROUTED1(ViewMsg_SetSurfaceIdNamespace,
                    uint32_t /* surface_id_namespace */)

IPC_MESSAGE_ROUTED0(ViewMsg_SelectWordAroundCaret)

// Sent by the browser to ask the renderer to redraw.
// If |request_id| is not zero, it is added to the forced frame's latency info
// as ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT.
IPC_MESSAGE_ROUTED1(ViewMsg_ForceRedraw,
                    int /* request_id */)

// Let renderer know begin frame messages won't be sent even if requested.
IPC_MESSAGE_ROUTED1(ViewMsg_SetBeginFramePaused, bool /* paused */)

// Sent by the browser when the renderer should generate a new frame.
IPC_MESSAGE_ROUTED1(ViewMsg_BeginFrame,
                    cc::BeginFrameArgs /* args */)

// Sent by the browser to deliver a compositor proto to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_HandleCompositorProto,
                    std::vector<uint8_t> /* proto */)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Sent by renderer to request a ViewMsg_BeginFrame message for upcoming
// display events. If |enabled| is true, the BeginFrame message will continue
// to be be delivered until the notification is disabled.
IPC_MESSAGE_ROUTED1(ViewHostMsg_SetNeedsBeginFrames,
                    bool /* enabled */)

// Sent by the renderer when it is creating a new window.  The browser creates a
// tab for it.  If |reply.route_id| is MSG_ROUTING_NONE, the view couldn't be
// created.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_CreateWindow,
                            ViewHostMsg_CreateWindow_Params,
                            ViewHostMsg_CreateWindow_Reply)

// Similar to ViewHostMsg_CreateWindow, except used for sub-widgets, like
// <select> dropdowns.  This message is sent to the WebContentsImpl that
// contains the widget being created.
IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_CreateWidget,
                            int /* opener_id */,
                            blink::WebPopupType /* popup type */,
                            int /* route_id */)

// Similar to ViewHostMsg_CreateWidget except the widget is a full screen
// window.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_CreateFullscreenWidget,
                            int /* opener_id */,
                            int /* route_id */)

// Asks the browser for a unique routing ID.
IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GenerateRoutingID,
                            int /* routing_id */)

// Asks the browser for the default audio hardware configuration.
IPC_SYNC_MESSAGE_CONTROL0_2(ViewHostMsg_GetAudioHardwareConfig,
                            media::AudioParameters /* input parameters */,
                            media::AudioParameters /* output parameters */)

// These three messages are sent to the parent RenderViewHost to display the
// page/widget that was created by
// CreateWindow/CreateWidget/CreateFullscreenWidget. routing_id
// refers to the id that was returned from the Create message above.
// The initial_rect parameter is in screen coordinates.
//
// FUTURE: there will probably be flags here to control if the result is
// in a new window.
IPC_MESSAGE_ROUTED4(ViewHostMsg_ShowView,
                    int /* route_id */,
                    WindowOpenDisposition /* disposition */,
                    gfx::Rect /* initial_rect */,
                    bool /* opened_by_user_gesture */)

IPC_MESSAGE_ROUTED2(ViewHostMsg_ShowWidget,
                    int /* route_id */,
                    gfx::Rect /* initial_rect */)

// Message to show a full screen widget.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowFullscreenWidget,
                    int /* route_id */)

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

// Indicates that the render view has been closed in respose to a
// Close message.
IPC_MESSAGE_CONTROL1(ViewHostMsg_Close_ACK,
                     int /* old_route_id */)

// Indicates that the current page has been closed, after a ClosePage
// message.
IPC_MESSAGE_ROUTED0(ViewHostMsg_ClosePage_ACK)

// Notifies the browser that we have session history information.
// page_id: unique ID that allows us to distinguish between history entries.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateState,
                    int32_t /* page_id */,
                    content::PageState /* state */)

// Notifies the browser that we want to show a destination url for a potential
// action (e.g. when the user is hovering over a link).
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateTargetURL,
                    GURL)

// Sent when the document element is available for the top-level frame.  This
// happens after the page starts loading, but before all resources are
// finished.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DocumentAvailableInMainFrame,
                    bool /* uses_temporary_zoom_level */)

// Sent to update part of the view.  In response to this message, the host
// generates a ViewMsg_UpdateRect_ACK message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateRect,
                    ViewHostMsg_UpdateRect_Params)

IPC_MESSAGE_ROUTED0(ViewHostMsg_Focus)

// Message sent from renderer to the browser when focus changes inside the
// webpage. The first parameter says whether the newly focused element needs
// keyboard input (true for textfields, text areas and content editable divs).
// The second parameter is the node bounds relative to RenderWidgetHostView.
IPC_MESSAGE_ROUTED2(ViewHostMsg_FocusedNodeChanged,
                    bool /* is_editable_node */,
                    gfx::Rect /* node_bounds */)

IPC_MESSAGE_ROUTED1(ViewHostMsg_SetCursor, content::WebCursor)

// Get the list of proxies to use for |url|, as a semicolon delimited list
// of "<TYPE> <HOST>:<PORT>" | "DIRECT".
IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_ResolveProxy,
                            GURL /* url */,
                            bool /* result */,
                            std::string /* proxy list */)

// A renderer sends this to the browser process when it wants to create a
// worker.  The browser will create the worker process if necessary, and
// will return the route id on in the reply on success.  On error returns
// MSG_ROUTING_NONE and an error type.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_CreateWorker,
                            ViewHostMsg_CreateWorker_Params,
                            ViewHostMsg_CreateWorker_Reply)

// A renderer sends this to the browser process when a document has been
// detached. The browser will use this to constrain the lifecycle of worker
// processes (SharedWorkers are shut down when their last associated document
// is detached).
IPC_MESSAGE_CONTROL1(ViewHostMsg_DocumentDetached, uint64_t /* document_id */)

// Wraps an IPC message that's destined to the worker on the renderer->browser
// hop.
IPC_MESSAGE_CONTROL1(ViewHostMsg_ForwardToWorker,
                     IPC::Message /* message */)

// Tells the browser that a specific Appcache manifest in the current page
// was accessed.
IPC_MESSAGE_ROUTED2(ViewHostMsg_AppCacheAccessed,
                    GURL /* manifest url */,
                    bool /* blocked by policy */)

// Used to go to the session history entry at the given offset (ie, -1 will
// return the "back" item).
IPC_MESSAGE_ROUTED1(ViewHostMsg_GoToEntryAtOffset,
                    int /* offset (from current) of history item to get */)

// Sent from an inactive renderer for the browser to route to the active
// renderer, instructing it to close.
IPC_MESSAGE_ROUTED0(ViewHostMsg_RouteCloseEvent)

// Notifies that the preferred size of the content changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidContentsPreferredSizeChange,
                    gfx::Size /* pref_size */)

// Notifies whether there are JavaScript touch event handlers or not.
IPC_MESSAGE_ROUTED1(ViewHostMsg_HasTouchEventHandlers,
                    bool /* has_handlers */)

// A message from HTML-based UI.  When (trusted) Javascript calls
// send(message, args), this message is sent to the browser.
IPC_MESSAGE_ROUTED3(ViewHostMsg_WebUISend,
                    GURL /* source_url */,
                    std::string  /* message */,
                    base::ListValue /* args */)

#if defined(ENABLE_PLUGINS)
// A renderer sends this to the browser process when it wants to access a PPAPI
// broker. In contrast to FrameHostMsg_OpenChannelToPpapiBroker, this is called
// for every connection.
// The browser will respond with ViewMsg_PpapiBrokerPermissionResult.
IPC_MESSAGE_ROUTED3(ViewHostMsg_RequestPpapiBrokerPermission,
                    int /* routing_id */,
                    GURL /* document_url */,
                    base::FilePath /* plugin_path */)
#endif  // defined(ENABLE_PLUGINS)

// Send the tooltip text for the current mouse position to the browser.
IPC_MESSAGE_ROUTED2(ViewHostMsg_SetTooltipText,
                    base::string16 /* tooltip text string */,
                    blink::WebTextDirection /* text direction hint */)

// Notification that the text selection has changed.
// Note: The secound parameter is the character based offset of the
// base::string16
// text in the document.
IPC_MESSAGE_ROUTED3(ViewHostMsg_SelectionChanged,
                    base::string16 /* text covers the selection range */,
                    uint32_t /* the offset of the text in the document */,
                    gfx::Range /* selection range in the document */)

// Notification that the selection bounds have changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_SelectionBoundsChanged,
                    ViewHostMsg_SelectionBounds_Params)

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
                    content::TextInputState /* text_input_state */)

// Sent when the renderer changes the zoom level for a particular url, so the
// browser can update its records.  If the view is a plugin doc, then url is
// used to update the zoom level for all pages in that site.  Otherwise, the
// render view's id is used so that only the menu is updated.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DidZoomURL,
                    double /* zoom_level */,
                    GURL /* url */)

// Sent when the renderer changes its page scale factor.
IPC_MESSAGE_ROUTED1(ViewHostMsg_PageScaleFactorChanged,
                    float /* page_scale_factor */)

// Updates the minimum/maximum allowed zoom percent for this tab from the
// default values.  If |remember| is true, then the zoom setting is applied to
// other pages in the site and is saved, otherwise it only applies to this
// tab.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateZoomLimits,
                    int /* minimum_percent */,
                    int /* maximum_percent */)

IPC_MESSAGE_ROUTED3(
    ViewHostMsg_SwapCompositorFrame,
    uint32_t /* output_surface_id */,
    cc::CompositorFrame /* frame */,
    std::vector<IPC::Message> /* messages_to_deliver_with_frame */)

// Send back a string to be recorded by UserMetrics.
IPC_MESSAGE_CONTROL1(ViewHostMsg_UserMetricsRecordAction,
                     std::string /* action */)

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

// Notifies the browser that document has parsed the body. This is used by the
// ResourceScheduler as an indication that bandwidth contention won't block
// first paint.
IPC_MESSAGE_ROUTED0(ViewHostMsg_WillInsertBody)

// Notification that the urls for the favicon of a site has been determined.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateFaviconURL,
                    std::vector<content::FaviconURL> /* candidates */)

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

// Sent once a paint happens after the first non empty layout. In other words,
// after the frame widget has painted something.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidFirstVisuallyNonEmptyPaint)

// Send after a paint happens after any page commit, including a blank one.
// TODO(kenrb): This, and all ViewHostMsg_* messages that actually pertain to
// RenderWidget(Host), should be renamed to WidgetHostMsg_*.
// See https://crbug.com/537793.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidFirstPaintAfterLoad)

// Sent by the renderer to deliver a compositor proto to the browser.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ForwardCompositorProto,
                    std::vector<uint8_t> /* proto */)

// Sent in reply to ViewMsg_WaitForNextFrameForTests.
IPC_MESSAGE_ROUTED0(ViewHostMsg_WaitForNextFrameForTests_ACK)

#if defined(OS_ANDROID)
// Start an android intent with the given URI.
IPC_MESSAGE_ROUTED2(ViewHostMsg_StartContentIntent,
                    GURL /* content_url */,
                    bool /* is_main_frame */)

// Reply to the ViewMsg_ExtractSmartClipData message.
IPC_MESSAGE_ROUTED3(ViewHostMsg_SmartClipDataExtracted,
                    base::string16 /* text */,
                    base::string16 /* html */,
                    gfx::Rect /* rect */)

// Notifies that an unhandled tap has occurred at the specified x,y position
// and that the UI may need to be triggered.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ShowUnhandledTapUIIfNeeded,
                    int /* x */,
                    int /* y */)

#elif defined(OS_MACOSX)
// Receives content of a web page as plain text.
IPC_MESSAGE_ROUTED1(ViewMsg_GetRenderedTextCompleted, std::string)
#endif

// Adding a new message? Stick to the sort order above: first platform
// independent ViewMsg, then ifdefs for platform specific ViewMsg, then platform
// independent ViewHostMsg, then ifdefs for platform specific ViewHostMsg.
