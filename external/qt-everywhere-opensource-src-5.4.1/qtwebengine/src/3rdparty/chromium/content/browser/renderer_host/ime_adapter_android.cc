// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ime_adapter_android.h"

#include <algorithm>
#include <android/input.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "jni/ImeAdapter_jni.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

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
    jobject java_key_event,
    int action,
    int modifiers,
    long time_ms,
    int key_code,
    bool is_system_key,
    int unicode_char) {
  blink::WebInputEvent::Type type = blink::WebInputEvent::Undefined;
  if (action == AKEY_EVENT_ACTION_DOWN)
    type = blink::WebInputEvent::RawKeyDown;
  else if (action == AKEY_EVENT_ACTION_UP)
    type = blink::WebInputEvent::KeyUp;
  return NativeWebKeyboardEvent(java_key_event, type, modifiers,
      time_ms / 1000.0, key_code, unicode_char, is_system_key);
}

}  // anonymous namespace

bool RegisterImeAdapter(JNIEnv* env) {
  if (!RegisterNativesImpl(env))
    return false;

  Java_ImeAdapter_initializeWebInputEvents(env,
                                           blink::WebInputEvent::RawKeyDown,
                                           blink::WebInputEvent::KeyUp,
                                           blink::WebInputEvent::Char,
                                           blink::WebInputEvent::ShiftKey,
                                           blink::WebInputEvent::AltKey,
                                           blink::WebInputEvent::ControlKey,
                                           blink::WebInputEvent::CapsLockOn,
                                           blink::WebInputEvent::NumLockOn);
  Java_ImeAdapter_initializeTextInputTypes(
      env,
      ui::TEXT_INPUT_TYPE_NONE,
      ui::TEXT_INPUT_TYPE_TEXT,
      ui::TEXT_INPUT_TYPE_TEXT_AREA,
      ui::TEXT_INPUT_TYPE_PASSWORD,
      ui::TEXT_INPUT_TYPE_SEARCH,
      ui::TEXT_INPUT_TYPE_URL,
      ui::TEXT_INPUT_TYPE_EMAIL,
      ui::TEXT_INPUT_TYPE_TELEPHONE,
      ui::TEXT_INPUT_TYPE_NUMBER,
      ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE);
  return true;
}

// Callback from Java to convert BackgroundColorSpan data to a
// blink::WebCompositionUnderline instance, and append it to |underlines_ptr|.
void AppendBackgroundColorSpan(JNIEnv*,
                               jclass,
                               jlong underlines_ptr,
                               jint start,
                               jint end,
                               jint background_color) {
  DCHECK(start >= 0);
  DCHECK(end >= 0);
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
                         jclass,
                         jlong underlines_ptr,
                         jint start,
                         jint end) {
  DCHECK(start >= 0);
  DCHECK(end >= 0);
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
                                              jobject,
                                              int type,
                                              long time_ms,
                                              int key_code,
                                              int text) {
  NativeWebKeyboardEvent event(static_cast<blink::WebInputEvent::Type>(type),
                               0 /* modifiers */, time_ms / 1000.0, key_code,
                               text, false /* is_system_key */);
  rwhva_->SendKeyEvent(event);
  return true;
}

bool ImeAdapterAndroid::SendKeyEvent(JNIEnv* env, jobject,
                                     jobject original_key_event,
                                     int action, int modifiers,
                                     long time_ms, int key_code,
                                     bool is_system_key, int unicode_char) {
  NativeWebKeyboardEvent event = NativeWebKeyboardEventFromKeyEvent(
          env, original_key_event, action, modifiers,
          time_ms, key_code, is_system_key, unicode_char);
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
                                      time_ms / 1000.0, key_code, unicode_char,
                                      is_system_key);
    char_event.skip_in_browser = key_down_text_insertion;
    rwhva_->SendKeyEvent(char_event);
  }
  return true;
}

void ImeAdapterAndroid::SetComposingText(JNIEnv* env,
                                         jobject obj,
                                         jobject text,
                                         jstring text_str,
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

  rwhi->ImeSetComposition(text16, underlines, new_cursor_pos, new_cursor_pos);
}

void ImeAdapterAndroid::CommitText(JNIEnv* env, jobject, jstring text_str) {
  RenderWidgetHostImpl* rwhi = GetRenderWidgetHostImpl();
  if (!rwhi)
    return;

  base::string16 text16 = ConvertJavaStringToUTF16(env, text_str);
  rwhi->ImeConfirmComposition(text16, gfx::Range::InvalidRange(), false);
}

void ImeAdapterAndroid::FinishComposingText(JNIEnv* env, jobject) {
  RenderWidgetHostImpl* rwhi = GetRenderWidgetHostImpl();
  if (!rwhi)
    return;

  rwhi->ImeConfirmComposition(base::string16(), gfx::Range::InvalidRange(),
                              true);
}

void ImeAdapterAndroid::AttachImeAdapter(JNIEnv* env, jobject java_object) {
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

void ImeAdapterAndroid::SetEditableSelectionOffsets(JNIEnv*, jobject,
                                                    int start, int end) {
  RenderFrameHost* rfh = GetFocusedFrame();
  if (!rfh)
    return;

  rfh->Send(new FrameMsg_SetEditableSelectionOffsets(rfh->GetRoutingID(),
                                                     start, end));
}

void ImeAdapterAndroid::SetComposingRegion(JNIEnv*, jobject,
                                           int start, int end) {
  RenderFrameHost* rfh = GetFocusedFrame();
  if (!rfh)
    return;

  std::vector<blink::WebCompositionUnderline> underlines;
  underlines.push_back(blink::WebCompositionUnderline(
      0, end - start, SK_ColorBLACK, false, SK_ColorTRANSPARENT));

  rfh->Send(new FrameMsg_SetCompositionFromExistingText(
      rfh->GetRoutingID(), start, end, underlines));
}

void ImeAdapterAndroid::DeleteSurroundingText(JNIEnv*, jobject,
                                              int before, int after) {
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(GetFocusedFrame());
  if (rfh)
    rfh->ExtendSelectionAndDelete(before, after);
}

void ImeAdapterAndroid::Unselect(JNIEnv* env, jobject) {
  WebContents* wc = GetWebContents();
  if (wc)
    wc->Unselect();
}

void ImeAdapterAndroid::SelectAll(JNIEnv* env, jobject) {
  WebContents* wc = GetWebContents();
  if (wc)
    wc->SelectAll();
}

void ImeAdapterAndroid::Cut(JNIEnv* env, jobject) {
  WebContents* wc = GetWebContents();
  if (wc)
    wc->Cut();
}

void ImeAdapterAndroid::Copy(JNIEnv* env, jobject) {
  WebContents* wc = GetWebContents();
  if (wc)
    wc->Copy();
}

void ImeAdapterAndroid::Paste(JNIEnv* env, jobject) {
  WebContents* wc = GetWebContents();
  if (wc)
    wc->Paste();
}

void ImeAdapterAndroid::ResetImeAdapter(JNIEnv* env, jobject) {
  java_ime_adapter_.reset();
}

RenderWidgetHostImpl* ImeAdapterAndroid::GetRenderWidgetHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(rwhva_);
  RenderWidgetHost* rwh = rwhva_->GetRenderWidgetHost();
  if (!rwh)
    return NULL;

  return RenderWidgetHostImpl::From(rwh);
}

RenderFrameHost* ImeAdapterAndroid::GetFocusedFrame() {
  RenderWidgetHostImpl* rwh = GetRenderWidgetHostImpl();
  if (!rwh)
    return NULL;
  if (!rwh->IsRenderView())
    return NULL;
  RenderViewHost* rvh = RenderViewHost::From(rwh);
  FrameTreeNode* focused_frame =
      rvh->GetDelegate()->GetFrameTree()->GetFocusedFrame();
  if (!focused_frame)
    return NULL;

  return focused_frame->current_frame_host();
}

WebContents* ImeAdapterAndroid::GetWebContents() {
  RenderWidgetHostImpl* rwh = GetRenderWidgetHostImpl();
  if (!rwh)
    return NULL;
  if (!rwh->IsRenderView())
    return NULL;
  return WebContents::FromRenderViewHost(RenderViewHost::From(rwh));
}

}  // namespace content
