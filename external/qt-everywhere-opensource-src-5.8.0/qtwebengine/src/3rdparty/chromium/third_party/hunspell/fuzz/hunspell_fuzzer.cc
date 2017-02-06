// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "third_party/hunspell/src/hunspell/hunspell.hxx"
#include "third_party/hunspell/fuzz/hunspell_fuzzer_hunspell_dictionary.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size)
    return 0;
  
  static Hunspell* hunspell = new Hunspell(kHunspellDictionary,
                                           sizeof(kHunspellDictionary));

  std::string data_string(reinterpret_cast<const char*>(data), size);
  hunspell->spell(data_string.c_str());

  char** suggestions = nullptr;
  int suggetion_length = hunspell->suggest(&suggestions, data_string.c_str());
  hunspell->free_list(&suggestions, suggetion_length);

  return 0;
}
