// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_LOCAL_SHARED_OBJECTS_COUNTER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_LOCAL_SHARED_OBJECTS_COUNTER_H_

#include <stddef.h>

#include "base/macros.h"

class GURL;

// An interface to retrieve counts of browser data objects.
class LocalSharedObjectsCounter {
 public:
  LocalSharedObjectsCounter() {}
  virtual ~LocalSharedObjectsCounter() {}

  virtual size_t GetObjectCount() const = 0;

  virtual size_t GetObjectCountForDomain(const GURL& url) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalSharedObjectsCounter);
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_LOCAL_SHARED_OBJECTS_COUNTER_H_
