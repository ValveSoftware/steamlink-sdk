// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_COMMON_BLIMP_CONTENT_CLIENT_H_
#define BLIMP_ENGINE_COMMON_BLIMP_CONTENT_CLIENT_H_

#include <string>

#include "content/public/common/content_client.h"

namespace blimp {
namespace engine {

std::string GetBlimpEngineUserAgent();
void SetClientOSInfo(std::string client_os_info);

class BlimpContentClient : public content::ContentClient {
 public:
  ~BlimpContentClient() override;

  // content::ContentClient implementation.
  std::string GetUserAgent() const override;
  base::string16 GetLocalizedString(int message_id) const override;
  base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const override;
  base::RefCountedMemory* GetDataResourceBytes(
      int resource_id) const override;
  gfx::Image& GetNativeImageNamed(int resource_id) const override;
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_COMMON_BLIMP_CONTENT_CLIENT_H_
