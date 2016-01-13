// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_GCM_STORE_IMPL_H_
#define GOOGLE_APIS_GCM_ENGINE_GCM_STORE_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/engine/gcm_store.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace gcm {

class Encryptor;

// An implementation of GCM Store that uses LevelDB for persistence.
// It performs all blocking operations on the blocking task runner, and posts
// all callbacks to the thread on which the GCMStoreImpl is created.
class GCM_EXPORT GCMStoreImpl : public GCMStore {
 public:
  GCMStoreImpl(const base::FilePath& path,
               scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
               scoped_ptr<Encryptor> encryptor);
  virtual ~GCMStoreImpl();

  // Load the directory and pass the initial state back to caller.
  virtual void Load(const LoadCallback& callback) OVERRIDE;

  // Closes the GCM store.
  virtual void Close() OVERRIDE;

  // Clears the GCM store of all data and destroys any LevelDB files associated
  // with this store.
  // WARNING: this will permanently destroy any pending outgoing messages
  // and require the device to re-create credentials and serial number mapping
  // tables.
  virtual void Destroy(const UpdateCallback& callback) OVERRIDE;

  // Sets this device's messaging credentials.
  virtual void SetDeviceCredentials(uint64 device_android_id,
                                    uint64 device_security_token,
                                    const UpdateCallback& callback) OVERRIDE;

  // Registration info.
  virtual void AddRegistration(const std::string& app_id,
                               const linked_ptr<RegistrationInfo>& registration,
                               const UpdateCallback& callback) OVERRIDE;
  virtual void RemoveRegistration(const std::string& app_id,
                                  const UpdateCallback& callback) OVERRIDE;

  // Unacknowledged incoming message handling.
  virtual void AddIncomingMessage(const std::string& persistent_id,
                                  const UpdateCallback& callback) OVERRIDE;
  virtual void RemoveIncomingMessage(const std::string& persistent_id,
                                     const UpdateCallback& callback) OVERRIDE;
  virtual void RemoveIncomingMessages(const PersistentIdList& persistent_ids,
                                      const UpdateCallback& callback) OVERRIDE;

  // Unacknowledged outgoing messages handling.
  virtual bool AddOutgoingMessage(const std::string& persistent_id,
                                  const MCSMessage& message,
                                  const UpdateCallback& callback) OVERRIDE;
  virtual void OverwriteOutgoingMessage(const std::string& persistent_id,
                                        const MCSMessage& message,
                                        const UpdateCallback& callback)
      OVERRIDE;
  virtual void RemoveOutgoingMessage(const std::string& persistent_id,
                                     const UpdateCallback& callback) OVERRIDE;
  virtual void RemoveOutgoingMessages(const PersistentIdList& persistent_ids,
                                      const UpdateCallback& callback) OVERRIDE;

  // Sets last device's checkin time.
  virtual void SetLastCheckinTime(const base::Time& last_checkin_time,
                                  const UpdateCallback& callback) OVERRIDE;

  // G-service settings handling.
  virtual void SetGServicesSettings(
      const std::map<std::string, std::string>& settings,
      const std::string& settings_digest,
      const UpdateCallback& callback) OVERRIDE;

 private:
  typedef std::map<std::string, int> AppIdToMessageCountMap;

  // Continuation to update the per-app message counts after a load.
  void LoadContinuation(const LoadCallback& callback,
                        scoped_ptr<LoadResult> result);

  // Continuation to update the per-app message counts when adding messages.
  // In particular, if a message fails to add, the message count is decremented.
  void AddOutgoingMessageContinuation(const UpdateCallback& callback,
                                      const std::string& app_id,
                                      bool success);

  // Continuation to update the per-app message counts when removing messages.
  // Note: if doing a read-then-write when removing messages proves expensive,
  // an in-memory mapping of persisted message id to app could be maintained
  // instead.
  void RemoveOutgoingMessagesContinuation(
      const UpdateCallback& callback,
      bool success,
      const std::map<std::string, int>& removed_message_counts);

  class Backend;

  // Map of App ids to their message counts.
  AppIdToMessageCountMap app_message_counts_;

  scoped_refptr<Backend> backend_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::WeakPtrFactory<GCMStoreImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMStoreImpl);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_GCM_STORE_IMPL_H_
