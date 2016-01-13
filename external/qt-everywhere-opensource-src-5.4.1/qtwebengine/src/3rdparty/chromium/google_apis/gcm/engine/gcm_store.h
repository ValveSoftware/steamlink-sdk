// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_GCM_STORE_H_
#define GOOGLE_APIS_GCM_ENGINE_GCM_STORE_H_

#include <map>
#include <string>
#include <vector>

#include <google/protobuf/message_lite.h>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/engine/registration_info.h"

namespace gcm {

class MCSMessage;

// A GCM data store interface. GCM Store will handle persistence portion of RMQ,
// as well as store device and user checkin information.
class GCM_EXPORT GCMStore {
 public:
  // Map of message id to message data for outgoing messages.
  typedef std::map<std::string, linked_ptr<google::protobuf::MessageLite> >
      OutgoingMessageMap;

  // Container for Load(..) results.
  struct GCM_EXPORT LoadResult {
    LoadResult();
    ~LoadResult();

    bool success;
    uint64 device_android_id;
    uint64 device_security_token;
    RegistrationInfoMap registrations;
    std::vector<std::string> incoming_messages;
    OutgoingMessageMap outgoing_messages;
    std::map<std::string, std::string> gservices_settings;
    std::string gservices_digest;
    base::Time last_checkin_time;
  };

  typedef std::vector<std::string> PersistentIdList;
  typedef base::Callback<void(scoped_ptr<LoadResult> result)> LoadCallback;
  typedef base::Callback<void(bool success)> UpdateCallback;

  GCMStore();
  virtual ~GCMStore();

  // Load the data from persistent store and pass the initial state back to
  // caller.
  virtual void Load(const LoadCallback& callback) = 0;

  // Close the persistent store.
  virtual void Close() = 0;

  // Clears the GCM store of all data.
  virtual void Destroy(const UpdateCallback& callback) = 0;

  // Sets this device's messaging credentials.
  virtual void SetDeviceCredentials(uint64 device_android_id,
                                    uint64 device_security_token,
                                    const UpdateCallback& callback) = 0;

  // Registration info.
  virtual void AddRegistration(const std::string& app_id,
                               const linked_ptr<RegistrationInfo>& registration,
                               const UpdateCallback& callback) = 0;
  virtual void RemoveRegistration(const std::string& app_id,
                                  const UpdateCallback& callback) = 0;

  // Unacknowledged incoming message handling.
  virtual void AddIncomingMessage(const std::string& persistent_id,
                                  const UpdateCallback& callback) = 0;
  virtual void RemoveIncomingMessage(const std::string& persistent_id,
                                     const UpdateCallback& callback) = 0;
  virtual void RemoveIncomingMessages(const PersistentIdList& persistent_ids,
                                      const UpdateCallback& callback) = 0;

  // Unacknowledged outgoing messages handling.
  // Returns false if app has surpassed message limits, else returns true. Note
  // that the message isn't persisted until |callback| is invoked with
  // |success| == true.
  virtual bool AddOutgoingMessage(const std::string& persistent_id,
                                  const MCSMessage& message,
                                  const UpdateCallback& callback) = 0;
  virtual void OverwriteOutgoingMessage(const std::string& persistent_id,
                                        const MCSMessage& message,
                                        const UpdateCallback& callback) = 0;
  virtual void RemoveOutgoingMessage(const std::string& persistent_id,
                                     const UpdateCallback& callback) = 0;
  virtual void RemoveOutgoingMessages(const PersistentIdList& persistent_ids,
                                      const UpdateCallback& callback) = 0;

  // Sets last device's checkin time.
  virtual void SetLastCheckinTime(const base::Time& last_checkin_time,
                                  const UpdateCallback& callback) = 0;

  // G-service settings handling.
  // Persists |settings| and |settings_digest|. It completely replaces the
  // existing data.
  virtual void SetGServicesSettings(
      const std::map<std::string, std::string>& settings,
      const std::string& settings_digest,
      const UpdateCallback& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMStore);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_GCM_STORE_H_
