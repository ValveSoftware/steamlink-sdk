// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_BLIMP_ENGINE_CONFIG_H_
#define BLIMP_ENGINE_APP_BLIMP_ENGINE_CONFIG_H_

#include <memory>
#include <string>

#include "base/macros.h"

namespace base {
class CommandLine;
}  // namespace base

namespace blimp {
namespace engine {

// Set internal command line switches for Blimp engine.
void SetCommandLineDefaults(base::CommandLine* command_line);

// Class to hold all of the configuration bits necessary for the Blimp engine.
//
// The BlimpEngineConfig class is initialized from parameters provided on the
// command line. For the switches to pass verification:
//   * A client token filepath must be provided and the file must have a
//     non-empty token.
//
// The BlimpEngineConfig object is intended to live as long as the engine is
// running. It should also be one of the first things to be set up.
class BlimpEngineConfig {
 public:
  ~BlimpEngineConfig();

  // Attempts to create a BlimpEngineConfig based on the parameters in the
  // specified CommandLine. This validates all of the command line switches and
  // parses all files specified. Returns a non-null std::unique_ptr on success.
  static std::unique_ptr<BlimpEngineConfig> Create(
      const base::CommandLine& cmd_line);

  // Creates a BlimpEngineConfig based on individual components. Should only
  // be used for testing.
  static std::unique_ptr<BlimpEngineConfig> CreateForTest(
      const std::string& client_token);

  // Returns the client token.
  const std::string& client_token() const;

 private:
  explicit BlimpEngineConfig(const std::string& client_token);

  const std::string client_token_;

  DISALLOW_COPY_AND_ASSIGN(BlimpEngineConfig);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_BLIMP_ENGINE_CONFIG_H_
