// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no include guard.

#include <vector>

#include "base/basictypes.h"
#include "ipc/ipc_message_macros.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/metro_viewer/ime_types.h"

#define IPC_MESSAGE_START MetroViewerMsgStart

IPC_ENUM_TRAITS(ui::EventType)
IPC_ENUM_TRAITS(ui::EventFlags)

// Contains the parameters sent for a mousebutton message.
IPC_STRUCT_BEGIN(MetroViewerHostMsg_MouseButtonParams)

  IPC_STRUCT_MEMBER(int32, x)
  IPC_STRUCT_MEMBER(int32, y)
  IPC_STRUCT_MEMBER(int32, extra)
  IPC_STRUCT_MEMBER(ui::EventType, event_type)
  IPC_STRUCT_MEMBER(uint32, flags)
  IPC_STRUCT_MEMBER(ui::EventFlags, changed_button)
  IPC_STRUCT_MEMBER(bool, is_horizontal_wheel)

IPC_STRUCT_END()

// Messages sent from the viewer to the browser:

// Inform the browser of the surface to target for compositing.
IPC_MESSAGE_CONTROL2(MetroViewerHostMsg_SetTargetSurface,
                     gfx::NativeViewId /* target hwnd */,
                     float             /* device scale */)
// Informs the browser that the mouse moved.
IPC_MESSAGE_CONTROL3(MetroViewerHostMsg_MouseMoved,
                     int32,       /* x-coordinate */
                     int32,       /* y-coordinate */
                     int32        /* flags */)
// Informs the brower that a mouse button was pressed.
IPC_MESSAGE_CONTROL1(MetroViewerHostMsg_MouseButton,
                     MetroViewerHostMsg_MouseButtonParams)
// Informs the browser that a key was pressed.
IPC_MESSAGE_CONTROL4(MetroViewerHostMsg_KeyDown,
                     uint32,       /* virtual key */
                     uint32,       /* repeat count */
                     uint32,       /* scan code */
                     uint32        /* key state */);
// Informs the browser that a key was released.
IPC_MESSAGE_CONTROL4(MetroViewerHostMsg_KeyUp,
                     uint32,       /* virtual key */
                     uint32,       /* repeat count */
                     uint32,       /* scan code */
                     uint32        /* key state */);
IPC_MESSAGE_CONTROL4(MetroViewerHostMsg_Character,
                     uint32,       /* virtual key */
                     uint32,       /* repeat count */
                     uint32,       /* scan code */
                     uint32        /* key state */);
// Informs the browser that the Metro window has been activated.
IPC_MESSAGE_CONTROL0(MetroViewerHostMsg_WindowActivated);

// Informs the browser that the user has completed an edge gesture.
IPC_MESSAGE_CONTROL0(MetroViewerHostMsg_EdgeGesture)

IPC_MESSAGE_CONTROL4(MetroViewerHostMsg_TouchDown,
                     int32,           /* x-coordinate */
                     int32,           /* y-coordinate */
                     uint64,          /* timestamp */
                     uint32)          /* pointer_id */
IPC_MESSAGE_CONTROL4(MetroViewerHostMsg_TouchUp,
                     int32,           /* x-coordinate */
                     int32,           /* y-coordinate */
                     uint64,          /* timestamp */
                     uint32)          /* pointer_id */
IPC_MESSAGE_CONTROL4(MetroViewerHostMsg_TouchMoved,
                     int32,           /* x-coordinate */
                     int32,           /* y-coordinate */
                     uint64,          /* timestamp */
                     uint32)          /* pointer_id */

// Informs the browser of the result of a file save as operation.
IPC_MESSAGE_CONTROL3(MetroViewerHostMsg_FileSaveAsDone,
                     bool,           /* success */
                     base::FilePath, /* filename */
                     int)            /* filter_index */

// Informs the browser of the result of a file open operation.
IPC_MESSAGE_CONTROL2(MetroViewerHostMsg_FileOpenDone,
                     bool,           /* success */
                     base::FilePath) /* filename */

IPC_MESSAGE_CONTROL2(MetroViewerHostMsg_MultiFileOpenDone,
                     bool,                    /* success */
                     std::vector<base::FilePath>)   /* filenames */

// Informs the browser of the result of a select folder operation.
IPC_MESSAGE_CONTROL2(MetroViewerHostMsg_SelectFolderDone,
                     bool,           /* success */
                     base::FilePath) /* filepath*/

// Messages sent from the browser to the viewer:

// Requests the viewer to activate desktop mode.
IPC_MESSAGE_CONTROL2(MetroViewerHostMsg_ActivateDesktop,
                     base::FilePath /* shortcut */,
                     bool           /* ash exit */);

// Request the viewer to close itself gracefully.
IPC_MESSAGE_CONTROL0(MetroViewerHostMsg_MetroExit);

// Requests the viewer to open a URL in desktop mode.
IPC_MESSAGE_CONTROL2(MetroViewerHostMsg_OpenURLOnDesktop,
                     base::FilePath,  /* shortcut */
                     base::string16         /* url */);

// Requests the viewer to change the pointer to a new cursor.
IPC_MESSAGE_CONTROL1(MetroViewerHostMsg_SetCursor,
                     int64         /* cursor */);

// This structure contains the parameters sent to the viewer process to display
// the file save dialog.
IPC_STRUCT_BEGIN(MetroViewerHostMsg_SaveAsDialogParams)

  // The title of the file save dialog if any.
  IPC_STRUCT_MEMBER(base::string16, title)

  // The suggested file name.
  IPC_STRUCT_MEMBER(base::FilePath, suggested_name)

  // The save as filter to be used.
  IPC_STRUCT_MEMBER(base::string16, filter)

  // The filter index.
  IPC_STRUCT_MEMBER(uint32, filter_index)

  // The default extension.
  IPC_STRUCT_MEMBER(base::string16, default_extension)

IPC_STRUCT_END()

// Requests the viewer to display the file save dialog.
IPC_MESSAGE_CONTROL1(MetroViewerHostMsg_DisplayFileSaveAs,
                     MetroViewerHostMsg_SaveAsDialogParams)

// Requests the viewer to display the file open dialog.
IPC_MESSAGE_CONTROL4(MetroViewerHostMsg_DisplayFileOpen,
                     base::string16,       /* title */
                     base::string16,       /* filter */
                     base::FilePath, /* Default path */
                     bool)           /* allow_multi_select */

// Requests the viewer to display the select folder dialog.
IPC_MESSAGE_CONTROL1(MetroViewerHostMsg_DisplaySelectFolder,
                     base::string16)   /* title */

// Sent to the viewer process to set the cursor position.
IPC_MESSAGE_CONTROL2(MetroViewerHostMsg_SetCursorPos,
                     int,  /* x */
                     int)  /* y */

// Ack sent by the viewer process indicating that the SetCursorPos operation
// was completed.
IPC_MESSAGE_CONTROL0(MetroViewerHostMsg_SetCursorPosAck)

IPC_MESSAGE_CONTROL1(MetroViewerHostMsg_OpenURL,
                     base::string16)  /* url */

IPC_MESSAGE_CONTROL1(MetroViewerHostMsg_SearchRequest,
                     base::string16)  /* search_string */

// Sent from the metro viewer process to the browser process to indicate that
// the viewer window size has changed.
IPC_MESSAGE_CONTROL2(MetroViewerHostMsg_WindowSizeChanged,
                     uint32,   /* width */
                     uint32)   /* height */

IPC_STRUCT_TRAITS_BEGIN(metro_viewer::UnderlineInfo)
  IPC_STRUCT_TRAITS_MEMBER(start_offset)
  IPC_STRUCT_TRAITS_MEMBER(end_offset)
  IPC_STRUCT_TRAITS_MEMBER(thick)
IPC_STRUCT_TRAITS_END()

// Sent from the metro viewer process to the browser process to update the
// composition string.
IPC_MESSAGE_CONTROL4(
    MetroViewerHostMsg_ImeCompositionChanged,
    base::string16,                                  /* text */
    int32,                                     /* selection_start */
    int32,                                     /* selection_end */
    std::vector<metro_viewer::UnderlineInfo>)  /* underlines */

// Sent from the metro viewer process to the browser process to update the
// status of popup window that is managed by an IME.
IPC_MESSAGE_CONTROL1(
    MetroViewerHostMsg_ImeCandidatePopupChanged,
    bool)                                      /* UI visibility */

// Sent from the metro viewer process to the browser process to commit strings.
IPC_MESSAGE_CONTROL1(MetroViewerHostMsg_ImeTextCommitted,
                     base::string16)                 /* text */

// Sent from the metro viewer process to the browser process to notify that the
// active text input source is changed.
IPC_MESSAGE_CONTROL2(MetroViewerHostMsg_ImeInputSourceChanged,
                     uint16,                   /* Win32 LangID */
                     bool)                     /* is IME or not */

// Requests the viewer to cancel the on-going composition.
IPC_MESSAGE_CONTROL0(MetroViewerHostMsg_ImeCancelComposition)

IPC_STRUCT_TRAITS_BEGIN(metro_viewer::CharacterBounds)
  IPC_STRUCT_TRAITS_MEMBER(left)
  IPC_STRUCT_TRAITS_MEMBER(top)
  IPC_STRUCT_TRAITS_MEMBER(right)
  IPC_STRUCT_TRAITS_MEMBER(bottom)
IPC_STRUCT_TRAITS_END()

// Requests the viewer to update the document context such as attached
// InputScopes and character bounds.
IPC_MESSAGE_CONTROL2(
    MetroViewerHostMsg_ImeTextInputClientUpdated,
    std::vector<int32>,                           /* InputScope enums */
    std::vector<metro_viewer::CharacterBounds>)   /* character bounds */
