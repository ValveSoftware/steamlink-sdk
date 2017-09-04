// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_MACHINE_ID_PROVIDER_H_
#define COMPONENTS_METRICS_MACHINE_ID_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace metrics {

// Provides machine characteristics used as a machine id. The implementation is
// platform specific with a default implementation that gives an empty id. The
// class is ref-counted thread safe so it can be used to post to the FILE thread
// and communicate back to the UI thread.
// This raw machine id should not be stored or transmitted over the network.
// TODO(jwd): Simplify implementation to get rid of the need for
// RefCountedThreadSafe (crbug.com/354882).
class MachineIdProvider : public base::RefCountedThreadSafe<MachineIdProvider> {
 public:
  // Get a string containing machine characteristics, to be used as a machine
  // id. The implementation is platform specific, with a default implementation
  // returning an empty string.
  // The return value should not be stored to disk or transmitted.
  std::string GetMachineId();

  // Returns a pointer to a new MachineIdProvider or NULL if there is no
  // provider implemented on a given platform. This is done to avoid posting a
  // task to the FILE thread on platforms with no implementation.
  static MachineIdProvider* CreateInstance();

 private:
  friend class base::RefCountedThreadSafe<MachineIdProvider>;

  MachineIdProvider();
  virtual ~MachineIdProvider();

  DISALLOW_COPY_AND_ASSIGN(MachineIdProvider);
};

}  //  namespace metrics

#endif  // COMPONENTS_METRICS_MACHINE_ID_PROVIDER_H_
