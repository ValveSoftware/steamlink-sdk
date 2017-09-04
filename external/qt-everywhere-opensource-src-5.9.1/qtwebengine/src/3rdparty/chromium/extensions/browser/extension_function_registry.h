// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FUNCTION_REGISTRY_H_
#define EXTENSIONS_BROWSER_EXTENSION_FUNCTION_REGISTRY_H_

#include <map>
#include <string>
#include <vector>

#include "extensions/browser/extension_function_histogram_value.h"

class ExtensionFunction;

// A factory function for creating new ExtensionFunction instances.
typedef ExtensionFunction* (*ExtensionFunctionFactory)();

// Template for defining ExtensionFunctionFactory.
template <class T>
ExtensionFunction* NewExtensionFunction() {
  return new T();
}

// Contains a list of all known extension functions and allows clients to
// create instances of them.
class ExtensionFunctionRegistry {
 public:
  static ExtensionFunctionRegistry* GetInstance();
  explicit ExtensionFunctionRegistry();
  virtual ~ExtensionFunctionRegistry();

  // Allows overriding of specific functions (e.g. for testing).  Functions
  // must be previously registered.  Returns true if successful.
  bool OverrideFunction(const std::string& name,
                        ExtensionFunctionFactory factory);

  // Factory method for the ExtensionFunction registered as 'name'.
  ExtensionFunction* NewFunction(const std::string& name);

  template <class T>
  void RegisterFunction() {
    ExtensionFunctionFactory factory = &NewExtensionFunction<T>;
    factories_[T::function_name()] =
        FactoryEntry(factory, T::function_name(), T::histogram_value());
  }

  struct FactoryEntry {
   public:
    explicit FactoryEntry();
    explicit FactoryEntry(
        ExtensionFunctionFactory factory,
        const char* function_name,
        extensions::functions::HistogramValue histogram_value);

    ExtensionFunctionFactory factory_;
    const char* function_name_;
    extensions::functions::HistogramValue histogram_value_;
  };

  typedef std::map<std::string, FactoryEntry> FactoryMap;
  FactoryMap factories_;
};

#endif  // EXTENSIONS_BROWSER_EXTENSION_FUNCTION_REGISTRY_H_
