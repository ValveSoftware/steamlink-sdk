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
#include "base/metrics/histogram.h"
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
#include "content/browser/geolocation/geolocation_service_context.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/input/web_input_event_builders_android.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/frame_messages.h"
#include "content/common/input/web_input_event_traits.h"
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
#include "jni/ContentViewCore_jni.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using blink::WebGestureEvent;
using blink::WebInputEvent;

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
                   << WebInputEventTraits::GetName(type);
      return -1;
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

// static
ContentViewCore* ContentViewCore::GetNativeContentViewCore(JNIEnv* env,
                                                           jobject obj) {
  return reinterpret_cast<ContentViewCore*>(
      Java_ContentViewCore_getNativeContentViewCore(env, obj));
}

ContentViewCoreImpl::ContentViewCoreImpl(
    JNIEnv* env,
    jobject obj,
    WebContents* web_contents,
    jobject view_android_delegate,
    ui::WindowAndroid* window_android,
    jobject java_bridge_retained_object_set)
    : WebContentsObserver(web_contents),
      java_ref_(env, obj),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)),
      root_layer_(cc::SolidColorLayer::Create()),
      page_scale_(1),
      dpi_scale_(ui::GetScaleFactorForNativeView(this)),
      window_android_(window_android),
      device_orientation_(0),
      accessibility_enabled_(false) {
  CHECK(web_contents) <<
      "A ContentViewCoreImpl should be created with a valid WebContents.";
  DCHECK(window_android_);
  DCHECK(view_android_delegate);
  view_android_delegate_.Reset(AttachCurrentThread(), view_android_delegate);
  root_layer_->SetBackgroundColor(GetBackgroundColor(env, obj));
  gfx::Size physical_size(
      Java_ContentViewCore_getPhysicalBackingWidthPix(env, obj),
      Java_ContentViewCore_getPhysicalBackingHeightPix(env, obj));
  root_layer_->SetBounds(physical_size);
  root_layer_->SetIsDrawable(true);

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
  root_layer_->RemoveFromParent();
  FOR_EACH_OBSERVER(ContentViewCoreImplObserver,
                    observer_list_,
                    OnContentViewCoreDestroyed());
  observer_list_.Clear();

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  java_ref_.reset();
  if (!j_obj.is_null()) {
    Java_ContentViewCore_onNativeContentViewCoreDestroyed(
        env, j_obj.obj(), reinterpret_cast<intptr_t>(this));
  }
}

void ContentViewCoreImpl::UpdateWindowAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong window_android) {
  if (window_android) {
    DCHECK(!window_android_);
    window_android_ = reinterpret_cast<ui::WindowAndroid*>(window_android);
    FOR_EACH_OBSERVER(ContentViewCoreImplObserver,
                      observer_list_,
                      OnAttachedToWindow());
  } else {
    FOR_EACH_OBSERVER(ContentViewCoreImplObserver,
                      observer_list_,
                      OnDetachedFromWindow());
    window_android_ = NULL;
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
  if (!window_android_)
    return ScopedJavaLocalRef<jobject>();
  return window_android_->GetJavaObject();
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
    Java_ContentViewCore_onRenderProcessChange(env, obj.obj());

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
      Java_ContentViewCore_onRenderProcessChange(env, obj.obj());
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

void ContentViewCoreImpl::PauseOrResumeGeolocation(bool should_pause) {
  if (should_pause)
    web_contents_->GetGeolocationServiceContext()->PauseUpdates();
  else
    web_contents_->GetGeolocationServiceContext()->ResumeUpdates();
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
    const gfx::Vector2dF& controls_offset,
    const gfx::Vector2dF& content_offset,
    bool is_mobile_optimized_hint,
    const gfx::SelectionBound& selection_start) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null() || !window_android_)
    return;

  window_android_->set_content_offset(
      gfx::ScaleVector2d(content_offset, dpi_scale_));

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
      env, obj.obj(),
      scroll_offset.x(),
      scroll_offset.y(),
      page_scale_factor,
      page_scale_factor_limits.x(),
      page_scale_factor_limits.y(),
      content_size.width(),
      content_size.height(),
      viewport_size.width(),
      viewport_size.height(),
      controls_offset.y(),
      content_offset.y(),
      is_mobile_optimized_hint,
      has_insertion_marker,
      is_insertion_marker_visible,
      insertion_marker_horizontal,
      insertion_marker_top,
      insertion_marker_bottom);
}

void ContentViewCoreImpl::SetTitle(const base::string16& title) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jtitle =
      ConvertUTF8ToJavaString(env, base::UTF16ToUTF8(title));
  Java_ContentViewCore_setTitle(env, obj.obj(), jtitle.obj());
}

void ContentViewCoreImpl::OnBackgroundColorChanged(SkColor color) {
  root_layer_->SetBackgroundColor(color);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_onBackgroundColorChanged(env, obj.obj(), color);
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

  ScopedJavaLocalRef<jobject> bounds_rect(CreateJavaRect(env, bounds));

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
  Java_ContentViewCore_showSelectPopup(
      env, j_obj.obj(), reinterpret_cast<intptr_t>(frame), bounds_rect.obj(),
      items_array.obj(), enabled_array.obj(), multiple, selected_array.obj(),
      right_aligned);
}

void ContentViewCoreImpl::HideSelectPopupMenu() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null())
    Java_ContentViewCore_hideSelectPopup(env, j_obj.obj());
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
        Java_ContentViewCore_onFlingStartEventConsumed(env, j_obj.obj(),
            event.data.flingStart.velocityX * dpi_scale(),
            event.data.flingStart.velocityY * dpi_scale());
      } else {
        // If a scroll ends with a fling, a SCROLL_END event is never sent.
        // However, if that fling went unconsumed, we still need to let the
        // listeners know that scrolling has ended.
        Java_ContentViewCore_onScrollEndEventAck(env, j_obj.obj());
      }
      break;
    case WebInputEvent::GestureFlingCancel:
      Java_ContentViewCore_onFlingCancelEventAck(env, j_obj.obj());
      break;
    case WebInputEvent::GestureScrollBegin:
      Java_ContentViewCore_onScrollBeginEventAck(env, j_obj.obj());
      break;
    case WebInputEvent::GestureScrollUpdate:
      if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
        Java_ContentViewCore_onScrollUpdateGestureConsumed(env, j_obj.obj());
      break;
    case WebInputEvent::GestureScrollEnd:
      Java_ContentViewCore_onScrollEndEventAck(env, j_obj.obj());
      break;
    case WebInputEvent::GesturePinchBegin:
      Java_ContentViewCore_onPinchBeginEventAck(env, j_obj.obj());
      break;
    case WebInputEvent::GesturePinchEnd:
      Java_ContentViewCore_onPinchEndEventAck(env, j_obj.obj());
      break;
    case WebInputEvent::GestureTap:
      Java_ContentViewCore_onSingleTapEventAck(
          env,
          j_obj.obj(),
          ack_result == INPUT_EVENT_ACK_STATE_CONSUMED,
          event.x * dpi_scale(),
          event.y * dpi_scale());
      break;
    case WebInputEvent::GestureLongPress:
      if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
        Java_ContentViewCore_performLongPressHapticFeedback(env, j_obj.obj());
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
  return Java_ContentViewCore_filterTapOrPressEvent(env,
                                                    j_obj.obj(),
                                                    gesture_type,
                                                    gesture.x * dpi_scale(),
                                                    gesture.y * dpi_scale());
}

bool ContentViewCoreImpl::HasFocus() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_ContentViewCore_hasFocus(env, obj.obj());
}

void ContentViewCoreImpl::RequestDisallowInterceptTouchEvent() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_requestDisallowInterceptTouchEvent(env, obj.obj());
}

void ContentViewCoreImpl::OnSelectionChanged(const std::string& text) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jtext = ConvertUTF8ToJavaString(env, text);
  Java_ContentViewCore_onSelectionChanged(env, obj.obj(), jtext.obj());
}

void ContentViewCoreImpl::OnSelectionEvent(ui::SelectionEventType event,
                                           const gfx::PointF& selection_anchor,
                                           const gfx::RectF& selection_rect) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;

  gfx::PointF selection_anchor_pix =
      gfx::ScalePoint(selection_anchor, dpi_scale());
  gfx::RectF selection_rect_pix = gfx::ScaleRect(selection_rect, dpi_scale());
  Java_ContentViewCore_onSelectionEvent(
      env, j_obj.obj(), event, selection_anchor_pix.x(),
      selection_anchor_pix.y(), selection_rect_pix.x(), selection_rect_pix.y(),
      selection_rect_pix.right(), selection_rect_pix.bottom());
}

bool ContentViewCoreImpl::ShowPastePopup(int x_dip, int y_dip) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (!view)
    return false;

  view->OnShowingPastePopup(gfx::PointF(x_dip, y_dip));

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_ContentViewCore_showPastePopupWithFeedback(
      env, obj.obj(), static_cast<jint>(x_dip * dpi_scale()),
      static_cast<jint>(y_dip * dpi_scale()));
}

void ContentViewCoreImpl::StartContentIntent(const GURL& content_url,
                                             bool is_main_frame) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jcontent_url =
      ConvertUTF8ToJavaString(env, content_url.spec());
  Java_ContentViewCore_startContentIntent(env,
                                          j_obj.obj(),
                                          jcontent_url.obj(),
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

  Java_ContentViewCore_showDisambiguationPopup(env,
                                               obj.obj(),
                                               rect_object.obj(),
                                               java_bitmap.obj());
}

ScopedJavaLocalRef<jobject>
ContentViewCoreImpl::CreateMotionEventSynthesizer() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return ScopedJavaLocalRef<jobject>();
  return Java_ContentViewCore_createMotionEventSynthesizer(env, obj.obj());
}

bool ContentViewCoreImpl::ShouldBlockMediaRequest(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return true;
  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env, url.spec());
  return Java_ContentViewCore_shouldBlockMediaRequest(env, obj.obj(),
                                                      j_url.obj());
}

void ContentViewCoreImpl::DidStopFlinging() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_onNativeFlingStopped(env, obj.obj());
}

ScopedJavaLocalRef<jobject> ContentViewCoreImpl::GetContext() const {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return ScopedJavaLocalRef<jobject>();

  return Java_ContentViewCore_getContext(env, obj.obj());
}

gfx::Size ContentViewCoreImpl::GetViewSizeWithOSKHidden() const {
  gfx::Size size_pix;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return size_pix = gfx::Size();
  size_pix = gfx::Size(
      Java_ContentViewCore_getViewportWidthPix(env, j_obj.obj()),
      Java_ContentViewCore_getViewportHeightWithOSKHiddenPix(env, j_obj.obj()));

  gfx::Size size_dip = gfx::ScaleToCeiledSize(size_pix, 1.0f / dpi_scale());
  if (DoTopControlsShrinkBlinkSize())
    size_dip.Enlarge(0, -GetTopControlsHeightDip());
  return size_dip;
}

gfx::Size ContentViewCoreImpl::GetViewSize() const {
  gfx::Size size = GetViewportSizeDip();
  if (DoTopControlsShrinkBlinkSize())
    size.Enlarge(0, -GetTopControlsHeightDip());
  return size;
}

gfx::Size ContentViewCoreImpl::GetPhysicalBackingSize() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return gfx::Size();
  return gfx::Size(
      Java_ContentViewCore_getPhysicalBackingWidthPix(env, j_obj.obj()),
      Java_ContentViewCore_getPhysicalBackingHeightPix(env, j_obj.obj()));
}

gfx::Size ContentViewCoreImpl::GetViewportSizePix() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return gfx::Size();
  return gfx::Size(
      Java_ContentViewCore_getViewportWidthPix(env, j_obj.obj()),
      Java_ContentViewCore_getViewportHeightPix(env, j_obj.obj()));
}

int ContentViewCoreImpl::GetTopControlsHeightPix() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return 0;
  return Java_ContentViewCore_getTopControlsHeightPix(env, j_obj.obj());
}

gfx::Size ContentViewCoreImpl::GetViewportSizeDip() const {
  return gfx::ScaleToCeiledSize(GetViewportSizePix(), 1.0f / dpi_scale());
}

bool ContentViewCoreImpl::DoTopControlsShrinkBlinkSize() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return false;
  return Java_ContentViewCore_doTopControlsShrinkBlinkSize(env, j_obj.obj());
}

float ContentViewCoreImpl::GetTopControlsHeightDip() const {
  return GetTopControlsHeightPix() / dpi_scale();
}

void ContentViewCoreImpl::AttachLayer(scoped_refptr<cc::Layer> layer) {
  root_layer_->InsertChild(layer, 0);
  root_layer_->SetIsDrawable(false);
}

void ContentViewCoreImpl::RemoveLayer(scoped_refptr<cc::Layer> layer) {
  layer->RemoveFromParent();

  if (!root_layer_->children().size())
    root_layer_->SetIsDrawable(true);
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

ScopedJavaLocalRef<jobject> ContentViewCoreImpl::GetViewAndroidDelegate()
    const {
  return base::android::ScopedJavaLocalRef<jobject>(view_android_delegate_);
}

ui::WindowAndroid* ContentViewCoreImpl::GetWindowAndroid() const {
  return window_android_;
}

const scoped_refptr<cc::Layer>& ContentViewCoreImpl::GetLayer() const {
  return root_layer_;
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

  ui::MotionEventAndroid::Pointer pointer0(pointer_id_0,
                                       pos_x_0,
                                       pos_y_0,
                                       touch_major_0,
                                       touch_minor_0,
                                       orientation_0,
                                       tilt_0,
                                       android_tool_type_0);
  ui::MotionEventAndroid::Pointer pointer1(pointer_id_1,
                                       pos_x_1,
                                       pos_y_1,
                                       touch_major_1,
                                       touch_minor_1,
                                       orientation_1,
                                       tilt_1,
                                       android_tool_type_1);
  ui::MotionEventAndroid event(1.f / dpi_scale(),
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
                           pointer0,
                           pointer1);

  return is_touch_handle_event ? rwhv->OnTouchHandleEvent(event)
                               : rwhv->OnTouchEvent(event);
}

float ContentViewCoreImpl::GetDpiScale() const {
  return dpi_scale_;
}

jboolean ContentViewCoreImpl::SendMouseMoveEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong time_ms,
    jfloat x,
    jfloat y,
    jint tool_type) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (!rwhv)
    return false;

  blink::WebMouseEvent event = WebMouseEventBuilder::Build(
      WebInputEvent::MouseMove,
      blink::WebMouseEvent::ButtonNone,
      time_ms / 1000.0, x / dpi_scale(), y / dpi_scale(), 0, 1,
      ui::ToWebPointerType(static_cast<ui::MotionEvent::ToolType>(tool_type)));

  rwhv->SendMouseEvent(event);
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

void ContentViewCoreImpl::SelectBetweenCoordinates(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jfloat x1,
    jfloat y1,
    jfloat x2,
    jfloat y2) {
  SelectBetweenCoordinates(gfx::PointF(x1 / dpi_scale(), y1 / dpi_scale()),
                           gfx::PointF(x2 / dpi_scale(), y2 / dpi_scale()));
}

void ContentViewCoreImpl::DismissTextHandles(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->DismissTextHandles();
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
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  gfx::Size physical_size(
      Java_ContentViewCore_getPhysicalBackingWidthPix(env, obj),
      Java_ContentViewCore_getPhysicalBackingHeightPix(env, obj));
  root_layer_->SetBounds(physical_size);

  if (view) {
    web_contents_->SendScreenRects();
    view->WasResized();
  }
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
  Java_ContentViewCore_forceUpdateImeAdapter(env, obj.obj(),
                                             native_ime_adapter);
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
  Java_ContentViewCore_updateImeAdapter(env,
                                        obj.obj(),
                                        native_ime_adapter,
                                        text_input_type,
                                        text_input_flags,
                                        jstring_text.obj(),
                                        selection_start,
                                        selection_end,
                                        composition_start,
                                        composition_end,
                                        show_ime_if_needed,
                                        is_non_ime_change);
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
  return Java_ContentViewCore_isFullscreenRequiredForOrientationLock(env,
                                                                     obj.obj());
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
    rwhv->UpdateScreenInfo(this);

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

void ContentViewCoreImpl::RequestTextSurroundingSelection(
    int max_length,
    const base::Callback<
        void(const base::string16& content, int start_offset, int end_offset)>&
        callback) {
  DCHECK(!callback.is_null());
  RenderFrameHost* focused_frame = web_contents_->GetFocusedFrame();
  if (!focused_frame)
    return;
  if (GetRenderWidgetHostViewAndroid()) {
    GetRenderWidgetHostViewAndroid()->SetTextSurroundingSelectionCallback(
        callback);
    focused_frame->Send(new FrameMsg_TextSurroundingSelectionRequest(
        focused_frame->GetRoutingID(), max_length));
  }
}

void ContentViewCoreImpl::OnShowUnhandledTapUIIfNeeded(int x_dip, int y_dip) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_onShowUnhandledTapUIIfNeeded(
      env, obj.obj(), static_cast<jint>(x_dip * dpi_scale()),
      static_cast<jint>(y_dip * dpi_scale()));
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
  Java_ContentViewCore_onSmartClipDataExtracted(
      env, obj.obj(), jtext.obj(), jhtml.obj(), clip_rect_object.obj());
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
    return Java_ContentViewCore_onOverscrollRefreshStart(env, obj.obj());
  return false;
}

void ContentViewCoreImpl::PullUpdate(float delta) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_onOverscrollRefreshUpdate(env, obj.obj(), delta);
}

void ContentViewCoreImpl::PullRelease(bool allow_refresh) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    Java_ContentViewCore_onOverscrollRefreshRelease(env, obj.obj(),
                                                    allow_refresh);
  }
}

void ContentViewCoreImpl::PullReset() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCore_onOverscrollRefreshReset(env, obj.obj());
}

// This is called for each ContentView.
jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& web_contents,
           const JavaParamRef<jobject>& view_android_delegate,
           jlong window_android,
           const JavaParamRef<jobject>& retained_objects_set) {
  ContentViewCoreImpl* view = new ContentViewCoreImpl(
      env, obj, WebContents::FromJavaWebContents(web_contents),
      view_android_delegate,
      reinterpret_cast<ui::WindowAndroid*>(window_android),
      retained_objects_set);
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
