// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BAD_MESSAGE_H_
#define EXTENSIONS_BROWSER_BAD_MESSAGE_H_

namespace content {
class RenderProcessHost;
}

namespace extensions {
namespace bad_message {

// The browser process often chooses to terminate a renderer if it receives
// a bad IPC message. The reasons are tracked for metrics.
//
// See also content/browser/bad_message.h.
//
// NOTE: Do not remove or reorder elements in this list. Add new entries at the
// end. Items may be renamed but do not change the values. We rely on the enum
// values in histograms.
enum BadMessageReason {
  EOG_BAD_ORIGIN = 0,
  EVG_BAD_ORIGIN = 1,
  BH_BLOB_NOT_OWNED = 2,
  EH_BAD_EVENT_ID = 3,
  AVG_BAD_INST_ID = 4,
  AVG_BAD_EXT_ID = 5,
  AVG_NULL_AVG = 6,
  // Please add new elements here. The naming convention is abbreviated class
  // name (e.g. ExtensionHost becomes EH) plus a unique description of the
  // reason. After making changes, you MUST update histograms.xml by running:
  // "python tools/metrics/histograms/update_bad_message_reasons.py"
  BAD_MESSAGE_MAX
};

// Called when the browser receives a bad IPC message from an extension process.
// Logs the event, records a histogram metric for the |reason|, and terminates
// the process for |host|.
void ReceivedBadMessage(content::RenderProcessHost* host,
                        BadMessageReason reason);

}  // namespace bad_message
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BAD_MESSAGE_H_
