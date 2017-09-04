// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_PROTO_ZERO_PLUGIN_PROTO_ZERO_GENERATOR_H_
#define COMPONENTS_TRACING_PROTO_ZERO_PLUGIN_PROTO_ZERO_GENERATOR_H_

#include <string>

#include "third_party/protobuf/src/google/protobuf/compiler/code_generator.h"

namespace tracing {
namespace proto {

class ProtoZeroGenerator : public google::protobuf::compiler::CodeGenerator {
 public:
  explicit ProtoZeroGenerator();
  ~ProtoZeroGenerator() override;

  // CodeGenerator implementation
  bool Generate(const google::protobuf::FileDescriptor* file,
                const std::string& options,
                google::protobuf::compiler::GeneratorContext* context,
                std::string* error) const override;
};

}  // namesapce proto
}  // namespace tracing

#endif  // COMPONENTS_TRACING_PROTO_ZERO_PLUGIN_PROTO_ZERO_GENERATOR_H_
