// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_SERVICE_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CDM_SERVICE_IMPL_H_

#include <vector>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/cdm_service.h"

namespace content {

struct CdmInfo;

class CONTENT_EXPORT CdmServiceImpl : NON_EXPORTED_BASE(public CdmService) {
 public:
  // Returns the CdmServiceImpl singleton.
  static CdmServiceImpl* GetInstance();

  // CdmService implementation.
  void Init() override;
  void RegisterCdm(const CdmInfo& info) override;
  const std::vector<CdmInfo>& GetAllRegisteredCdms() override;

 private:
  friend struct base::DefaultLazyInstanceTraits<CdmServiceImpl>;
  friend class CdmServiceImplTest;

  CdmServiceImpl();
  ~CdmServiceImpl() override;

  std::vector<CdmInfo> cdms_;

  DISALLOW_COPY_AND_ASSIGN(CdmServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_SERVICE_IMPL_H_
