// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/protobuf/src/google/protobuf/compiler/plugin.h"

#include "proto_zero_generator.h"

int main(int argc, char* argv[]) {
  tracing::proto::ProtoZeroGenerator generator;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
