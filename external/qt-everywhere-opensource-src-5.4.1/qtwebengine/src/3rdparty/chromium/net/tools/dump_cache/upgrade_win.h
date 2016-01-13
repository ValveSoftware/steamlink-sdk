// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"

// Creates a new server, and returns a new named pipe to communicate with it.
HANDLE CreateServer(base::string16* pipe_number);

// Runs a loop to write a new cache with all the data available from a slave
// process connected through the provided |pipe|.
int UpgradeCache(const base::FilePath& output_path, HANDLE pipe);

// This process will only execute commands from the controller.
int RunSlave(const base::FilePath& input_path,
             const base::string16& pipe_number);
