// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include <vector>

#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

#define IPC_MESSAGE_START ImageMsgStart

// Messages sent from the browser to the renderer.

// Requests the renderer to download the specified image, decode it,
// and send the image data back via ImageHostMsg_DidDownloadImage.
IPC_MESSAGE_ROUTED4(ImageMsg_DownloadImage,
                    int /* Identifier for the request */,
                    GURL /* URL of the image */,
                    bool /* is favicon (turn off cookies) */,
                    uint32_t /* Maximal bitmap size in pixel. The results are
                                filtered according the max size. If there are no
                                bitmaps at the passed in GURL <= max size, the
                                smallest bitmap is resized to the max size and
                                is the only result. A max size of zero means
                                that the max size is unlimited. */)

// Messages sent from the renderer to the browser.

IPC_MESSAGE_ROUTED5(ImageHostMsg_DidDownloadImage,
                    int /* Identifier of the request */,
                    int /* HTTP response status */,
                    GURL /* URL of the image */,
                    std::vector<SkBitmap> /* bitmap data */,
                    /* The sizes in pixel of the bitmaps before they were
                       resized due to the maximal bitmap size passed to
                       ImageMsg_DownloadImage. Each entry in the bitmaps vector
                       corresponds to an entry in the sizes vector. If a bitmap
                       was resized, there should be a single returned bitmap. */
                    std::vector<gfx::Size>)
