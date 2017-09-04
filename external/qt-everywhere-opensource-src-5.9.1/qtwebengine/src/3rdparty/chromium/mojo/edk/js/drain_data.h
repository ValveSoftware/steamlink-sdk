// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_JS_DRAIN_DATA_H_
#define MOJO_EDK_JS_DRAIN_DATA_H_

#include "base/memory/scoped_vector.h"
#include "gin/runner.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/watcher.h"
#include "v8/include/v8.h"

namespace mojo {
namespace edk {
namespace js {

// This class is the implementation of the Mojo JavaScript core module's
// drainData() method. It is not intended to be used directly. The caller
// allocates a DrainData on the heap and returns GetPromise() to JS. The
// implementation deletes itself after reading as much data as possible
// and rejecting or resolving the Promise.

class DrainData {
 public:
  // Starts waiting for data on the specified data pipe consumer handle.
  // See WaitForData(). The constructor does not block.
  DrainData(v8::Isolate* isolate, mojo::Handle handle);

  // Returns a Promise that will be settled when no more data can be read.
  // Should be called just once on a newly allocated DrainData object.
  v8::Handle<v8::Value> GetPromise();

 private:
  ~DrainData();

  // Waits for data to be available. DataReady() will be notified.
  void WaitForData();

  // Use ReadData() to read whatever is availble now on handle_ and save
  // it in data_buffers_.
  void DataReady(MojoResult result);
  MojoResult ReadData();

  // When the remote data pipe handle is closed, or an error occurs, deliver
  // all of the buffered data to the JS Promise and then delete this.
  void DeliverData(MojoResult result);

  using DataBuffer = std::vector<char>;

  v8::Isolate* isolate_;
  ScopedDataPipeConsumerHandle handle_;
  Watcher handle_watcher_;
  base::WeakPtr<gin::Runner> runner_;
  v8::UniquePersistent<v8::Promise::Resolver> resolver_;
  ScopedVector<DataBuffer> data_buffers_;
};

}  // namespace js
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_JS_DRAIN_DATA_H_
