// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_ui_mojo.h"

#include "content/common/view_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/web_ui_mojo_context_state.h"
#include "gin/per_context_data.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

namespace content {

namespace {

const char kWebUIMojoContextStateKey[] = "WebUIMojoContextState";

struct WebUIMojoContextStateData : public base::SupportsUserData::Data {
  scoped_ptr<WebUIMojoContextState> state;
};

}  // namespace

WebUIMojo::MainFrameObserver::MainFrameObserver(WebUIMojo* web_ui_mojo)
    : RenderFrameObserver(RenderFrame::FromWebFrame(
          web_ui_mojo->render_view()->GetWebView()->mainFrame())),
      web_ui_mojo_(web_ui_mojo) {
}

WebUIMojo::MainFrameObserver::~MainFrameObserver() {
}

void WebUIMojo::MainFrameObserver::WillReleaseScriptContext(
    v8::Handle<v8::Context> context,
    int world_id) {
  web_ui_mojo_->DestroyContextState(context);
}

void WebUIMojo::MainFrameObserver::DidFinishDocumentLoad() {
  web_ui_mojo_->OnDidFinishDocumentLoad();
}

WebUIMojo::WebUIMojo(RenderView* render_view)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<WebUIMojo>(render_view),
      main_frame_observer_(this),
      did_finish_document_load_(false) {
  CreateContextState();
}

void WebUIMojo::SetBrowserHandle(mojo::ScopedMessagePipeHandle handle) {
  if (did_finish_document_load_)
    SetHandleOnContextState(handle.Pass());
  else
    pending_handle_ = handle.Pass();
}

WebUIMojo::~WebUIMojo() {
}

void WebUIMojo::CreateContextState() {
  v8::HandleScope handle_scope(blink::mainThreadIsolate());
  blink::WebLocalFrame* frame =
      render_view()->GetWebView()->mainFrame()->toWebLocalFrame();
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  gin::PerContextData* context_data = gin::PerContextData::From(context);
  WebUIMojoContextStateData* data = new WebUIMojoContextStateData;
  data->state.reset(new WebUIMojoContextState(
                        render_view()->GetWebView()->mainFrame(), context));
  context_data->SetUserData(kWebUIMojoContextStateKey, data);
}

void WebUIMojo::DestroyContextState(v8::Handle<v8::Context> context) {
  gin::PerContextData* context_data = gin::PerContextData::From(context);
  if (!context_data)
    return;
  context_data->RemoveUserData(kWebUIMojoContextStateKey);
}

void WebUIMojo::OnDidFinishDocumentLoad() {
  did_finish_document_load_ = true;
  if (pending_handle_.is_valid())
    SetHandleOnContextState(pending_handle_.Pass());
}

void WebUIMojo::SetHandleOnContextState(mojo::ScopedMessagePipeHandle handle) {
  DCHECK(did_finish_document_load_);
  v8::HandleScope handle_scope(blink::mainThreadIsolate());
  WebUIMojoContextState* state = GetContextState();
  if (state)
    state->SetHandle(handle.Pass());
}

WebUIMojoContextState* WebUIMojo::GetContextState() {
  blink::WebLocalFrame* frame =
      render_view()->GetWebView()->mainFrame()->toWebLocalFrame();
  v8::HandleScope handle_scope(blink::mainThreadIsolate());
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  gin::PerContextData* context_data = gin::PerContextData::From(context);
  if (!context_data)
    return NULL;
  WebUIMojoContextStateData* context_state =
      static_cast<WebUIMojoContextStateData*>(
          context_data->GetUserData(kWebUIMojoContextStateKey));
  return context_state ? context_state->state.get() : NULL;
}

void WebUIMojo::DidClearWindowObject(blink::WebLocalFrame* frame) {
  if (frame != render_view()->GetWebView()->mainFrame())
    return;

  // NOTE: this function may be called early on twice. From the constructor
  // mainWorldScriptContext() may trigger this to be called. If we are created
  // before the page is loaded (which is very likely), then on first load this
  // is called. In the case of the latter we may have already supplied the
  // handle to the context state so that if we destroy now the handle is
  // lost. If this is the result of the first load then the contextstate should
  // be empty and we don't need to destroy it.
  WebUIMojoContextState* state = GetContextState();
  if (state && !state->module_added())
    return;

  v8::HandleScope handle_scope(blink::mainThreadIsolate());
  DestroyContextState(frame->mainWorldScriptContext());
  CreateContextState();
}

}  // namespace content
