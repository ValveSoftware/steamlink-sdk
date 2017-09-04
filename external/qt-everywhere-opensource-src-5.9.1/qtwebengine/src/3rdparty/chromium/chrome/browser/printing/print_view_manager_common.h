// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_COMMON_H_
#define CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_COMMON_H_

#include "printing/features/features.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace printing {

// Start printing using the appropriate PrintViewManagerBase subclass.
void StartPrint(content::WebContents* web_contents,
                bool print_preview_disabled,
                bool has_selection);

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
// Start printing using the system print dialog.
void StartBasicPrint(content::WebContents* contents);
#endif

// If the user has selected text in the currently focused frame, print only that
// frame (this makes print selection work for multiple frames).
content::RenderFrameHost* GetFrameToPrint(content::WebContents* contents);

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_COMMON_H_
