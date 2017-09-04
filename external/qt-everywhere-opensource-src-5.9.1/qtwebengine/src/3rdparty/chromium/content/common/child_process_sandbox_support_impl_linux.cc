// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_process_sandbox_support_impl_linux.h"

#include <stddef.h>
#include <sys/stat.h>

#include <limits>
#include <memory>

#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/sys_byteorder.h"
#include "base/trace_event/trace_event.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "third_party/WebKit/public/platform/linux/WebFallbackFont.h"
#include "third_party/WebKit/public/platform/linux/WebFontRenderStyle.h"

namespace content {

void GetFallbackFontForCharacter(int32_t character,
                                 const char* preferred_locale,
                                 blink::WebFallbackFont* fallbackFont) {
  TRACE_EVENT0("sandbox_ipc", "GetFontFamilyForCharacter");

  base::Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_GET_FALLBACK_FONT_FOR_CHAR);
  request.WriteInt(character);
  request.WriteString(preferred_locale);

  uint8_t buf[512];
  const ssize_t n = base::UnixDomainSocket::SendRecvMsg(
      GetSandboxFD(), buf, sizeof(buf), NULL, request);

  std::string family_name;
  std::string filename;
  int fontconfigInterfaceId = 0;
  int ttcIndex = 0;
  bool isBold = false;
  bool isItalic = false;
  if (n != -1) {
    base::Pickle reply(reinterpret_cast<char*>(buf), n);
    base::PickleIterator pickle_iter(reply);
    if (pickle_iter.ReadString(&family_name) &&
        pickle_iter.ReadString(&filename) &&
        pickle_iter.ReadInt(&fontconfigInterfaceId) &&
        pickle_iter.ReadInt(&ttcIndex) &&
        pickle_iter.ReadBool(&isBold) &&
        pickle_iter.ReadBool(&isItalic)) {
      fallbackFont->name = family_name;
      fallbackFont->filename = filename;
      fallbackFont->fontconfigInterfaceId = fontconfigInterfaceId;
      fallbackFont->ttcIndex = ttcIndex;
      fallbackFont->isBold = isBold;
      fallbackFont->isItalic = isItalic;
    }
  }
}

void GetRenderStyleForStrike(const char* family,
                             int size_and_style,
                             blink::WebFontRenderStyle* out) {
  TRACE_EVENT0("sandbox_ipc", "GetRenderStyleForStrike");

  out->setDefaults();

  if (size_and_style < 0)
    return;

  const bool bold = size_and_style & 1;
  const bool italic = size_and_style & 2;
  const int pixel_size = size_and_style >> 2;
  if (pixel_size > std::numeric_limits<uint16_t>::max())
    return;

  base::Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_GET_STYLE_FOR_STRIKE);
  request.WriteString(family);
  request.WriteBool(bold);
  request.WriteBool(italic);
  request.WriteUInt16(pixel_size);

  uint8_t buf[512];
  const ssize_t n = base::UnixDomainSocket::SendRecvMsg(
      GetSandboxFD(), buf, sizeof(buf), NULL, request);
  if (n == -1)
    return;

  base::Pickle reply(reinterpret_cast<char*>(buf), n);
  base::PickleIterator pickle_iter(reply);
  int use_bitmaps, use_autohint, use_hinting, hint_style, use_antialias;
  int use_subpixel_rendering, use_subpixel_positioning;
  if (pickle_iter.ReadInt(&use_bitmaps) &&
      pickle_iter.ReadInt(&use_autohint) &&
      pickle_iter.ReadInt(&use_hinting) &&
      pickle_iter.ReadInt(&hint_style) &&
      pickle_iter.ReadInt(&use_antialias) &&
      pickle_iter.ReadInt(&use_subpixel_rendering) &&
      pickle_iter.ReadInt(&use_subpixel_positioning)) {
    out->useBitmaps = use_bitmaps;
    out->useAutoHint = use_autohint;
    out->useHinting = use_hinting;
    out->hintStyle = hint_style;
    out->useAntiAlias = use_antialias;
    out->useSubpixelRendering = use_subpixel_rendering;
    out->useSubpixelPositioning = use_subpixel_positioning;
  }
}

int MatchFontWithFallback(const std::string& face,
                          bool bold,
                          bool italic,
                          int charset,
                          PP_BrowserFont_Trusted_Family fallback_family) {
  TRACE_EVENT0("sandbox_ipc", "MatchFontWithFallback");

  base::Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_MATCH_WITH_FALLBACK);
  request.WriteString(face);
  request.WriteBool(bold);
  request.WriteBool(italic);
  request.WriteUInt32(charset);
  request.WriteUInt32(fallback_family);
  uint8_t reply_buf[64];
  int fd = -1;
  base::UnixDomainSocket::SendRecvMsg(
      GetSandboxFD(), reply_buf, sizeof(reply_buf), &fd, request);
  return fd;
}

bool GetFontTable(int fd, uint32_t table_tag, off_t offset,
                  uint8_t* output, size_t* output_length) {
  if (offset < 0)
    return false;

  size_t data_length = 0;  // the length of the file data.
  off_t data_offset = 0;   // the offset of the data in the file.
  if (table_tag == 0) {
    // Get the entire font file.
    struct stat st;
    if (fstat(fd, &st) < 0)
      return false;
    data_length = base::checked_cast<size_t>(st.st_size);
  } else {
    // Get a font table. Read the header to find its offset in the file.
    uint16_t num_tables;
    ssize_t n = HANDLE_EINTR(pread(fd, &num_tables, sizeof(num_tables),
                             4 /* skip the font type */));
    if (n != sizeof(num_tables))
      return false;
    // Font data is stored in net (big-endian) order.
    num_tables = base::NetToHost16(num_tables);

    // Read the table directory.
    static const size_t kTableEntrySize = 16;
    const size_t directory_size = num_tables * kTableEntrySize;
    std::unique_ptr<uint8_t[]> table_entries(new uint8_t[directory_size]);
    n = HANDLE_EINTR(pread(fd, table_entries.get(), directory_size,
                           12 /* skip the SFNT header */));
    if (n != base::checked_cast<ssize_t>(directory_size))
      return false;

    for (uint16_t i = 0; i < num_tables; ++i) {
      uint8_t* entry = table_entries.get() + i * kTableEntrySize;
      uint32_t tag = *reinterpret_cast<uint32_t*>(entry);
      if (tag == table_tag) {
        // Font data is stored in net (big-endian) order.
        data_offset =
            base::NetToHost32(*reinterpret_cast<uint32_t*>(entry + 8));
        data_length =
            base::NetToHost32(*reinterpret_cast<uint32_t*>(entry + 12));
        break;
      }
    }
  }

  if (!data_length)
    return false;
  // Clamp |offset| inside the allowable range. This allows the read to succeed
  // but return 0 bytes.
  offset = std::min(offset, base::checked_cast<off_t>(data_length));
  // Make sure it's safe to add the data offset and the caller's logical offset.
  // Define the maximum positive offset on 32 bit systems.
  static const off_t kMaxPositiveOffset32 = 0x7FFFFFFF;  // 2 GB - 1.
  if ((offset > kMaxPositiveOffset32 / 2) ||
      (data_offset > kMaxPositiveOffset32 / 2))
    return false;
  data_offset += offset;
  data_length -= offset;

  if (output) {
    // 'output_length' holds the maximum amount of data the caller can accept.
    data_length = std::min(data_length, *output_length);
    ssize_t n = HANDLE_EINTR(pread(fd, output, data_length, data_offset));
    if (n != base::checked_cast<ssize_t>(data_length))
      return false;
  }
  *output_length = data_length;

  return true;
}

}  // namespace content
