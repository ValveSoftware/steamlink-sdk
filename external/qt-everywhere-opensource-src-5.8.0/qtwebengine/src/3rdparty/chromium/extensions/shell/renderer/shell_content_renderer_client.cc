// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/renderer/shell_content_renderer_client.h"

#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extensions_client.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/dispatcher_delegate.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/renderer/guest_view/extensions_guest_view_container.h"
#include "extensions/renderer/guest_view/extensions_guest_view_container_dispatcher.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container.h"
#include "extensions/shell/common/shell_extensions_client.h"
#include "extensions/shell/renderer/shell_extensions_renderer_client.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

#if !defined(DISABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#include "components/nacl/renderer/nacl_helper.h"
#endif

using blink::WebFrame;
using blink::WebString;
using content::RenderThread;

namespace extensions {

ShellContentRendererClient::ShellContentRendererClient() {
}

ShellContentRendererClient::~ShellContentRendererClient() {
}

void ShellContentRendererClient::RenderThreadStarted() {
  RenderThread* thread = RenderThread::Get();

  extensions_client_.reset(CreateExtensionsClient());
  ExtensionsClient::Set(extensions_client_.get());

  extensions_renderer_client_.reset(new ShellExtensionsRendererClient);
  ExtensionsRendererClient::Set(extensions_renderer_client_.get());

  extension_dispatcher_delegate_.reset(new DispatcherDelegate());

  // Must be initialized after ExtensionsRendererClient.
  extension_dispatcher_.reset(
      new Dispatcher(extension_dispatcher_delegate_.get()));
  thread->AddObserver(extension_dispatcher_.get());

  guest_view_container_dispatcher_.reset(
      new ExtensionsGuestViewContainerDispatcher());
  thread->AddObserver(guest_view_container_dispatcher_.get());
}

void ShellContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // ExtensionFrameHelper destroys itself when the RenderFrame is destroyed.
  new ExtensionFrameHelper(render_frame, extension_dispatcher_.get());
  extension_dispatcher_->OnRenderFrameCreated(render_frame);

  // TODO(jamescook): Do we need to add a new PepperHelper(render_frame) here?
  // It doesn't seem necessary for either Pepper or NaCl.
  // http://crbug.com/403004
#if !defined(DISABLE_NACL)
  new nacl::NaClHelper(render_frame);
#endif
}

void ShellContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  new ExtensionHelper(render_view, extension_dispatcher_.get());
}

bool ShellContentRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  // Allow the content module to create the plugin.
  return false;
}

blink::WebPlugin* ShellContentRendererClient::CreatePluginReplacement(
    content::RenderFrame* render_frame,
    const base::FilePath& plugin_path) {
  // Don't provide a custom "failed to load" plugin.
  return NULL;
}

bool ShellContentRendererClient::WillSendRequest(
    blink::WebFrame* frame,
    ui::PageTransition transition_type,
    const GURL& url,
    const GURL& first_party_for_cookies,
    GURL* new_url) {
  // TODO(jamescook): Cause an error for bad extension scheme requests?
  return false;
}

bool ShellContentRendererClient::IsExternalPepperPlugin(
    const std::string& module_name) {
#if !defined(DISABLE_NACL)
  // TODO(bbudge) remove this when the trusted NaCl plugin has been removed.
  // We must defer certain plugin events for NaCl instances since we switch
  // from the in-process to the out-of-process proxy after instantiating them.
  return module_name == nacl::kNaClPluginName;
#else
  return false;
#endif
}

bool ShellContentRendererClient::ShouldGatherSiteIsolationStats() const {
  return false;
}

content::BrowserPluginDelegate*
ShellContentRendererClient::CreateBrowserPluginDelegate(
    content::RenderFrame* render_frame,
    const std::string& mime_type,
    const GURL& original_url) {
  if (mime_type == content::kBrowserPluginMimeType) {
    return new extensions::ExtensionsGuestViewContainer(render_frame);
  } else {
    return new extensions::MimeHandlerViewContainer(
        render_frame, mime_type, original_url);
  }
}

void ShellContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentStart(render_frame);
}

void ShellContentRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentEnd(render_frame);
}

ExtensionsClient* ShellContentRendererClient::CreateExtensionsClient() {
  return new ShellExtensionsClient;
}

}  // namespace extensions
