// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_AUDIO_ARC_AUDIO_BRIDGE_H_
#define COMPONENTS_ARC_AUDIO_ARC_AUDIO_BRIDGE_H_

#include <string>

#include "base/macros.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcAudioBridge : public ArcService,
                       public InstanceHolder<mojom::AudioInstance>::Observer,
                       public mojom::AudioHost,
                       public chromeos::CrasAudioHandler::AudioObserver {
 public:
  explicit ArcAudioBridge(ArcBridgeService* bridge_service);
  ~ArcAudioBridge() override;

  // InstanceHolder<mojom::AudioInstance>::Observer overrides.
  void OnInstanceReady() override;

  // mojom::AudioHost overrides.
  void ShowVolumeControls() override;

 private:
  mojo::Binding<mojom::AudioHost> binding_;

  chromeos::CrasAudioHandler* cras_audio_handler_ = nullptr;

  // chromeos::CrasAudioHandler::AudioObserver overrides.
  void OnAudioNodesChanged() override;

  void SendSwitchState(bool headphone_inserted, bool microphone_inserted);

  DISALLOW_COPY_AND_ASSIGN(ArcAudioBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_AUDIO_ARC_AUDIO_BRIDGE_H_
