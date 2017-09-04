// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_BDICT_LANGUAGE_H_
#define COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_BDICT_LANGUAGE_H_

#include <string>

#include "ipc/ipc_platform_file.h"

struct SpellCheckBDictLanguage {
  IPC::PlatformFileForTransit file;
  std::string language;
};

#endif  // COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_BDICT_LANGUAGE_H_
