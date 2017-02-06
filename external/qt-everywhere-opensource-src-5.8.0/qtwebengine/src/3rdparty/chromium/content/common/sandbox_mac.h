// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_MAC_H_
#define CONTENT_COMMON_SANDBOX_MAC_H_

#include <map>
#include <string>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/sandbox_type.h"

namespace base {
class FilePath;
}

#if __OBJC__
@class NSArray;
@class NSString;
#else
class NSArray;
class NSString;
#endif

namespace content {

// This class wraps the C-style sandbox APIs in a class to ensure proper
// initialization and cleanup.
class CONTENT_EXPORT SandboxCompiler {
 public:
  explicit SandboxCompiler(const std::string& profile_str);

  ~SandboxCompiler();

  // Inserts a boolean into the parameters key/value map. A duplicate key is not
  // allowed, and will cause the function to return false. The value is not
  // inserted in this case.
  bool InsertBooleanParam(const std::string& key, bool value);

  // Inserts a string into the parameters key/value map. A duplicate key is not
  // allowed, and will cause the function to return false. The value is not
  // inserted in this case.
  bool InsertStringParam(const std::string& key, const std::string& value);

  // Compiles and applies the profile; returns true on success.
  bool CompileAndApplyProfile(std::string* error);

 private:
  // Storage of the key/value pairs of strings that are used in the sandbox
  // profile.
  std::map<std::string, std::string> params_map_;

  // The sandbox profile source code.
  const std::string profile_str_;

  DISALLOW_COPY_AND_ASSIGN(SandboxCompiler);
};

class CONTENT_EXPORT Sandbox {
 public:

  // Warm up System APIs that empirically need to be accessed before the
  // sandbox is turned on. |sandbox_type| is the type of sandbox to warm up.
  // Valid |sandbox_type| values are defined by the enum SandboxType, or can be
  // defined by the embedder via
  // ContentClient::GetSandboxProfileForProcessType().
  static void SandboxWarmup(int sandbox_type);

  // Turns on the OS X sandbox for this process.
  // |sandbox_type| - type of Sandbox to use. See SandboxWarmup() for legal
  // values.
  // |allowed_dir| - directory to allow access to, currently the only sandbox
  // profile that supports this is SANDBOX_TYPE_UTILITY .
  //
  // Returns true on success, false if an error occurred enabling the sandbox.
  static bool EnableSandbox(int sandbox_type,
                            const base::FilePath& allowed_dir);

  // Returns true if the sandbox has been enabled for the current process.
  static bool SandboxIsCurrentlyActive();

  // Escape |src_utf8| for use in a plain string variable in a sandbox
  // configuraton file.  On return |dst| is set to the quoted output.
  // Returns: true on success, false otherwise.
  static bool QuotePlainString(const std::string& src_utf8, std::string* dst);

  // Escape |str_utf8| for use in a regex literal in a sandbox
  // configuraton file.  On return |dst| is set to the utf-8 encoded quoted
  // output.
  //
  // The implementation of this function is based on empirical testing of the
  // OS X sandbox on 10.5.8 & 10.6.2 which is undocumented and subject to
  // change.
  //
  // Note: If str_utf8 contains any characters < 32 || >125 then the function
  // fails and false is returned.
  //
  // Returns: true on success, false otherwise.
  static bool QuoteStringForRegex(const std::string& str_utf8,
                                  std::string* dst);

 private:
  // Convert provided path into a "canonical" path matching what the Sandbox
  // expects i.e. one without symlinks.
  // This path is not necessarily unique e.g. in the face of hardlinks.
  static base::FilePath GetCanonicalSandboxPath(const base::FilePath& path);

  FRIEND_TEST_ALL_PREFIXES(MacDirAccessSandboxTest, StringEscape);
  FRIEND_TEST_ALL_PREFIXES(MacDirAccessSandboxTest, RegexEscape);
  FRIEND_TEST_ALL_PREFIXES(MacDirAccessSandboxTest, SandboxAccess);

  DISALLOW_IMPLICIT_CONSTRUCTORS(Sandbox);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_MAC_H_
