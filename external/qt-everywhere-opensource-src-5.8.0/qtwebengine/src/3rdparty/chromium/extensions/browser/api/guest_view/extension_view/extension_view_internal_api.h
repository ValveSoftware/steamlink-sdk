// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_EXTENSION_VIEW_EXTENSION_VIEW_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_EXTENSION_VIEW_EXTENSION_VIEW_INTERNAL_API_H_

#include "base/macros.h"
#include "extensions/browser/api/execute_code_function.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/guest_view/extension_view/extension_view_guest.h"

namespace extensions {

// An abstract base class for async extensionview APIs. It does a process ID
// check in RunAsync, and then calls RunAsyncSafe which must be overriden by
// all subclasses.
class ExtensionViewInternalExtensionFunction : public AsyncExtensionFunction {
 public:
  ExtensionViewInternalExtensionFunction() {}

 protected:
  ~ExtensionViewInternalExtensionFunction() override {}

  // ExtensionFunction implementation.
  bool RunAsync() final;

 private:
  virtual bool RunAsyncSafe(ExtensionViewGuest* guest) = 0;
};

// Attempts to load a src into the extensionview.
class ExtensionViewInternalLoadSrcFunction
    : public ExtensionViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionViewInternal.loadSrc",
                             EXTENSIONVIEWINTERNAL_LOADSRC);
  ExtensionViewInternalLoadSrcFunction() {}

 protected:
  ~ExtensionViewInternalLoadSrcFunction() override {}

 private:
  // ExtensionViewInternalLoadSrcFunction implementation.
  bool RunAsyncSafe(ExtensionViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewInternalLoadSrcFunction);
};

// Parses a src and determines whether or not it is valid.
class ExtensionViewInternalParseSrcFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionViewInternal.parseSrc",
                             EXTENSIONVIEWINTERNAL_PARSESRC);
  ExtensionViewInternalParseSrcFunction() {}

 protected:
  ~ExtensionViewInternalParseSrcFunction() override {}

 private:
  // ExtensionViewInternalParseSrcFunction implementation.
  bool RunAsync() final;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewInternalParseSrcFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_VIEW_EXTENSION_VIEW_INTERNAL_API_H_
