// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_client.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"

namespace content {

static base::LazyInstance<RenderMediaClient>::Leaky g_render_media_client =
    LAZY_INSTANCE_INITIALIZER;

void RenderMediaClient::Initialize() {
  g_render_media_client.Get();
}

RenderMediaClient::RenderMediaClient()
    : has_updated_(false),
      is_update_needed_(true),
      tick_clock_(new base::DefaultTickClock()) {
  media::SetMediaClient(this);
}

RenderMediaClient::~RenderMediaClient() {
}

void RenderMediaClient::AddKeySystemsInfoForUMA(
    std::vector<media::KeySystemInfoForUMA>* key_systems_info_for_uma) {
  DVLOG(2) << __FUNCTION__;
#if defined(WIDEVINE_CDM_AVAILABLE)
  key_systems_info_for_uma->push_back(media::KeySystemInfoForUMA(
      kWidevineKeySystem, kWidevineKeySystemNameForUMA));
#endif  // WIDEVINE_CDM_AVAILABLE
}

bool RenderMediaClient::IsKeySystemsUpdateNeeded() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Always needs update if we have never updated, regardless the
  // |last_update_time_ticks_|'s initial value.
  if (!has_updated_) {
    DCHECK(is_update_needed_);
    return true;
  }

  if (!is_update_needed_)
    return false;

  // The update could be expensive. For example, it could involve a sync IPC to
  // the browser process. Use a minimum update interval to avoid unnecessarily
  // frequent update.
  static const int kMinUpdateIntervalInMilliseconds = 1000;
  if ((tick_clock_->NowTicks() - last_update_time_ticks_).InMilliseconds() <
      kMinUpdateIntervalInMilliseconds) {
    return false;
  }

  return true;
}

void RenderMediaClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<media::KeySystemProperties>>*
        key_systems_properties) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  GetContentClient()->renderer()->AddSupportedKeySystems(
      key_systems_properties);

  has_updated_ = true;
  last_update_time_ticks_ = tick_clock_->NowTicks();

  // Check whether all potentially supported key systems are supported. If so,
  // no need to update again.
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
  for (const auto& properties : *key_systems_properties) {
    if (properties->GetKeySystemName() == kWidevineKeySystem)
      is_update_needed_ = false;
  }
#else
  is_update_needed_ = false;
#endif
}

void RenderMediaClient::RecordRapporURL(const std::string& metric,
                                        const GURL& url) {
  GetContentClient()->renderer()->RecordRapporURL(metric, url);
}

void RenderMediaClient::SetTickClockForTesting(
    std::unique_ptr<base::TickClock> tick_clock) {
  tick_clock_.swap(tick_clock);
}

// This functions is for testing purpose only. The declaration in the
// header file is guarded by "#if defined(UNIT_TEST)" so that it can be used
// by tests but not non-test code. However, this .cc file is compiled as part of
// "content" where "UNIT_TEST" is not defined. So we need to specify
// "CONTENT_EXPORT" here again so that it is visible to tests.
CONTENT_EXPORT RenderMediaClient* GetRenderMediaClientInstanceForTesting() {
  return g_render_media_client.Pointer();
}

}  // namespace content
