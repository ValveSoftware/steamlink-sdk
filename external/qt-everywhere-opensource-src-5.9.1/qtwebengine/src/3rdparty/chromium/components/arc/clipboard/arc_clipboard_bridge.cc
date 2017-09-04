// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/clipboard/arc_clipboard_bridge.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_bridge_service.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_types.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace arc {

ArcClipboardBridge::ArcClipboardBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->clipboard()->AddObserver(this);
}

ArcClipboardBridge::~ArcClipboardBridge() {
  DCHECK(CalledOnValidThread());
  arc_bridge_service()->clipboard()->RemoveObserver(this);
}

void ArcClipboardBridge::OnInstanceReady() {
  mojom::ClipboardInstance* clipboard_instance =
      arc_bridge_service()->clipboard()->GetInstanceForMethod("Init");
  DCHECK(clipboard_instance);
  clipboard_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcClipboardBridge::SetTextContent(const std::string& text) {
  DCHECK(CalledOnValidThread());
  ui::ScopedClipboardWriter writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
  writer.WriteText(base::UTF8ToUTF16(text));
}

void ArcClipboardBridge::GetTextContent() {
  DCHECK(CalledOnValidThread());

  base::string16 text;
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &text);

  mojom::ClipboardInstance* clipboard_instance =
      arc_bridge_service()->clipboard()->GetInstanceForMethod(
          "OnGetTextContent");
  if (!clipboard_instance)
    return;
  clipboard_instance->OnGetTextContent(base::UTF16ToUTF8(text));
}

bool ArcClipboardBridge::CalledOnValidThread() {
  // Make sure access to the Chrome clipboard is happening in the UI thread.
  return thread_checker_.CalledOnValidThread();
}

}  // namespace arc
