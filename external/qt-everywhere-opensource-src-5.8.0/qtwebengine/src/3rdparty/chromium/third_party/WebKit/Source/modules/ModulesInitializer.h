// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModulesInitializer_h
#define ModulesInitializer_h

#include "core/CoreInitializer.h"
#include "modules/ModulesExport.h"

namespace blink {

class MODULES_EXPORT ModulesInitializer : public CoreInitializer {
public:
    void initialize() override;
    void shutdown() override;
};

} // namespace blink

#endif // ModulesInitializer_h
