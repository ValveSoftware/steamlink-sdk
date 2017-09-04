// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_core_impl.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/output/begin_frame_args.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/android/gesture_event_type.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"
#include "content/browser/android/load_url_params.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/input/web_input_event_builders_android.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/screen_orientation_dispatcher_host.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/menu_item.h"
#include "content/public/common/user_agent.h"
#include "device/geolocation/geolocation_service_context.h"
#include "jni/ContentViewCore_jni.h"
#include "jni/DragEvent_jni.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using ui::MotionEventAndroid;

namespace content {

namespace {

// Describes the type and enabled state of a select popup item.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.browser.input
enum PopupItemType {
  // Popup item is of type group
  POPUP_ITEM_TYPE_GROUP,

  // Popup item is disabled
  POPUP_ITEM_TYPE_DISABLED,

  // Popup item is enabled
  POPUP_ITEM_TYPE_ENABLED,
};

const void* const kContentViewUserDataKey = &kContentViewUserDataKey;

int GetRenderProcessIdFromRenderViewHost(RenderViewHost* host) {
  DCHECK(host);
  RenderProcessHost* render_process = host->GetProcess();
  DCHECK(render_process);
  if (render_process->HasConnection())
    return render_process->GetHandle();
  return 0;
}

ScopedJavaLocalRef<jobject> CreateJavaRect(
    JNIEnv* env,
    const gfx::Rect& rect) {
  return ScopedJavaLocalRef<jobject>(
      Java_ContentViewCore_createRect(env,
                                      static_cast<int>(rect.x()),
                                      static_cast<int>(rect.y()),
                                      static_cast<int>(rect.right()),
                                      static_cast<int>(rect.bottom())));
}

int ToGestureEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::GestureScrollBegin:
      return GESTURE_EVENT_TYPE_SCROLL_START;
    case WebInputEvent::GestureScrollEnd:
      return GESTURE_EVENT_TYPE_SCROLL_END;
    case WebInputEvent::GestureScrollUpdate:
      return GESTURE_EVENT_TYPE_SCROLL_BY;
    case WebInputEvent::GestureFlingStart:
      return GESTURE_EVENT_TYPE_FLING_START;
    case WebInputEvent::GestureFlingCancel:
      return GESTURE_EVENT_TYPE_FLING_CANCEL;
    case WebInputEvent::GestureShowPress:
      return GESTURE_EVENT_TYPE_SHOW_PRESS;
    case WebInputEvent::GestureTap:
      return GESTURE_EVENT_TYPE_SINGLE_TAP_CONFIRMED;
    case WebInputEvent::GestureTapUnconfirmed:
      return GESTURE_EVENT_TYPE_SINGLE_TAP_UNCONFIRMED;
    case WebInputEvent::GestureTapDown:
      return GESTURE_EVENT_TYPE_TAP_DOWN;
    case WebInputEvent::GestureTapCancel:
      return GESTURE_EVENT_TYPE_TAP_CANCEL;
    case WebInputEvent::GestureDoubleTap:
      return GESTURE_EVENT_TYPE_DOUBLE_TAP;
    case WebInputEvent::GestureLongPress:
      return GESTURE_EVENT_TYPE_LONG_PRESS;
    case WebInputEvent::GestureLongTap:
      return GESTURE_EVENT_TYPE_LONG_TAP;
    case WebInputEvent::GesturePinchBegin:
      return GESTURE_EVENT_TYPE_PINCH_BEGIN;
    case WebInputEvent::GesturePinchEnd:
      return GESTURE_EVENT_TYPE_PINCH_END;
    case WebInputEvent::GesturePinchUpdate:
      return GESTURE_EVENT_TYPE_PINCH_BY;
    case WebInputEvent::GestureTwoFingerTap:
    default:
      NOTREACHED() << "Invalid source gesture type: "
                   << WebInputEvent::GetName(type);
      return -1;
  }
}

void RecordToolTypeForActionDown(MotionEventAndroid& event) {
  MotionEventAndroid::Action action = event.GetAction();
  if (action == MotionEventAndroid::ACTION_DOWN ||
      action == MotionEventAndroid::ACTION_POINTER_DOWN ||
      action == MotionEventAndroid::ACTION_BUTTON_PRESS) {
    UMA_HISTOGRAM_ENUMERATION("Event.AndroidActionDown.ToolType",
                              event.GetToolType(0),
                              MotionEventAndroid::TOOL_TYPE_LAST + 1);
  }
}

}  // namespace

// Enables a callback when the underlying WebContents is destroyed, to enable
// nulling the back-pointer.
class ContentViewCoreImpl::ContentViewUserData
    : public base::SupportsUserData::Data {
 public:
  explicit ContentViewUserData(ContentViewCoreImpl* content_view_core)
      : content_view_core_(content_view_core) {
  }

  ~ContentViewUserData() override {
    // TODO(joth): When chrome has finished removing the TabContents class (see
    // crbug.com/107201) consider inverting relationship, so ContentViewCore
    // would own WebContents. That effectively implies making the WebContents
    // destructor private on Android.
    delete content_view_core_;
  }

  ContentViewCoreImpl* get() const { return content_view_core_; }

 private:
  // Not using scoped_ptr as ContentViewCoreImpl destructor is private.
  ContentViewCoreImpl* content_view_core_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentViewUserData);
};

// static
ContentViewCoreImpl* ContentViewCoreImpl::FromWebContents(
    content::WebContents* web_contents) {
  ContentViewCoreImpl::ContentViewUserData* data =
      static_cast<ContentViewCoreImpl::ContentViewUserData*>(
          web_contents->GetUserData(kContentViewUserDataKey));
  return data ? data->get() : NULL;
}

// static
ContentViewCore* ContentViewCore::FromWebContents(
    content::WebContents* web_contents) {
  return ContentViewCoreImpl::FromWebContents(web_contents);
}

ContentViewCoreImpl::ContentViewCoreImpl(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    WebContents* web_contents,
    float dpi_scale,
    const JavaRef<jobject>& java_bridge_retained_object_set)
    : WebContentsObserver(web_contents),
      java_ref_(env, obj),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)),
      page_scale_(1),
      dpi_scale_(dpi_scale),
      device_orientation_(0),
      accessibility_enabled_(false) {
  GetViewAndroid()->SetLayer(cc::Layer::Create());
  gfx::Size physical_size(
      Java_ContentViewCore_getPhysicalBackingWidthPix(env, obj),
      Java_ContentViewCore_getPhysicalBackingHeightPix(env, obj));
  GetViewAndroid()->GetLayer()->SetBounds(physical_size);

  // Currently, the only use case we have for overriding a user agent involves
  // spoofing a desktop Linux user agent for "Request desktop site".
  // Automatically set it for all WebContents so that it is available when a
  // NavigationEntry requires the user agent to be overridden.
  const char kLinuxInfoStr[] = "X11; Linux x86_64";
  std::string product = content::GetContentClient()->GetProduct();
  std::string spoofed_ua =
      BuildUserAgentFromOSAndProduct(kLinuxInfoStr, product);
  web_contents->SetUserAgentOverride(spoofed_ua);

  java_bridge_dispatcher_host_ =
      new GinJavaBridgeDispatcherHost(web_contents,
                                      java_bridge_retained_object_set);

  InitWebContents();
}

void ContentViewCoreImpl::AddObserver(
    ContentViewCoreImplObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ContentViewCoreImpl::RemoveObserver(
    ContentViewCoreImplObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

ContentViewCoreImpl::~ContentViewCoreImpl() {
  for (auto& observer : observer_list_)
    observer.OnContentViewCoreDestroyed();
  observer_list_.Clear();

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  java_ref_.reset();
  if (!j_obj.is_null()) {
    Java_ContentViewCore_onNativeContentViewCoreDestroyed(
        env, j_obj, reinterpret_cast<intptr_t>(this));
  }
}

void ContentViewCoreImpl::UpdateWindowAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong window_android) {
  ui::ViewAndroid* view = GetViewAndroid();
  ui::WindowAndroid* window =
      reinterpret_cast<ui::WindowAndroid*>(window_android);
  if (window == GetWindowAndroid())
    return;
  if (GetWindowAndroid()) {
    for (auto& observer : observer_list_)
      observer.OnDetachedFromWindow();
    view->RemoveFromParent();
  }
  if (window) {
    window->AddChild(view);
    for (auto& observer : observer_list_)
      observer.OnAttachedToWindow();
  }
}

base::android::ScopedJavaLocalRef<jobject>
ContentViewCoreImpl::GetWebContentsAndroid(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  return web_contents_->GetJavaWebContents();
}

base::android::ScopedJavaLocalRef<jobject>
ContentViewCoreImpl::GetJavaWindowAndroid(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  if (!GetWindowAndroid())
    return ScopedJavaLocalRef<jobject>();
  return GetWindowAndroid()->GetJavaObject();
}

void ContentViewCoreImpl::OnJavaContentViewCoreDestroyed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK(env->IsSameObject(java_ref_.get(env).obj(), obj));
  java_ref_.reset();
  // Java peer has gone, ContentViewCore is not functional and waits to
  // be destroyed with WebContents.
  // We need to reset WebContentsViewAndroid's reference, otherwise, there
  // could have call in when swapping the WebContents,
  // see http://crbug.com/383939 .
  DCHECK(web_contents_);
  static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(web_contents_)->GetView())->
          SetContentViewCore(NULL);
}

void ContentViewCoreImpl::InitWebContents() {
  DCHECK(web_contents_);
  static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(web_contents_)->GetView())->
          SetContentViewCore(this);
  DCHECK(!web_contents_->GetUserData(kContentViewUserDataKey));
  web_contents_->SetUserData(kContentViewUserDataKey,
                             new ContentViewUserData(this));
}

void ContentViewCoreImpl::RenderViewReady() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_onRenderProcessChange(env, obj);

  if (device_orientation_ != 0)
    SendOrientationChangeEventInternal();
}

void ContentViewCoreImpl::RenderViewHostChanged(RenderViewHost* old_host,
                                                RenderViewHost* new_host) {
  int old_pid = 0;
  if (old_host) {
    old_pid = GetRenderProcessIdFromRenderViewHost(old_host);

    RenderWidgetHostViewAndroid* view =
        static_cast<RenderWidgetHostViewAndroid*>(
            old_host->GetWidget()->GetView());
    if (view)
      view->SetContentViewCore(NULL);

    view = static_cast<RenderWidgetHostViewAndroid*>(
        new_host->GetWidget()->GetView());
    if (view)
      view->SetContentViewCore(this);
  }
  int new_pid = GetRenderProcessIdFromRenderViewHost(
      web_contents_->GetRenderViewHost());
  if (new_pid != old_pid) {
    // Notify the Java side that the renderer process changed.
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (!obj.is_null()) {
      Java_ContentViewCore_onRenderProcessChange(env, obj);
    }
  }

  SetFocusInternal(HasFocus());
  SetAccessibilityEnabledInternal(accessibility_enabled_);
}

RenderWidgetHostViewAndroid*
    ContentViewCoreImpl::GetRenderWidgetHostViewAndroid() const {
  RenderWidgetHostView* rwhv = NULL;
  if (web_contents_) {
    rwhv = web_contents_->GetRenderWidgetHostView();
    if (web_contents_->ShowingInterstitialPage()) {
      rwhv = web_contents_->GetInterstitialPage()
                 ->GetMainFrame()
                 ->GetRenderViewHost()
                 ->GetWidget()
                 ->GetView();
    }
  }
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

ScopedJavaLocalRef<jobject> ContentViewCoreImpl::GetJavaObject() {
  JNIEnv* env = AttachCurrentThread();
  return java_ref_.get(env);
}

jint ContentViewCoreImpl::GetBackgroundColor(JNIEnv* env, jobject obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return SK_ColorWHITE;
  return rwhva->GetCachedBackgroundColor();
}

// All positions and sizes are in CSS pixels.
// Note that viewport_width/height is a best effort based.
// ContentViewCore has the actual information about the physical viewport size.
void ContentViewCoreImpl::UpdateFrameInfo(
    const gfx::Vector2dF& scroll_offset,
    float page_scale_factor,
    const gfx::Vector2dF& page_scale_factor_limits,
    const gfx::SizeF& content_size,
    const gfx::SizeF& viewport_size,
    const float top_controls_height,
    const float top_controls_shown_ratio,
    const float bottom_controls_height,
    const float bottom_controls_shown_ratio,
    bool is_mobile_optimized_hint,
    const gfx::SelectionBound& selection_start) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null() || !GetWindowAndroid())
    return;

  GetWindowAndroid()->set_content_offset(
      gfx::Vector2dF(0.0f, top_controls_height * top_controls_shown_ratio));

  page_scale_ = page_scale_factor;

  // The CursorAnchorInfo API in Android only supports zero width selection
  // bounds.
  const jboolean has_insertion_marker =
      selection_start.type() == gfx::SelectionBound::CENTER;
  const jboolean is_insertion_marker_visible = selection_start.visible();
  const jfloat insertion_marker_horizontal =
      has_insertion_marker ? selection_start.edge_top().x() : 0.0f;
  const jfloat insertion_marker_top =
      has_insertion_marker ? selection_start.edge_top().y() : 0.0f;
  const jfloat insertion_marker_bottom =
      has_insertion_marker ? selection_start.edge_bottom().y() : 0.0f;

  Java_ContentViewCore_updateFrameInfo(
      env, obj, scroll_offset.x(), scroll_offset.y(), page_scale_factor,
      page_scale_factor_limits.x(), page_scale_factor_limits.y(),
      content_size.width(), content_size.height(), viewport_size.width(),
      viewport_size.height(), top_controls_height, top_controls_shown_ratio,
      bottom_controls_height, bottom_controls_shown_ratio,
      is_mobile_optimized_hint, has_insertion_marker,
      is_insertion_marker_visible, insertion_marker_horizontal,
      insertion_marker_top, insertion_marker_bottom);
}

void ContentViewCoreImpl::SetTitle(const base::string16& title) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jtitle =
      ConvertUTF8ToJavaString(env, base::UTF16ToUTF8(title));
  Java_ContentViewCore_setTitle(env, obj, jtitle);
}

void ContentViewCoreImpl::OnBackgroundColorChanged(SkColor color) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_onBackgroundColorChanged(env, obj, color);
}

void ContentViewCoreImpl::ShowSelectPopupMenu(
    RenderFrameHost* frame,
    const gfx::Rect& bounds,
    const std::vector<MenuItem>& items,
    int selected_item,
    bool multiple,
    bool right_aligned) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;

  // For multi-select list popups we find the list of previous selections by
  // iterating through the items. But for single selection popups we take the
  // given |selected_item| as is.
  ScopedJavaLocalRef<jintArray> selected_array;
  if (multiple) {
    std::unique_ptr<jint[]> native_selected_array(new jint[items.size()]);
    size_t selected_count = 0;
    for (size_t i = 0; i < items.size(); ++i) {
      if (items[i].checked)
        native_selected_array[selected_count++] = i;
    }

    selected_array = ScopedJavaLocalRef<jintArray>(
        env, env->NewIntArray(selected_count));
    env->SetIntArrayRegion(selected_array.obj(), 0, selected_count,
                           native_selected_array.get());
  } else {
    selected_array = ScopedJavaLocalRef<jintArray>(env, env->NewIntArray(1));
    jint value = selected_item;
    env->SetIntArrayRegion(selected_array.obj(), 0, 1, &value);
  }

  ScopedJavaLocalRef<jintArray> enabled_array(env,
                                              env->NewIntArray(items.size()));
  std::vector<base::string16> labels;
  labels.reserve(items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    labels.push_back(items[i].label);
    jint enabled =
        (items[i].type == MenuItem::GROUP ? POPUP_ITEM_TYPE_GROUP :
            (items[i].enabled ? POPUP_ITEM_TYPE_ENABLED :
                POPUP_ITEM_TYPE_DISABLED));
    env->SetIntArrayRegion(enabled_array.obj(), i, 1, &enabled);
  }
  ScopedJavaLocalRef<jobjectArray> items_array(
      base::android::ToJavaArrayOfStrings(env, labels));
  ui::ViewAndroid* view = GetViewAndroid();
  select_popup_ = view->AcquireAnchorView();
  const ScopedJavaLocalRef<jobject> popup_view = select_popup_.view();
  if (popup_view.is_null())
    return;
  view->SetAnchorRect(popup_view,
                      gfx::ScaleRect(gfx::RectF(bounds), page_scale_));
  Java_ContentViewCore_showSelectPopup(
      env, j_obj, popup_view, reinterpret_cast<intptr_t>(frame), items_array,
      enabled_array, multiple, selected_array, right_aligned);
}

void ContentViewCoreImpl::HideSelectPopupMenu() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null())
    Java_ContentViewCore_hideSelectPopup(env, j_obj);
  select_popup_.Reset();
}

void ContentViewCoreImpl::OnGestureEventAck(const blink::WebGestureEvent& event,
                                            InputEventAckState ack_result) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;

  switch (event.type) {
    case WebInputEvent::GestureFlingStart:
      if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED) {
        // The view expects the fling velocity in pixels/s.
        Java_ContentViewCore_onFlingStartEventConsumed(env, j_obj);
      } else {
        // If a scroll ends with a fling, a SCROLL_END event is never sent.
        // However, if that fling went unconsumed, we still need to let the
        // listeners know that scrolling has ended.
        Java_ContentViewCore_onScrollEndEventAck(env, j_obj);
      }
      break;
    case WebInputEvent::GestureFlingCancel:
      Java_ContentViewCore_onFlingCancelEventAck(env, j_obj);
      break;
    case WebInputEvent::GestureScrollBegin:
      Java_ContentViewCore_onScrollBeginEventAck(env, j_obj);
      break;
    case WebInputEvent::GestureScrollUpdate:
      if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
        Java_ContentViewCore_onScrollUpdateGestureConsumed(env, j_obj);
      break;
    case WebInputEvent::GestureScrollEnd:
      Java_ContentViewCore_onScrollEndEventAck(env, j_obj);
      break;
    case WebInputEvent::GesturePinchBegin:
      Java_ContentViewCore_onPinchBeginEventAck(env, j_obj);
      break;
    case WebInputEvent::GesturePinchEnd:
      Java_ContentViewCore_onPinchEndEventAck(env, j_obj);
      break;
    case WebInputEvent::GestureTap:
      Java_ContentViewCore_onSingleTapEventAck(
          env, j_obj, ack_result == INPUT_EVENT_ACK_STATE_CONSUMED);
      break;
    case WebInputEvent::GestureLongPress:
      if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
        Java_ContentViewCore_performLongPressHapticFeedback(env, j_obj);
      break;
    default:
      break;
  }
}

bool ContentViewCoreImpl::FilterInputEvent(const blink::WebInputEvent& event) {
  if (event.type != WebInputEvent::GestureTap &&
      event.type != WebInputEvent::GestureDoubleTap &&
      event.type != WebInputEvent::GestureLongTap &&
      event.type != WebInputEvent::GestureLongPress)
    return false;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return false;

  const blink::WebGestureEvent& gesture =
      static_cast<const blink::WebGestureEvent&>(event);
  int gesture_type = ToGestureEventType(event.type);
  return Java_ContentViewCore_filterTapOrPressEvent(env, j_obj, gesture_type,
                                                    gesture.x * dpi_scale(),
                                                    gesture.y * dpi_scale());
}

bool ContentViewCoreImpl::HasFocus() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_ContentViewCore_hasFocus(env, obj);
}

void ContentViewCoreImpl::RequestDisallowInterceptTouchEvent() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_requestDisallowInterceptTouchEvent(env, obj);
}

void ContentViewCoreImpl::OnSelectionChanged(const std::string& text) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jtext = ConvertUTF8ToJavaString(env, text);
  Java_ContentViewCore_onSelectionChanged(env, obj, jtext);
}

void ContentViewCoreImpl::OnSelectionEvent(ui::SelectionEventType event,
                                           const gfx::PointF& selection_anchor,
                                           const gfx::RectF& selection_rect) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;

  Java_ContentViewCore_onSelectionEvent(
      env, j_obj, event, selection_anchor.x(), selection_anchor.y(),
      selection_rect.x(), selection_rect.y(), selection_rect.right(),
      selection_rect.bottom());
}

void ContentViewCoreImpl::ShowPastePopup(int x_dip, int y_dip) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (!view)
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_showPastePopup(env, obj, x_dip,
                                      y_dip);
}

void ContentViewCoreImpl::StartContentIntent(const GURL& content_url,
                                             bool is_main_frame) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jcontent_url =
      ConvertUTF8ToJavaString(env, content_url.spec());
  Java_ContentViewCore_startContentIntent(env, j_obj, jcontent_url,
                                          is_main_frame);
}

void ContentViewCoreImpl::ShowDisambiguationPopup(
    const gfx::Rect& rect_pixels,
    const SkBitmap& zoomed_bitmap) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jobject> rect_object(CreateJavaRect(env, rect_pixels));

  ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(&zoomed_bitmap);
  DCHECK(!java_bitmap.is_null());

  Java_ContentViewCore_showDisambiguationPopup(env, obj, rect_object,
                                               java_bitmap);
}

ScopedJavaLocalRef<jobject>
ContentViewCoreImpl::CreateMotionEventSynthesizer() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return ScopedJavaLocalRef<jobject>();
  return Java_ContentViewCore_createMotionEventSynthesizer(env, obj);
}

void ContentViewCoreImpl::DidStopFlinging() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_onNativeFlingStopped(env, obj);
}

ScopedJavaLocalRef<jobject> ContentViewCoreImpl::GetContext() const {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return ScopedJavaLocalRef<jobject>();

  return Java_ContentViewCore_getContext(env, obj);
}

gfx::Size ContentViewCoreImpl::GetViewSizeWithOSKHidden() const {
  gfx::Size size_pix;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return size_pix = gfx::Size();
  size_pix = gfx::Size(
      Java_ContentViewCore_getViewportWidthPix(env, j_obj),
      Java_ContentViewCore_getViewportHeightWithOSKHiddenPix(env, j_obj));

  gfx::Size size_dip = gfx::ScaleToCeiledSize(size_pix, 1.0f / dpi_scale());
  if (DoBrowserControlsShrinkBlinkSize())
    size_dip.Enlarge(0, -GetTopControlsHeightDip());
  return size_dip;
}

gfx::Size ContentViewCoreImpl::GetViewSize() const {
  gfx::Size size = GetViewportSizeDip();
  if (DoBrowserControlsShrinkBlinkSize())
    size.Enlarge(0, -GetTopControlsHeightDip() - GetBottomControlsHeightDip());
  return size;
}

gfx::Size ContentViewCoreImpl::GetPhysicalBackingSize() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return gfx::Size();
  return gfx::Size(
      Java_ContentViewCore_getPhysicalBackingWidthPix(env, j_obj),
      Java_ContentViewCore_getPhysicalBackingHeightPix(env, j_obj));
}

gfx::Size ContentViewCoreImpl::GetViewportSizePix() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return gfx::Size();
  return gfx::Size(Java_ContentViewCore_getViewportWidthPix(env, j_obj),
                   Java_ContentViewCore_getViewportHeightPix(env, j_obj));
}

int ContentViewCoreImpl::GetTopControlsHeightPix() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return 0;
  return Java_ContentViewCore_getTopControlsHeightPix(env, j_obj);
}

int ContentViewCoreImpl::GetBottomControlsHeightPix() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return 0;
  return Java_ContentViewCore_getBottomControlsHeightPix(env, j_obj);
}

gfx::Size ContentViewCoreImpl::GetViewportSizeDip() const {
  return gfx::ScaleToCeiledSize(GetViewportSizePix(), 1.0f / dpi_scale());
}

bool ContentViewCoreImpl::DoBrowserControlsShrinkBlinkSize() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return false;
  return Java_ContentViewCore_doBrowserControlsShrinkBlinkSize(env, j_obj);
}

float ContentViewCoreImpl::GetTopControlsHeightDip() const {
  return GetTopControlsHeightPix() / dpi_scale();
}

float ContentViewCoreImpl::GetBottomControlsHeightDip() const {
  return GetBottomControlsHeightPix() / dpi_scale();
}

void ContentViewCoreImpl::SendScreenRectsAndResizeWidget() {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (view) {
    // |SendScreenRects()| indirectly calls GetViewSize() that asks Java layer.
    web_contents_->SendScreenRects();
    view->WasResized();
  }
}

void ContentViewCoreImpl::MoveRangeSelectionExtent(const gfx::PointF& extent) {
  if (!web_contents_)
    return;

  web_contents_->MoveRangeSelectionExtent(gfx::ToRoundedPoint(extent));
}

void ContentViewCoreImpl::SelectBetweenCoordinates(const gfx::PointF& base,
                                                   const gfx::PointF& extent) {
  if (!web_contents_)
    return;

  gfx::Point base_point = gfx::ToRoundedPoint(base);
  gfx::Point extent_point = gfx::ToRoundedPoint(extent);
  if (base_point == extent_point)
    return;

  web_contents_->SelectRange(base_point, extent_point);
}

ui::WindowAndroid* ContentViewCoreImpl::GetWindowAndroid() const {
  return GetViewAndroid()->GetWindowAndroid();
}

ui::ViewAndroid* ContentViewCoreImpl::GetViewAndroid() const {
  return web_contents_->GetView()->GetNativeView();
}


// ----------------------------------------------------------------------------
// Methods called from Java via JNI
// ----------------------------------------------------------------------------

void ContentViewCoreImpl::SelectPopupMenuItems(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong selectPopupSourceFrame,
    const JavaParamRef<jintArray>& indices) {
  RenderFrameHostImpl* rfhi =
      reinterpret_cast<RenderFrameHostImpl*>(selectPopupSourceFrame);
  DCHECK(rfhi);
  if (indices == NULL) {
    rfhi->DidCancelPopupMenu();
    return;
  }

  int selected_count = env->GetArrayLength(indices);
  std::vector<int> selected_indices;
  jint* indices_ptr = env->GetIntArrayElements(indices, NULL);
  for (int i = 0; i < selected_count; ++i)
    selected_indices.push_back(indices_ptr[i]);
  env->ReleaseIntArrayElements(indices, indices_ptr, JNI_ABORT);
  rfhi->DidSelectPopupMenuItems(selected_indices);
}

WebContents* ContentViewCoreImpl::GetWebContents() const {
  return web_contents_;
}

void ContentViewCoreImpl::SetFocus(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jboolean focused) {
  SetFocusInternal(focused);
}

void ContentViewCoreImpl::SetDIPScale(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jfloat dpi_scale) {
  if (dpi_scale_ == dpi_scale)
    return;

  dpi_scale_ = dpi_scale;
  SendScreenRectsAndResizeWidget();
}

void ContentViewCoreImpl::SetFocusInternal(bool focused) {
  if (!GetRenderWidgetHostViewAndroid())
    return;

  if (focused)
    GetRenderWidgetHostViewAndroid()->Focus();
  else
    GetRenderWidgetHostViewAndroid()->Blur();
}

void ContentViewCoreImpl::SendOrientationChangeEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint orientation) {
  if (device_orientation_ != orientation) {
    base::RecordAction(base::UserMetricsAction("ScreenOrientationChange"));
    device_orientation_ = orientation;
    SendOrientationChangeEventInternal();
  }
}

jboolean ContentViewCoreImpl::OnTouchEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& motion_event,
    jlong time_ms,
    jint android_action,
    jint pointer_count,
    jint history_size,
    jint action_index,
    jfloat pos_x_0,
    jfloat pos_y_0,
    jfloat pos_x_1,
    jfloat pos_y_1,
    jint pointer_id_0,
    jint pointer_id_1,
    jfloat touch_major_0,
    jfloat touch_major_1,
    jfloat touch_minor_0,
    jfloat touch_minor_1,
    jfloat orientation_0,
    jfloat orientation_1,
    jfloat tilt_0,
    jfloat tilt_1,
    jfloat raw_pos_x,
    jfloat raw_pos_y,
    jint android_tool_type_0,
    jint android_tool_type_1,
    jint android_button_state,
    jint android_meta_state,
    jboolean is_touch_handle_event) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  // Avoid synthesizing a touch event if it cannot be forwarded.
  if (!rwhv)
    return false;

  MotionEventAndroid::Pointer pointer0(pointer_id_0,
                                       pos_x_0,
                                       pos_y_0,
                                       touch_major_0,
                                       touch_minor_0,
                                       orientation_0,
                                       tilt_0,
                                       android_tool_type_0);
  MotionEventAndroid::Pointer pointer1(pointer_id_1,
                                       pos_x_1,
                                       pos_y_1,
                                       touch_major_1,
                                       touch_minor_1,
                                       orientation_1,
                                       tilt_1,
                                       android_tool_type_1);
  MotionEventAndroid event(1.f / dpi_scale(),
                           env,
                           motion_event,
                           time_ms,
                           android_action,
                           pointer_count,
                           history_size,
                           action_index,
                           android_button_state,
                           android_meta_state,
                           raw_pos_x - pos_x_0,
                           raw_pos_y - pos_y_0,
                           &pointer0,
                           &pointer1);

  RecordToolTypeForActionDown(event);

  return is_touch_handle_event ? rwhv->OnTouchHandleEvent(event)
                               : rwhv->OnTouchEvent(event);
}

jboolean ContentViewCoreImpl::SendMouseEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong time_ms,
    jint android_action,
    jfloat x,
    jfloat y,
    jint pointer_id,
    jfloat pressure,
    jfloat orientation,
    jfloat tilt,
    jint android_changed_button,
    jint android_button_state,
    jint android_meta_state,
    jint android_tool_type) {

  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (!rwhv)
    return false;

  // Construct a motion_event object minimally, only to convert the raw
  // parameters to ui::MotionEvent values. Since we used only the cached values
  // at index=0, it is okay to even pass a null event to the constructor.
  MotionEventAndroid::Pointer pointer0(
      pointer_id, x, y, 0.0f /* touch_major */, 0.0f /* touch_minor */,
      orientation, tilt, android_tool_type);

  MotionEventAndroid motion_event(1.f / dpi_scale(),
      env,
      nullptr /* event */,
      time_ms,
      android_action,
      1 /* pointer_count */,
      0 /* history_size */,
      0 /* action_index */,
      android_button_state,
      android_meta_state,
      0 /* raw_offset_x_pixels */,
      0 /* raw_offset_y_pixels */,
      &pointer0,
      nullptr);

  RecordToolTypeForActionDown(motion_event);

  // Note: This relies on identical button enum values in MotionEvent and
  // MotionEventAndroid.
  rwhv->SendMouseEvent(motion_event, android_changed_button);

  return true;
}

jboolean ContentViewCoreImpl::SendMouseWheelEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong time_ms,
    jfloat x,
    jfloat y,
    jfloat ticks_x,
    jfloat ticks_y,
    jfloat pixels_per_tick) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (!rwhv)
    return false;

  if (!ticks_x && !ticks_y)
    return false;

  // Compute Event.Latency.OS.MOUSE_WHEEL histogram.
  base::TimeTicks current_time = ui::EventTimeForNow();
  base::TimeTicks event_time = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(time_ms);
  base::TimeDelta delta = current_time - event_time;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.MOUSE_WHEEL",
      delta.InMicroseconds(), 1, 1000000, 50);

  blink::WebMouseWheelEvent event = WebMouseWheelEventBuilder::Build(
      ticks_x, ticks_y, pixels_per_tick / dpi_scale(), time_ms / 1000.0,
      x / dpi_scale(), y / dpi_scale());

  rwhv->SendMouseWheelEvent(event);
  return true;
}

WebGestureEvent ContentViewCoreImpl::MakeGestureEvent(WebInputEvent::Type type,
                                                      int64_t time_ms,
                                                      float x,
                                                      float y) const {
  return WebGestureEventBuilder::Build(
      type, time_ms / 1000.0, x / dpi_scale(), y / dpi_scale());
}

void ContentViewCoreImpl::SendGestureEvent(
    const blink::WebGestureEvent& event) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SendGestureEvent(event);
}

void ContentViewCoreImpl::ScrollBegin(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jlong time_ms,
                                      jfloat x,
                                      jfloat y,
                                      jfloat hintx,
                                      jfloat hinty,
                                      jboolean target_viewport) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureScrollBegin, time_ms, x, y);
  event.data.scrollBegin.deltaXHint = hintx / dpi_scale();
  event.data.scrollBegin.deltaYHint = hinty / dpi_scale();
  event.data.scrollBegin.targetViewport = target_viewport;

  SendGestureEvent(event);
}

void ContentViewCoreImpl::ScrollEnd(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jlong time_ms) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureScrollEnd, time_ms, 0, 0);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::ScrollBy(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jlong time_ms,
                                   jfloat x,
                                   jfloat y,
                                   jfloat dx,
                                   jfloat dy) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureScrollUpdate, time_ms, x, y);
  event.data.scrollUpdate.deltaX = -dx / dpi_scale();
  event.data.scrollUpdate.deltaY = -dy / dpi_scale();

  SendGestureEvent(event);
}

void ContentViewCoreImpl::FlingStart(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     jlong time_ms,
                                     jfloat x,
                                     jfloat y,
                                     jfloat vx,
                                     jfloat vy,
                                     jboolean target_viewport) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureFlingStart, time_ms, x, y);
  event.data.flingStart.velocityX = vx / dpi_scale();
  event.data.flingStart.velocityY = vy / dpi_scale();
  event.data.flingStart.targetViewport = target_viewport;

  SendGestureEvent(event);
}

void ContentViewCoreImpl::FlingCancel(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jlong time_ms) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureFlingCancel, time_ms, 0, 0);
  event.data.flingCancel.preventBoosting = true;

  SendGestureEvent(event);
}

void ContentViewCoreImpl::SingleTap(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jlong time_ms,
                                    jfloat x,
                                    jfloat y) {
  // Tap gestures should always be preceded by a TapDown, ensuring consistency
  // with the touch-based gesture detection pipeline.
  WebGestureEvent tap_down_event = MakeGestureEvent(
      WebInputEvent::GestureTapDown, time_ms, x, y);
  tap_down_event.data.tap.tapCount = 1;
  SendGestureEvent(tap_down_event);

  WebGestureEvent tap_event = MakeGestureEvent(
      WebInputEvent::GestureTap, time_ms, x, y);
  tap_event.data.tap.tapCount = 1;
  SendGestureEvent(tap_event);
}

void ContentViewCoreImpl::DoubleTap(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jlong time_ms,
                                    jfloat x,
                                    jfloat y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureDoubleTap, time_ms, x, y);
  // Set the tap count to 1 even for DoubleTap, in order to be consistent with
  // double tap behavior on a mobile viewport. See crbug.com/234986 for context.
  event.data.tap.tapCount = 1;

  SendGestureEvent(event);
}

void ContentViewCoreImpl::LongPress(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jlong time_ms,
                                    jfloat x,
                                    jfloat y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureLongPress, time_ms, x, y);

  SendGestureEvent(event);
}

void ContentViewCoreImpl::PinchBegin(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     jlong time_ms,
                                     jfloat x,
                                     jfloat y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GesturePinchBegin, time_ms, x, y);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::PinchEnd(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jlong time_ms) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GesturePinchEnd, time_ms, 0, 0);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::PinchBy(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jlong time_ms,
                                  jfloat anchor_x,
                                  jfloat anchor_y,
                                  jfloat delta) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GesturePinchUpdate, time_ms, anchor_x, anchor_y);
  event.data.pinchUpdate.scale = delta;

  SendGestureEvent(event);
}

void ContentViewCoreImpl::SetTextHandlesTemporarilyHidden(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean hidden) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SetTextHandlesTemporarilyHidden(hidden);
}

void ContentViewCoreImpl::ResetGestureDetection(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->ResetGestureDetection();
}

void ContentViewCoreImpl::SetDoubleTapSupportEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SetDoubleTapSupportEnabled(enabled);
}

void ContentViewCoreImpl::SetMultiTouchZoomSupportEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SetMultiTouchZoomSupportEnabled(enabled);
}

void ContentViewCoreImpl::SetAllowJavascriptInterfacesInspection(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean allow) {
  java_bridge_dispatcher_host_->SetAllowObjectContentsInspection(allow);
}

void ContentViewCoreImpl::AddJavascriptInterface(
    JNIEnv* env,
    const JavaParamRef<jobject>& /* obj */,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jstring>& name,
    const JavaParamRef<jclass>& safe_annotation_clazz) {
  java_bridge_dispatcher_host_->AddNamedObject(
      ConvertJavaStringToUTF8(env, name), object, safe_annotation_clazz);
}

void ContentViewCoreImpl::RemoveJavascriptInterface(
    JNIEnv* env,
    const JavaParamRef<jobject>& /* obj */,
    const JavaParamRef<jstring>& name) {
  java_bridge_dispatcher_host_->RemoveNamedObject(
      ConvertJavaStringToUTF8(env, name));
}

void ContentViewCoreImpl::WasResized(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  gfx::Size physical_size(
      Java_ContentViewCore_getPhysicalBackingWidthPix(env, obj),
      Java_ContentViewCore_getPhysicalBackingHeightPix(env, obj));
  GetViewAndroid()->GetLayer()->SetBounds(physical_size);

  SendScreenRectsAndResizeWidget();
}

long ContentViewCoreImpl::GetNativeImeAdapter(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return 0;
  return rwhva->GetNativeImeAdapter();
}

void ContentViewCoreImpl::ForceUpdateImeAdapter(long native_ime_adapter) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_forceUpdateImeAdapter(env, obj, native_ime_adapter);
}

void ContentViewCoreImpl::UpdateImeAdapter(long native_ime_adapter,
                                           int text_input_type,
                                           int text_input_flags,
                                           const std::string& text,
                                           int selection_start,
                                           int selection_end,
                                           int composition_start,
                                           int composition_end,
                                           bool show_ime_if_needed,
                                           bool is_non_ime_change) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jstring_text = ConvertUTF8ToJavaString(env, text);
  Java_ContentViewCore_updateImeAdapter(
      env, obj, native_ime_adapter, text_input_type, text_input_flags,
      jstring_text, selection_start, selection_end, composition_start,
      composition_end, show_ime_if_needed, is_non_ime_change);
}

void ContentViewCoreImpl::SetAccessibilityEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    bool enabled) {
  SetAccessibilityEnabledInternal(enabled);
}

void ContentViewCoreImpl::SetTextTrackSettings(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean textTracksEnabled,
    const JavaParamRef<jstring>& textTrackBackgroundColor,
    const JavaParamRef<jstring>& textTrackFontFamily,
    const JavaParamRef<jstring>& textTrackFontStyle,
    const JavaParamRef<jstring>& textTrackFontVariant,
    const JavaParamRef<jstring>& textTrackTextColor,
    const JavaParamRef<jstring>& textTrackTextShadow,
    const JavaParamRef<jstring>& textTrackTextSize) {
  FrameMsg_TextTrackSettings_Params params;
  params.text_tracks_enabled = textTracksEnabled;
  params.text_track_background_color = ConvertJavaStringToUTF8(
      env, textTrackBackgroundColor);
  params.text_track_font_family = ConvertJavaStringToUTF8(
      env, textTrackFontFamily);
  params.text_track_font_style = ConvertJavaStringToUTF8(
      env, textTrackFontStyle);
  params.text_track_font_variant = ConvertJavaStringToUTF8(
      env, textTrackFontVariant);
  params.text_track_text_color = ConvertJavaStringToUTF8(
      env, textTrackTextColor);
  params.text_track_text_shadow = ConvertJavaStringToUTF8(
      env, textTrackTextShadow);
  params.text_track_text_size = ConvertJavaStringToUTF8(
      env, textTrackTextSize);
  web_contents_->GetMainFrame()->SetTextTrackSettings(params);
}

bool ContentViewCoreImpl::IsFullscreenRequiredForOrientationLock() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return true;
  return Java_ContentViewCore_isFullscreenRequiredForOrientationLock(env, obj);
}

void ContentViewCoreImpl::SetAccessibilityEnabledInternal(bool enabled) {
  accessibility_enabled_ = enabled;
  BrowserAccessibilityStateImpl* accessibility_state =
      BrowserAccessibilityStateImpl::GetInstance();
  if (enabled) {
    // First check if we already have a BrowserAccessibilityManager that
    // just needs to be connected to the ContentViewCore.
    if (web_contents_) {
      BrowserAccessibilityManagerAndroid* manager =
          static_cast<BrowserAccessibilityManagerAndroid*>(
              web_contents_->GetRootBrowserAccessibilityManager());
      if (manager) {
        manager->SetContentViewCore(GetJavaObject());
        return;
      }
    }

    // Otherwise, enable accessibility globally unless it was
    // explicitly disallowed by a command-line flag, then enable it for
    // this WebContents if that succeeded.
    accessibility_state->OnScreenReaderDetected();
    if (accessibility_state->IsAccessibleBrowser() && web_contents_)
      web_contents_->AddAccessibilityMode(AccessibilityModeComplete);
  } else {
    accessibility_state->ResetAccessibilityMode();
    if (web_contents_) {
      web_contents_->SetAccessibilityMode(
          accessibility_state->accessibility_mode());
    }
  }
}

void ContentViewCoreImpl::SendOrientationChangeEventInternal() {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->UpdateScreenInfo(GetViewAndroid());

  static_cast<WebContentsImpl*>(web_contents())->
      screen_orientation_dispatcher_host()->OnOrientationChange();
}

void ContentViewCoreImpl::ExtractSmartClipData(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               jint x,
                                               jint y,
                                               jint width,
                                               jint height) {
  gfx::Rect rect(
      static_cast<int>(x / dpi_scale()),
      static_cast<int>(y / dpi_scale()),
      static_cast<int>((width > 0 && width < dpi_scale()) ?
          1 : (int)(width / dpi_scale())),
      static_cast<int>((height > 0 && height < dpi_scale()) ?
          1 : (int)(height / dpi_scale())));
  GetWebContents()->Send(new ViewMsg_ExtractSmartClipData(
      GetWebContents()->GetRoutingID(), rect));
}

jint ContentViewCoreImpl::GetCurrentRenderProcessId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetRenderProcessIdFromRenderViewHost(
      web_contents_->GetRenderViewHost());
}

void ContentViewCoreImpl::SetBackgroundOpaque(JNIEnv* env,
                                              const JavaParamRef<jobject>& jobj,
                                              jboolean opaque) {
  if (GetRenderWidgetHostViewAndroid()) {
    if (opaque)
      GetRenderWidgetHostViewAndroid()->SetBackgroundColorToDefault();
    else
      GetRenderWidgetHostViewAndroid()->SetBackgroundColor(SK_ColorTRANSPARENT);
  }
}

bool ContentViewCoreImpl::IsTouchDragDropEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  return switches::IsTouchDragDropEnabled();
}

void ContentViewCoreImpl::OnDragEvent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    jint action,
    jint x,
    jint y,
    jint screen_x,
    jint screen_y,
    const base::android::JavaParamRef<jobjectArray>& j_mimeTypes,
    const base::android::JavaParamRef<jstring>& j_content) {
  WebContentsViewAndroid* wcva = static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(web_contents())->GetView());

  const gfx::Point location(x, y);
  const gfx::Point screen_location(screen_x, screen_y);

  std::vector<base::string16> mime_types;
  base::android::AppendJavaStringArrayToStringVector(env, j_mimeTypes,
                                                     &mime_types);
  switch (action) {
    case JNI_DragEvent::ACTION_DRAG_ENTERED: {
      std::vector<DropData::Metadata> metadata;
      for (const base::string16& mime_type : mime_types) {
        metadata.push_back(DropData::Metadata::CreateForMimeType(
            DropData::Kind::STRING, mime_type));
      }
      wcva->OnDragEntered(metadata, location, screen_location);
      break;
    }
    case JNI_DragEvent::ACTION_DRAG_LOCATION: {
      wcva->OnDragUpdated(location, screen_location);
      break;
    }
    case JNI_DragEvent::ACTION_DROP: {
      base::string16 text_to_drop = ConvertJavaStringToUTF16(env, j_content);
      DropData drop_data;
      drop_data.did_originate_from_renderer = false;
      for (const base::string16& mime_type : mime_types) {
        if (base::EqualsASCII(mime_type, ui::Clipboard::kMimeTypeURIList)) {
          drop_data.url = GURL(text_to_drop);
        } else if (base::EqualsASCII(mime_type, ui::Clipboard::kMimeTypeText)) {
          drop_data.text = base::NullableString16(text_to_drop, false);
        } else {
          drop_data.html = base::NullableString16(text_to_drop, false);
        }
      }

      wcva->OnPerformDrop(&drop_data, location, screen_location);
      break;
    }
    case JNI_DragEvent::ACTION_DRAG_EXITED:
      wcva->OnDragExited();
      break;
    case JNI_DragEvent::ACTION_DRAG_ENDED:
      wcva->OnDragEnded();
      break;
    case JNI_DragEvent::ACTION_DRAG_STARTED:
      // Nothing meaningful to do.
      break;
  }
}

void ContentViewCoreImpl::OnShowUnhandledTapUIIfNeeded(int x_dip, int y_dip) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_onShowUnhandledTapUIIfNeeded(
      env, obj, static_cast<jint>(x_dip * dpi_scale()),
      static_cast<jint>(y_dip * dpi_scale()));
}

void ContentViewCoreImpl::HidePopupsAndPreserveSelection() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_ContentViewCore_hidePopupsAndPreserveSelection(env, obj);
}

void ContentViewCoreImpl::OnSmartClipDataExtracted(
    const base::string16& text,
    const base::string16& html,
    const gfx::Rect& clip_rect) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jtext = ConvertUTF16ToJavaString(env, text);
  ScopedJavaLocalRef<jstring> jhtml = ConvertUTF16ToJavaString(env, html);
  ScopedJavaLocalRef<jobject> clip_rect_object(CreateJavaRect(env, clip_rect));
  Java_ContentViewCore_onSmartClipDataExtracted(env, obj, jtext, jhtml,
                                                clip_rect_object);
}

void ContentViewCoreImpl::WebContentsDestroyed() {
  WebContentsViewAndroid* wcva = static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(web_contents())->GetView());
  DCHECK(wcva);
  wcva->SetContentViewCore(NULL);
}

bool ContentViewCoreImpl::PullStart() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    return Java_ContentViewCore_onOverscrollRefreshStart(env, obj);
  return false;
}

void ContentViewCoreImpl::PullUpdate(float delta) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_onOverscrollRefreshUpdate(env, obj, delta);
}

void ContentViewCoreImpl::PullRelease(bool allow_refresh) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    Java_ContentViewCore_onOverscrollRefreshRelease(env, obj, allow_refresh);
  }
}

void ContentViewCoreImpl::PullReset() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_onOverscrollRefreshReset(env, obj);
}

// This is called for each ContentView.
jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& jweb_contents,
           const JavaParamRef<jobject>& jview_android_delegate,
           jlong jwindow_android,
           jfloat dipScale,
           const JavaParamRef<jobject>& retained_objects_set) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromJavaWebContents(jweb_contents));
  CHECK(web_contents) <<
      "A ContentViewCoreImpl should be created with a valid WebContents.";
  ui::ViewAndroid* view_android = web_contents->GetView()->GetNativeView();
  view_android->SetDelegate(jview_android_delegate);

  ui::WindowAndroid* window_android =
        reinterpret_cast<ui::WindowAndroid*>(jwindow_android);
  DCHECK(window_android);
  window_android->AddChild(view_android);

  // TODO: pass dipScale.
  ContentViewCoreImpl* view = new ContentViewCoreImpl(
      env, obj, web_contents, dipScale, retained_objects_set);
  return reinterpret_cast<intptr_t>(view);
}

static ScopedJavaLocalRef<jobject> FromWebContentsAndroid(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return ScopedJavaLocalRef<jobject>();

  ContentViewCore* view = ContentViewCore::FromWebContents(web_contents);
  if (!view)
    return ScopedJavaLocalRef<jobject>();

  return view->GetJavaObject();
}

bool RegisterContentViewCore(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
