// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_CALLBACK_SOURCE_H
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_CALLBACK_SOURCE_H

#include "ppapi/cpp/completion_callback.h"

namespace plugin {

// Classes that implement this interface can be used with FileDownloader's
// DOWNLOAD_STREAM mode. For each chunk of the file as it downloads,
// The FileDownloader will get a callback of type
// pp::CompletionCallbackWithOutput<std::vector<char>*> using the
// GetCallback function, and call it immediately, passing a pointer to a
// vector with the data as the output field. The class is templatized just
// in case there are any other use cases in the future.
// This class really only exists as a way to get callbacks
// bound to an object (i.e. we don't need the asynchronous behavior of PPAPI)
// All we really need is tr1::function or base::bind or some such, but we don't
// have those in the plugin.

template<class T>
class CallbackSource {
 public:
  virtual ~CallbackSource();
  // Returns a callback from callback_factory's NewCallbackWithOutput,
  // bound to the implementing object.
  virtual pp::CompletionCallbackWithOutput<T> GetCallback() = 0;
};
}
#endif // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_CALLBACK_SOURCE_H
