// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/context_menu_params_builder.h"

#include <stddef.h>

#include "content/common/ssl_status_serialization.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/menu_item_builder.h"

namespace content {

// static
ContextMenuParams ContextMenuParamsBuilder::Build(
    const blink::WebContextMenuData& data) {
  ContextMenuParams params;
  params.media_type = data.mediaType;
  params.x = data.mousePosition.x;
  params.y = data.mousePosition.y;
  params.link_url = data.linkURL;
  params.unfiltered_link_url = data.linkURL;
  params.src_url = data.srcURL;
  params.has_image_contents = data.hasImageContents;
  params.page_url = data.pageURL;
  params.keyword_url = data.keywordURL;
  params.frame_url = data.frameURL;
  params.media_flags = data.mediaFlags;
  params.selection_text = data.selectedText;
  params.title_text = data.titleText;
  params.misspelled_word = data.misspelledWord;
  params.misspelling_hash = data.misspellingHash;
  params.spellcheck_enabled = data.isSpellCheckingEnabled;
  params.is_editable = data.isEditable;
  params.writing_direction_default = data.writingDirectionDefault;
  params.writing_direction_left_to_right = data.writingDirectionLeftToRight;
  params.writing_direction_right_to_left = data.writingDirectionRightToLeft;
  params.edit_flags = data.editFlags;
  params.frame_charset = data.frameEncoding.utf8();
  params.referrer_policy = data.referrerPolicy;
  params.suggested_filename = data.suggestedFilename;
  params.input_field_type = data.inputFieldType;

  if (!data.imageResponse.isNull()) {
    GetContentClient()->renderer()->AddImageContextMenuProperties(
        data.imageResponse, &params.properties);
  }

  for (size_t i = 0; i < data.dictionarySuggestions.size(); ++i)
    params.dictionary_suggestions.push_back(data.dictionarySuggestions[i]);

  params.custom_context.is_pepper_menu = false;
  for (size_t i = 0; i < data.customItems.size(); ++i)
    params.custom_items.push_back(MenuItemBuilder::Build(data.customItems[i]));

  if (!data.frameHistoryItem.isNull()) {
    params.frame_page_state =
        SingleHistoryItemToPageState(data.frameHistoryItem);
  }

  params.link_text = data.linkText;

  // Deserialize the SSL info.
  if (!data.securityInfo.isEmpty())
    CHECK(DeserializeSecurityInfo(data.securityInfo, &params.security_info));

  return params;
}

}  // namespace content
