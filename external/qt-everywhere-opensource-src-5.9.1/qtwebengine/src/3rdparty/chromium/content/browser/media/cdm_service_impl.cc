// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_service_impl.h"

#include <stddef.h>

#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"

namespace content {

static base::LazyInstance<CdmServiceImpl>::Leaky g_cdm_service =
    LAZY_INSTANCE_INITIALIZER;

// static
CdmService* CdmService::GetInstance() {
  return CdmServiceImpl::GetInstance();
}

// static
CdmServiceImpl* CdmServiceImpl::GetInstance() {
  return g_cdm_service.Pointer();
}

CdmServiceImpl::CdmServiceImpl() {}

CdmServiceImpl::~CdmServiceImpl() {}

void CdmServiceImpl::Init() {
  // Let embedders register CDMs.
  GetContentClient()->AddContentDecryptionModules(&cdms_);
}

void CdmServiceImpl::RegisterCdm(const CdmInfo& info) {
  // Always register new CDMs at the beginning of the list, so that
  // subsequent requests get the latest.
  cdms_.insert(cdms_.begin(), info);
}

const std::vector<CdmInfo>& CdmServiceImpl::GetAllRegisteredCdms() {
  return cdms_;
}

}  // namespace media
