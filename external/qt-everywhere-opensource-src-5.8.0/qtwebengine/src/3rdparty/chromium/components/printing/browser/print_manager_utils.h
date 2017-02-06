// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_UTILS_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_UTILS_H_

struct PrintMsg_Print_Params;

namespace printing {

class PrintSettings;

// Converts given settings to Print_Params and stores them in the output
// parameter |params|.
void RenderParamsFromPrintSettings(const PrintSettings& settings,
                                   PrintMsg_Print_Params* params);

}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_UTILS_H_
