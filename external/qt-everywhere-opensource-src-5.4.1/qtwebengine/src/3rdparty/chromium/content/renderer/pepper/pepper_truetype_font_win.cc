// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_truetype_font.h"

#include <windows.h>
#include <set>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "content/public/renderer/render_thread.h"
#include "ppapi/c/dev/ppb_truetype_font_dev.h"
#include "ppapi/c/pp_errors.h"

namespace content {

namespace {

class PepperTrueTypeFontWin : public PepperTrueTypeFont {
 public:
  explicit PepperTrueTypeFontWin(
      const ppapi::proxy::SerializedTrueTypeFontDesc& desc);
  virtual ~PepperTrueTypeFontWin();

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
  DWORD GetFontData(HDC hdc,
                    DWORD table,
                    DWORD offset,
                    LPVOID buffer,
                    DWORD length);

  HFONT font_;

  DISALLOW_COPY_AND_ASSIGN(PepperTrueTypeFontWin);
};

PepperTrueTypeFontWin::PepperTrueTypeFontWin(
    const ppapi::proxy::SerializedTrueTypeFontDesc& desc) {
  DWORD pitch_and_family = DEFAULT_PITCH;
  switch (desc.generic_family) {
    case PP_TRUETYPEFONTFAMILY_SERIF:
      pitch_and_family |= FF_ROMAN;
      break;
    case PP_TRUETYPEFONTFAMILY_SANSSERIF:
      pitch_and_family |= FF_SWISS;
      break;
    case PP_TRUETYPEFONTFAMILY_CURSIVE:
      pitch_and_family |= FF_SCRIPT;
      break;
    case PP_TRUETYPEFONTFAMILY_FANTASY:
      pitch_and_family |= FF_DECORATIVE;
      break;
    case PP_TRUETYPEFONTFAMILY_MONOSPACE:
      pitch_and_family |= FF_MODERN;
      break;
  }
  // TODO(bbudge) support widths (extended, condensed).

  font_ = CreateFont(0 /* height */,
                     0 /* width */,
                     0 /* escapement */,
                     0 /* orientation */,
                     desc.weight,  // our weight enum matches Windows.
                     (desc.style & PP_TRUETYPEFONTSTYLE_ITALIC) ? 1 : 0,
                     0 /* underline */,
                     0 /* strikeout */,
                     desc.charset,        // our charset enum matches Windows.
                     OUT_OUTLINE_PRECIS,  // truetype and other outline fonts
                     CLIP_DEFAULT_PRECIS,
                     DEFAULT_QUALITY,
                     pitch_and_family,
                     base::UTF8ToUTF16(desc.family).c_str());
}

PepperTrueTypeFontWin::~PepperTrueTypeFontWin() {}

bool PepperTrueTypeFontWin::IsValid() { return font_ != NULL; }

int32_t PepperTrueTypeFontWin::Describe(
    ppapi::proxy::SerializedTrueTypeFontDesc* desc) {
  LOGFONT font_desc;
  if (!::GetObject(font_, sizeof(LOGFONT), &font_desc))
    return PP_ERROR_FAILED;

  switch (font_desc.lfPitchAndFamily & 0xF0) {  // Top 4 bits are family.
    case FF_ROMAN:
      desc->generic_family = PP_TRUETYPEFONTFAMILY_SERIF;
      break;
    case FF_SWISS:
      desc->generic_family = PP_TRUETYPEFONTFAMILY_SANSSERIF;
      break;
    case FF_SCRIPT:
      desc->generic_family = PP_TRUETYPEFONTFAMILY_CURSIVE;
      break;
    case FF_DECORATIVE:
      desc->generic_family = PP_TRUETYPEFONTFAMILY_FANTASY;
      break;
    case FF_MODERN:
      desc->generic_family = PP_TRUETYPEFONTFAMILY_MONOSPACE;
      break;
  }

  desc->style = font_desc.lfItalic ? PP_TRUETYPEFONTSTYLE_ITALIC
                                   : PP_TRUETYPEFONTSTYLE_NORMAL;
  desc->weight = static_cast<PP_TrueTypeFontWeight_Dev>(font_desc.lfWeight);
  desc->width = PP_TRUETYPEFONTWIDTH_NORMAL;
  desc->charset = static_cast<PP_TrueTypeFontCharset_Dev>(font_desc.lfCharSet);

  // To get the face name, select the font and query for the name. GetObject
  // doesn't fill in the name field of the LOGFONT structure.
  base::win::ScopedCreateDC hdc(::CreateCompatibleDC(NULL));
  if (hdc) {
    base::win::ScopedSelectObject select_object(hdc, font_);
    WCHAR name[LF_FACESIZE];
    GetTextFace(hdc, LF_FACESIZE, name);
    desc->family = base::UTF16ToUTF8(name);
  }
  return PP_OK;
}

DWORD PepperTrueTypeFontWin::GetFontData(HDC hdc,
                                         DWORD table,
                                         DWORD offset,
                                         void* buffer,
                                         DWORD length) {
  // If this is a zero byte read, return a successful result.
  if (buffer && !length)
    return 0;

  DWORD result = ::GetFontData(hdc, table, offset, buffer, length);
  if (result == GDI_ERROR) {
    // The font may not be cached by the OS, causing an attempt to read it in
    // the renderer process to fail. Attempt to pre-cache it.
    LOGFONTW logfont;
    if (!::GetObject(font_, sizeof(LOGFONTW), &logfont))
      return GDI_ERROR;
    RenderThread* render_thread = RenderThread::Get();
    if (!render_thread)
      return GDI_ERROR;
    render_thread->PreCacheFont(logfont);

    result = ::GetFontData(hdc, table, offset, buffer, length);
  }
  return result;
}

int32_t PepperTrueTypeFontWin::GetTableTags(std::vector<uint32_t>* tags) {
  base::win::ScopedCreateDC hdc(::CreateCompatibleDC(NULL));
  if (!hdc)
    return PP_ERROR_FAILED;

  base::win::ScopedSelectObject select_object(hdc, font_);

  // Get the whole font header.
  static const DWORD kFontHeaderSize = 12;
  uint8_t header_buf[kFontHeaderSize];
  if (GetFontData(hdc, 0, 0, header_buf, kFontHeaderSize) == GDI_ERROR)
    return PP_ERROR_FAILED;

  // The numTables follows a 4 byte scalerType tag. Font data is stored in
  // big-endian order.
  DWORD num_tables = (header_buf[4] << 8) | header_buf[5];

  // The size in bytes of an entry in the table directory.
  static const DWORD kDirectoryEntrySize = 16;
  DWORD directory_size = num_tables * kDirectoryEntrySize;
  scoped_ptr<uint8_t[]> directory(new uint8_t[directory_size]);
  // Get the table directory entries after the font header.
  if (GetFontData(
          hdc, 0 /* tag */, kFontHeaderSize, directory.get(), directory_size) ==
      GDI_ERROR)
    return PP_ERROR_FAILED;

  tags->resize(num_tables);
  for (DWORD i = 0; i < num_tables; i++) {
    const uint8_t* entry = directory.get() + i * kDirectoryEntrySize;
    uint32_t tag = static_cast<uint32_t>(entry[0]) << 24 |
                   static_cast<uint32_t>(entry[1]) << 16 |
                   static_cast<uint32_t>(entry[2]) << 8 |
                   static_cast<uint32_t>(entry[3]);
    (*tags)[i] = tag;
  }

  return num_tables;
}

int32_t PepperTrueTypeFontWin::GetTable(uint32_t table_tag,
                                        int32_t offset,
                                        int32_t max_data_length,
                                        std::string* data) {
  base::win::ScopedCreateDC hdc(::CreateCompatibleDC(NULL));
  if (!hdc)
    return PP_ERROR_FAILED;

  base::win::ScopedSelectObject select_object(hdc, font_);

  // Tags are byte swapped on Windows.
  table_tag = base::ByteSwap(table_tag);
  // Get the size of the font table first.
  DWORD table_size = GetFontData(hdc, table_tag, 0, NULL, 0);
  if (table_size == GDI_ERROR)
    return PP_ERROR_FAILED;

  DWORD safe_offset = std::min(static_cast<DWORD>(offset), table_size);
  DWORD safe_length =
      std::min(table_size - safe_offset, static_cast<DWORD>(max_data_length));
  data->resize(safe_length);
  if (safe_length == 0) {
    table_size = 0;
  } else {
    table_size = GetFontData(hdc,
                             table_tag,
                             safe_offset,
                             reinterpret_cast<uint8_t*>(&(*data)[0]),
                             safe_length);
    if (table_size == GDI_ERROR)
      return PP_ERROR_FAILED;
  }
  return static_cast<int32_t>(table_size);
}

}  // namespace

// static
PepperTrueTypeFont* PepperTrueTypeFont::Create(
    const ppapi::proxy::SerializedTrueTypeFontDesc& desc) {
  return new PepperTrueTypeFontWin(desc);
}

}  // namespace content
