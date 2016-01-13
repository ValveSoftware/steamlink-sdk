// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A wifi data provider provides wifi data from the device that is used by a
// NetworkLocationProvider to obtain a position fix. We use a singleton
// instance of the wifi data provider, which is used by multiple
// NetworkLocationProvider objects.
//
// This file provides WifiDataProvider, which provides static methods to
// access the singleton instance. The singleton instance uses a private
// implementation to abstract across platforms and also to allow mock providers
// to be used for testing.
//
// This file also provides WifiDataProviderImplBase, a base class which
// provides common functionality for the private implementations.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_H_

#include <set>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "content/browser/geolocation/wifi_data.h"
#include "content/common/content_export.h"

namespace content {

class WifiDataProvider;

// See class WifiDataProvider for the public client API.
// WifiDataProvider uses containment to hide platform-specific implementation
// details from common code. This class provides common functionality for these
// contained implementation classes. This is a modified pimpl pattern.
class CONTENT_EXPORT WifiDataProviderImplBase
    : public base::RefCountedThreadSafe<WifiDataProviderImplBase> {
 public:
  WifiDataProviderImplBase();

  // Tells the provider to start looking for data. Callbacks will start
  // receiving notifications after this call.
  virtual void StartDataProvider() = 0;

  // Tells the provider to stop looking for data. Callbacks will stop
  // receiving notifications after this call.
  virtual void StopDataProvider() = 0;

  // Provides whatever data the provider has, which may be nothing. Return
  // value indicates whether this is all the data the provider could ever
  // obtain.
  virtual bool GetData(WifiData* data) = 0;

  // Sets the container of this class, which is of type WifiDataProvider.
  // This is required to pass as a parameter when calling a callback.
  void SetContainer(WifiDataProvider* container);

  typedef base::Callback<void(WifiDataProvider*)> WifiDataUpdateCallback;

  void AddCallback(WifiDataUpdateCallback* callback);

  bool RemoveCallback(WifiDataUpdateCallback* callback);

  bool has_callbacks() const;

 protected:
  friend class base::RefCountedThreadSafe<WifiDataProviderImplBase>;
  virtual ~WifiDataProviderImplBase();

  typedef std::set<WifiDataUpdateCallback*> CallbackSet;

  // Runs all callbacks via a posted task, so we can unwind callstack here and
  // avoid client reentrancy.
  void RunCallbacks();

  bool CalledOnClientThread() const;

  base::MessageLoop* client_loop() const;

 private:
  void DoRunCallbacks();

  WifiDataProvider* container_;

  // Reference to the client's message loop. All callbacks should happen in this
  // context.
  base::MessageLoop* client_loop_;

  CallbackSet callbacks_;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderImplBase);
};

// A wifi data provider
//
// We use a singleton instance of this class which is shared by multiple network
// location providers. These location providers access the instance through the
// Register and Unregister methods.
class CONTENT_EXPORT WifiDataProvider {
 public:
  // Sets the factory function which will be used by Register to create the
  // implementation used by the singleton instance. This factory approach is
  // used both to abstract accross platform-specific implementations and to
  // inject mock implementations for testing.
  typedef WifiDataProviderImplBase* (*ImplFactoryFunction)(void);
  static void SetFactory(ImplFactoryFunction factory_function_in);

  // Resets the factory function to the default.
  static void ResetFactory();

  typedef base::Callback<void(WifiDataProvider*)> WifiDataUpdateCallback;

  // Registers a callback, which will be run whenever new data is available.
  // Instantiates the singleton if necessary, and always returns it.
  static WifiDataProvider* Register(WifiDataUpdateCallback* callback);

  // Removes a callback. If this is the last callback, deletes the singleton
  // instance. Return value indicates success.
  static bool Unregister(WifiDataUpdateCallback* callback);

  // Provides whatever data the provider has, which may be nothing. Return
  // value indicates whether this is all the data the provider could ever
  // obtain.
  bool GetData(WifiData* data);

 private:
  // Private constructor and destructor, callers access singleton through
  // Register and Unregister.
  WifiDataProvider();
  virtual ~WifiDataProvider();

  void AddCallback(WifiDataUpdateCallback* callback);
  bool RemoveCallback(WifiDataUpdateCallback* callback);
  bool has_callbacks() const;

  void StartDataProvider();
  void StopDataProvider();

  static WifiDataProviderImplBase* DefaultFactoryFunction();

  // The singleton-like instance of this class. (Not 'true' singleton, as it
  // may go through multiple create/destroy/create cycles per process instance,
  // e.g. when under test).
  static WifiDataProvider* instance_;

  // The factory function used to create the singleton instance.
  static ImplFactoryFunction factory_function_;

  // The internal implementation.
  scoped_refptr<WifiDataProviderImplBase> impl_;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_H_
