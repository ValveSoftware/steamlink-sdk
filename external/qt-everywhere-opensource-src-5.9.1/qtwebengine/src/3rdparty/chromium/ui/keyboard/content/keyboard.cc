// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/content/keyboard.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "ui/base/resource/resource_bundle.h"

namespace keyboard {

void InitializeKeyboard() {
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;

  base::FilePath pak_dir;
  PathService::Get(base::DIR_MODULE, &pak_dir);
  base::FilePath pak_file = pak_dir.Append(
      FILE_PATH_LITERAL("keyboard_resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::SCALE_FACTOR_100P);
}

}  // namespace keyboard
