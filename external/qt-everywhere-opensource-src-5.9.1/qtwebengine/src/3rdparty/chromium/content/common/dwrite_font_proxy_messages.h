// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START DWriteFontProxyMsgStart

#ifndef CONTENT_COMMON_DWRITE_FONT_PROXY_MESSAGES_H_
#define CONTENT_COMMON_DWRITE_FONT_PROXY_MESSAGES_H_

// The macros can't handle a complex template declaration, so we typedef it.
typedef std::pair<base::string16, base::string16> DWriteStringPair;

#endif  // CONTENT_COMMON_DWRITE_FONT_PROXY_MESSAGES_H_

IPC_STRUCT_BEGIN(DWriteFontStyle)
  IPC_STRUCT_MEMBER(uint16_t, font_weight)
  IPC_STRUCT_MEMBER(uint8_t, font_slant)
  IPC_STRUCT_MEMBER(uint8_t, font_stretch)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(MapCharactersResult)
  IPC_STRUCT_MEMBER(uint32_t, family_index)
  IPC_STRUCT_MEMBER(base::string16, family_name)
  IPC_STRUCT_MEMBER(uint32_t, mapped_length)
  IPC_STRUCT_MEMBER(float, scale)
  IPC_STRUCT_MEMBER(DWriteFontStyle, font_style)
IPC_STRUCT_END()

// Locates the index of the specified font family within the system collection.
IPC_SYNC_MESSAGE_CONTROL1_1(DWriteFontProxyMsg_FindFamily,
                            base::string16 /* family_name */,
                            uint32_t /* out index */)

// Returns the number of font families in the system collection.
IPC_SYNC_MESSAGE_CONTROL0_1(DWriteFontProxyMsg_GetFamilyCount,
                            uint32_t /* out count */)

// Returns the list of locale and family name pairs for the font family at the
// specified index.
IPC_SYNC_MESSAGE_CONTROL1_1(
    DWriteFontProxyMsg_GetFamilyNames,
    uint32_t /* family_index */,
    std::vector<DWriteStringPair> /* out family_names */)

// Returns the list of font file paths in the system font directory that contain
// font data for the font family at the specified index.
IPC_SYNC_MESSAGE_CONTROL1_2(
    DWriteFontProxyMsg_GetFontFiles,
    uint32_t /* family_index */,
    std::vector<base::string16> /* out file_paths */,
    std::vector<IPC::PlatformFileForTransit> /* out file_handles*/)

// Locates a font family that is able to render the specified text using the
// specified style. If successful, the family_index and family_name will
// indicate which family in the system font collection can render the requested
// text and the mapped_length will indicate how many characters can be
// rendered. If no font exists that can render the text, family_index will be
// UINT32_MAX and mapped_length will indicate how many characters cannot be
// rendered by any installed font.
IPC_SYNC_MESSAGE_CONTROL5_1(DWriteFontProxyMsg_MapCharacters,
                            base::string16 /* text */,
                            DWriteFontStyle /* font_style */,
                            base::string16 /* locale_name */,
                            uint32_t /* reading_direction */,
                            base::string16 /* base_family_name - optional */,
                            MapCharactersResult /* out */)
