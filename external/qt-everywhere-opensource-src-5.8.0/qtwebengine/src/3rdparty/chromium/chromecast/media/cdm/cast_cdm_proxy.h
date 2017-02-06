// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CDM_CAST_CDM_PROXY_H_
#define CHROMECAST_MEDIA_CDM_CAST_CDM_PROXY_H_

#include <stdint.h>

#include "base/threading/thread_checker.h"
#include "chromecast/media/cdm/cast_cdm.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromecast {
namespace media {

// MediaKeys implementation that lives on the UI thread and forwards all calls
// to a CastCdm instance on the CMA thread. This is used to simplify the
// UI-CMA threading interaction.
// TODO(slan): Remove this class when CMA is deprecated.
class CastCdmProxy : public ::media::MediaKeys {
 public:
  CastCdmProxy(const scoped_refptr<CastCdm>& cast_cdm,
               const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Returns the CDM instance which lives on the CMA thread.
  CastCdm* cast_cdm() const;

 private:
  ~CastCdmProxy() override;

  // ::media::MediaKeys implementation:
  void SetServerCertificate(
      const std::vector<uint8_t>& certificate,
      std::unique_ptr<::media::SimpleCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      ::media::MediaKeys::SessionType session_type,
      ::media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<::media::NewSessionCdmPromise> promise) override;
  void LoadSession(
      ::media::MediaKeys::SessionType session_type,
      const std::string& session_id,
      std::unique_ptr<::media::NewSessionCdmPromise> promise) override;
  void UpdateSession(
      const std::string& session_id,
      const std::vector<uint8_t>& response,
      std::unique_ptr<::media::SimpleCdmPromise> promise) override;
  void CloseSession(
      const std::string& session_id,
      std::unique_ptr<::media::SimpleCdmPromise> promise) override;
  void RemoveSession(
      const std::string& session_id,
      std::unique_ptr<::media::SimpleCdmPromise> promise) override;
  ::media::CdmContext* GetCdmContext() override;

  scoped_refptr<CastCdm> cast_cdm_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(CastCdmProxy);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CDM_CAST_CDM_PROXY_H_
