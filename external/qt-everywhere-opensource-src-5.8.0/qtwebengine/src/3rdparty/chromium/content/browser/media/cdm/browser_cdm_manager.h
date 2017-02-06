// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_BROWSER_CDM_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_CDM_BROWSER_CDM_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/media/cdm_messages.h"
#include "content/common/media/cdm_messages_enums.h"
#include "content/public/browser/browser_message_filter.h"
#include "ipc/ipc_message.h"
#include "media/base/cdm_promise.h"
#include "media/base/eme_constants.h"
#include "media/base/media_keys.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"
#include "url/gurl.h"

struct CdmHostMsg_CreateSessionAndGenerateRequest_Params;

namespace media {
class CdmFactory;
}

namespace content {

// This class manages all CDM objects. It receives control operations from the
// the render process, and forwards them to corresponding CDM object. Callbacks
// from CDM objects are converted to IPCs and then sent to the render process.
class CONTENT_EXPORT BrowserCdmManager : public BrowserMessageFilter {
 public:
  // Returns the BrowserCdmManager associated with the |render_process_id|.
  // Returns NULL if no BrowserCdmManager is associated.
  static BrowserCdmManager* FromProcess(int render_process_id);

  // Constructs the BrowserCdmManager for |render_process_id| which runs on
  // |task_runner|.
  // If |task_runner| is not NULL, all CDM messages are posted to it. Otherwise,
  // all messages are posted to the browser UI thread.
  BrowserCdmManager(int render_process_id,
                    const scoped_refptr<base::TaskRunner>& task_runner);

  // BrowserMessageFilter implementations.
  void OnDestruct() const override;
  base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Returns the CDM associated with |render_frame_id| and |cdm_id|. Returns
  // null if no such CDM exists.
  scoped_refptr<media::MediaKeys> GetCdm(int render_frame_id, int cdm_id) const;

  // Notifies that the render frame has been deleted so that all CDMs belongs
  // to this render frame needs to be destroyed as well. This is needed because
  // in some cases (e.g. fast termination of the renderer), the message to
  // destroy the CDM will not be received.
  void RenderFrameDeleted(int render_frame_id);

  // Promise handlers.
  void ResolvePromise(int render_frame_id, int cdm_id, uint32_t promise_id);
  void ResolvePromiseWithSession(int render_frame_id,
                                 int cdm_id,
                                 uint32_t promise_id,
                                 const std::string& session_id);
  void RejectPromise(int render_frame_id,
                     int cdm_id,
                     uint32_t promise_id,
                     media::MediaKeys::Exception exception,
                     uint32_t system_code,
                     const std::string& error_message);

 protected:
  friend class base::RefCountedThreadSafe<BrowserCdmManager>;
  friend class base::DeleteHelper<BrowserCdmManager>;
  ~BrowserCdmManager() override;

 private:
  // Returns the CdmFactory that can be used to create CDMs. Returns null if
  // CDM is not supported.
  media::CdmFactory* GetCdmFactory();

  // CDM callbacks.
  void OnSessionMessage(int render_frame_id,
                        int cdm_id,
                        const std::string& session_id,
                        media::MediaKeys::MessageType message_type,
                        const std::vector<uint8_t>& message,
                        const GURL& legacy_destination_url);
  void OnSessionClosed(int render_frame_id,
                       int cdm_id,
                       const std::string& session_id);
  void OnLegacySessionError(int render_frame_id,
                            int cdm_id,
                            const std::string& session_id,
                            media::MediaKeys::Exception exception_code,
                            uint32_t system_code,
                            const std::string& error_message);
  void OnSessionKeysChange(int render_frame_id,
                           int cdm_id,
                           const std::string& session_id,
                           bool has_additional_usable_key,
                           media::CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(int render_frame_id,
                                 int cdm_id,
                                 const std::string& session_id,
                                 const base::Time& new_expiry_time);

  // Message handlers.
  void OnInitializeCdm(int render_frame_id,
                       int cdm_id,
                       uint32_t promise_id,
                       const CdmHostMsg_InitializeCdm_Params& params);
  void OnSetServerCertificate(int render_frame_id,
                              int cdm_id,
                              uint32_t promise_id,
                              const std::vector<uint8_t>& certificate);
  void OnCreateSessionAndGenerateRequest(
      const CdmHostMsg_CreateSessionAndGenerateRequest_Params& params);
  void OnLoadSession(
      int render_frame_id,
      int cdm_id,
      uint32_t promise_id,
      media::MediaKeys::SessionType session_type,
      const std::string& session_id);
  void OnUpdateSession(int render_frame_id,
                       int cdm_id,
                       uint32_t promise_id,
                       const std::string& session_id,
                       const std::vector<uint8_t>& response);
  void OnCloseSession(int render_frame_id,
                      int cdm_id,
                      uint32_t promise_id,
                      const std::string& session_id);
  void OnRemoveSession(int render_frame_id,
                       int cdm_id,
                       uint32_t promise_id,
                       const std::string& session_id);
  void OnDestroyCdm(int render_frame_id, int cdm_id);

  // Callback for CDM creation.
  void OnCdmCreated(int render_frame_id,
                    int cdm_id,
                    const GURL& security_origin,
                    std::unique_ptr<media::SimpleCdmPromise> promise,
                    const scoped_refptr<media::MediaKeys>& cdm,
                    const std::string& error_message);

  // Removes all CDMs associated with |render_frame_id|.
  void RemoveAllCdmForFrame(int render_frame_id);

  // Removes the CDM with the specified id.
  void RemoveCdm(uint64_t id);

  using PermissionStatusCB = base::Callback<void(bool)>;

  // Checks protected media identifier permission for the given
  // |render_frame_id| and |cdm_id|.
  void CheckPermissionStatus(int render_frame_id,
                             int cdm_id,
                             const PermissionStatusCB& permission_status_cb);

  // Checks permission status on Browser UI thread. Runs |permission_status_cb|
  // on the |task_runner_| with the permission status.
  void CheckPermissionStatusOnUIThread(
      int render_frame_id,
      const GURL& security_origin,
      const PermissionStatusCB& permission_status_cb);

  // Calls CreateSessionAndGenerateRequest() on the CDM if
  // |permission_was_allowed| is true. Otherwise rejects the |promise|.
  void CreateSessionAndGenerateRequestIfPermitted(
      int render_frame_id,
      int cdm_id,
      media::MediaKeys::SessionType session_type,
      media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<media::NewSessionCdmPromise> promise,
      bool permission_was_allowed);

  // Calls LoadSession() on the CDM if |permission_was_allowed| is true.
  // Otherwise rejects |promise|.
  void LoadSessionIfPermitted(
      int render_frame_id,
      int cdm_id,
      media::MediaKeys::SessionType session_type,
      const std::string& session_id,
      std::unique_ptr<media::NewSessionCdmPromise> promise,
      bool permission_was_allowed);

  const int render_process_id_;

  // TaskRunner to dispatch all CDM messages to. If it's NULL, all messages are
  // dispatched to the browser UI thread.
  scoped_refptr<base::TaskRunner> task_runner_;

  std::unique_ptr<media::CdmFactory> cdm_factory_;

  // The key in the following maps is a combination of |render_frame_id| and
  // |cdm_id|.

  // Map of managed CDMs.
  typedef std::map<uint64_t, scoped_refptr<media::MediaKeys>> CdmMap;
  CdmMap cdm_map_;

  // Map of CDM's security origin.
  std::map<uint64_t, GURL> cdm_security_origin_map_;

  base::WeakPtrFactory<BrowserCdmManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCdmManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_BROWSER_CDM_MANAGER_H_
