// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "FindBadConstructsAction.h"

#include "clang/Frontend/FrontendPluginRegistry.h"

#include "FindBadConstructsConsumer.h"

using namespace clang;

namespace chrome_checker {

FindBadConstructsAction::FindBadConstructsAction() {
}

ASTConsumer* FindBadConstructsAction::CreateASTConsumer(
    CompilerInstance& instance,
    llvm::StringRef ref) {
  return new FindBadConstructsConsumer(instance, options_);
}

bool FindBadConstructsAction::ParseArgs(const CompilerInstance& instance,
                                        const std::vector<std::string>& args) {
  bool parsed = true;

  for (size_t i = 0; i < args.size() && parsed; ++i) {
    if (args[i] == "skip-virtuals-in-implementations") {
      // TODO(rsleevi): Remove this once http://crbug.com/115047 is fixed.
      options_.check_virtuals_in_implementations = false;
    } else if (args[i] == "check-base-classes") {
      // TODO(rsleevi): Remove this once http://crbug.com/123295 is fixed.
      options_.check_base_classes = true;
    } else if (args[i] == "check-weak-ptr-factory-order") {
      // TODO(dmichael): Remove this once http://crbug.com/303818 is fixed.
      options_.check_weak_ptr_factory_order = true;
    } else if (args[i] == "check-enum-last-value") {
      // TODO(tsepez): Enable this by default once http://crbug.com/356815
      // and http://crbug.com/356816 are fixed.
      options_.check_enum_last_value = true;
    } else {
      parsed = false;
      llvm::errs() << "Unknown clang plugin argument: " << args[i] << "\n";
    }
  }

  return parsed;
}

}  // namespace chrome_checker

static FrontendPluginRegistry::Add<chrome_checker::FindBadConstructsAction> X(
    "find-bad-constructs",
    "Finds bad C++ constructs");
