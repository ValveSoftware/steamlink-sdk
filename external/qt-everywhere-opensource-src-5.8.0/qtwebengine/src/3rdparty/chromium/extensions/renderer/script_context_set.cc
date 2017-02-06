// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_context_set.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/extension_groups.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_injection.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {
// There is only ever one instance of the ScriptContextSet.
ScriptContextSet* g_context_set = nullptr;
}

ScriptContextSet::ScriptContextSet(ExtensionIdSet* active_extension_ids)
    : active_extension_ids_(active_extension_ids) {
  DCHECK(!g_context_set);
  g_context_set = this;
}

ScriptContextSet::~ScriptContextSet() {
  g_context_set = nullptr;
}

ScriptContext* ScriptContextSet::Register(
    blink::WebLocalFrame* frame,
    const v8::Local<v8::Context>& v8_context,
    int extension_group,
    int world_id) {
  const Extension* extension =
      GetExtensionFromFrameAndWorld(frame, world_id, false);
  const Extension* effective_extension =
      GetExtensionFromFrameAndWorld(frame, world_id, true);

  GURL frame_url = ScriptContext::GetDataSourceURLForFrame(frame);
  Feature::Context context_type =
      ClassifyJavaScriptContext(extension, extension_group, frame_url,
                                frame->document().getSecurityOrigin());
  Feature::Context effective_context_type = ClassifyJavaScriptContext(
      effective_extension, extension_group,
      ScriptContext::GetEffectiveDocumentURL(frame, frame_url, true),
      frame->document().getSecurityOrigin());

  ScriptContext* context =
      new ScriptContext(v8_context, frame, extension, context_type,
                        effective_extension, effective_context_type);
  contexts_.insert(context);  // takes ownership
  return context;
}

void ScriptContextSet::Remove(ScriptContext* context) {
  if (contexts_.erase(context)) {
    context->Invalidate();
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, context);
  }
}

ScriptContext* ScriptContextSet::GetCurrent() const {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  return isolate->InContext() ? GetByV8Context(isolate->GetCurrentContext())
                              : nullptr;
}

ScriptContext* ScriptContextSet::GetByV8Context(
    const v8::Local<v8::Context>& v8_context) const {
  for (ScriptContext* script_context : contexts_) {
    if (script_context->v8_context() == v8_context)
      return script_context;
  }
  return nullptr;
}

ScriptContext* ScriptContextSet::GetContextByObject(
    const v8::Local<v8::Object>& object) {
  return GetContextByV8Context(object->CreationContext());
}

ScriptContext* ScriptContextSet::GetContextByV8Context(
    const v8::Local<v8::Context>& v8_context) {
  // g_context_set can be null in unittests.
  return g_context_set ? g_context_set->GetByV8Context(v8_context) : nullptr;
}

void ScriptContextSet::ForEach(
    const std::string& extension_id,
    content::RenderFrame* render_frame,
    const base::Callback<void(ScriptContext*)>& callback) const {
  // We copy the context list, because calling into javascript may modify it
  // out from under us.
  std::set<ScriptContext*> contexts_copy = contexts_;

  for (ScriptContext* context : contexts_copy) {
    // For the same reason as above, contexts may become invalid while we run.
    if (!context->is_valid())
      continue;

    if (!extension_id.empty()) {
      const Extension* extension = context->extension();
      if (!extension || (extension_id != extension->id()))
        continue;
    }

    content::RenderFrame* context_render_frame = context->GetRenderFrame();
    if (!context_render_frame)
      continue;

    if (render_frame && render_frame != context_render_frame)
      continue;

    callback.Run(context);
  }
}

std::set<ScriptContext*> ScriptContextSet::OnExtensionUnloaded(
    const std::string& extension_id) {
  std::set<ScriptContext*> removed;
  ForEach(extension_id, base::Bind(&ScriptContextSet::RecordAndRemove,
                                   base::Unretained(this), &removed));
  return removed;
}

const Extension* ScriptContextSet::GetExtensionFromFrameAndWorld(
    const blink::WebLocalFrame* frame,
    int world_id,
    bool use_effective_url) {
  std::string extension_id;
  if (world_id != 0) {
    // Isolated worlds (content script).
    extension_id = ScriptInjection::GetHostIdForIsolatedWorld(world_id);
  } else {
    // For looking up the extension associated with this frame, we either want
    // to use the current url or possibly the data source url (which this frame
    // may be navigating to shortly), depending on the security origin of the
    // frame. We don't always want to use the data source url because some
    // frames (eg iframes and windows created via window.open) briefly contain
    // an about:blank script context that is scriptable by their parent/opener
    // before they finish navigating.
    GURL frame_url = ScriptContext::GetAccessCheckedFrameURL(frame);
    frame_url = ScriptContext::GetEffectiveDocumentURL(frame, frame_url,
                                                       use_effective_url);
    extension_id =
        RendererExtensionRegistry::Get()->GetExtensionOrAppIDByURL(frame_url);
  }

  // There are conditions where despite a context being associated with an
  // extension, no extension actually gets found. Ignore "invalid" because CSP
  // blocks extension page loading by switching the extension ID to "invalid".
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (!extension && !extension_id.empty() && extension_id != "invalid") {
    // TODO(kalman): Do something here?
  }
  return extension;
}

Feature::Context ScriptContextSet::ClassifyJavaScriptContext(
    const Extension* extension,
    int extension_group,
    const GURL& url,
    const blink::WebSecurityOrigin& origin) {
  // WARNING: This logic must match ProcessMap::GetContextType, as much as
  // possible.

  DCHECK_GE(extension_group, 0);
  if (extension_group == EXTENSION_GROUP_CONTENT_SCRIPTS) {
    return extension ?  // TODO(kalman): when does this happen?
               Feature::CONTENT_SCRIPT_CONTEXT
                     : Feature::UNSPECIFIED_CONTEXT;
  }

  // We have an explicit check for sandboxed pages before checking whether the
  // extension is active in this process because:
  // 1. Sandboxed pages run in the same process as regular extension pages, so
  //    the extension is considered active.
  // 2. ScriptContext creation (which triggers bindings injection) happens
  //    before the SecurityContext is updated with the sandbox flags (after
  //    reading the CSP header), so the caller can't check if the context's
  //    security origin is unique yet.
  if (ScriptContext::IsSandboxedPage(url))
    return Feature::WEB_PAGE_CONTEXT;

  if (extension && active_extension_ids_->count(extension->id()) > 0) {
    // |extension| is active in this process, but it could be either a true
    // extension process or within the extent of a hosted app. In the latter
    // case this would usually be considered a (blessed) web page context,
    // unless the extension in question is a component extension, in which case
    // we cheat and call it blessed.
    return (extension->is_hosted_app() &&
            extension->location() != Manifest::COMPONENT)
               ? Feature::BLESSED_WEB_PAGE_CONTEXT
               : Feature::BLESSED_EXTENSION_CONTEXT;
  }

  // TODO(kalman): This isUnique() check is wrong, it should be performed as
  // part of ScriptContext::IsSandboxedPage().
  if (!origin.isUnique() &&
      RendererExtensionRegistry::Get()->ExtensionBindingsAllowed(url)) {
    if (!extension)  // TODO(kalman): when does this happen?
      return Feature::UNSPECIFIED_CONTEXT;
    return extension->is_hosted_app() ? Feature::BLESSED_WEB_PAGE_CONTEXT
                                      : Feature::UNBLESSED_EXTENSION_CONTEXT;
  }

  if (!url.is_valid())
    return Feature::UNSPECIFIED_CONTEXT;

  if (url.SchemeIs(content::kChromeUIScheme))
    return Feature::WEBUI_CONTEXT;

  return Feature::WEB_PAGE_CONTEXT;
}

void ScriptContextSet::RecordAndRemove(std::set<ScriptContext*>* removed,
                                       ScriptContext* context) {
  removed->insert(context);
  Remove(context);  // Note: context deletion is deferred to the message loop.
}

}  // namespace extensions
