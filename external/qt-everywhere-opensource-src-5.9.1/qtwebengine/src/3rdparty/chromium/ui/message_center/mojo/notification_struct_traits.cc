// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/mojo/notification_struct_traits.h"

#include "mojo/common/common_custom_types_struct_traits.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"
#include "url/mojo/url_gurl_struct_traits.h"

namespace mojo {

// static
const std::string& StructTraits<
    message_center::mojom::NotificationDataView,
    message_center::Notification>::id(const message_center::Notification& n) {
  return n.id();
}

// static
const base::string16& StructTraits<message_center::mojom::NotificationDataView,
                                   message_center::Notification>::
    title(const message_center::Notification& n) {
  return n.title();
}

// static
const base::string16& StructTraits<message_center::mojom::NotificationDataView,
                                   message_center::Notification>::
    message(const message_center::Notification& n) {
  return n.message();
}

// static
const SkBitmap& StructTraits<
    message_center::mojom::NotificationDataView,
    message_center::Notification>::icon(const message_center::Notification& n) {
  // TODO(mhashmi): Remove this when we have a gfx::Image mojom
  static const SkBitmap kNullSkBitmap;
  gfx::Image icon = n.icon();
  return icon.IsEmpty() ? kNullSkBitmap : *icon.ToSkBitmap();
}

// static
const base::string16& StructTraits<message_center::mojom::NotificationDataView,
                                   message_center::Notification>::
    display_source(const message_center::Notification& n) {
  return n.display_source();
}

// static
const GURL& StructTraits<message_center::mojom::NotificationDataView,
                         message_center::Notification>::
    origin_url(const message_center::Notification& n) {
  return n.origin_url();
}

// static
bool StructTraits<message_center::mojom::NotificationDataView,
                  message_center::Notification>::
    Read(message_center::mojom::NotificationDataView data,
         message_center::Notification* out) {
  SkBitmap icon;
  if (!data.ReadId(&out->id_) || !data.ReadTitle(&out->title_) ||
      !data.ReadMessage(&out->message_) ||
      !data.ReadDisplaySource(&out->display_source_) || !data.ReadIcon(&icon) ||
      !data.ReadOriginUrl(&out->origin_url_)) {
    return false;
  }
  out->icon_ = gfx::Image::CreateFrom1xBitmap(icon);
  return true;
}

}  // namespace mojo
