// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store.h"

#include <utility>

#include "base/logging.h"

// Implementation of Status.

ValueStore::Status::Status() : code(OK), restore_status(RESTORE_NONE) {}

ValueStore::Status::Status(StatusCode code, const std::string& message)
    : Status(code, RESTORE_NONE, message) {}

ValueStore::Status::Status(StatusCode code,
                           BackingStoreRestoreStatus restore_status,
                           const std::string& message)
    : code(code), restore_status(restore_status), message(message) {}

ValueStore::Status::~Status() {}

void ValueStore::Status::Merge(const Status& status) {
  if (code == OK)
    code = status.code;
  if (message.empty() && !status.message.empty())
    message = status.message;
  if (restore_status == RESTORE_NONE)
    restore_status = status.restore_status;
}

// Implementation of ReadResultType.

ValueStore::ReadResultType::ReadResultType(
    std::unique_ptr<base::DictionaryValue> settings,
    const Status& status)
    : settings_(std::move(settings)), status_(status) {
  CHECK(settings_);
}

ValueStore::ReadResultType::ReadResultType(const Status& status)
    : status_(status) {}

ValueStore::ReadResultType::~ReadResultType() {}

// Implementation of WriteResultType.

ValueStore::WriteResultType::WriteResultType(
    std::unique_ptr<ValueStoreChangeList> changes,
    const Status& status)
    : changes_(std::move(changes)), status_(status) {
  CHECK(changes_);
}

ValueStore::WriteResultType::WriteResultType(const Status& status)
    : status_(status) {}

ValueStore::WriteResultType::~WriteResultType() {}
