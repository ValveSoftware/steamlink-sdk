// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_android.h"

#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/media/android/media_web_contents_observer_android.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/devtools_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "jni/WebContentsImpl_jni.h"
#include "net/android/network_library.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ToJavaIntArray;

namespace content {

namespace {

// Track all WebContentsAndroid objects here so that we don't deserialize a
// destroyed WebContents object.
base::LazyInstance<base::hash_set<WebContentsAndroid*> >::Leaky
    g_allocated_web_contents_androids = LAZY_INSTANCE_INITIALIZER;

void JavaScriptResultCallback(const ScopedJavaGlobalRef<jobject>& callback,
                              const base::Value* result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json;
  base::JSONWriter::Write(*result, &json);
  ScopedJavaLocalRef<jstring> j_json = ConvertUTF8ToJavaString(env, json);
  Java_WebContentsImpl_onEvaluateJavaScriptResult(
      env, j_json.obj(), callback.obj());
}

struct AccessibilitySnapshotParams {
  AccessibilitySnapshotParams()
      : has_tree_data(false), should_select_leaf_nodes(false) {}

  bool has_tree_data;
  // The current text selection within this tree, if any, expressed as the
  // node ID and character offset of the anchor (selection start) and focus
  // (selection end).
  int32_t sel_anchor_object_id;
  int32_t sel_anchor_offset;
  int32_t sel_focus_object_id;
  int32_t sel_focus_offset;
  // if the flag is true, mark the leaf node as selected.
  bool should_select_leaf_nodes;
};

ScopedJavaLocalRef<jobject> WalkAXTreeDepthFirst(
    JNIEnv* env,
    BrowserAccessibilityAndroid* node,
    const gfx::Rect& parent_rect,
    AccessibilitySnapshotParams* params) {
  ScopedJavaLocalRef<jstring> j_text =
      ConvertUTF16ToJavaString(env, node->GetText());
  ScopedJavaLocalRef<jstring> j_class =
      ConvertUTF8ToJavaString(env, node->GetClassName());
  // The style attributes exists and valid if size attribute exists. Otherwise,
  // they are not. Use a negative size information to indicate the existence
  // of style information.
  float size = -1.0;
  int color = 0;
  int bgcolor = 0;
  int text_style = 0;

  if (node->HasFloatAttribute(ui::AX_ATTR_FONT_SIZE)) {
    color = node->GetIntAttribute(ui::AX_ATTR_COLOR);
    bgcolor = node->GetIntAttribute(ui::AX_ATTR_BACKGROUND_COLOR);
    size =  node->GetFloatAttribute(ui::AX_ATTR_FONT_SIZE);
    text_style = node->GetIntAttribute(ui::AX_ATTR_TEXT_STYLE);
  }

  const gfx::Rect& absolute_rect = node->GetLocalBoundsRect();
  gfx::Rect parent_relative_rect = absolute_rect;
  bool is_root = node->GetParent() == nullptr;
  if (!is_root) {
    parent_relative_rect.Offset(-parent_rect.OffsetFromOrigin());
  }
  ScopedJavaLocalRef<jobject> j_node =
      Java_WebContentsImpl_createAccessibilitySnapshotNode(
          env, parent_relative_rect.x(), parent_relative_rect.y(),
          absolute_rect.width(), absolute_rect.height(), is_root, j_text.obj(),
          color, bgcolor, size, text_style, j_class.obj());

  if (params->has_tree_data && node->PlatformIsLeaf()) {
    int start_selection = 0;
    int end_selection = 0;
    if (params->sel_anchor_object_id == node->GetId()) {
      start_selection = params->sel_anchor_offset;
      params->should_select_leaf_nodes = true;
    }
    if (params->should_select_leaf_nodes)
      end_selection = node->GetText().length();

    if (params->sel_focus_object_id == node->GetId()) {
      end_selection = params->sel_focus_offset;
      params->should_select_leaf_nodes = false;
    }
    if (end_selection > 0)
      Java_WebContentsImpl_setAccessibilitySnapshotSelection(
          env, j_node.obj(), start_selection, end_selection);
  }

  for (uint32_t i = 0; i < node->PlatformChildCount(); i++) {
    BrowserAccessibilityAndroid* child =
        static_cast<BrowserAccessibilityAndroid*>(
            node->PlatformGetChild(i));
    Java_WebContentsImpl_addAccessibilityNodeAsChild(
        env, j_node.obj(),
        WalkAXTreeDepthFirst(env, child, absolute_rect, params).obj());
  }
  return j_node;
}

// Walks over the AXTreeUpdate and creates a light weight snapshot.
void AXTreeSnapshotCallback(const ScopedJavaGlobalRef<jobject>& callback,
                            const ui::AXTreeUpdate& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (result.nodes.empty()) {
    Java_WebContentsImpl_onAccessibilitySnapshot(env, nullptr, callback.obj());
    return;
  }
  std::unique_ptr<BrowserAccessibilityManagerAndroid> manager(
      static_cast<BrowserAccessibilityManagerAndroid*>(
          BrowserAccessibilityManager::Create(result, nullptr)));
  manager->set_prune_tree_for_screen_reader(false);
  BrowserAccessibilityAndroid* root =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetRoot());
  AccessibilitySnapshotParams params;
  if (result.has_tree_data) {
    params.has_tree_data = true;
    params.sel_anchor_object_id = result.tree_data.sel_anchor_object_id;
    params.sel_anchor_offset = result.tree_data.sel_anchor_offset;
    params.sel_focus_object_id = result.tree_data.sel_focus_object_id;
    params.sel_focus_offset = result.tree_data.sel_focus_offset;
  }
  gfx::Rect parent_rect;
  ScopedJavaLocalRef<jobject> j_root =
      WalkAXTreeDepthFirst(env, root, parent_rect, &params);
  Java_WebContentsImpl_onAccessibilitySnapshot(
      env, j_root.obj(), callback.obj());
}

}  // namespace

// static
WebContents* WebContents::FromJavaWebContents(
    jobject jweb_contents_android) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!jweb_contents_android)
    return NULL;

  WebContentsAndroid* web_contents_android =
      reinterpret_cast<WebContentsAndroid*>(
          Java_WebContentsImpl_getNativePointer(AttachCurrentThread(),
                                                jweb_contents_android));
  if (!web_contents_android)
    return NULL;
  return web_contents_android->web_contents();
}

// static
static void DestroyWebContents(JNIEnv* env,
                               const JavaParamRef<jclass>& clazz,
                               jlong jweb_contents_android_ptr) {
  WebContentsAndroid* web_contents_android =
      reinterpret_cast<WebContentsAndroid*>(jweb_contents_android_ptr);
  if (!web_contents_android)
    return;

  WebContents* web_contents = web_contents_android->web_contents();
  if (!web_contents)
    return;

  delete web_contents;
}

// static
ScopedJavaLocalRef<jobject> FromNativePtr(JNIEnv* env,
                                          const JavaParamRef<jclass>& clazz,
                                          jlong web_contents_ptr) {
  WebContentsAndroid* web_contents_android =
      reinterpret_cast<WebContentsAndroid*>(web_contents_ptr);

  if (!web_contents_android)
    return ScopedJavaLocalRef<jobject>();

  // Check to make sure this object hasn't been destroyed.
  if (g_allocated_web_contents_androids.Get().find(web_contents_android) ==
      g_allocated_web_contents_androids.Get().end()) {
    return ScopedJavaLocalRef<jobject>();
  }

  return web_contents_android->GetJavaObject();
}

// static
bool WebContentsAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

WebContentsAndroid::WebContentsAndroid(WebContentsImpl* web_contents)
    : web_contents_(web_contents),
      navigation_controller_(&(web_contents->GetController())),
      synchronous_compositor_client_(nullptr),
      weak_factory_(this) {
  g_allocated_web_contents_androids.Get().insert(this);
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(env,
             Java_WebContentsImpl_create(
                 env,
                 reinterpret_cast<intptr_t>(this),
                 navigation_controller_.GetJavaObject().obj()).obj());
  RendererPreferences* prefs = web_contents_->GetMutableRendererPrefs();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  prefs->network_contry_iso =
      command_line->HasSwitch(switches::kNetworkCountryIso) ?
          command_line->GetSwitchValueASCII(switches::kNetworkCountryIso)
          : net::android::GetTelephonyNetworkCountryIso();
}

WebContentsAndroid::~WebContentsAndroid() {
  DCHECK(g_allocated_web_contents_androids.Get().find(this) !=
      g_allocated_web_contents_androids.Get().end());
  g_allocated_web_contents_androids.Get().erase(this);
  Java_WebContentsImpl_clearNativePtr(AttachCurrentThread(), obj_.obj());
}

base::android::ScopedJavaLocalRef<jobject>
WebContentsAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetTitle(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return base::android::ConvertUTF16ToJavaString(env,
                                                 web_contents_->GetTitle());
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetVisibleURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return base::android::ConvertUTF8ToJavaString(
      env, web_contents_->GetVisibleURL().spec());
}

bool WebContentsAndroid::IsLoading(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) const {
  return web_contents_->IsLoading();
}

bool WebContentsAndroid::IsLoadingToDifferentDocument(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return web_contents_->IsLoadingToDifferentDocument();
}

void WebContentsAndroid::Stop(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->Stop();
}

void WebContentsAndroid::Cut(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->Cut();
}

void WebContentsAndroid::Copy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->Copy();
}

void WebContentsAndroid::Paste(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->Paste();
}

void WebContentsAndroid::Replace(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 const JavaParamRef<jstring>& jstr) {
  web_contents_->Replace(base::android::ConvertJavaStringToUTF16(env, jstr));
}

void WebContentsAndroid::SelectAll(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  web_contents_->SelectAll();
}

void WebContentsAndroid::Unselect(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  web_contents_->Unselect();
}

void WebContentsAndroid::InsertCSS(JNIEnv* env,
                                   const JavaParamRef<jobject>& jobj,
                                   const JavaParamRef<jstring>& jcss) {
  web_contents_->InsertCSS(base::android::ConvertJavaStringToUTF8(env, jcss));
}

RenderWidgetHostViewAndroid*
    WebContentsAndroid::GetRenderWidgetHostViewAndroid() {
  RenderWidgetHostView* rwhv = NULL;
  rwhv = web_contents_->GetRenderWidgetHostView();
  if (web_contents_->ShowingInterstitialPage()) {
    rwhv = web_contents_->GetInterstitialPage()
               ->GetMainFrame()
               ->GetRenderViewHost()
               ->GetWidget()
               ->GetView();
  }
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

jint WebContentsAndroid::GetBackgroundColor(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return SK_ColorWHITE;
  return rwhva->GetCachedBackgroundColor();
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return ConvertUTF8ToJavaString(env, web_contents_->GetURL().spec());
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetLastCommittedURL(
    JNIEnv* env,
    const JavaParamRef<jobject>&) const {
  return ConvertUTF8ToJavaString(env,
                                 web_contents_->GetLastCommittedURL().spec());
}

jboolean WebContentsAndroid::IsIncognito(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return web_contents_->GetBrowserContext()->IsOffTheRecord();
}

void WebContentsAndroid::ResumeLoadingCreatedWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  web_contents_->ResumeLoadingCreatedWebContents();
}

void WebContentsAndroid::OnHide(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->WasHidden();
}

void WebContentsAndroid::OnShow(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->WasShown();
}

void WebContentsAndroid::SuspendAllMediaPlayers(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  MediaWebContentsObserverAndroid::FromWebContents(web_contents_)
      ->SuspendAllMediaPlayers();
}

void WebContentsAndroid::SetAudioMuted(JNIEnv* env,
                                       const JavaParamRef<jobject>& jobj,
                                       jboolean mute) {
  web_contents_->SetAudioMuted(mute);
}

void WebContentsAndroid::ShowInterstitialPage(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              const JavaParamRef<jstring>& jurl,
                                              jlong delegate_ptr) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  InterstitialPageDelegateAndroid* delegate =
      reinterpret_cast<InterstitialPageDelegateAndroid*>(delegate_ptr);
  InterstitialPage* interstitial = InterstitialPage::Create(
      web_contents_, false, url, delegate);
  delegate->set_interstitial_page(interstitial);
  interstitial->Show();
}

jboolean WebContentsAndroid::IsShowingInterstitialPage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return web_contents_->ShowingInterstitialPage();
}

jboolean WebContentsAndroid::FocusLocationBarByDefault(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return web_contents_->FocusLocationBarByDefault();
}

jboolean WebContentsAndroid::IsRenderWidgetHostViewReady(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  return view && view->HasValidFrame();
}

void WebContentsAndroid::ExitFullscreen(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  web_contents_->ExitFullscreen(/*will_cause_resize=*/false);
}

void WebContentsAndroid::UpdateTopControlsState(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    bool enable_hiding,
    bool enable_showing,
    bool animate) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (!host)
    return;
  host->Send(new ViewMsg_UpdateTopControlsState(host->GetRoutingID(),
                                                enable_hiding,
                                                enable_showing,
                                                animate));
}

void WebContentsAndroid::ShowImeIfNeeded(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (!host)
    return;
  host->Send(new ViewMsg_ShowImeIfNeeded(host->GetRoutingID()));
}

void WebContentsAndroid::ScrollFocusedEditableNodeIntoView(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (!host)
    return;
  host->Send(new InputMsg_ScrollFocusedEditableNodeIntoRect(
      host->GetRoutingID(), gfx::Rect()));
}

void WebContentsAndroid::SelectWordAroundCaret(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (!host)
    return;
  host->SelectWordAroundCaret();
}

void WebContentsAndroid::AdjustSelectionByCharacterOffset(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint start_adjust,
    jint end_adjust) {
  web_contents_->AdjustSelectionByCharacterOffset(start_adjust, end_adjust);
}

void WebContentsAndroid::EvaluateJavaScript(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& script,
    const JavaParamRef<jobject>& callback) {
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  DCHECK(rvh);

  if (!rvh->IsRenderViewLive()) {
    if (!static_cast<WebContentsImpl*>(web_contents_)->
        CreateRenderViewForInitialEmptyDocument()) {
      LOG(ERROR) << "Failed to create RenderView in EvaluateJavaScript";
      return;
    }
  }

  if (!callback) {
    // No callback requested.
    web_contents_->GetMainFrame()->ExecuteJavaScript(
        ConvertJavaStringToUTF16(env, script));
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  RenderFrameHost::JavaScriptResultCallback js_callback =
      base::Bind(&JavaScriptResultCallback, j_callback);

  web_contents_->GetMainFrame()->ExecuteJavaScript(
      ConvertJavaStringToUTF16(env, script), js_callback);
}

void WebContentsAndroid::EvaluateJavaScriptForTests(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& script,
    const JavaParamRef<jobject>& callback) {
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  DCHECK(rvh);

  if (!rvh->IsRenderViewLive()) {
    if (!static_cast<WebContentsImpl*>(web_contents_)->
        CreateRenderViewForInitialEmptyDocument()) {
      LOG(ERROR) << "Failed to create RenderView in EvaluateJavaScriptForTests";
      return;
    }
  }

  if (!callback) {
    // No callback requested.
    web_contents_->GetMainFrame()->ExecuteJavaScriptForTests(
        ConvertJavaStringToUTF16(env, script));
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  RenderFrameHost::JavaScriptResultCallback js_callback =
      base::Bind(&JavaScriptResultCallback, j_callback);

  web_contents_->GetMainFrame()->ExecuteJavaScriptForTests(
      ConvertJavaStringToUTF16(env, script), js_callback);
}

void WebContentsAndroid::AddMessageToDevToolsConsole(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint level,
    const JavaParamRef<jstring>& message) {
  DCHECK_GE(level, 0);
  DCHECK_LE(level, CONSOLE_MESSAGE_LEVEL_LAST);

  web_contents_->GetMainFrame()->AddMessageToConsole(
      static_cast<ConsoleMessageLevel>(level),
      ConvertJavaStringToUTF8(env, message));
}

void WebContentsAndroid::SendMessageToFrame(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& frame_name,
    const JavaParamRef<jstring>& message,
    const JavaParamRef<jstring>& target_origin) {
  base::string16 source_origin;
  base::string16 j_target_origin(ConvertJavaStringToUTF16(env, target_origin));
  base::string16 j_message(ConvertJavaStringToUTF16(env, message));
  std::vector<int> ports;
  content::MessagePortProvider::PostMessageToFrame(
      web_contents_, source_origin, j_target_origin, j_message, ports);
}

jboolean WebContentsAndroid::HasAccessedInitialDocument(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  return static_cast<WebContentsImpl*>(web_contents_)->
      HasAccessedInitialDocument();
}

jint WebContentsAndroid::GetThemeColor(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  return web_contents_->GetThemeColor();
}

void WebContentsAndroid::RequestAccessibilitySnapshot(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& callback) {
  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  gfx::DeviceDisplayInfo device_info;

  WebContentsImpl::AXTreeSnapshotCallback snapshot_callback =
      base::Bind(&AXTreeSnapshotCallback, j_callback);
  static_cast<WebContentsImpl*>(web_contents_)->RequestAXTreeSnapshot(
      snapshot_callback);
}

void WebContentsAndroid::ResumeMediaSession(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  web_contents_->ResumeMediaSession();
}

void WebContentsAndroid::SuspendMediaSession(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  web_contents_->SuspendMediaSession();
}

void WebContentsAndroid::StopMediaSession(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  web_contents_->StopMediaSession();
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetEncoding(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return base::android::ConvertUTF8ToJavaString(env,
                                                web_contents_->GetEncoding());
}

void WebContentsAndroid::GetContentBitmap(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jcallback,
    const JavaParamRef<jobject>& color_type,
    jfloat scale,
    jfloat x,
    jfloat y,
    jfloat width,
    jfloat height) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  const ReadbackRequestCallback result_callback =
      base::Bind(&WebContentsAndroid::OnFinishGetContentBitmap,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(new ScopedJavaGlobalRef<jobject>(env, obj)),
                 base::Owned(new ScopedJavaGlobalRef<jobject>(env, jcallback)));
  SkColorType pref_color_type = gfx::ConvertToSkiaColorType(color_type.obj());
  if (!view || pref_color_type == kUnknown_SkColorType) {
    result_callback.Run(SkBitmap(), READBACK_FAILED);
    return;
  }
  if (!view->IsSurfaceAvailableForCopy()) {
    result_callback.Run(SkBitmap(), READBACK_SURFACE_UNAVAILABLE);
    return;
  }
  view->GetScaledContentBitmap(scale,
                               pref_color_type,
                               gfx::Rect(x, y, width, height),
                               result_callback);
}

void WebContentsAndroid::OnContextMenuClosed(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  static_cast<WebContentsImpl*>(web_contents_)
      ->NotifyContextMenuClosed(CustomContextMenuContext());
}

void WebContentsAndroid::ReloadLoFiImages(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  static_cast<WebContentsImpl*>(web_contents_)->ReloadLoFiImages();
}

int WebContentsAndroid::DownloadImage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& jurl,
    jboolean is_fav_icon,
    jint max_bitmap_size,
    jboolean bypass_cache,
    const base::android::JavaParamRef<jobject>& jcallback) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  return web_contents_->DownloadImage(
      url, is_fav_icon, max_bitmap_size, bypass_cache,
      base::Bind(&WebContentsAndroid::OnFinishDownloadImage,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(new ScopedJavaGlobalRef<jobject>(
                     env, obj)),
                 base::Owned(new ScopedJavaGlobalRef<jobject>(
                     env, jcallback))));
}

void WebContentsAndroid::OnFinishGetContentBitmap(
    ScopedJavaGlobalRef<jobject>* obj,
    ScopedJavaGlobalRef<jobject>* callback,
    const SkBitmap& bitmap,
    ReadbackResponse response) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (response == READBACK_SUCCESS)
    java_bitmap = gfx::ConvertToJavaBitmap(&bitmap);
  Java_WebContentsImpl_onGetContentBitmapFinished(env,
                                                  obj->obj(),
                                                  callback->obj(),
                                                  java_bitmap.obj(),
                                                  response);
}

void WebContentsAndroid::OnFinishDownloadImage(
    base::android::ScopedJavaGlobalRef<jobject>* obj,
    base::android::ScopedJavaGlobalRef<jobject>* callback,
    int id,
    int http_status_code,
    const GURL& url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jbitmaps =
      Java_WebContentsImpl_createBitmapList(env);
  ScopedJavaLocalRef<jobject> jsizes =
      Java_WebContentsImpl_createSizeList(env);
  ScopedJavaLocalRef<jstring> jurl =
      base::android::ConvertUTF8ToJavaString(env, url.spec());

  for (const SkBitmap& bitmap : bitmaps) {
    // WARNING: convering to java bitmaps results in duplicate memory
    // allocations, which increases the chance of OOMs if DownloadImage() is
    // misused.
    ScopedJavaLocalRef<jobject> jbitmap = gfx::ConvertToJavaBitmap(&bitmap);
    Java_WebContentsImpl_addToBitmapList(env, jbitmaps.obj(), jbitmap.obj());
  }
  for (const gfx::Size& size : sizes) {
    Java_WebContentsImpl_createSizeAndAddToList(
        env, jsizes.obj(), size.width(), size.height());
  }
  Java_WebContentsImpl_onDownloadImageFinished(
      env, obj->obj(), callback->obj(), id,
      http_status_code, jurl.obj(), jbitmaps.obj(), jsizes.obj());
}
}  // namespace content
