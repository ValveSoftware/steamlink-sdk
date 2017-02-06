// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/dispatcher.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/grit/content_resources.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/common/browser_plugin_guest_mode.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/feature_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/content_capabilities_handler.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/common/view_type.h"
#include "extensions/renderer/api_activity_logger.h"
#include "extensions/renderer/api_definitions_natives.h"
#include "extensions/renderer/app_window_custom_bindings.h"
#include "extensions/renderer/binding_generating_native_handler.h"
#include "extensions/renderer/blob_native_handler.h"
#include "extensions/renderer/content_watcher.h"
#include "extensions/renderer/context_menus_custom_bindings.h"
#include "extensions/renderer/dispatcher_delegate.h"
#include "extensions/renderer/display_source_custom_bindings.h"
#include "extensions/renderer/document_custom_bindings.h"
#include "extensions/renderer/dom_activity_logger.h"
#include "extensions/renderer/event_bindings.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/file_system_natives.h"
#include "extensions/renderer/guest_view/guest_view_internal_custom_bindings.h"
#include "extensions/renderer/id_generator_custom_bindings.h"
#include "extensions/renderer/logging_native_handler.h"
#include "extensions/renderer/messaging_bindings.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/process_info_native_handler.h"
#include "extensions/renderer/render_frame_observer_natives.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/request_sender.h"
#include "extensions/renderer/runtime_custom_bindings.h"
#include "extensions/renderer/safe_builtins.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/script_injection_manager.h"
#include "extensions/renderer/send_request_natives.h"
#include "extensions/renderer/set_icon_natives.h"
#include "extensions/renderer/static_v8_external_one_byte_string_resource.h"
#include "extensions/renderer/test_features_native_handler.h"
#include "extensions/renderer/test_native_handler.h"
#include "extensions/renderer/user_gestures_native_handler.h"
#include "extensions/renderer/utils_native_handler.h"
#include "extensions/renderer/v8_context_native_handler.h"
#include "extensions/renderer/v8_helpers.h"
#include "extensions/renderer/wake_event_page.h"
#include "extensions/renderer/worker_script_context_set.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "grit/extensions_renderer_resources.h"
#include "mojo/public/js/constants.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebCustomElement.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebScriptController.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

using base::UserMetricsAction;
using blink::WebDataSource;
using blink::WebDocument;
using blink::WebScopedUserGesture;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebVector;
using blink::WebView;
using content::RenderThread;

namespace extensions {

namespace {

static const int64_t kInitialExtensionIdleHandlerDelayMs = 5 * 1000;
static const int64_t kMaxExtensionIdleHandlerDelayMs = 5 * 60 * 1000;
static const char kEventDispatchFunction[] = "dispatchEvent";
static const char kOnSuspendEvent[] = "runtime.onSuspend";
static const char kOnSuspendCanceledEvent[] = "runtime.onSuspendCanceled";

void CrashOnException(const v8::TryCatch& trycatch) {
  NOTREACHED();
};

// Returns the global value for "chrome" from |context|. If one doesn't exist
// creates a new object for it.
//
// Note that this isn't necessarily an object, since webpages can write, for
// example, "window.chrome = true".
v8::Local<v8::Value> GetOrCreateChrome(ScriptContext* context) {
  v8::Local<v8::String> chrome_string(
      v8::String::NewFromUtf8(context->isolate(), "chrome"));
  v8::Local<v8::Object> global(context->v8_context()->Global());
  v8::Local<v8::Value> chrome(global->Get(chrome_string));
  if (chrome->IsUndefined()) {
    chrome = v8::Object::New(context->isolate());
    global->Set(chrome_string, chrome);
  }
  return chrome;
}

// Returns |value| cast to an object if possible, else an empty handle.
v8::Local<v8::Object> AsObjectOrEmpty(v8::Local<v8::Value> value) {
  return value->IsObject() ? value.As<v8::Object>() : v8::Local<v8::Object>();
}

// Calls a method |method_name| in a module |module_name| belonging to the
// module system from |context|. Intended as a callback target from
// ScriptContextSet::ForEach.
void CallModuleMethod(const std::string& module_name,
                      const std::string& method_name,
                      const base::ListValue* args,
                      ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  v8::Context::Scope context_scope(context->v8_context());

  std::unique_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());

  std::vector<v8::Local<v8::Value>> arguments;
  for (const auto& arg : *args) {
    arguments.push_back(converter->ToV8Value(arg.get(), context->v8_context()));
  }

  context->module_system()->CallModuleMethod(
      module_name, method_name, &arguments);
}

// This handles the "chrome." root API object in script contexts.
class ChromeNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit ChromeNativeHandler(ScriptContext* context)
      : ObjectBackedNativeHandler(context) {
    RouteFunction(
        "GetChrome",
        base::Bind(&ChromeNativeHandler::GetChrome, base::Unretained(this)));
  }

  void GetChrome(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(GetOrCreateChrome(context()));
  }
};

base::LazyInstance<WorkerScriptContextSet> g_worker_script_context_set =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Note that we can't use Blink public APIs in the constructor becase Blink
// is not initialized at the point we create Dispatcher.
Dispatcher::Dispatcher(DispatcherDelegate* delegate)
    : delegate_(delegate),
      content_watcher_(new ContentWatcher()),
      source_map_(&ResourceBundle::GetSharedInstance()),
      v8_schema_registry_(new V8SchemaRegistry),
      user_script_set_manager_observer_(this),
      webrequest_used_(false),
      activity_logging_enabled_(false) {
  const base::CommandLine& command_line =
      *(base::CommandLine::ForCurrentProcess());
  set_idle_notifications_ =
      command_line.HasSwitch(switches::kExtensionProcess) ||
      command_line.HasSwitch(::switches::kSingleProcess);

  if (set_idle_notifications_) {
    RenderThread::Get()->SetIdleNotificationDelayInMs(
        kInitialExtensionIdleHandlerDelayMs);
  }

  script_context_set_.reset(new ScriptContextSet(&active_extension_ids_));
  user_script_set_manager_.reset(new UserScriptSetManager());
  script_injection_manager_.reset(
      new ScriptInjectionManager(user_script_set_manager_.get()));
  user_script_set_manager_observer_.Add(user_script_set_manager_.get());
  request_sender_.reset(new RequestSender());
  PopulateSourceMap();
  WakeEventPage::Get()->Init(RenderThread::Get());
  // Ideally this should be done after checking
  // ExtensionAPIEnabledInExtensionServiceWorkers(), but the Dispatcher is
  // created so early that sending an IPC from browser/ process to synchronize
  // this enabled-ness is too late.
  WorkerThreadDispatcher::Get()->Init(RenderThread::Get());

  RenderThread::Get()->RegisterExtension(SafeBuiltins::CreateV8Extension());

  // WebSecurityPolicy whitelists. They should be registered for both
  // chrome-extension: and chrome-extension-resource.
  using RegisterFunction = void (*)(const WebString&);
  RegisterFunction register_functions[] = {
      // Treat as secure because communication with them is entirely in the
      // browser, so there is no danger of manipulation or eavesdropping on
      // communication with them by third parties.
      WebSecurityPolicy::registerURLSchemeAsSecure,
      // As far as Blink is concerned, they should be allowed to receive CORS
      // requests. At the Extensions layer, requests will actually be blocked
      // unless overridden by the web_accessible_resources manifest key.
      // TODO(kalman): See what happens with a service worker.
      WebSecurityPolicy::registerURLSchemeAsCORSEnabled,
      // Resources should bypass Content Security Policy checks when included in
      // protected resources. TODO(kalman): What are "protected resources"?
      WebSecurityPolicy::registerURLSchemeAsBypassingContentSecurityPolicy,
      // Extension resources are HTTP-like and safe to expose to the fetch API.
      // The rules for the fetch API are consistent with XHR.
      WebSecurityPolicy::registerURLSchemeAsSupportingFetchAPI,
      // Extension resources, when loaded as the top-level document, should
      // bypass Blink's strict first-party origin checks.
      WebSecurityPolicy::registerURLSchemeAsFirstPartyWhenTopLevel,
  };

  WebString extension_scheme(base::ASCIIToUTF16(kExtensionScheme));
  WebString extension_resource_scheme(base::ASCIIToUTF16(
      kExtensionResourceScheme));
  for (RegisterFunction func : register_functions) {
    func(extension_scheme);
    func(extension_resource_scheme);
  }

  // For extensions, we want to ensure we call the IdleHandler every so often,
  // even if the extension keeps up activity.
  if (set_idle_notifications_) {
    forced_idle_timer_.reset(new base::RepeatingTimer);
    forced_idle_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kMaxExtensionIdleHandlerDelayMs),
        RenderThread::Get(),
        &RenderThread::IdleHandler);
  }

  // Initialize host permissions for any extensions that were activated before
  // WebKit was initialized.
  for (const std::string& extension_id : active_extension_ids_) {
    const Extension* extension =
        RendererExtensionRegistry::Get()->GetByID(extension_id);
    CHECK(extension);
    InitOriginPermissions(extension);
  }

  EnableCustomElementWhiteList();
}

Dispatcher::~Dispatcher() {
}

void Dispatcher::OnRenderFrameCreated(content::RenderFrame* render_frame) {
  script_injection_manager_->OnRenderFrameCreated(render_frame);
}

bool Dispatcher::IsExtensionActive(const std::string& extension_id) const {
  bool is_active =
      active_extension_ids_.find(extension_id) != active_extension_ids_.end();
  if (is_active)
    CHECK(RendererExtensionRegistry::Get()->Contains(extension_id));
  return is_active;
}

void Dispatcher::DidCreateScriptContext(
    blink::WebLocalFrame* frame,
    const v8::Local<v8::Context>& v8_context,
    int extension_group,
    int world_id) {
  const base::TimeTicks start_time = base::TimeTicks::Now();

  ScriptContext* context = script_context_set_->Register(
      frame, v8_context, extension_group, world_id);

  // Initialize origin permissions for content scripts, which can't be
  // initialized in |OnActivateExtension|.
  if (context->context_type() == Feature::CONTENT_SCRIPT_CONTEXT)
    InitOriginPermissions(context->extension());

  {
    std::unique_ptr<ModuleSystem> module_system(
        new ModuleSystem(context, &source_map_));
    context->set_module_system(std::move(module_system));
  }
  ModuleSystem* module_system = context->module_system();

  // Enable natives in startup.
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system);

  RegisterNativeHandlers(module_system, context, request_sender_.get(),
                         v8_schema_registry_.get());

  // chrome.Event is part of the public API (although undocumented). Make it
  // lazily evalulate to Event from event_bindings.js. For extensions only
  // though, not all webpages!
  if (context->extension()) {
    v8::Local<v8::Object> chrome = AsObjectOrEmpty(GetOrCreateChrome(context));
    if (!chrome.IsEmpty())
      module_system->SetLazyField(chrome, "Event", kEventBindings, "Event");
  }

  UpdateBindingsForContext(context);

  bool is_within_platform_app = IsWithinPlatformApp();
  // Inject custom JS into the platform app context.
  if (is_within_platform_app) {
    module_system->Require("platformApp");
  }

  RequireGuestViewModules(context);
  delegate_->RequireAdditionalModules(context, is_within_platform_app);

  const base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
  switch (context->context_type()) {
    case Feature::UNSPECIFIED_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_Unspecified",
                          elapsed);
      break;
    case Feature::BLESSED_EXTENSION_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_Blessed", elapsed);
      break;
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_Unblessed",
                          elapsed);
      break;
    case Feature::CONTENT_SCRIPT_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_ContentScript",
                          elapsed);
      break;
    case Feature::WEB_PAGE_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_WebPage", elapsed);
      break;
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_BlessedWebPage",
                          elapsed);
      break;
    case Feature::WEBUI_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_WebUI", elapsed);
      break;
    case Feature::SERVICE_WORKER_CONTEXT:
      // Handled in DidInitializeServiceWorkerContextOnWorkerThread().
      NOTREACHED();
      break;
  }

  VLOG(1) << "Num tracked contexts: " << script_context_set_->size();
}

void Dispatcher::DidInitializeServiceWorkerContextOnWorkerThread(
    v8::Local<v8::Context> v8_context,
    int embedded_worker_id,
    const GURL& url) {
  const base::TimeTicks start_time = base::TimeTicks::Now();

  if (!url.SchemeIs(kExtensionScheme) &&
      !url.SchemeIs(kExtensionResourceScheme)) {
    // Early-out if this isn't a chrome-extension:// or resource scheme,
    // because looking up the extension registry is unnecessary if it's not.
    // Checking this will also skip over hosted apps, which is the desired
    // behavior - hosted app service workers are not our concern.
    return;
  }

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(url);

  if (!extension) {
    // TODO(kalman): This is no good. Instead we need to either:
    //
    // - Hold onto the v8::Context and create the ScriptContext and install
    //   our bindings when this extension is loaded.
    // - Deal with there being an extension ID (url.host()) but no
    //   extension associated with it, then document that getBackgroundClient
    //   may fail if the extension hasn't loaded yet.
    //
    // The former is safer, but is unfriendly to caching (e.g. session restore).
    // It seems to contradict the service worker idiom.
    //
    // The latter is friendly to caching, but running extension code without an
    // installed extension makes me nervous, and means that we won't be able to
    // expose arbitrary (i.e. capability-checked) extension APIs to service
    // workers. We will probably need to relax some assertions - we just need
    // to find them.
    //
    // Perhaps this could be solved with our own event on the service worker
    // saying that an extension is ready, and documenting that extension APIs
    // won't work before that event has fired?
    return;
  }

  ScriptContext* context = new ScriptContext(
      v8_context, nullptr, extension, Feature::SERVICE_WORKER_CONTEXT,
      extension, Feature::SERVICE_WORKER_CONTEXT);
  context->set_url(url);

  if (ExtensionsClient::Get()->ExtensionAPIEnabledInExtensionServiceWorkers()) {
    WorkerThreadDispatcher::Get()->AddWorkerData(embedded_worker_id);
    {
      // TODO(lazyboy): Make sure accessing |source_map_| in worker thread is
      // safe.
      std::unique_ptr<ModuleSystem> module_system(
          new ModuleSystem(context, &source_map_));
      context->set_module_system(std::move(module_system));
    }

    ModuleSystem* module_system = context->module_system();
    // Enable natives in startup.
    ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system);
    RegisterNativeHandlers(
        module_system, context,
        WorkerThreadDispatcher::Get()->GetRequestSender(),
        WorkerThreadDispatcher::Get()->GetV8SchemaRegistry());
    // chrome.Event is part of the public API (although undocumented). Make it
    // lazily evalulate to Event from event_bindings.js.
    v8::Local<v8::Object> chrome = AsObjectOrEmpty(GetOrCreateChrome(context));
    if (!chrome.IsEmpty())
      module_system->SetLazyField(chrome, "Event", kEventBindings, "Event");

    UpdateBindingsForContext(context);
    // TODO(lazyboy): Get rid of RequireGuestViewModules() as this doesn't seem
    // necessary for Extension SW.
    RequireGuestViewModules(context);
    delegate_->RequireAdditionalModules(context,
                                        false /* is_within_platform_app */);
  }

  g_worker_script_context_set.Get().Insert(base::WrapUnique(context));

  v8::Isolate* isolate = context->isolate();

  // Fetch the source code for service_worker_bindings.js.
  base::StringPiece script_resource =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_SERVICE_WORKER_BINDINGS_JS);
  v8::Local<v8::String> script = v8::String::NewExternal(
      isolate, new StaticV8ExternalOneByteStringResource(script_resource));

  // Run service_worker.js to get the main function.
  v8::Local<v8::Function> main_function;
  {
    v8::Local<v8::Value> result = context->RunScript(
        v8_helpers::ToV8StringUnsafe(isolate, "service_worker"), script,
        base::Bind(&CrashOnException));
    CHECK(result->IsFunction());
    main_function = result.As<v8::Function>();
  }

  // Expose CHECK/DCHECK/NOTREACHED to the main function with a
  // LoggingNativeHandler. Admire the neat base::Bind trick to both Invalidate
  // and delete the native handler.
  LoggingNativeHandler* logging = new LoggingNativeHandler(context);
  context->AddInvalidationObserver(
      base::Bind(&NativeHandler::Invalidate, base::Owned(logging)));

  // Execute the main function with its dependencies passed in as arguments.
  v8::Local<v8::Value> args[] = {
      // The extension's background URL.
      v8_helpers::ToV8StringUnsafe(
          isolate, BackgroundInfo::GetBackgroundURL(extension).spec()),
      // The wake-event-page native function.
      WakeEventPage::Get()->GetForContext(context),
      // The logging module.
      logging->NewInstance(),
  };
  context->CallFunction(main_function, arraysize(args), args);

  const base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES(
      "Extensions.DidInitializeServiceWorkerContextOnWorkerThread", elapsed);
}

void Dispatcher::WillReleaseScriptContext(
    blink::WebLocalFrame* frame,
    const v8::Local<v8::Context>& v8_context,
    int world_id) {
  ScriptContext* context = script_context_set_->GetByV8Context(v8_context);
  if (!context)
    return;

  // TODO(kalman): Make |request_sender| use |context->AddInvalidationObserver|.
  // In fact |request_sender_| should really be owned by ScriptContext.
  request_sender_->InvalidateSource(context);

  script_context_set_->Remove(context);
  VLOG(1) << "Num tracked contexts: " << script_context_set_->size();
}

// static
void Dispatcher::WillDestroyServiceWorkerContextOnWorkerThread(
    v8::Local<v8::Context> v8_context,
    int embedded_worker_id,
    const GURL& url) {
  if (url.SchemeIs(kExtensionScheme) ||
      url.SchemeIs(kExtensionResourceScheme)) {
    // See comment in DidInitializeServiceWorkerContextOnWorkerThread.
    g_worker_script_context_set.Get().Remove(v8_context, url);
  }
  if (ExtensionsClient::Get()->ExtensionAPIEnabledInExtensionServiceWorkers())
    WorkerThreadDispatcher::Get()->RemoveWorkerData(embedded_worker_id);
}

void Dispatcher::DidCreateDocumentElement(blink::WebLocalFrame* frame) {
  // Note: use GetEffectiveDocumentURL not just frame->document()->url()
  // so that this also injects the stylesheet on about:blank frames that
  // are hosted in the extension process.
  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      frame, frame->document().url(), true /* match_about_blank */);

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(
          effective_document_url);

  if (extension &&
      (extension->is_extension() || extension->is_platform_app())) {
    int resource_id = extension->is_platform_app() ? IDR_PLATFORM_APP_CSS
                                                   : IDR_EXTENSION_FONTS_CSS;
    std::string stylesheet = ResourceBundle::GetSharedInstance()
                                 .GetRawDataResource(resource_id)
                                 .as_string();
    base::ReplaceFirstSubstringAfterOffset(
        &stylesheet, 0, "$FONTFAMILY", system_font_family_);
    base::ReplaceFirstSubstringAfterOffset(
        &stylesheet, 0, "$FONTSIZE", system_font_size_);

    // Blink doesn't let us define an additional user agent stylesheet, so
    // we insert the default platform app or extension stylesheet into all
    // documents that are loaded in each app or extension.
    frame->document().insertStyleSheet(WebString::fromUTF8(stylesheet));
  }

  // If this is an extension options page, and the extension has opted into
  // using Chrome styles, then insert the Chrome extension stylesheet.
  if (extension && extension->is_extension() &&
      OptionsPageInfo::ShouldUseChromeStyle(extension) &&
      effective_document_url == OptionsPageInfo::GetOptionsPage(extension)) {
    frame->document().insertStyleSheet(
        WebString::fromUTF8(ResourceBundle::GetSharedInstance()
                                .GetRawDataResource(IDR_EXTENSION_CSS)
                                .as_string()));
  }

  // In testing, the document lifetime events can happen after the render
  // process shutdown event.
  // See: http://crbug.com/21508 and http://crbug.com/500851
  if (content_watcher_) {
    content_watcher_->DidCreateDocumentElement(frame);
  }
}

void Dispatcher::RunScriptsAtDocumentStart(content::RenderFrame* render_frame) {
  ExtensionFrameHelper* frame_helper = ExtensionFrameHelper::Get(render_frame);
  if (!frame_helper)
    return;  // The frame is invisible to extensions.

  frame_helper->RunScriptsAtDocumentStart();
  // |frame_helper| and |render_frame| might be dead by now.
}

void Dispatcher::RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) {
  ExtensionFrameHelper* frame_helper = ExtensionFrameHelper::Get(render_frame);
  if (!frame_helper)
    return;  // The frame is invisible to extensions.

  frame_helper->RunScriptsAtDocumentEnd();
  // |frame_helper| and |render_frame| might be dead by now.
}

void Dispatcher::OnExtensionResponse(int request_id,
                                     bool success,
                                     const base::ListValue& response,
                                     const std::string& error) {
  request_sender_->HandleResponse(request_id, success, response, error);
}

void Dispatcher::DispatchEvent(const std::string& extension_id,
                               const std::string& event_name) const {
  base::ListValue args;
  args.Set(0, new base::StringValue(event_name));
  args.Set(1, new base::ListValue());

  // Needed for Windows compilation, since kEventBindings is declared extern.
  const char* local_event_bindings = kEventBindings;
  script_context_set_->ForEach(
      extension_id, base::Bind(&CallModuleMethod, local_event_bindings,
                               kEventDispatchFunction, &args));
}

void Dispatcher::InvokeModuleSystemMethod(content::RenderFrame* render_frame,
                                          const std::string& extension_id,
                                          const std::string& module_name,
                                          const std::string& function_name,
                                          const base::ListValue& args,
                                          bool user_gesture) {
  std::unique_ptr<WebScopedUserGesture> web_user_gesture;
  if (user_gesture)
    web_user_gesture.reset(new WebScopedUserGesture);

  script_context_set_->ForEach(
      extension_id, render_frame,
      base::Bind(&CallModuleMethod, module_name, function_name, &args));

  // Reset the idle handler each time there's any activity like event or message
  // dispatch, for which Invoke is the chokepoint.
  if (set_idle_notifications_) {
    RenderThread::Get()->ScheduleIdleHandler(
        kInitialExtensionIdleHandlerDelayMs);
  }

  // Tell the browser process when an event has been dispatched with a lazy
  // background page active.
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (extension && BackgroundInfo::HasLazyBackgroundPage(extension) &&
      module_name == kEventBindings &&
      function_name == kEventDispatchFunction) {
    content::RenderFrame* background_frame =
        ExtensionFrameHelper::GetBackgroundPageFrame(extension_id);
    if (background_frame) {
      int message_id;
      args.GetInteger(3, &message_id);
      background_frame->Send(new ExtensionHostMsg_EventAck(
          background_frame->GetRoutingID(), message_id));
    }
  }
}

// static
std::vector<std::pair<std::string, int> > Dispatcher::GetJsResources() {
  std::vector<std::pair<std::string, int> > resources;

  // Libraries.
  resources.push_back(std::make_pair("appView", IDR_APP_VIEW_JS));
  resources.push_back(std::make_pair("entryIdManager", IDR_ENTRY_ID_MANAGER));
  resources.push_back(std::make_pair(kEventBindings, IDR_EVENT_BINDINGS_JS));
  resources.push_back(std::make_pair("extensionOptions",
                                     IDR_EXTENSION_OPTIONS_JS));
  resources.push_back(std::make_pair("extensionOptionsAttributes",
                                     IDR_EXTENSION_OPTIONS_ATTRIBUTES_JS));
  resources.push_back(std::make_pair("extensionOptionsConstants",
                                     IDR_EXTENSION_OPTIONS_CONSTANTS_JS));
  resources.push_back(std::make_pair("extensionOptionsEvents",
                                     IDR_EXTENSION_OPTIONS_EVENTS_JS));
  resources.push_back(std::make_pair("extensionView", IDR_EXTENSION_VIEW_JS));
  resources.push_back(std::make_pair("extensionViewApiMethods",
                                     IDR_EXTENSION_VIEW_API_METHODS_JS));
  resources.push_back(std::make_pair("extensionViewAttributes",
                                     IDR_EXTENSION_VIEW_ATTRIBUTES_JS));
  resources.push_back(std::make_pair("extensionViewConstants",
                                     IDR_EXTENSION_VIEW_CONSTANTS_JS));
  resources.push_back(std::make_pair("extensionViewEvents",
                                     IDR_EXTENSION_VIEW_EVENTS_JS));
  resources.push_back(std::make_pair(
      "extensionViewInternal", IDR_EXTENSION_VIEW_INTERNAL_CUSTOM_BINDINGS_JS));
  resources.push_back(std::make_pair("guestView", IDR_GUEST_VIEW_JS));
  resources.push_back(std::make_pair("guestViewAttributes",
                                     IDR_GUEST_VIEW_ATTRIBUTES_JS));
  resources.push_back(std::make_pair("guestViewContainer",
                                     IDR_GUEST_VIEW_CONTAINER_JS));
  resources.push_back(std::make_pair("guestViewDeny", IDR_GUEST_VIEW_DENY_JS));
  resources.push_back(std::make_pair("guestViewEvents",
                                     IDR_GUEST_VIEW_EVENTS_JS));

  if (content::BrowserPluginGuestMode::UseCrossProcessFramesForGuests()) {
    resources.push_back(std::make_pair("guestViewIframe",
                                       IDR_GUEST_VIEW_IFRAME_JS));
    resources.push_back(std::make_pair("guestViewIframeContainer",
                                       IDR_GUEST_VIEW_IFRAME_CONTAINER_JS));
  }

  resources.push_back(std::make_pair("imageUtil", IDR_IMAGE_UTIL_JS));
  resources.push_back(std::make_pair("json_schema", IDR_JSON_SCHEMA_JS));
  resources.push_back(std::make_pair("lastError", IDR_LAST_ERROR_JS));
  resources.push_back(std::make_pair("messaging", IDR_MESSAGING_JS));
  resources.push_back(std::make_pair("messaging_utils",
                                     IDR_MESSAGING_UTILS_JS));
  resources.push_back(std::make_pair(kSchemaUtils, IDR_SCHEMA_UTILS_JS));
  resources.push_back(std::make_pair("sendRequest", IDR_SEND_REQUEST_JS));
  resources.push_back(std::make_pair("setIcon", IDR_SET_ICON_JS));
  resources.push_back(std::make_pair("test", IDR_TEST_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("test_environment_specific_bindings",
                     IDR_BROWSER_TEST_ENVIRONMENT_SPECIFIC_BINDINGS_JS));
  resources.push_back(std::make_pair("uncaught_exception_handler",
                                     IDR_UNCAUGHT_EXCEPTION_HANDLER_JS));
  resources.push_back(std::make_pair("utils", IDR_UTILS_JS));
  resources.push_back(std::make_pair("webRequest",
                                     IDR_WEB_REQUEST_CUSTOM_BINDINGS_JS));
  resources.push_back(
       std::make_pair("webRequestInternal",
                      IDR_WEB_REQUEST_INTERNAL_CUSTOM_BINDINGS_JS));
  // Note: webView not webview so that this doesn't interfere with the
  // chrome.webview API bindings.
  resources.push_back(std::make_pair("webView", IDR_WEB_VIEW_JS));
  resources.push_back(std::make_pair("webViewActionRequests",
                                     IDR_WEB_VIEW_ACTION_REQUESTS_JS));
  resources.push_back(std::make_pair("webViewApiMethods",
                                     IDR_WEB_VIEW_API_METHODS_JS));
  resources.push_back(std::make_pair("webViewAttributes",
                                     IDR_WEB_VIEW_ATTRIBUTES_JS));
  resources.push_back(std::make_pair("webViewConstants",
                                     IDR_WEB_VIEW_CONSTANTS_JS));
  resources.push_back(std::make_pair("webViewEvents", IDR_WEB_VIEW_EVENTS_JS));
  resources.push_back(std::make_pair("webViewInternal",
                                     IDR_WEB_VIEW_INTERNAL_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair(mojo::kBindingsModuleName, IDR_MOJO_BINDINGS_JS));
  resources.push_back(
      std::make_pair(mojo::kBufferModuleName, IDR_MOJO_BUFFER_JS));
  resources.push_back(
      std::make_pair(mojo::kCodecModuleName, IDR_MOJO_CODEC_JS));
  resources.push_back(
      std::make_pair(mojo::kConnectionModuleName, IDR_MOJO_CONNECTION_JS));
  resources.push_back(
      std::make_pair(mojo::kConnectorModuleName, IDR_MOJO_CONNECTOR_JS));
  resources.push_back(
      std::make_pair(mojo::kRouterModuleName, IDR_MOJO_ROUTER_JS));
  resources.push_back(
      std::make_pair(mojo::kUnicodeModuleName, IDR_MOJO_UNICODE_JS));
  resources.push_back(
      std::make_pair(mojo::kValidatorModuleName, IDR_MOJO_VALIDATOR_JS));
  resources.push_back(std::make_pair("async_waiter", IDR_ASYNC_WAITER_JS));
  resources.push_back(std::make_pair("data_receiver", IDR_DATA_RECEIVER_JS));
  resources.push_back(std::make_pair("data_sender", IDR_DATA_SENDER_JS));
  resources.push_back(std::make_pair("keep_alive", IDR_KEEP_ALIVE_JS));
  resources.push_back(std::make_pair("extensions/common/mojo/keep_alive.mojom",
                                     IDR_KEEP_ALIVE_MOJOM_JS));
  resources.push_back(std::make_pair("device/serial/data_stream.mojom",
                                     IDR_DATA_STREAM_MOJOM_JS));
  resources.push_back(
      std::make_pair("device/serial/data_stream_serialization.mojom",
                     IDR_DATA_STREAM_SERIALIZATION_MOJOM_JS));
  resources.push_back(std::make_pair("stash_client", IDR_STASH_CLIENT_JS));
  resources.push_back(
      std::make_pair("extensions/common/mojo/stash.mojom", IDR_STASH_MOJOM_JS));

  // Custom bindings.
  resources.push_back(
      std::make_pair("app.runtime", IDR_APP_RUNTIME_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("app.window", IDR_APP_WINDOW_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("declarativeWebRequest",
                     IDR_DECLARATIVE_WEBREQUEST_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("displaySource",
                     IDR_DISPLAY_SOURCE_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("contextMenus", IDR_CONTEXT_MENUS_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("contextMenusHandlers", IDR_CONTEXT_MENUS_HANDLERS_JS));
  resources.push_back(
      std::make_pair("extension", IDR_EXTENSION_CUSTOM_BINDINGS_JS));
  resources.push_back(std::make_pair("i18n", IDR_I18N_CUSTOM_BINDINGS_JS));
  resources.push_back(std::make_pair(
      "mimeHandlerPrivate", IDR_MIME_HANDLER_PRIVATE_CUSTOM_BINDINGS_JS));
  resources.push_back(std::make_pair("extensions/common/api/mime_handler.mojom",
                                     IDR_MIME_HANDLER_MOJOM_JS));
  resources.push_back(
      std::make_pair("mojoPrivate", IDR_MOJO_PRIVATE_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("permissions", IDR_PERMISSIONS_CUSTOM_BINDINGS_JS));
  resources.push_back(std::make_pair("printerProvider",
                                     IDR_PRINTER_PROVIDER_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("runtime", IDR_RUNTIME_CUSTOM_BINDINGS_JS));
  resources.push_back(std::make_pair("windowControls", IDR_WINDOW_CONTROLS_JS));
  resources.push_back(
      std::make_pair("webViewRequest",
                     IDR_WEB_VIEW_REQUEST_CUSTOM_BINDINGS_JS));
  resources.push_back(std::make_pair("binding", IDR_BINDING_JS));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableMojoSerialService)) {
    resources.push_back(
        std::make_pair("serial", IDR_SERIAL_CUSTOM_BINDINGS_JS));
  }
  resources.push_back(std::make_pair("serial_service", IDR_SERIAL_SERVICE_JS));
  resources.push_back(
      std::make_pair("device/serial/serial.mojom", IDR_SERIAL_MOJOM_JS));
  resources.push_back(std::make_pair("device/serial/serial_serialization.mojom",
                                     IDR_SERIAL_SERIALIZATION_MOJOM_JS));

  // Custom types sources.
  resources.push_back(std::make_pair("StorageArea", IDR_STORAGE_AREA_JS));

  // Platform app sources that are not API-specific..
  resources.push_back(std::make_pair("platformApp", IDR_PLATFORM_APP_JS));

#if defined(ENABLE_MEDIA_ROUTER)
  resources.push_back(
      std::make_pair("chrome/browser/media/router/mojo/media_router.mojom",
                     IDR_MEDIA_ROUTER_MOJOM_JS));
  resources.push_back(
      std::make_pair("media_router_bindings", IDR_MEDIA_ROUTER_BINDINGS_JS));
#endif  // defined(ENABLE_MEDIA_ROUTER)

  return resources;
}

// NOTE: please use the naming convention "foo_natives" for these.
// static
void Dispatcher::RegisterNativeHandlers(ModuleSystem* module_system,
                                        ScriptContext* context,
                                        Dispatcher* dispatcher,
                                        RequestSender* request_sender,
                                        V8SchemaRegistry* v8_schema_registry) {
  module_system->RegisterNativeHandler(
      "chrome",
      std::unique_ptr<NativeHandler>(new ChromeNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "logging",
      std::unique_ptr<NativeHandler>(new LoggingNativeHandler(context)));
  module_system->RegisterNativeHandler("schema_registry",
                                       v8_schema_registry->AsNativeHandler());
  module_system->RegisterNativeHandler(
      "test_features",
      std::unique_ptr<NativeHandler>(new TestFeaturesNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "test_native_handler",
      std::unique_ptr<NativeHandler>(new TestNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "user_gestures",
      std::unique_ptr<NativeHandler>(new UserGesturesNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "utils", std::unique_ptr<NativeHandler>(new UtilsNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "v8_context",
      std::unique_ptr<NativeHandler>(new V8ContextNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "event_natives",
      std::unique_ptr<NativeHandler>(new EventBindings(context)));
  module_system->RegisterNativeHandler(
      "messaging_natives",
      std::unique_ptr<NativeHandler>(MessagingBindings::Get(context)));
  module_system->RegisterNativeHandler(
      "apiDefinitions", std::unique_ptr<NativeHandler>(
                            new ApiDefinitionsNatives(dispatcher, context)));
  module_system->RegisterNativeHandler(
      "sendRequest", std::unique_ptr<NativeHandler>(
                         new SendRequestNatives(request_sender, context)));
  module_system->RegisterNativeHandler(
      "setIcon", std::unique_ptr<NativeHandler>(new SetIconNatives(context)));
  module_system->RegisterNativeHandler(
      "activityLogger", std::unique_ptr<NativeHandler>(
                            new APIActivityLogger(context, dispatcher)));
  module_system->RegisterNativeHandler(
      "renderFrameObserverNatives",
      std::unique_ptr<NativeHandler>(new RenderFrameObserverNatives(context)));

  // Natives used by multiple APIs.
  module_system->RegisterNativeHandler(
      "file_system_natives",
      std::unique_ptr<NativeHandler>(new FileSystemNatives(context)));

  // Custom bindings.
  module_system->RegisterNativeHandler(
      "app_window_natives",
      std::unique_ptr<NativeHandler>(new AppWindowCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "blob_natives",
      std::unique_ptr<NativeHandler>(new BlobNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "context_menus",
      std::unique_ptr<NativeHandler>(new ContextMenusCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "document_natives",
      std::unique_ptr<NativeHandler>(new DocumentCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "guest_view_internal", std::unique_ptr<NativeHandler>(
                                 new GuestViewInternalCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "id_generator",
      std::unique_ptr<NativeHandler>(new IdGeneratorCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "runtime",
      std::unique_ptr<NativeHandler>(new RuntimeCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "display_source",
      std::unique_ptr<NativeHandler>(new DisplaySourceCustomBindings(context)));
}

bool Dispatcher::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(Dispatcher, message)
  IPC_MESSAGE_HANDLER(ExtensionMsg_ActivateExtension, OnActivateExtension)
  IPC_MESSAGE_HANDLER(ExtensionMsg_CancelSuspend, OnCancelSuspend)
  IPC_MESSAGE_HANDLER(ExtensionMsg_DeliverMessage, OnDeliverMessage)
  IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnConnect, OnDispatchOnConnect)
  IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnDisconnect, OnDispatchOnDisconnect)
  IPC_MESSAGE_HANDLER(ExtensionMsg_Loaded, OnLoaded)
  IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnMessageInvoke)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetChannel, OnSetChannel)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetScriptingWhitelist,
                      OnSetScriptingWhitelist)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetSystemFont, OnSetSystemFont)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetWebViewPartitionID,
                      OnSetWebViewPartitionID)
  IPC_MESSAGE_HANDLER(ExtensionMsg_ShouldSuspend, OnShouldSuspend)
  IPC_MESSAGE_HANDLER(ExtensionMsg_Suspend, OnSuspend)
  IPC_MESSAGE_HANDLER(ExtensionMsg_TransferBlobs, OnTransferBlobs)
  IPC_MESSAGE_HANDLER(ExtensionMsg_Unloaded, OnUnloaded)
  IPC_MESSAGE_HANDLER(ExtensionMsg_UpdatePermissions, OnUpdatePermissions)
  IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateTabSpecificPermissions,
                      OnUpdateTabSpecificPermissions)
  IPC_MESSAGE_HANDLER(ExtensionMsg_ClearTabSpecificPermissions,
                      OnClearTabSpecificPermissions)
  IPC_MESSAGE_HANDLER(ExtensionMsg_UsingWebRequestAPI, OnUsingWebRequestAPI)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetActivityLoggingEnabled,
                      OnSetActivityLoggingEnabled)
  IPC_MESSAGE_FORWARD(ExtensionMsg_WatchPages,
                      content_watcher_.get(),
                      ContentWatcher::OnWatchPages)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}
void Dispatcher::IdleNotification() {
  if (set_idle_notifications_ && forced_idle_timer_) {
    // Dampen the forced delay as well if the extension stays idle for long
    // periods of time. (forced_idle_timer_ can be NULL after
    // OnRenderProcessShutdown has been called.)
    int64_t forced_delay_ms =
        std::max(RenderThread::Get()->GetIdleNotificationDelayInMs(),
                 kMaxExtensionIdleHandlerDelayMs);
    forced_idle_timer_->Stop();
    forced_idle_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(forced_delay_ms),
        RenderThread::Get(),
        &RenderThread::IdleHandler);
  }
}

void Dispatcher::OnRenderProcessShutdown() {
  v8_schema_registry_.reset();
  forced_idle_timer_.reset();
  content_watcher_.reset();
  script_context_set_->ForEach(
      std::string(), nullptr,
      base::Bind(&ScriptContextSet::Remove,
                 base::Unretained(script_context_set_.get())));
}

void Dispatcher::OnActivateExtension(const std::string& extension_id) {
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (!extension) {
    NOTREACHED();
    // Extension was activated but was never loaded. This probably means that
    // the renderer failed to load it (or the browser failed to tell us when it
    // did). Failures shouldn't happen, but instead of crashing there (which
    // executes on all renderers) be conservative and only crash in the renderer
    // of the extension which failed to load; this one.
    std::string& error = extension_load_errors_[extension_id];
    char minidump[256];
    base::debug::Alias(&minidump);
    base::snprintf(minidump,
                   arraysize(minidump),
                   "e::dispatcher:%s:%s",
                   extension_id.c_str(),
                   error.c_str());
    LOG(FATAL) << extension_id << " was never loaded: " << error;
  }

  active_extension_ids_.insert(extension_id);

  // This is called when starting a new extension page, so start the idle
  // handler ticking.
  RenderThread::Get()->ScheduleIdleHandler(kInitialExtensionIdleHandlerDelayMs);

  if (activity_logging_enabled_) {
    DOMActivityLogger::AttachToWorld(DOMActivityLogger::kMainWorldId,
                                     extension_id);
  }

  InitOriginPermissions(extension);

  UpdateActiveExtensions();
}

void Dispatcher::OnCancelSuspend(const std::string& extension_id) {
  DispatchEvent(extension_id, kOnSuspendCanceledEvent);
}

void Dispatcher::OnDeliverMessage(int target_port_id,
                                  int source_tab_id,
                                  const Message& message) {
  std::unique_ptr<RequestSender::ScopedTabID> scoped_tab_id;
  if (source_tab_id != -1) {
    scoped_tab_id.reset(
        new RequestSender::ScopedTabID(request_sender(), source_tab_id));
  }

  MessagingBindings::DeliverMessage(*script_context_set_, target_port_id,
                                    message,
                                    NULL);  // All render frames.
}

void Dispatcher::OnDispatchOnConnect(
    int target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo& source,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id) {
  DCHECK_EQ(1, target_port_id % 2);  // target renderer ports have odd IDs.

  MessagingBindings::DispatchOnConnect(*script_context_set_, target_port_id,
                                       channel_name, source, info,
                                       tls_channel_id,
                                       NULL);  // All render frames.
}

void Dispatcher::OnDispatchOnDisconnect(int port_id,
                                        const std::string& error_message) {
  MessagingBindings::DispatchOnDisconnect(*script_context_set_, port_id,
                                          error_message,
                                          NULL);  // All render frames.
}

void Dispatcher::OnLoaded(
    const std::vector<ExtensionMsg_Loaded_Params>& loaded_extensions) {
  for (const auto& param : loaded_extensions) {
    std::string error;
    scoped_refptr<const Extension> extension = param.ConvertToExtension(&error);
    if (!extension.get()) {
      NOTREACHED() << error;
      // Note: in tests |param.id| has been observed to be empty (see comment
      // just below) so this isn't all that reliable.
      extension_load_errors_[param.id] = error;
      continue;
    }

    RendererExtensionRegistry* extension_registry =
        RendererExtensionRegistry::Get();
    // TODO(kalman): This test is deliberately not a CHECK (though I wish it
    // could be) and uses extension->id() not params.id:
    // 1. For some reason params.id can be empty. I've only seen it with
    //    the webstore extension, in tests, and I've spent some time trying to
    //    figure out why - but cost/benefit won.
    // 2. The browser only sends this IPC to RenderProcessHosts once, but the
    //    Dispatcher is attached to a RenderThread. Presumably there is a
    //    mismatch there. In theory one would think it's possible for the
    //    browser to figure this out itself - but again, cost/benefit.
    if (!extension_registry->Insert(extension)) {
      // TODO(devlin): This may be fixed by crbug.com/528026. Monitor, and
      // consider making this a release CHECK.
      NOTREACHED();
    }
  }

  // Update the available bindings for all contexts. These may have changed if
  // an externally_connectable extension was loaded that can connect to an
  // open webpage.
  UpdateBindings("");
}

void Dispatcher::OnMessageInvoke(const std::string& extension_id,
                                 const std::string& module_name,
                                 const std::string& function_name,
                                 const base::ListValue& args,
                                 bool user_gesture) {
  InvokeModuleSystemMethod(
      NULL, extension_id, module_name, function_name, args, user_gesture);
}

void Dispatcher::OnSetChannel(int channel) {
  delegate_->SetChannel(channel);
}

void Dispatcher::OnSetScriptingWhitelist(
    const ExtensionsClient::ScriptingWhitelist& extension_ids) {
  ExtensionsClient::Get()->SetScriptingWhitelist(extension_ids);
}

void Dispatcher::OnSetSystemFont(const std::string& font_family,
                                 const std::string& font_size) {
  system_font_family_ = font_family;
  system_font_size_ = font_size;
}

void Dispatcher::OnSetWebViewPartitionID(const std::string& partition_id) {
  // |webview_partition_id_| cannot be changed once set.
  CHECK(webview_partition_id_.empty() || webview_partition_id_ == partition_id);
  webview_partition_id_ = partition_id;
}

void Dispatcher::OnShouldSuspend(const std::string& extension_id,
                                 uint64_t sequence_id) {
  RenderThread::Get()->Send(
      new ExtensionHostMsg_ShouldSuspendAck(extension_id, sequence_id));
}

void Dispatcher::OnSuspend(const std::string& extension_id) {
  // Dispatch the suspend event. This doesn't go through the standard event
  // dispatch machinery because it requires special handling. We need to let
  // the browser know when we are starting and stopping the event dispatch, so
  // that it still considers the extension idle despite any activity the suspend
  // event creates.
  DispatchEvent(extension_id, kOnSuspendEvent);
  RenderThread::Get()->Send(new ExtensionHostMsg_SuspendAck(extension_id));
}

void Dispatcher::OnTransferBlobs(const std::vector<std::string>& blob_uuids) {
  RenderThread::Get()->Send(new ExtensionHostMsg_TransferBlobsAck(blob_uuids));
}

void Dispatcher::OnUnloaded(const std::string& id) {
  // See comment in OnLoaded for why it would be nice, but perhaps incorrect,
  // to CHECK here rather than guarding.
  // TODO(devlin): This may be fixed by crbug.com/528026. Monitor, and
  // consider making this a release CHECK.
  if (!RendererExtensionRegistry::Get()->Remove(id)) {
    NOTREACHED();
    return;
  }

  active_extension_ids_.erase(id);

  script_injection_manager_->OnExtensionUnloaded(id);

  // If the extension is later reloaded with a different set of permissions,
  // we'd like it to get a new isolated world ID, so that it can pick up the
  // changed origin whitelist.
  ScriptInjection::RemoveIsolatedWorld(id);

  // Invalidate all of the contexts that were removed.
  // TODO(kalman): add an invalidation observer interface to ScriptContext.
  std::set<ScriptContext*> removed_contexts =
      script_context_set_->OnExtensionUnloaded(id);
  for (ScriptContext* context : removed_contexts) {
    request_sender_->InvalidateSource(context);
  }

  // Update the available bindings for the remaining contexts. These may have
  // changed if an externally_connectable extension is unloaded and a webpage
  // is no longer accessible.
  UpdateBindings("");

  // Invalidates the messages map for the extension in case the extension is
  // reloaded with a new messages map.
  EraseL10nMessagesMap(id);

  // We don't do anything with existing platform-app stylesheets. They will
  // stay resident, but the URL pattern corresponding to the unloaded
  // extension's URL just won't match anything anymore.
}

void Dispatcher::OnUpdatePermissions(
    const ExtensionMsg_UpdatePermissions_Params& params) {
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(params.extension_id);
  if (!extension)
    return;

  std::unique_ptr<const PermissionSet> active =
      params.active_permissions.ToPermissionSet();
  std::unique_ptr<const PermissionSet> withheld =
      params.withheld_permissions.ToPermissionSet();

  UpdateOriginPermissions(
      extension->url(),
      extension->permissions_data()->GetEffectiveHostPermissions(),
      active->effective_hosts());

  extension->permissions_data()->SetPermissions(std::move(active),
                                                std::move(withheld));
  UpdateBindings(extension->id());
}

void Dispatcher::OnUpdateTabSpecificPermissions(const GURL& visible_url,
                                                const std::string& extension_id,
                                                const URLPatternSet& new_hosts,
                                                bool update_origin_whitelist,
                                                int tab_id) {
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (!extension)
    return;

  URLPatternSet old_effective =
      extension->permissions_data()->GetEffectiveHostPermissions();
  extension->permissions_data()->UpdateTabSpecificPermissions(
      tab_id,
      extensions::PermissionSet(extensions::APIPermissionSet(),
                                extensions::ManifestPermissionSet(), new_hosts,
                                extensions::URLPatternSet()));

  if (update_origin_whitelist) {
    UpdateOriginPermissions(
        extension->url(),
        old_effective,
        extension->permissions_data()->GetEffectiveHostPermissions());
  }
}

void Dispatcher::OnClearTabSpecificPermissions(
    const std::vector<std::string>& extension_ids,
    bool update_origin_whitelist,
    int tab_id) {
  for (const std::string& id : extension_ids) {
    const Extension* extension = RendererExtensionRegistry::Get()->GetByID(id);
    if (extension) {
      URLPatternSet old_effective =
          extension->permissions_data()->GetEffectiveHostPermissions();
      extension->permissions_data()->ClearTabSpecificPermissions(tab_id);
      if (update_origin_whitelist) {
        UpdateOriginPermissions(
            extension->url(),
            old_effective,
            extension->permissions_data()->GetEffectiveHostPermissions());
      }
    }
  }
}

void Dispatcher::OnUsingWebRequestAPI(bool webrequest_used) {
  webrequest_used_ = webrequest_used;
}

void Dispatcher::OnSetActivityLoggingEnabled(bool enabled) {
  activity_logging_enabled_ = enabled;
  if (enabled) {
    for (const std::string& id : active_extension_ids_)
      DOMActivityLogger::AttachToWorld(DOMActivityLogger::kMainWorldId, id);
  }
  script_injection_manager_->set_activity_logging_enabled(enabled);
  user_script_set_manager_->set_activity_logging_enabled(enabled);
}

void Dispatcher::OnUserScriptsUpdated(const std::set<HostID>& changed_hosts,
                                      const std::vector<UserScript*>& scripts) {
  UpdateActiveExtensions();
}

void Dispatcher::UpdateActiveExtensions() {
  std::set<std::string> active_extensions = active_extension_ids_;
  user_script_set_manager_->GetAllActiveExtensionIds(&active_extensions);
  delegate_->OnActiveExtensionsUpdated(active_extensions);
}

void Dispatcher::InitOriginPermissions(const Extension* extension) {
  delegate_->InitOriginPermissions(extension,
                                   IsExtensionActive(extension->id()));
  UpdateOriginPermissions(
      extension->url(),
      URLPatternSet(),  // No old permissions.
      extension->permissions_data()->GetEffectiveHostPermissions());
}

void Dispatcher::UpdateOriginPermissions(const GURL& extension_url,
                                         const URLPatternSet& old_patterns,
                                         const URLPatternSet& new_patterns) {
  static const char* kSchemes[] = {
    url::kHttpScheme,
    url::kHttpsScheme,
    url::kFileScheme,
    content::kChromeUIScheme,
    url::kFtpScheme,
#if defined(OS_CHROMEOS)
    content::kExternalFileScheme,
#endif
    extensions::kExtensionScheme,
  };
  for (size_t i = 0; i < arraysize(kSchemes); ++i) {
    const char* scheme = kSchemes[i];
    // Remove all old patterns...
    for (URLPatternSet::const_iterator pattern = old_patterns.begin();
         pattern != old_patterns.end(); ++pattern) {
      if (pattern->MatchesScheme(scheme)) {
        WebSecurityPolicy::removeOriginAccessWhitelistEntry(
            extension_url,
            WebString::fromUTF8(scheme),
            WebString::fromUTF8(pattern->host()),
            pattern->match_subdomains());
      }
    }
    // ...And add the new ones.
    for (URLPatternSet::const_iterator pattern = new_patterns.begin();
         pattern != new_patterns.end(); ++pattern) {
      if (pattern->MatchesScheme(scheme)) {
        WebSecurityPolicy::addOriginAccessWhitelistEntry(
            extension_url,
            WebString::fromUTF8(scheme),
            WebString::fromUTF8(pattern->host()),
            pattern->match_subdomains());
      }
    }
  }
}

void Dispatcher::EnableCustomElementWhiteList() {
  blink::WebCustomElement::addEmbedderCustomElementName("appview");
  blink::WebCustomElement::addEmbedderCustomElementName("appviewbrowserplugin");
  blink::WebCustomElement::addEmbedderCustomElementName("extensionoptions");
  blink::WebCustomElement::addEmbedderCustomElementName(
      "extensionoptionsbrowserplugin");
  blink::WebCustomElement::addEmbedderCustomElementName("extensionview");
  blink::WebCustomElement::addEmbedderCustomElementName(
      "extensionviewbrowserplugin");
  blink::WebCustomElement::addEmbedderCustomElementName("webview");
  blink::WebCustomElement::addEmbedderCustomElementName("webviewbrowserplugin");
}

void Dispatcher::UpdateBindings(const std::string& extension_id) {
  script_context_set().ForEach(extension_id,
                               base::Bind(&Dispatcher::UpdateBindingsForContext,
                                          base::Unretained(this)));
}

// Note: this function runs on multiple threads: main renderer thread and
// service worker threads.
void Dispatcher::UpdateBindingsForContext(ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  v8::Context::Scope context_scope(context->v8_context());

  // TODO(kalman): Make the bindings registration have zero overhead then run
  // the same code regardless of context type.
  switch (context->context_type()) {
    case Feature::UNSPECIFIED_CONTEXT:
    case Feature::WEB_PAGE_CONTEXT:
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      // Hard-code registration of any APIs that are exposed to webpage-like
      // contexts, because it's too expensive to run the full bindings code.
      // All of the same permission checks will still apply.
      if (context->GetAvailability("app").is_available())
        RegisterBinding("app", context);
      if (context->GetAvailability("webstore").is_available())
        RegisterBinding("webstore", context);
      if (context->GetAvailability("dashboardPrivate").is_available())
        RegisterBinding("dashboardPrivate", context);
      if (IsRuntimeAvailableToContext(context))
        RegisterBinding("runtime", context);
      UpdateContentCapabilities(context);
      break;

    case Feature::SERVICE_WORKER_CONTEXT:
      DCHECK(ExtensionsClient::Get()
                 ->ExtensionAPIEnabledInExtensionServiceWorkers());
    // Intentional fallthrough.
    case Feature::BLESSED_EXTENSION_CONTEXT:
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
    case Feature::CONTENT_SCRIPT_CONTEXT:
    case Feature::WEBUI_CONTEXT: {
      // Extension context; iterate through all the APIs and bind the available
      // ones.
      const FeatureProvider* api_feature_provider =
          FeatureProvider::GetAPIFeatures();
      for (const auto& map_entry : api_feature_provider->GetAllFeatures()) {
        // Internal APIs are included via require(api_name) from internal code
        // rather than chrome[api_name].
        if (map_entry.second->IsInternal())
          continue;

        // If this API has a parent feature (and isn't marked 'noparent'),
        // then this must be a function or event, so we should not register.
        if (api_feature_provider->GetParent(map_entry.second.get()) != nullptr)
          continue;

        // Skip chrome.test if this isn't a test.
        if (map_entry.first == "test" &&
            !base::CommandLine::ForCurrentProcess()->HasSwitch(
                ::switches::kTestType)) {
          continue;
        }

        if (context->IsAnyFeatureAvailableToContext(*map_entry.second.get())) {
          // TODO(lazyboy): RegisterBinding() uses |source_map_|, any thread
          // safety issue?
          RegisterBinding(map_entry.first, context);
        }
      }
      break;
    }
  }
}

void Dispatcher::RegisterBinding(const std::string& api_name,
                                 ScriptContext* context) {
  std::string bind_name;
  v8::Local<v8::Object> bind_object =
      GetOrCreateBindObjectIfAvailable(api_name, &bind_name, context);

  // Empty if the bind object failed to be created, probably because the
  // extension overrode chrome with a non-object, e.g. window.chrome = true.
  if (bind_object.IsEmpty())
    return;

  v8::Local<v8::String> v8_bind_name =
      v8::String::NewFromUtf8(context->isolate(), bind_name.c_str());
  if (bind_object->HasRealNamedProperty(v8_bind_name)) {
    // The bind object may already have the property if the API has been
    // registered before (or if the extension has put something there already,
    // but, whatevs).
    //
    // In the former case, we need to re-register the bindings for the APIs
    // which the extension now has permissions for (if any), but not touch any
    // others so that we don't destroy state such as event listeners.
    //
    // TODO(kalman): Only register available APIs to make this all moot.
    if (bind_object->HasRealNamedCallbackProperty(v8_bind_name))
      return;  // lazy binding still there, nothing to do
    if (bind_object->Get(v8_bind_name)->IsObject())
      return;  // binding has already been fully installed
  }

  ModuleSystem* module_system = context->module_system();
  if (!source_map_.Contains(api_name)) {
    module_system->RegisterNativeHandler(
        api_name,
        std::unique_ptr<NativeHandler>(
            new BindingGeneratingNativeHandler(context, api_name, "binding")));
    module_system->SetNativeLazyField(
        bind_object, bind_name, api_name, "binding");
  } else {
    module_system->SetLazyField(bind_object, bind_name, api_name, "binding");
  }
}

// NOTE: please use the naming convention "foo_natives" for these.
void Dispatcher::RegisterNativeHandlers(ModuleSystem* module_system,
                                        ScriptContext* context,
                                        RequestSender* request_sender,
                                        V8SchemaRegistry* v8_schema_registry) {
  RegisterNativeHandlers(module_system, context, this, request_sender,
                         v8_schema_registry);
  const Extension* extension = context->extension();
  int manifest_version = extension ? extension->manifest_version() : 1;
  bool is_component_extension =
      extension && Manifest::IsComponentLocation(extension->location());
  bool send_request_disabled =
      (extension && Manifest::IsUnpackedLocation(extension->location()) &&
       BackgroundInfo::HasLazyBackgroundPage(extension));
  module_system->RegisterNativeHandler(
      "process",
      std::unique_ptr<NativeHandler>(new ProcessInfoNativeHandler(
          context, context->GetExtensionID(),
          context->GetContextTypeDescription(),
          ExtensionsRendererClient::Get()->IsIncognitoProcess(),
          is_component_extension, manifest_version, send_request_disabled)));

  delegate_->RegisterNativeHandlers(this, module_system, context);
}

bool Dispatcher::IsRuntimeAvailableToContext(ScriptContext* context) {
  for (const auto& extension :
       *RendererExtensionRegistry::Get()->GetMainThreadExtensionSet()) {
    ExternallyConnectableInfo* info = static_cast<ExternallyConnectableInfo*>(
        extension->GetManifestData(manifest_keys::kExternallyConnectable));
    if (info && info->matches.MatchesURL(context->url()))
      return true;
  }
  return false;
}

void Dispatcher::UpdateContentCapabilities(ScriptContext* context) {
  APIPermissionSet permissions;
  for (const auto& extension :
       *RendererExtensionRegistry::Get()->GetMainThreadExtensionSet()) {
    blink::WebLocalFrame* web_frame = context->web_frame();
    GURL url = context->url();
    // We allow about:blank pages to take on the privileges of their parents if
    // they aren't sandboxed.
    if (web_frame && !web_frame->getSecurityOrigin().isUnique())
      url = ScriptContext::GetEffectiveDocumentURL(web_frame, url, true);
    const ContentCapabilitiesInfo& info =
        ContentCapabilitiesInfo::Get(extension.get());
    if (info.url_patterns.MatchesURL(url)) {
      APIPermissionSet new_permissions;
      APIPermissionSet::Union(permissions, info.permissions, &new_permissions);
      permissions = new_permissions;
    }
  }
  context->set_content_capabilities(permissions);
}

void Dispatcher::PopulateSourceMap() {
  const std::vector<std::pair<std::string, int> > resources = GetJsResources();
  for (std::vector<std::pair<std::string, int> >::const_iterator resource =
           resources.begin();
       resource != resources.end();
       ++resource) {
    source_map_.RegisterSource(resource->first, resource->second);
  }
  delegate_->PopulateSourceMap(&source_map_);
}

bool Dispatcher::IsWithinPlatformApp() {
  for (std::set<std::string>::iterator iter = active_extension_ids_.begin();
       iter != active_extension_ids_.end();
       ++iter) {
    const Extension* extension =
        RendererExtensionRegistry::Get()->GetByID(*iter);
    if (extension && extension->is_platform_app())
      return true;
  }
  return false;
}

// static.
v8::Local<v8::Object> Dispatcher::GetOrCreateObject(
    const v8::Local<v8::Object>& object,
    const std::string& field,
    v8::Isolate* isolate) {
  v8::Local<v8::String> key = v8::String::NewFromUtf8(isolate, field.c_str());
  // If the object has a callback property, it is assumed it is an unavailable
  // API, so it is safe to delete. This is checked before GetOrCreateObject is
  // called.
  if (object->HasRealNamedCallbackProperty(key)) {
    object->Delete(key);
  } else if (object->HasRealNamedProperty(key)) {
    v8::Local<v8::Value> value = object->Get(key);
    CHECK(value->IsObject());
    return v8::Local<v8::Object>::Cast(value);
  }

  v8::Local<v8::Object> new_object = v8::Object::New(isolate);
  object->Set(key, new_object);
  return new_object;
}

// static.
v8::Local<v8::Object> Dispatcher::GetOrCreateBindObjectIfAvailable(
    const std::string& api_name,
    std::string* bind_name,
    ScriptContext* context) {
  std::vector<std::string> split = base::SplitString(
      api_name, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  v8::Local<v8::Object> bind_object;

  // Check if this API has an ancestor. If the API's ancestor is available and
  // the API is not available, don't install the bindings for this API. If
  // the API is available and its ancestor is not, delete the ancestor and
  // install the bindings for the API. This is to prevent loading the ancestor
  // API schema if it will not be needed.
  //
  // For example:
  //  If app is available and app.window is not, just install app.
  //  If app.window is available and app is not, delete app and install
  //  app.window on a new object so app does not have to be loaded.
  const FeatureProvider* api_feature_provider =
      FeatureProvider::GetAPIFeatures();
  std::string ancestor_name;
  bool only_ancestor_available = false;

  for (size_t i = 0; i < split.size() - 1; ++i) {
    ancestor_name += (i ? "." : "") + split[i];
    if (api_feature_provider->GetFeature(ancestor_name) &&
        context->GetAvailability(ancestor_name).is_available() &&
        !context->GetAvailability(api_name).is_available()) {
      only_ancestor_available = true;
      break;
    }

    if (bind_object.IsEmpty()) {
      bind_object = AsObjectOrEmpty(GetOrCreateChrome(context));
      if (bind_object.IsEmpty())
        return v8::Local<v8::Object>();
    }
    bind_object = GetOrCreateObject(bind_object, split[i], context->isolate());
  }

  if (only_ancestor_available)
    return v8::Local<v8::Object>();

  if (bind_name)
    *bind_name = split.back();

  return bind_object.IsEmpty() ? AsObjectOrEmpty(GetOrCreateChrome(context))
                               : bind_object;
}

void Dispatcher::RequireGuestViewModules(ScriptContext* context) {
  Feature::Context context_type = context->context_type();
  ModuleSystem* module_system = context->module_system();
  bool requires_guest_view_module = false;

  // Require AppView.
  if (context->GetAvailability("appViewEmbedderInternal").is_available()) {
    requires_guest_view_module = true;
    module_system->Require("appView");
  }

  // Require ExtensionOptions.
  if (context->GetAvailability("extensionOptionsInternal").is_available()) {
    requires_guest_view_module = true;
    module_system->Require("extensionOptions");
    module_system->Require("extensionOptionsAttributes");
  }

  // Require ExtensionView.
  if (context->GetAvailability("extensionViewInternal").is_available()) {
    requires_guest_view_module = true;
    module_system->Require("extensionView");
    module_system->Require("extensionViewApiMethods");
    module_system->Require("extensionViewAttributes");
  }

  // Require WebView.
  if (context->GetAvailability("webViewInternal").is_available()) {
    requires_guest_view_module = true;
    module_system->Require("webView");
    module_system->Require("webViewApiMethods");
    module_system->Require("webViewAttributes");
  }

  if (requires_guest_view_module &&
      content::BrowserPluginGuestMode::UseCrossProcessFramesForGuests()) {
    module_system->Require("guestViewIframe");
    module_system->Require("guestViewIframeContainer");
  }

  // The "guestViewDeny" module must always be loaded last. It registers
  // error-providing custom elements for the GuestView types that are not
  // available, and thus all of those types must have been checked and loaded
  // (or not loaded) beforehand.
  if (context_type == Feature::BLESSED_EXTENSION_CONTEXT) {
    module_system->Require("guestViewDeny");
  }
}

}  // namespace extensions
