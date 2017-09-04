// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NetworkLog_h
#define NetworkLog_h

#include "wtf/Assertions.h"

#if DCHECK_IS_ON()
// We can see logs with |--v=N| or |--vmodule=NetworkLog=N| where N is a
// verbose level.
#define NETWORK_DVLOG(verbose_level)      \
  LAZY_STREAM(VLOG_STREAM(verbose_level), \
              ((verbose_level) <= ::logging::GetVlogLevel("NetworkLog.h")))
#else
#define NETWORK_DVLOG(verbose_level) EAT_STREAM_PARAMETERS
#endif

#endif  // NetworkLog_h
