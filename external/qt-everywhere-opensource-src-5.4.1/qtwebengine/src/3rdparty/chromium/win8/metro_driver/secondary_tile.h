// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_METRO_DRIVER_SECONDARY_TILE_H_
#define CHROME_BROWSER_UI_METRO_DRIVER_SECONDARY_TILE_H_

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/win/metro.h"

extern "C" __declspec(dllexport)
BOOL MetroIsPinnedToStartScreen(const base::string16& tile_id);

extern "C" __declspec(dllexport)
void MetroUnPinFromStartScreen(
    const base::string16& tile_id,
    const base::win::MetroPinUmaResultCallback& callback);

extern "C" __declspec(dllexport)
void MetroPinToStartScreen(
    const base::string16& tile_id,
    const base::string16& title,
    const base::string16& url,
    const base::FilePath& logo_path,
    const base::win::MetroPinUmaResultCallback& callback);

#endif  // CHROME_BROWSER_UI_METRO_DRIVER_SECONDARY_TILE_H_
