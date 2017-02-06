// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_injection.h"

#include <map>
#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/host_id.h"
#include "extensions/renderer/dom_activity_logger.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extension_groups.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/script_injection_callback.h"
#include "extensions/renderer/scripts_run_info.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using IsolatedWorldMap = std::map<std::string, int>;
base::LazyInstance<IsolatedWorldMap> g_isolated_worlds =
    LAZY_INSTANCE_INITIALIZER;

const int64_t kInvalidRequestId = -1;

// The id of the next pending injection.
int64_t g_next_pending_id = 0;

// Gets the isolated world ID to use for the given |injection_host|
// in the given |frame|. If no isolated world has been created for that
// |injection_host| one will be created and initialized.
int GetIsolatedWorldIdForInstance(const InjectionHost* injection_host,
                                  blink::WebLocalFrame* frame) {
  static int g_next_isolated_world_id =
      ExtensionsRendererClient::Get()->GetLowestIsolatedWorldId();

  IsolatedWorldMap& isolated_worlds = g_isolated_worlds.Get();

  int id = 0;
  const std::string& key = injection_host->id().id();
  IsolatedWorldMap::iterator iter = isolated_worlds.find(key);
  if (iter != isolated_worlds.end()) {
    id = iter->second;
  } else {
    id = g_next_isolated_world_id++;
    // This map will tend to pile up over time, but realistically, you're never
    // going to have enough injection hosts for it to matter.
    isolated_worlds[key] = id;
  }

  // We need to set the isolated world origin and CSP even if it's not a new
  // world since these are stored per frame, and we might not have used this
  // isolated world in this frame before.
  frame->setIsolatedWorldSecurityOrigin(
      id, blink::WebSecurityOrigin::create(injection_host->url()));
  frame->setIsolatedWorldContentSecurityPolicy(
      id, blink::WebString::fromUTF8(
          injection_host->GetContentSecurityPolicy()));
  frame->setIsolatedWorldHumanReadableName(
      id, blink::WebString::fromUTF8(injection_host->name()));

  return id;
}

}  // namespace

// Watches for the deletion of a RenderFrame, after which is_valid will return
// false.
class ScriptInjection::FrameWatcher : public content::RenderFrameObserver {
 public:
  FrameWatcher(content::RenderFrame* render_frame,
               ScriptInjection* injection)
      : content::RenderFrameObserver(render_frame),
        injection_(injection) {}
  ~FrameWatcher() override {}

 private:
  void FrameDetached() override { injection_->invalidate_render_frame(); }
  void OnDestruct() override { injection_->invalidate_render_frame(); }

  ScriptInjection* injection_;

  DISALLOW_COPY_AND_ASSIGN(FrameWatcher);
};

// static
std::string ScriptInjection::GetHostIdForIsolatedWorld(int isolated_world_id) {
  const IsolatedWorldMap& isolated_worlds = g_isolated_worlds.Get();

  for (const auto& iter : isolated_worlds) {
    if (iter.second == isolated_world_id)
      return iter.first;
  }
  return std::string();
}

// static
void ScriptInjection::RemoveIsolatedWorld(const std::string& host_id) {
  g_isolated_worlds.Get().erase(host_id);
}

ScriptInjection::ScriptInjection(
    std::unique_ptr<ScriptInjector> injector,
    content::RenderFrame* render_frame,
    std::unique_ptr<const InjectionHost> injection_host,
    UserScript::RunLocation run_location,
    bool log_activity)
    : injector_(std::move(injector)),
      render_frame_(render_frame),
      injection_host_(std::move(injection_host)),
      run_location_(run_location),
      request_id_(kInvalidRequestId),
      complete_(false),
      did_inject_js_(false),
      log_activity_(log_activity),
      frame_watcher_(new FrameWatcher(render_frame, this)),
      weak_ptr_factory_(this) {
  CHECK(injection_host_.get());
}

ScriptInjection::~ScriptInjection() {
  if (!complete_)
    NotifyWillNotInject(ScriptInjector::WONT_INJECT);
}

ScriptInjection::InjectionResult ScriptInjection::TryToInject(
    UserScript::RunLocation current_location,
    ScriptsRunInfo* scripts_run_info,
    const CompletionCallback& async_completion_callback) {
  if (current_location < run_location_)
    return INJECTION_WAITING;  // Wait for the right location.

  if (request_id_ != kInvalidRequestId) {
    // We're waiting for permission right now, try again later.
    return INJECTION_WAITING;
  }

  if (!injection_host_) {
    NotifyWillNotInject(ScriptInjector::EXTENSION_REMOVED);
    return INJECTION_FINISHED;  // We're done.
  }

  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  switch (injector_->CanExecuteOnFrame(
      injection_host_.get(), web_frame,
      ExtensionFrameHelper::Get(render_frame_)->tab_id())) {
    case PermissionsData::ACCESS_DENIED:
      NotifyWillNotInject(ScriptInjector::NOT_ALLOWED);
      return INJECTION_FINISHED;  // We're done.
    case PermissionsData::ACCESS_WITHHELD:
      RequestPermissionFromBrowser();
      return INJECTION_WAITING;  // Wait around for permission.
    case PermissionsData::ACCESS_ALLOWED:
      InjectionResult result = Inject(scripts_run_info);
      // If the injection is blocked, we need to set the manager so we can
      // notify it upon completion.
      if (result == INJECTION_BLOCKED)
        async_completion_callback_ = async_completion_callback;
      return result;
  }

  NOTREACHED();
  return INJECTION_FINISHED;
}

ScriptInjection::InjectionResult ScriptInjection::OnPermissionGranted(
    ScriptsRunInfo* scripts_run_info) {
  if (!injection_host_) {
    NotifyWillNotInject(ScriptInjector::EXTENSION_REMOVED);
    return INJECTION_FINISHED;
  }

  return Inject(scripts_run_info);
}

void ScriptInjection::OnHostRemoved() {
  injection_host_.reset(nullptr);
}

void ScriptInjection::RequestPermissionFromBrowser() {
  // If we are just notifying the browser of the injection, then send an
  // invalid request (which is treated like a notification).
  request_id_ = g_next_pending_id++;
  render_frame_->Send(new ExtensionHostMsg_RequestScriptInjectionPermission(
      render_frame_->GetRoutingID(), host_id().id(), injector_->script_type(),
      run_location_, request_id_));
}

void ScriptInjection::NotifyWillNotInject(
    ScriptInjector::InjectFailureReason reason) {
  complete_ = true;
  injector_->OnWillNotInject(reason, render_frame_);
}

ScriptInjection::InjectionResult ScriptInjection::Inject(
    ScriptsRunInfo* scripts_run_info) {
  DCHECK(injection_host_);
  DCHECK(scripts_run_info);
  DCHECK(!complete_);

  bool should_inject_js = injector_->ShouldInjectJs(run_location_);
  bool should_inject_css = injector_->ShouldInjectCss(run_location_);
  DCHECK(should_inject_js || should_inject_css);

  if (should_inject_js)
    InjectJs();
  if (should_inject_css)
    InjectCss();

  complete_ = did_inject_js_ || !should_inject_js;

  injector_->GetRunInfo(scripts_run_info, run_location_);

  if (complete_) {
    injector_->OnInjectionComplete(std::move(execution_result_), run_location_,
                                   render_frame_);
  } else {
    ++scripts_run_info->num_blocking_js;
  }

  return complete_ ? INJECTION_FINISHED : INJECTION_BLOCKED;
}

void ScriptInjection::InjectJs() {
  DCHECK(!did_inject_js_);
  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  std::vector<blink::WebScriptSource> sources =
      injector_->GetJsSources(run_location_);
  bool in_main_world = injector_->ShouldExecuteInMainWorld();
  int world_id = in_main_world
                     ? DOMActivityLogger::kMainWorldId
                     : GetIsolatedWorldIdForInstance(injection_host_.get(),
                                                     web_frame);
  bool is_user_gesture = injector_->IsUserGesture();

  std::unique_ptr<blink::WebScriptExecutionCallback> callback(
      new ScriptInjectionCallback(
          base::Bind(&ScriptInjection::OnJsInjectionCompleted,
                     weak_ptr_factory_.GetWeakPtr())));

  base::ElapsedTimer exec_timer;
  if (injection_host_->id().type() == HostID::EXTENSIONS && log_activity_)
    DOMActivityLogger::AttachToWorld(world_id, injection_host_->id().id());
  if (in_main_world) {
    // We only inject in the main world for javascript: urls.
    DCHECK_EQ(1u, sources.size());

    web_frame->requestExecuteScriptAndReturnValue(sources.front(),
                                                  is_user_gesture,
                                                  callback.release());
  } else {
    web_frame->requestExecuteScriptInIsolatedWorld(
        world_id,
        &sources.front(),
        sources.size(),
        EXTENSION_GROUP_CONTENT_SCRIPTS,
        is_user_gesture,
        callback.release());
  }

  if (injection_host_->id().type() == HostID::EXTENSIONS)
    UMA_HISTOGRAM_TIMES("Extensions.InjectScriptTime", exec_timer.Elapsed());
}

void ScriptInjection::OnJsInjectionCompleted(
    const blink::WebVector<v8::Local<v8::Value> >& results) {
  DCHECK(!did_inject_js_);

  bool expects_results = injector_->ExpectsResults();
  if (expects_results) {
    if (!results.isEmpty() && !results[0].IsEmpty()) {
      // Right now, we only support returning single results (per frame).
      std::unique_ptr<content::V8ValueConverter> v8_converter(
          content::V8ValueConverter::create());
      // It's safe to always use the main world context when converting
      // here. V8ValueConverterImpl shouldn't actually care about the
      // context scope, and it switches to v8::Object's creation context
      // when encountered.
      v8::Local<v8::Context> context =
          render_frame_->GetWebFrame()->mainWorldScriptContext();
      execution_result_ = v8_converter->FromV8Value(results[0], context);
    }
    if (!execution_result_.get())
      execution_result_ = base::Value::CreateNullValue();
  }
  did_inject_js_ = true;

  // If |async_completion_callback_| is set, it means the script finished
  // asynchronously, and we should run it.
  if (!async_completion_callback_.is_null()) {
    injector_->OnInjectionComplete(std::move(execution_result_), run_location_,
                                   render_frame_);
    // Warning: this object can be destroyed after this line!
    async_completion_callback_.Run(this);
  }
}

void ScriptInjection::InjectCss() {
  std::vector<std::string> css_sources =
      injector_->GetCssSources(run_location_);
  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  for (const std::string& css : css_sources)
    web_frame->document().insertStyleSheet(blink::WebString::fromUTF8(css));
}

}  // namespace extensions
