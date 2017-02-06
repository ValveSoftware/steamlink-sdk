// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_browser_main_parts.h"

#include "base/command_line.h"
#include "base/threading/thread_restrictions.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/engine/app/blimp_engine_config.h"
#include "blimp/engine/app/settings_manager.h"
#include "blimp/engine/common/blimp_browser_context.h"
#include "blimp/engine/session/blimp_engine_session.h"
#include "blimp/net/blimp_connection.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/main_function_params.h"
#include "net/base/net_module.h"
#include "net/log/net_log.h"

namespace blimp {
namespace engine {

BlimpBrowserMainParts::BlimpBrowserMainParts(
    const content::MainFunctionParams& parameters) {}

BlimpBrowserMainParts::~BlimpBrowserMainParts() {}

void BlimpBrowserMainParts::PreEarlyInitialization() {
  // Fetch the engine config from the command line, and crash if invalid. Allow
  // IO operations even though this is not in the FILE thread as this is
  // necessary for Blimp startup and occurs before any user interaction.
  {
    const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    engine_config_ = BlimpEngineConfig::Create(*cmd_line);
    CHECK(engine_config_);
  }
}

void BlimpBrowserMainParts::PreMainMessageLoopRun() {
  net_log_.reset(new net::NetLog());
  settings_manager_.reset(new SettingsManager);
  std::unique_ptr<BlimpBrowserContext> browser_context(
      new BlimpBrowserContext(false, net_log_.get()));
  engine_session_.reset(
      new BlimpEngineSession(std::move(browser_context), net_log_.get(),
                             engine_config_.get(), settings_manager_.get()));
  engine_session_->Initialize();
}

void BlimpBrowserMainParts::PostMainMessageLoopRun() {
  engine_session_.reset();
}

BlimpBrowserContext* BlimpBrowserMainParts::GetBrowserContext() {
  return engine_session_->browser_context();
}

SettingsManager* BlimpBrowserMainParts::GetSettingsManager() {
  return settings_manager_.get();
}

BlobChannelSender* BlimpBrowserMainParts::GetBlobChannelSender() {
  return engine_session_->blob_channel_sender();
}

BlimpEngineSession* BlimpBrowserMainParts::GetBlimpEngineSession() {
  return engine_session_.get();
}

}  // namespace engine
}  // namespace blimp
