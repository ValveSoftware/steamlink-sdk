// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ime_adapter_android.h"

#include <algorithm>
#include <android/input.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "jni/ImeAdapter_jni.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebTextInputType.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;

namespace content {
namespace {

// Maps a java KeyEvent into a NativeWebKeyboardEvent.
// |java_key_event| is used to maintain a globalref for KeyEvent.
// |action| will help determine the WebInputEvent type.
// type, |modifiers|, |time_ms|, |key_code|, |unicode_char| is used to create
// WebKeyboardEvent. |key_code| is also needed ad need to treat the enter key
// as a key press of character \r.
NativeWebKeyboardEvent NativeWebKeyboardEventFromKeyEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_key_event,
    int action,
    int modifiers,
    long time_ms,
    int key_code,
    int scan_code,
    bool is_system_key,
    int unicode_char) {
  blink::WebInputEvent::Type type = blink::WebInputEvent::Undefined;
  if (action == AKEY_EVENT_ACTION_DOWN)
    type = blink::WebInputEvent::RawKeyDown;
  else if (action == AKEY_EVENT_ACTION_UP)
    type = blink::WebInputEvent::KeyUp;
  else
    NOTREACHED() << "Invalid Android key event action: " << action;
  return NativeWebKeyboardEvent(env, java_key_event, type, modifiers,
                                time_ms / 1000.0, key_code, scan_code,
                                unicode_char, is_system_key);
}

}  // anonymous namespace

bool RegisterImeAdapter(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// Callback from Java to convert BackgroundColorSpan data to a
// blink::WebCompositionUnderline instance, and append it to |underlines_ptr|.
void AppendBackgroundColorSpan(JNIEnv*,
                               const JavaParamRef<jclass>&,
                               jlong underlines_ptr,
                               jint start,
                               jint end,
                               jint background_color) {
  DCHECK_GE(start, 0);
  DCHECK_GE(end, 0);
  // Do not check |background_color|.
  std::vector<blink::WebCompositionUnderline>* underlines =
      reinterpret_cast<std::vector<blink::WebCompositionUnderline>*>(
          underlines_ptr);
  underlines->push_back(
      blink::WebCompositionUnderline(static_cast<unsigned>(start),
                                     static_cast<unsigned>(end),
                                     SK_ColorTRANSPARENT,
                                     false,
                                     static_cast<unsigned>(background_color)));
}

// Callback from Java to convert UnderlineSpan data to a
// blink::WebCompositionUnderline instance, and append it to |underlines_ptr|.
void AppendUnderlineSpan(JNIEnv*,
                         const JavaParamRef<jclass>&,
                         jlong underlines_ptr,
                         jint start,
                         jint end) {
  DCHECK_GE(start, 0);
  DCHECK_GE(end, 0);
  std::vector<blink::WebCompositionUnderline>* underlines =
      reinterpret_cast<std::vector<blink::WebCompositionUnderline>*>(
          underlines_ptr);
  underlines->push_back(
      blink::WebCompositionUnderline(static_cast<unsigned>(start),
                                     static_cast<unsigned>(end),
                                     SK_ColorBLACK,
                                     false,
                                     SK_ColorTRANSPARENT));
}

ImeAdapterAndroid::ImeAdapterAndroid(RenderWidgetHostViewAndroid* rwhva)
    : rwhva_(rwhva) {
}

ImeAdapterAndroid::~ImeAdapterAndroid() {
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ime_adapter_.get(env);
  if (!obj.is_null())
    Java_ImeAdapter_detach(env, obj.obj());
}

bool ImeAdapterAndroid::SendSyntheticKeyEvent(JNIEnv*,
                                              const JavaParamRef<jobject>&,
                                              int type,
                                              long time_ms,
                                              int key_code,
                                              int modifiers,
                                              int text) {
  NativeWebKeyboardEvent event(static_cast<blink::WebInputEvent::Type>(type),
                               modifiers, time_ms / 1000.0, key_code, 0,
                               text, false /* is_system_key */);
  rwhva_->SendKeyEvent(event);
  return true;
}

bool ImeAdapterAndroid::SendKeyEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    const JavaParamRef<jobject>& original_key_event,
    int action,
    int modifiers,
    long time_ms,
    int key_code,
    int scan_code,
    bool is_system_key,
    int unicode_char) {
  NativeWebKeyboardEvent event = NativeWebKeyboardEventFromKeyEvent(
          env, original_key_event, action, modifiers,
          time_ms, key_code, scan_code, is_system_key, unicode_char);
  bool key_down_text_insertion =
      event.type == blink::WebInputEvent::RawKeyDown && event.text[0];
  // If we are going to follow up with a synthetic Char event, then that's the
  // one we expect to test if it's handled or unhandled, so skip handling the
  // "real" event in the browser.
  event.skip_in_browser = key_down_text_insertion;
  rwhva_->SendKeyEvent(event);
  if (key_down_text_insertion) {
    // Send a Char event, but without an os_event since we don't want to
    // roundtrip back to java such synthetic event.
    NativeWebKeyboardEvent char_event(blink::WebInputEvent::Char, modifiers,
                                      time_ms / 1000.0, key_code, scan_code,
                                      unicode_char,
                                      is_system_key);
    char_event.skip_in_browser = key_down_text_insertion;
    rwhva_->SendKeyEvent(char_event);
  }
  return true;
}

void ImeAdapterAndroid::SetComposingText(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         const JavaParamRef<jobject>& text,
                                         const JavaParamRef<jstring>& text_str,
                                         int new_cursor_pos) {
  RenderWidgetHostImpl* rwhi = GetRenderWidgetHostImpl();
  if (!rwhi)
    return;

  base::string16 text16 = ConvertJavaStringToUTF16(env, text_str);

  std::vector<blink::WebCompositionUnderline> underlines;
  // Iterate over spans in |text|, dispatch those that we care about (e.g.,
  // BackgroundColorSpan) to a matching callback (e.g.,
  // AppendBackgroundColorSpan()), and populate |underlines|.
  Java_ImeAdapter_populateUnderlinesFromSpans(
      env, obj, text, reinterpret_cast<jlong>(&underlines));

  // Default to plain underline if we didn't find any span that we care about.
  if (underlines.empty()) {
    underlines.push_back(blink::WebCompositionUnderline(
        0, text16.length(), SK_ColorBLACK, false, SK_ColorTRANSPARENT));
  }
  // Sort spans by |.startOffset|.
  std::sort(underlines.begin(), underlines.end());

  // new_cursor_position is as described in the Android API for
  // InputConnection#setComposingText, whereas the parameters for
  // ImeSetComposition are relative to the start of the composition.
  if (new_cursor_pos > 0)
    new_cursor_pos = text16.length() + new_cursor_pos - 1;

  rwhi->ImeSetComposition(text16, underlines, gfx::Range::InvalidRange(),
                          new_cursor_pos, new_cursor_pos);
}

void ImeAdapterAndroid::CommitText(JNIEnv* env,
                                   const JavaParamRef<jobject>&,
                                   const JavaParamRef<jstring>& text_str) {
  RenderWidgetHostImpl* rwhi = GetRenderWidgetHostImpl();
  if (!rwhi)
    return;

  base::string16 text16 = ConvertJavaStringToUTF16(env, text_str);
  rwhi->ImeConfirmComposition(text16, gfx::Range::InvalidRange(), false);
}

void ImeAdapterAndroid::FinishComposingText(JNIEnv* env,
                                            const JavaParamRef<jobject>&) {
  RenderWidgetHostImpl* rwhi = GetRenderWidgetHostImpl();
  if (!rwhi)
    return;

  rwhi->ImeConfirmComposition(base::string16(), gfx::Range::InvalidRange(),
                              true);
}

void ImeAdapterAndroid::AttachImeAdapter(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object) {
  java_ime_adapter_ = JavaObjectWeakGlobalRef(env, java_object);
}

void ImeAdapterAndroid::CancelComposition() {
  base::android::ScopedJavaLocalRef<jobject> obj =
      java_ime_adapter_.get(AttachCurrentThread());
  if (!obj.is_null())
    Java_ImeAdapter_cancelComposition(AttachCurrentThread(), obj.obj());
}

void ImeAdapterAndroid::FocusedNodeChanged(bool is_editable_node) {
  base::android::ScopedJavaLocalRef<jobject> obj =
      java_ime_adapter_.get(AttachCurrentThread());
  if (!obj.is_null()) {
    Java_ImeAdapter_focusedNodeChanged(AttachCurrentThread(),
                                       obj.obj(),
                                       is_editable_node);
  }
}

void ImeAdapterAndroid::SetEditableSelectionOffsets(
    JNIEnv*,
    const JavaParamRef<jobject>&,
    int start,
    int end) {
  RenderFrameHost* rfh = GetFocusedFrame();
  if (!rfh)
    return;

  rfh->Send(new InputMsg_SetEditableSelectionOffsets(rfh->GetRoutingID(), start,
                                                     end));
}

void ImeAdapterAndroid::SetCharacterBounds(
    const std::vector<gfx::RectF>& character_bounds) {
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ime_adapter_.get(env);
  if (obj.is_null())
    return;

  const size_t coordinates_array_size = character_bounds.size() * 4;
  std::unique_ptr<float[]> coordinates_array(new float[coordinates_array_size]);
  for (size_t i = 0; i < character_bounds.size(); ++i) {
    const gfx::RectF& rect = character_bounds[i];
    const size_t coordinates_array_index = i * 4;
    coordinates_array[coordinates_array_index + 0] = rect.x();
    coordinates_array[coordinates_array_index + 1] = rect.y();
    coordinates_array[coordinates_array_index + 2] = rect.right();
    coordinates_array[coordinates_array_index + 3] = rect.bottom();
  }
  Java_ImeAdapter_setCharacterBounds(
      env,
      obj.obj(),
      base::android::ToJavaFloatArray(env,
                                      coordinates_array.get(),
                                      coordinates_array_size).obj());
}

void ImeAdapterAndroid::SetComposingRegion(JNIEnv*,
                                           const JavaParamRef<jobject>&,
                                           int start,
                                           int end) {
  RenderFrameHost* rfh = GetFocusedFrame();
  if (!rfh)
    return;

  std::vector<blink::WebCompositionUnderline> underlines;
  underlines.push_back(blink::WebCompositionUnderline(
      0, end - start, SK_ColorBLACK, false, SK_ColorTRANSPARENT));

  rfh->Send(new InputMsg_SetCompositionFromExistingText(
      rfh->GetRoutingID(), start, end, underlines));
}

void ImeAdapterAndroid::DeleteSurroundingText(JNIEnv*,
                                              const JavaParamRef<jobject>&,
                                              int before,
                                              int after) {
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(GetFocusedFrame());
  if (rfh)
    rfh->ExtendSelectionAndDelete(before, after);
}

bool ImeAdapterAndroid::RequestTextInputStateUpdate(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  RenderWidgetHostImpl* rwhi = GetRenderWidgetHostImpl();
  if (!rwhi)
    return false;
  rwhi->Send(new InputMsg_RequestTextInputStateUpdate(rwhi->GetRoutingID()));
  return true;
}

bool ImeAdapterAndroid::IsImeThreadEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&) {
  return base::FeatureList::IsEnabled(features::kImeThread);
}

void ImeAdapterAndroid::ResetImeAdapter(JNIEnv* env,
                                        const JavaParamRef<jobject>&) {
  java_ime_adapter_.reset();
}

RenderWidgetHostImpl* ImeAdapterAndroid::GetRenderWidgetHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(rwhva_);
  RenderWidgetHost* rwh = rwhva_->GetRenderWidgetHost();
  if (!rwh)
    return nullptr;

  return RenderWidgetHostImpl::From(rwh);
}

RenderFrameHost* ImeAdapterAndroid::GetFocusedFrame() {
  RenderWidgetHostImpl* rwh = GetRenderWidgetHostImpl();
  if (!rwh)
    return nullptr;
  RenderViewHost* rvh = RenderViewHost::From(rwh);
  if (!rvh)
    return nullptr;
  FrameTreeNode* focused_frame =
      rvh->GetDelegate()->GetFrameTree()->GetFocusedFrame();
  if (!focused_frame)
    return nullptr;

  return focused_frame->current_frame_host();
}

WebContents* ImeAdapterAndroid::GetWebContents() {
  RenderWidgetHostImpl* rwh = GetRenderWidgetHostImpl();
  if (!rwh)
    return nullptr;
  return WebContents::FromRenderViewHost(RenderViewHost::From(rwh));
}

}  // namespace content
