// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/key_systems_support_uma.h"

#include <string>

#include "content/public/renderer/render_thread.h"
#include "content/renderer/media/crypto/key_systems.h"

namespace content {

namespace {

const char kKeySystemSupportActionPrefix[] = "KeySystemSupport.";

// Reports an event only once.
class OneTimeReporter {
 public:
  OneTimeReporter(const std::string& key_system,
                  bool has_type,
                  const std::string& event);
  ~OneTimeReporter();

  void Report();

 private:
  bool is_reported_;
  std::string action_;
};

OneTimeReporter::OneTimeReporter(const std::string& key_system,
                                 bool has_type,
                                 const std::string& event)
    : is_reported_(false) {
  action_ = kKeySystemSupportActionPrefix + KeySystemNameForUMA(key_system);
  if (has_type)
    action_ += "WithType";
  action_ += '.' + event;
}

OneTimeReporter::~OneTimeReporter() {}

void OneTimeReporter::Report() {
  if (is_reported_)
    return;
  RenderThread::Get()->RecordComputedAction(action_);
  is_reported_ = true;
}

}  // namespace

class KeySystemsSupportUMA::Reporter {
 public:
  explicit Reporter(const std::string& key_system);
  ~Reporter();

  void Report(bool has_type, bool is_supported);

 private:
  const std::string key_system_;

  OneTimeReporter call_reporter_;
  OneTimeReporter call_with_type_reporter_;
  OneTimeReporter support_reporter_;
  OneTimeReporter support_with_type_reporter_;
};

KeySystemsSupportUMA::Reporter::Reporter(const std::string& key_system)
    : key_system_(key_system),
      call_reporter_(key_system, false, "Queried"),
      call_with_type_reporter_(key_system, true, "Queried"),
      support_reporter_(key_system, false, "Supported"),
      support_with_type_reporter_(key_system, true, "Supported") {}

KeySystemsSupportUMA::Reporter::~Reporter() {}

void KeySystemsSupportUMA::Reporter::Report(bool has_type, bool is_supported) {
  call_reporter_.Report();
  if (has_type)
    call_with_type_reporter_.Report();

  if (!is_supported)
    return;

  support_reporter_.Report();
  if (has_type)
    support_with_type_reporter_.Report();
}

KeySystemsSupportUMA::KeySystemsSupportUMA() {}

KeySystemsSupportUMA::~KeySystemsSupportUMA() {}

void KeySystemsSupportUMA::AddKeySystemToReport(const std::string& key_system) {
  DCHECK(!GetReporter(key_system));
  reporters_.set(key_system, scoped_ptr<Reporter>(new Reporter(key_system)));
}

void KeySystemsSupportUMA::ReportKeySystemQuery(const std::string& key_system,
                                                bool has_type) {
  Reporter* reporter = GetReporter(key_system);
  if (!reporter)
    return;
  reporter->Report(has_type, false);
}

void KeySystemsSupportUMA::ReportKeySystemSupport(const std::string& key_system,
                                                  bool has_type) {
  Reporter* reporter = GetReporter(key_system);
  if (!reporter)
    return;
  reporter->Report(has_type, true);
}

KeySystemsSupportUMA::Reporter* KeySystemsSupportUMA::GetReporter(
    const std::string& key_system) {
  Reporters::iterator reporter = reporters_.find(key_system);
  if (reporter == reporters_.end())
    return NULL;
  return reporter->second;
}

}  // namespace content
