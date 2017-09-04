// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_SQLITE_SQLITE3_H_
#define THIRD_PARTY_SQLITE_SQLITE3_H_
#pragma once

// This is a shim header to include the right sqlite3 header.
// Use this instead of referencing the sqlite3 header directly.

#if defined(USE_SYSTEM_SQLITE)
#include <sqlite3.h>
#else
#include "third_party/sqlite/amalgamation/sqlite3.h"
#endif

#endif  // THIRD_PARTY_SQLITE_SQLITE3_H_
