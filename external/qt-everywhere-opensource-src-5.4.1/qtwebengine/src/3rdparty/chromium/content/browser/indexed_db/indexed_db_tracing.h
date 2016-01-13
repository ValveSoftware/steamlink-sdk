// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRACING_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRACING_H_

#include "base/debug/trace_event.h"
#define IDB_TRACE(a) TRACE_EVENT0("IndexedDB", (a));
#define IDB_TRACE1(a, arg1_name, arg1_val) \
  TRACE_EVENT1("IndexedDB", (a), (arg1_name), (arg1_val));

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRACING_H_
