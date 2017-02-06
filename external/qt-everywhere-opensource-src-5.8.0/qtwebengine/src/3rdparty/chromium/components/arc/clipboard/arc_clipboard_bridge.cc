// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/clipboard/arc_clipboard_bridge.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_types.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace {

static base::string16 ConvertMojoStringToString16(const mojo::String& input) {
  return base::UTF8ToUTF16(input.get());
}

static mojo::String ConvertString16ToMojoString(const base::string16& input) {
  return mojo::String(base::UTF16ToUTF8(input));
}

}  // namespace

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
      arc_bridge_service()->clipboard()->instance();
  if (!clipboard_instance) {
    LOG(ERROR) << "OnClipboardInstanceReady called, "
               << "but no clipboard instance found";
    return;
  }
  clipboard_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcClipboardBridge::SetTextContent(const mojo::String& text) {
  DCHECK(CalledOnValidThread());
  ui::ScopedClipboardWriter writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
  writer.WriteText(ConvertMojoStringToString16(text));
}

void ArcClipboardBridge::GetTextContent() {
  DCHECK(CalledOnValidThread());

  base::string16 text;
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &text);

  mojom::ClipboardInstance* clipboard_instance =
      arc_bridge_service()->clipboard()->instance();
  clipboard_instance->OnGetTextContent(ConvertString16ToMojoString(text));
}

bool ArcClipboardBridge::CalledOnValidThread() {
  // Make sure access to the Chrome clipboard is happening in the UI thread.
  return thread_checker_.CalledOnValidThread();
}

}  // namespace arc
