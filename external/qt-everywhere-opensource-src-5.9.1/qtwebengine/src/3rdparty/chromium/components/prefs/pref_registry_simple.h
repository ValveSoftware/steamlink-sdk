// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_REGISTRY_SIMPLE_H_
#define COMPONENTS_PREFS_PREF_REGISTRY_SIMPLE_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "components/prefs/base_prefs_export.h"
#include "components/prefs/pref_registry.h"

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
}

// A simple implementation of PrefRegistry.
class COMPONENTS_PREFS_EXPORT PrefRegistrySimple : public PrefRegistry {
 public:
  PrefRegistrySimple();

  void RegisterBooleanPref(const std::string& path, bool default_value);
  void RegisterIntegerPref(const std::string& path, int default_value);
  void RegisterDoublePref(const std::string& path, double default_value);
  void RegisterStringPref(const std::string& path,
                          const std::string& default_value);
  void RegisterFilePathPref(const std::string& path,
                            const base::FilePath& default_value);
  void RegisterListPref(const std::string& path);
  void RegisterDictionaryPref(const std::string& path);
  void RegisterListPref(const std::string& path,
                        base::ListValue* default_value);
  void RegisterDictionaryPref(const std::string& path,
                              base::DictionaryValue* default_value);
  void RegisterInt64Pref(const std::string& path, int64_t default_value);
  void RegisterUint64Pref(const std::string&, uint64_t default_value);

  // Versions of registration functions that accept PrefRegistrationFlags.
  // |flags| is a bitmask of PrefRegistrationFlags.
  void RegisterBooleanPref(const std::string&,
                           bool default_value,
                           uint32_t flags);
  void RegisterIntegerPref(const std::string&,
                           int default_value,
                           uint32_t flags);
  void RegisterDoublePref(const std::string&,
                          double default_value,
                          uint32_t flags);
  void RegisterStringPref(const std::string&,
                          const std::string& default_value,
                          uint32_t flags);
  void RegisterFilePathPref(const std::string&,
                            const base::FilePath& default_value,
                            uint32_t flags);
  void RegisterListPref(const std::string&, uint32_t flags);
  void RegisterDictionaryPref(const std::string&, uint32_t flags);
  void RegisterListPref(const std::string&,
                        base::ListValue* default_value,
                        uint32_t flags);
  void RegisterDictionaryPref(const std::string&,
                              base::DictionaryValue* default_value,
                              uint32_t flags);
  void RegisterInt64Pref(const std::string&,
                         int64_t default_value,
                         uint32_t flags);
  void RegisterUint64Pref(const std::string&,
                          uint64_t default_value,
                          uint32_t flags);

 protected:
  ~PrefRegistrySimple() override;

  // Allows subclasses to hook into pref registration.
  virtual void OnPrefRegistered(const std::string&,
                                base::Value* default_value,
                                uint32_t flags);

 private:
  void RegisterPrefAndNotify(const std::string&,
                             base::Value* default_value,
                             uint32_t flags);

  DISALLOW_COPY_AND_ASSIGN(PrefRegistrySimple);
};

#endif  // COMPONENTS_PREFS_PREF_REGISTRY_SIMPLE_H_
