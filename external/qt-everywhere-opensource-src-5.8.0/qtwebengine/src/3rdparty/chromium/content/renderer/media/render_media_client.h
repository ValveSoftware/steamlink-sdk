// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDER_MEDIA_CLIENT_H_
#define CONTENT_RENDERER_MEDIA_RENDER_MEDIA_CLIENT_H_

#include <memory>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "media/base/media_client.h"
#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

namespace content {

class CONTENT_EXPORT RenderMediaClient : public media::MediaClient {
 public:
  // Initialize RenderMediaClient and SetMediaClient(). Note that the instance
  // is not exposed because no content code needs to directly access it.
  static void Initialize();

  // MediaClient implementation.
  void AddKeySystemsInfoForUMA(
      std::vector<media::KeySystemInfoForUMA>* key_systems_info_for_uma) final;
  bool IsKeySystemsUpdateNeeded() final;
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<media::KeySystemProperties>>*
          key_systems_properties) final;
  void RecordRapporURL(const std::string& metric, const GURL& url) final;

  void SetTickClockForTesting(std::unique_ptr<base::TickClock> tick_clock);

 private:
  friend struct base::DefaultLazyInstanceTraits<RenderMediaClient>;

  RenderMediaClient();
  ~RenderMediaClient() override;

  // Makes sure all methods are called from the same thread.
  base::ThreadChecker thread_checker_;

  // Whether AddSupportedKeySystems() has ever been called.
  bool has_updated_;

  // Whether a future update is needed. For example, when some potentially
  // supported key systems are NOT supported yet. This could happen when the
  // required component for a key system is not yet available.
  bool is_update_needed_;

  base::TimeTicks last_update_time_ticks_;
  std::unique_ptr<base::TickClock> tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(RenderMediaClient);
};

#if defined(UNIT_TEST)
// Helper function to access the RenderMediaClient instance. Used only by unit
// tests.
CONTENT_EXPORT RenderMediaClient* GetRenderMediaClientInstanceForTesting();
#endif

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDER_MEDIA_CLIENT_H_
