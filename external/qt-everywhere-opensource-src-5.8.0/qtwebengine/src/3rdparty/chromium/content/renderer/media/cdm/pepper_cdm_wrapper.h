// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CDM_PEPPER_CDM_WRAPPER_H_
#define CONTENT_RENDERER_MEDIA_CDM_PEPPER_CDM_WRAPPER_H_

#if !defined(ENABLE_PEPPER_CDMS)
#error This file should only be included when ENABLE_PEPPER_CDMS is defined
#endif

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"

class GURL;

namespace content {
class ContentDecryptorDelegate;

// PepperCdmWrapper provides access to the Pepper CDM instance.
class PepperCdmWrapper {
 public:
  virtual ~PepperCdmWrapper() {}

  // Returns the ContentDecryptorDelegate* associated with this plugin.
  virtual ContentDecryptorDelegate* GetCdmDelegate() = 0;

 protected:
  PepperCdmWrapper() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperCdmWrapper);
};

// Callback used to create a PepperCdmWrapper. This may return null if the
// Pepper CDM can not be created.
typedef base::Callback<std::unique_ptr<PepperCdmWrapper>(
    const std::string& pluginType,
    const GURL& security_origin)>
    CreatePepperCdmCB;

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CDM_PEPPER_CDM_WRAPPER_H_
