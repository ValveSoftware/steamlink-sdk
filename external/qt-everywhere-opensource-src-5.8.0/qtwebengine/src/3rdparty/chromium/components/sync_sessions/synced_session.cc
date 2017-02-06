// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session.h"

#include "base/stl_util.h"

namespace sync_driver {

SyncedSession::SyncedSession()
    : session_tag("invalid"), device_type(TYPE_UNSET) {}

SyncedSession::~SyncedSession() {
  STLDeleteContainerPairSecondPointers(windows.begin(), windows.end());
}

sync_pb::SessionHeader SyncedSession::ToSessionHeader() const {
  sync_pb::SessionHeader header;
  SyncedWindowMap::const_iterator iter;
  for (iter = windows.begin(); iter != windows.end(); ++iter) {
    sync_pb::SessionWindow* w = header.add_window();
    w->CopyFrom(iter->second->ToSyncData());
  }
  header.set_client_name(session_name);
  switch (device_type) {
    case SyncedSession::TYPE_WIN:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_WIN);
      break;
    case SyncedSession::TYPE_MACOSX:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_MAC);
      break;
    case SyncedSession::TYPE_LINUX:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
      break;
    case SyncedSession::TYPE_CHROMEOS:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_CROS);
      break;
    case SyncedSession::TYPE_PHONE:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_PHONE);
      break;
    case SyncedSession::TYPE_TABLET:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_TABLET);
      break;
    case SyncedSession::TYPE_OTHER:
    // Intentionally fall-through
    default:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_OTHER);
      break;
  }
  return header;
}

}  // namespace sync_driver
