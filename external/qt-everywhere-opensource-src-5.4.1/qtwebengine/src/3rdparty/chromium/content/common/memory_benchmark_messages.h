// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for memory benchmark.
// Multiply-included message file, hence no include guard.

#include <string>

#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START MemoryBenchmarkMsgStart

IPC_MESSAGE_CONTROL1(MemoryBenchmarkHostMsg_HeapProfilerDump,
                     std::string /* dump reason */)
