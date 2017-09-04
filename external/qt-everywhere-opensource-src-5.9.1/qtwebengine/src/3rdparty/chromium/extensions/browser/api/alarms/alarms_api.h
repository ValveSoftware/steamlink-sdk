// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_ALARMS_ALARMS_API_H_
#define EXTENSIONS_BROWSER_API_ALARMS_ALARMS_API_H_

#include <vector>

#include "extensions/browser/extension_function.h"

namespace base {
class Clock;
}  // namespace base

namespace extensions {
struct Alarm;
using AlarmList = std::vector<std::unique_ptr<Alarm>>;

class AlarmsCreateFunction : public AsyncExtensionFunction {
 public:
  AlarmsCreateFunction();
  // Use |clock| instead of the default clock. Does not take ownership
  // of |clock|. Used for testing.
  explicit AlarmsCreateFunction(base::Clock* clock);

 protected:
  ~AlarmsCreateFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;
  DECLARE_EXTENSION_FUNCTION("alarms.create", ALARMS_CREATE)
 private:
  void Callback();

  base::Clock* const clock_;
  // Whether or not we own |clock_|. This is needed because we own it
  // when we create it ourselves, but not when it's passed in for
  // testing.
  bool owns_clock_;
};

class AlarmsGetFunction : public AsyncExtensionFunction {
 protected:
  ~AlarmsGetFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  void Callback(const std::string& name, Alarm* alarm);
  DECLARE_EXTENSION_FUNCTION("alarms.get", ALARMS_GET)
};

class AlarmsGetAllFunction : public AsyncExtensionFunction {
 protected:
  ~AlarmsGetAllFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  void Callback(const AlarmList* alarms);
  DECLARE_EXTENSION_FUNCTION("alarms.getAll", ALARMS_GETALL)
};

class AlarmsClearFunction : public AsyncExtensionFunction {
 protected:
  ~AlarmsClearFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  void Callback(const std::string& name, bool success);
  DECLARE_EXTENSION_FUNCTION("alarms.clear", ALARMS_CLEAR)
};

class AlarmsClearAllFunction : public AsyncExtensionFunction {
 protected:
  ~AlarmsClearAllFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  void Callback();
  DECLARE_EXTENSION_FUNCTION("alarms.clearAll", ALARMS_CLEARALL)
};

}  //  namespace extensions

#endif  // EXTENSIONS_BROWSER_API_ALARMS_ALARMS_API_H_
