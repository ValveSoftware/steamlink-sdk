// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/sync_call_restrictions.h"

#if ENABLE_SYNC_CALL_RESTRICTIONS

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_local.h"
#include "mojo/public/c/system/core.h"

namespace mojo {

namespace {

class SyncCallSettings {
 public:
  static SyncCallSettings* current();

  bool allowed() const {
    return scoped_allow_count_ > 0 || system_defined_value_;
  }

  void IncreaseScopedAllowCount() { scoped_allow_count_++; }
  void DecreaseScopedAllowCount() {
    DCHECK_LT(0u, scoped_allow_count_);
    scoped_allow_count_--;
  }

 private:
  SyncCallSettings();
  ~SyncCallSettings();

  bool system_defined_value_ = true;
  size_t scoped_allow_count_ = 0;
};

base::LazyInstance<base::ThreadLocalPointer<SyncCallSettings>>
    g_sync_call_settings = LAZY_INSTANCE_INITIALIZER;

// static
SyncCallSettings* SyncCallSettings::current() {
  SyncCallSettings* result = g_sync_call_settings.Pointer()->Get();
  if (!result) {
    result = new SyncCallSettings();
    DCHECK_EQ(result, g_sync_call_settings.Pointer()->Get());
  }
  return result;
}

SyncCallSettings::SyncCallSettings() {
  MojoResult result = MojoGetProperty(MOJO_PROPERTY_TYPE_SYNC_CALL_ALLOWED,
                                      &system_defined_value_);
  DCHECK_EQ(MOJO_RESULT_OK, result);

  DCHECK(!g_sync_call_settings.Pointer()->Get());
  g_sync_call_settings.Pointer()->Set(this);
}

SyncCallSettings::~SyncCallSettings() {
  g_sync_call_settings.Pointer()->Set(nullptr);
}

}  // namespace

// static
void SyncCallRestrictions::AssertSyncCallAllowed() {
  if (!SyncCallSettings::current()->allowed()) {
      LOG(FATAL) << "Mojo sync calls are not allowed in this process because "
                 << "they can lead to jank and deadlock. If you must make an "
                 << "exception, please see "
                 << "SyncCallRestrictions::ScopedAllowSyncCall and consult "
                 << "mojo/OWNERS.";
  }
}

// static
void SyncCallRestrictions::IncreaseScopedAllowCount() {
  SyncCallSettings::current()->IncreaseScopedAllowCount();
}

// static
void SyncCallRestrictions::DecreaseScopedAllowCount() {
  SyncCallSettings::current()->DecreaseScopedAllowCount();
}

}  // namespace mojo

#endif  // ENABLE_SYNC_CALL_RESTRICTIONS
