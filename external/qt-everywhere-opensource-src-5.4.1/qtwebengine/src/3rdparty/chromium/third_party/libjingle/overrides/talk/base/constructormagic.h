// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file overrides the inclusion of talk/base/constructormagic.h
// We do this because constructor magic defines DISALLOW_EVIL_CONSTRUCTORS,
// but we want to use the version from Chromium.

#ifndef OVERRIDES_TALK_BASE_CONSTRUCTORMAGIC_H__
#define OVERRIDES_TALK_BASE_CONSTRUCTORMAGIC_H__

#include "base/basictypes.h"

#endif  // OVERRIDES_TALK_BASE_CONSTRUCTORMAGIC_H__
