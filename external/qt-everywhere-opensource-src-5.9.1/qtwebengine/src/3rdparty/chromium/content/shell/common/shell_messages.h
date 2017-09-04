// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <string>
#include <vector>

#include "base/values.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/page_state.h"
#include "content/shell/common/leak_detection_result.h"
#include "content/shell/common/shell_test_configuration.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

#define IPC_MESSAGE_START ShellMsgStart

IPC_STRUCT_TRAITS_BEGIN(content::ShellTestConfiguration)
IPC_STRUCT_TRAITS_MEMBER(current_working_directory)
IPC_STRUCT_TRAITS_MEMBER(temp_path)
IPC_STRUCT_TRAITS_MEMBER(test_url)
IPC_STRUCT_TRAITS_MEMBER(enable_pixel_dumping)
IPC_STRUCT_TRAITS_MEMBER(allow_external_pages)
IPC_STRUCT_TRAITS_MEMBER(expected_pixel_hash)
IPC_STRUCT_TRAITS_MEMBER(initial_size)
IPC_STRUCT_TRAITS_END()

// Tells the renderer to reset all test runners.
IPC_MESSAGE_ROUTED0(ShellViewMsg_Reset)

// Sets the path to the WebKit checkout.
IPC_MESSAGE_CONTROL1(ShellViewMsg_SetWebKitSourceDir,
                     base::FilePath /* webkit source dir */)

// Sets the test config for a layout test that is being started.  This message
// is sent only to a renderer that hosts parts of the main test window.
IPC_MESSAGE_ROUTED1(ShellViewMsg_SetTestConfiguration,
                    content::ShellTestConfiguration)

// Replicates test config (for an already started test) to a new renderer
// that hosts parts of the main test window.
IPC_MESSAGE_ROUTED1(ShellViewMsg_ReplicateTestConfiguration,
                    content::ShellTestConfiguration)

// Sets up a secondary renderer (renderer that doesn't [yet] host parts of the
// main test window) for a layout test.
IPC_MESSAGE_ROUTED0(ShellViewMsg_SetupSecondaryRenderer)

// Tells the main window that a secondary renderer in a different process asked
// to finish the test.
IPC_MESSAGE_ROUTED0(ShellViewMsg_TestFinishedInSecondaryRenderer)

// Pushes a snapshot of the current session history from the browser process.
// This includes only information about those RenderViews that are in the
// same process as the main window of the layout test and that are the current
// active RenderView of their WebContents.
IPC_MESSAGE_ROUTED3(
    ShellViewMsg_SessionHistory,
    std::vector<int> /* routing_ids */,
    std::vector<std::vector<content::PageState> > /* session_histories */,
    std::vector<unsigned> /* current_entry_indexes */)

IPC_MESSAGE_ROUTED0(ShellViewMsg_TryLeakDetection)

// Asks a frame to dump its contents into a string and send them back over IPC.
IPC_MESSAGE_ROUTED0(ShellViewMsg_LayoutDumpRequest)

// Notifies BlinkTestRunner that the layout dump has completed
// (and that it can proceed with finishing up the test).
IPC_MESSAGE_ROUTED1(ShellViewMsg_LayoutDumpCompleted,
                    std::string /* completed/stitched layout dump */)

// Send a text dump of the WebContents to the render host.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_TextDump,
                    std::string /* dump */)

// Asks the browser process to perform a layout dump spanning all the
// (potentially cross-process) frames.  This triggers multiple
// ShellViewMsg_LayoutDumpRequest / ShellViewHostMsg_LayoutDumpResponse messages
// and ends with sending of ShellViewMsg_LayoutDumpCompleted.
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_InitiateLayoutDump)

// Sends a layout dump of a frame (response to ShellViewMsg_LayoutDumpRequest).
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_LayoutDumpResponse, std::string /* dump */)

// Send an image dump of the WebContents to the render host.
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_ImageDump,
                    std::string /* actual pixel hash */,
                    SkBitmap /* image */)

// Send an audio dump to the render host.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_AudioDump,
                    std::vector<unsigned char> /* audio data */)

IPC_MESSAGE_ROUTED0(ShellViewHostMsg_TestFinished)

IPC_MESSAGE_ROUTED0(ShellViewHostMsg_ResetDone)

// WebTestDelegate related.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_OverridePreferences,
                    content::WebPreferences /* preferences */)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_PrintMessage,
                    std::string /* message */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_ClearDevToolsLocalStorage)
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_ShowDevTools,
                    std::string /* settings */,
                    std::string /* frontend_url */)
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_EvaluateInDevTools,
                    int /* call_id */,
                    std::string /* script */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_CloseDevTools)
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_GoToOffset,
                    int /* offset */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_Reload)
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_LoadURLForFrame,
                    GURL /* url */,
                    std::string /* frame_name */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_CaptureSessionHistory)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_CloseRemainingWindows)

IPC_STRUCT_TRAITS_BEGIN(content::LeakDetectionResult)
IPC_STRUCT_TRAITS_MEMBER(leaked)
IPC_STRUCT_TRAITS_MEMBER(detail)
IPC_STRUCT_TRAITS_END()

IPC_MESSAGE_ROUTED1(ShellViewHostMsg_LeakDetectionDone,
                    content::LeakDetectionResult /* result */)

IPC_MESSAGE_ROUTED1(ShellViewHostMsg_SetBluetoothManualChooser,
                    bool /* enable */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_GetBluetoothManualChooserEvents)
IPC_MESSAGE_ROUTED1(ShellViewMsg_ReplyBluetoothManualChooserEvents,
                    std::vector<std::string> /* events */)
IPC_MESSAGE_ROUTED2(ShellViewHostMsg_SendBluetoothManualChooserEvent,
                    std::string /* event */,
                    std::string /* argument */)
