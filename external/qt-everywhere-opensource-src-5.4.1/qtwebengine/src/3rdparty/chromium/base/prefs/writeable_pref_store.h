// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PREFS_WRITEABLE_PREF_STORE_H_
#define BASE_PREFS_WRITEABLE_PREF_STORE_H_

#include <string>

#include "base/basictypes.h"
#include "base/prefs/pref_store.h"

namespace base {
class Value;
}

// A pref store that can be written to as well as read from.
class BASE_PREFS_EXPORT WriteablePrefStore : public PrefStore {
 public:
  WriteablePrefStore() {}

  // Sets a |value| for |key| in the store. Assumes ownership of |value|, which
  // must be non-NULL.
  virtual void SetValue(const std::string& key, base::Value* value) = 0;

  // Removes the value for |key|.
  virtual void RemoveValue(const std::string& key) = 0;

  // Equivalent to PrefStore::GetValue but returns a mutable value.
  virtual bool GetMutableValue(const std::string& key,
                               base::Value** result) = 0;

  // Triggers a value changed notification. This function needs to be called
  // if one retrieves a list or dictionary with GetMutableValue and change its
  // value. SetValue takes care of notifications itself. Note that
  // ReportValueChanged will trigger notifications even if nothing has changed.
  virtual void ReportValueChanged(const std::string& key) = 0;


 protected:
  virtual ~WriteablePrefStore() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WriteablePrefStore);
};

#endif  // BASE_PREFS_WRITEABLE_PREF_STORE_H_
