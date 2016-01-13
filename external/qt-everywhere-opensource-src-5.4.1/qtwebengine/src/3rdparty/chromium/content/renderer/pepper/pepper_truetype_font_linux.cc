// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/sys_byteorder.h"
#include "content/public/common/child_process_sandbox_support_linux.h"
#include "content/renderer/pepper/pepper_truetype_font.h"
#include "ppapi/c/dev/ppb_truetype_font_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"

namespace content {

namespace {

class PepperTrueTypeFontLinux : public PepperTrueTypeFont {
 public:
  explicit PepperTrueTypeFontLinux(
      const ppapi::proxy::SerializedTrueTypeFontDesc& desc);
  virtual ~PepperTrueTypeFontLinux() OVERRIDE;

  // PepperTrueTypeFont overrides.
  virtual bool IsValid() OVERRIDE;
  virtual int32_t Describe(ppapi::proxy::SerializedTrueTypeFontDesc* desc)
      OVERRIDE;
  virtual int32_t GetTableTags(std::vector<uint32_t>* tags) OVERRIDE;
  virtual int32_t GetTable(uint32_t table_tag,
                           int32_t offset,
                           int32_t max_data_length,
                           std::string* data) OVERRIDE;

 private:
  // Save creation parameters here and use these to implement Describe.
  // TODO(bbudge) Modify content API to return results of font matching and
  // fallback.
  ppapi::proxy::SerializedTrueTypeFontDesc desc_;
  int fd_;

  DISALLOW_COPY_AND_ASSIGN(PepperTrueTypeFontLinux);
};

PepperTrueTypeFontLinux::PepperTrueTypeFontLinux(
    const ppapi::proxy::SerializedTrueTypeFontDesc& desc)
    : desc_(desc) {
  // If no face is provided, convert family to the platform defaults. These
  // names should be mapped by FontConfig to an appropriate default font.
  if (desc_.family.empty()) {
    switch (desc_.generic_family) {
      case PP_TRUETYPEFONTFAMILY_SERIF:
        desc_.family = "serif";
        break;
      case PP_TRUETYPEFONTFAMILY_SANSSERIF:
        desc_.family = "sans-serif";
        break;
      case PP_TRUETYPEFONTFAMILY_CURSIVE:
        desc_.family = "cursive";
        break;
      case PP_TRUETYPEFONTFAMILY_FANTASY:
        desc_.family = "fantasy";
        break;
      case PP_TRUETYPEFONTFAMILY_MONOSPACE:
        desc_.family = "monospace";
        break;
    }
  }

  fd_ = MatchFontWithFallback(desc_.family.c_str(),
                              desc_.weight >= PP_TRUETYPEFONTWEIGHT_BOLD,
                              desc_.style & PP_TRUETYPEFONTSTYLE_ITALIC,
                              desc_.charset,
                              PP_BROWSERFONT_TRUSTED_FAMILY_DEFAULT);
}

PepperTrueTypeFontLinux::~PepperTrueTypeFontLinux() {}

bool PepperTrueTypeFontLinux::IsValid() { return fd_ != -1; }

int32_t PepperTrueTypeFontLinux::Describe(
    ppapi::proxy::SerializedTrueTypeFontDesc* desc) {
  *desc = desc_;
  return PP_OK;
}

int32_t PepperTrueTypeFontLinux::GetTableTags(std::vector<uint32_t>* tags) {
  // Get the 2 byte numTables field at an offset of 4 in the font.
  uint8_t num_tables_buf[2];
  size_t output_length = sizeof(num_tables_buf);
  if (!GetFontTable(fd_,
                    0 /* tag */,
                    4 /* offset */,
                    reinterpret_cast<uint8_t*>(&num_tables_buf),
                    &output_length))
    return PP_ERROR_FAILED;
  DCHECK(output_length == sizeof(num_tables_buf));
  // Font data is stored in big-endian order.
  uint16_t num_tables = (num_tables_buf[0] << 8) | num_tables_buf[1];

  // The font has a header, followed by n table entries in its directory.
  static const size_t kFontHeaderSize = 12;
  static const size_t kTableEntrySize = 16;
  output_length = num_tables * kTableEntrySize;
  scoped_ptr<uint8_t[]> table_entries(new uint8_t[output_length]);
  // Get the table directory entries, which follow the font header.
  if (!GetFontTable(fd_,
                    0 /* tag */,
                    kFontHeaderSize /* offset */,
                    table_entries.get(),
                    &output_length))
    return PP_ERROR_FAILED;
  DCHECK(output_length == num_tables * kTableEntrySize);

  tags->resize(num_tables);
  for (uint16_t i = 0; i < num_tables; i++) {
    uint8_t* entry = table_entries.get() + i * kTableEntrySize;
    uint32_t tag = static_cast<uint32_t>(entry[0]) << 24 |
                   static_cast<uint32_t>(entry[1]) << 16 |
                   static_cast<uint32_t>(entry[2]) << 8 |
                   static_cast<uint32_t>(entry[3]);
    (*tags)[i] = tag;
  }

  return num_tables;
}

int32_t PepperTrueTypeFontLinux::GetTable(uint32_t table_tag,
                                          int32_t offset,
                                          int32_t max_data_length,
                                          std::string* data) {
  // Get the size of the font data first.
  size_t table_size = 0;
  // Tags are byte swapped on Linux.
  table_tag = base::ByteSwap(table_tag);
  if (!GetFontTable(fd_, table_tag, offset, NULL, &table_size))
    return PP_ERROR_FAILED;
  // Only retrieve as much as the caller requested.
  table_size = std::min(table_size, static_cast<size_t>(max_data_length));
  data->resize(table_size);
  if (!GetFontTable(fd_,
                    table_tag,
                    offset,
                    reinterpret_cast<uint8_t*>(&(*data)[0]),
                    &table_size))
    return PP_ERROR_FAILED;

  return base::checked_cast<int32_t>(table_size);
}

}  // namespace

// static
PepperTrueTypeFont* PepperTrueTypeFont::Create(
    const ppapi::proxy::SerializedTrueTypeFontDesc& desc) {
  return new PepperTrueTypeFontLinux(desc);
}

}  // namespace content
