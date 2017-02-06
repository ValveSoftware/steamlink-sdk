// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_ERROR_DELEGATE_UTIL_H_
#define SQL_ERROR_DELEGATE_UTIL_H_

#include "sql/sql_export.h"

namespace sql {

// Returns true if it is highly unlikely that the database can recover from
// |error|.
SQL_EXPORT bool IsErrorCatastrophic(int error);

}  // namespace sql

#endif  // SQL_ERROR_DELEGATE_UTIL_H_
