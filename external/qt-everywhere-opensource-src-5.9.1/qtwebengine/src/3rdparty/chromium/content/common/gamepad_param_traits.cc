// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gamepad_param_traits.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/pickle.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/WebKit/public/platform/WebGamepad.h"

using blink::WebGamepad;

namespace {

void LogWebUCharString(
    const blink::WebUChar web_string[],
    const size_t array_size,
    std::string* log) {
  base::string16 utf16;
  utf16.reserve(array_size);
  for (size_t i = 0; i < array_size && web_string[i]; ++i) {
    utf16[i] = web_string[i];
  }
  log->append(base::UTF16ToUTF8(utf16));
}

}

namespace IPC {

void ParamTraits<WebGamepad>::Write(base::Pickle* m, const WebGamepad& p) {
  m->WriteData(reinterpret_cast<const char*>(&p), sizeof(WebGamepad));
}

bool ParamTraits<WebGamepad>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   WebGamepad* p) {
  int length;
  const char* data;
  if (!iter->ReadData(&data, &length) || length != sizeof(WebGamepad))
    return false;
  memcpy(p, data, sizeof(WebGamepad));

  return true;
}

void ParamTraits<WebGamepad>::Log(
    const WebGamepad& p,
    std::string* l) {
  l->append("WebGamepad(");
  LogParam(p.connected, l);
  LogWebUCharString(p.id, WebGamepad::idLengthCap, l);
  l->append(",");
  LogWebUCharString(p.mapping, WebGamepad::mappingLengthCap, l);
  l->append(",");
  LogParam(p.timestamp, l);
  l->append(",");
  LogParam(p.axesLength, l);
  l->append(", [");
  for (size_t i = 0; i < arraysize(p.axes); ++i) {
    l->append(base::StringPrintf("%f%s", p.axes[i],
        i < (arraysize(p.axes) - 1) ? ", " : "], "));
  }
  LogParam(p.buttonsLength, l);
  l->append(", [");
  for (size_t i = 0; i < arraysize(p.buttons); ++i) {
    l->append(base::StringPrintf("(%u, %f)%s",
        p.buttons[i].pressed, p.buttons[i].value,
        i < (arraysize(p.buttons) - 1) ? ", " : "], "));
  }
  l->append(")");
}

} // namespace IPC
