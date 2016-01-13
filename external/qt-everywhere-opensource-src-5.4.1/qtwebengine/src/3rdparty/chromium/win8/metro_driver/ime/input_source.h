// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_IME_INPUT_SOURCE_H_
#define WIN8_METRO_DRIVER_IME_INPUT_SOURCE_H_

#include <Windows.h>

#include "base/memory/scoped_ptr.h"

namespace metro_driver {

class InputSourceObserver;

// An interface through which information about the input source can be
// retrieved, where an input source represents an IME or a keyboard layout.
class InputSource {
 public:
  virtual ~InputSource() {}
  // Create an instance. Returns NULL if fails.
  static scoped_ptr<InputSource> Create();

  // Returns true if |langid| and |is_ime| are filled based on the current
  // active input source.
  virtual bool GetActiveSource(LANGID* langid, bool* is_ime) = 0;

  // Adds/Removes an observer to receive notifications when the active input
  // source is changed.
  virtual void AddObserver(InputSourceObserver* observer) = 0;
  virtual void RemoveObserver(InputSourceObserver* observer) = 0;
};

}  // namespace metro_driver

#endif  // WIN8_METRO_DRIVER_IME_INPUT_SOURCE_H_
