// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_data_fetcher.h"

namespace device {

GamepadDataFetcher::GamepadDataFetcher() : provider_(nullptr) {}

void GamepadDataFetcher::InitializeProvider(GamepadPadStateProvider* provider) {
  DCHECK(provider);

  provider_ = provider;
  OnAddedToProvider();
}

GamepadDataFetcherFactory::GamepadDataFetcherFactory() {}

}  // namespace device
