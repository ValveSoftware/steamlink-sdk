// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTEXT_MENU_PARAMS_H_
#define CONTENT_PUBLIC_COMMON_CONTEXT_MENU_PARAMS_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/menu_item.h"
#include "content/public/common/page_state.h"
#include "content/public/common/ssl_status.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "ui/base/ui_base_types.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "ui/gfx/geometry/point.h"
#endif

namespace content {

struct CONTENT_EXPORT CustomContextMenuContext {
  static const int32_t kCurrentRenderWidget;

  CustomContextMenuContext();

  bool is_pepper_menu;
  int request_id;
  // The routing ID of the render widget on which the context menu is shown.
  // It could also be |kCurrentRenderWidget|, which means the render widget that
  // the corresponding ViewHostMsg_ContextMenu is sent to.
  int32_t render_widget_id;

  // If the context menu was created for a link, and we navigated to that url,
  // this will contain the url that was navigated. This field may not be set
  // if, for example, we are transitioning to an incognito window, since we
  // want to sever any connection to the old renderer.
  GURL link_followed;
};

// FIXME(beng): This would be more useful in the future and more efficient
//              if the parameters here weren't so literally mapped to what
//              they contain for the ContextMenu task. It might be better
//              to make the string fields more generic so that this object
//              could be used for more contextual actions.
struct CONTENT_EXPORT ContextMenuParams {
  ContextMenuParams();
  ContextMenuParams(const ContextMenuParams& other);
  ~ContextMenuParams();

  // This is the type of Context Node that the context menu was invoked on.
  blink::WebContextMenuData::MediaType media_type;

  // These values represent the coordinates of the mouse when the context menu
  // was invoked.  Coords are relative to the associated RenderView's origin.
  int x;
  int y;

  // This is the URL of the link that encloses the node the context menu was
  // invoked on.
  GURL link_url;

  // The text associated with the link. May be an empty string if the contents
  // of the link are an image.
  // Will be empty if link_url is empty.
  base::string16 link_text;

  // The link URL to be used ONLY for "copy link address". We don't validate
  // this field in the frontend process.
  GURL unfiltered_link_url;

  // This is the source URL for the element that the context menu was
  // invoked on.  Example of elements with source URLs are img, audio, and
  // video.
  GURL src_url;

  // This is true if the context menu was invoked on an image which has
  // non-empty contents.
  bool has_image_contents;

  // This is the URL of the top level page that the context menu was invoked
  // on.
  GURL page_url;

  // This is the absolute keyword search URL including the %s search tag when
  // the "Add as search engine..." option is clicked (left empty if not used).
  GURL keyword_url;

  // This is the URL of the subframe that the context menu was invoked on.
  GURL frame_url;

  // This is the page state of the frame on which the context menu was invoked.
  PageState frame_page_state;

  // These are the parameters for the media element that the context menu
  // was invoked on.
  int media_flags;

  // This is the text of the selection that the context menu was invoked on.
  base::string16 selection_text;

  // This is the title or alt (if title not available) text of the selection
  // that the context menu was invoked on.
  base::string16 title_text;

  // This is the suggested filename to be used when saving file through "Save
  // Link As" option of context menu.
  base::string16 suggested_filename;

  // The misspelled word under the cursor, if any. Used to generate the
  // |dictionary_suggestions| list.
  base::string16 misspelled_word;

  // The identifier of the misspelling under the cursor, if any.
  uint32_t misspelling_hash;

  // Suggested replacements for a misspelled word under the cursor.
  // This vector gets populated in the render process host
  // by intercepting ViewHostMsg_ContextMenu in ResourceMessageFilter
  // and populating dictionary_suggestions if the type is EDITABLE
  // and the misspelled_word is not empty.
  std::vector<base::string16> dictionary_suggestions;

  // If editable, flag for whether spell check is enabled or not.
  bool spellcheck_enabled;

  // Whether context is editable.
  bool is_editable;

  // Writing direction menu items.
  int writing_direction_default;
  int writing_direction_left_to_right;
  int writing_direction_right_to_left;

  // These flags indicate to the browser whether the renderer believes it is
  // able to perform the corresponding action.
  int edit_flags;

  // The security info for the resource we are showing the menu on.
  SSLStatus security_info;

  // The character encoding of the frame on which the menu is invoked.
  std::string frame_charset;

  // The referrer policy of the frame on which the menu is invoked.
  blink::WebReferrerPolicy referrer_policy;

  CustomContextMenuContext custom_context;
  std::vector<MenuItem> custom_items;

  ui::MenuSourceType source_type;

  // Extra properties for the context menu.
  std::map<std::string, std::string> properties;

#if defined(OS_ANDROID)
  // Points representing the coordinates in the document space of the start and
  // end of the selection, if there is one.
  gfx::Point selection_start;
  gfx::Point selection_end;
#endif

  // If this node is an input field, the type of that field.
  blink::WebContextMenuData::InputFieldType input_field_type;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTEXT_MENU_PARAMS_H_
