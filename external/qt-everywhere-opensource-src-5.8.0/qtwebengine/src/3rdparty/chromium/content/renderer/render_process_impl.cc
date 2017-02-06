// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_process_impl.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <objidl.h>
#include <mlang.h>
#endif

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/sys_info.h"
#include "content/child/site_isolation_stats_gatherer.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "v8/include/v8.h"

namespace {

const base::Feature kV8_ES2015_TailCalls_Feature {
  "V8_ES2015_TailCalls", base::FEATURE_DISABLED_BY_DEFAULT
};

const base::Feature kV8_ES2016_ExplicitTailCalls_Feature{
    "V8_ES2016_ExplicitTailCalls", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kV8SerializeEagerFeature{"V8_Serialize_Eager",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kV8SerializeAgeCodeFeature{
    "V8_Serialize_Age_Code", base::FEATURE_DISABLED_BY_DEFAULT};

void SetV8FlagIfFeature(const base::Feature& feature, const char* v8_flag) {
  if (base::FeatureList::IsEnabled(feature)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

void SetV8FlagIfHasSwitch(const char* switch_name, const char* v8_flag) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switch_name)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

}  // namespace

namespace content {

RenderProcessImpl::RenderProcessImpl()
    : enabled_bindings_(0) {
#if defined(OS_WIN)
  // HACK:  See http://b/issue?id=1024307 for rationale.
  if (GetModuleHandle(L"LPK.DLL") == NULL) {
    // Makes sure lpk.dll is loaded by gdi32 to make sure ExtTextOut() works
    // when buffering into a EMF buffer for printing.
    typedef BOOL (__stdcall *GdiInitializeLanguagePack)(int LoadedShapingDLLs);
    GdiInitializeLanguagePack gdi_init_lpk =
        reinterpret_cast<GdiInitializeLanguagePack>(GetProcAddress(
            GetModuleHandle(L"GDI32.DLL"),
            "GdiInitializeLanguagePack"));
    DCHECK(gdi_init_lpk);
    if (gdi_init_lpk) {
      gdi_init_lpk(0);
    }
  }
#endif

  if (base::SysInfo::IsLowEndDevice()) {
    std::string optimize_flag("--optimize-for-size");
    v8::V8::SetFlagsFromString(optimize_flag.c_str(),
                               static_cast<int>(optimize_flag.size()));
  }

  SetV8FlagIfFeature(kV8_ES2015_TailCalls_Feature, "--harmony-tailcalls");
  SetV8FlagIfFeature(kV8_ES2016_ExplicitTailCalls_Feature,
                     "--harmony-explicit-tailcalls");
  SetV8FlagIfFeature(kV8SerializeEagerFeature, "--serialize_eager");
  SetV8FlagIfFeature(kV8SerializeAgeCodeFeature, "--serialize_age_code");
  SetV8FlagIfHasSwitch(switches::kDisableJavaScriptHarmonyShipping,
                       "--noharmony-shipping");
  SetV8FlagIfHasSwitch(switches::kJavaScriptHarmony, "--harmony");
  SetV8FlagIfHasSwitch(switches::kEnableWasm, "--expose-wasm");

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kJavaScriptFlags)) {
    std::string flags(
        command_line.GetSwitchValueASCII(switches::kJavaScriptFlags));
    v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
  }

  SiteIsolationStatsGatherer::SetEnabled(
      GetContentClient()->renderer()->ShouldGatherSiteIsolationStats());
}

RenderProcessImpl::~RenderProcessImpl() {
#ifndef NDEBUG
  int count = blink::WebFrame::instanceCount();
  if (count)
    DLOG(ERROR) << "WebFrame LEAKED " << count << " TIMES";
#endif

  GetShutDownEvent()->Signal();
}

void RenderProcessImpl::AddBindings(int bindings) {
  enabled_bindings_ |= bindings;
}

int RenderProcessImpl::GetEnabledBindings() const {
  return enabled_bindings_;
}

}  // namespace content
