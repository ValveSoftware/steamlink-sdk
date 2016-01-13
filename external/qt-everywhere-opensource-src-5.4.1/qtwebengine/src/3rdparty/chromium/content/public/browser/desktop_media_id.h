// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DESKTOP_MEDIA_ID_H_
#define CONTENT_PUBLIC_BROWSER_DESKTOP_MEDIA_ID_H_

#include <string>

#include "base/basictypes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/common/content_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {

// Type used to identify desktop media sources. It's converted to string and
// stored in MediaStreamRequest::requested_video_device_id .
struct CONTENT_EXPORT DesktopMediaID {
 public:
  enum Type {
    TYPE_NONE,
    TYPE_SCREEN,
    TYPE_WINDOW,
    TYPE_AURA_WINDOW,
  };
  typedef intptr_t Id;

#if defined(USE_AURA)
  // Assigns integer identifier to the |window| and returns DesktopMediaID of
  // type TYPE_AURA_WINDOW that corresponds to that |window|.
  static DesktopMediaID RegisterAuraWindow(aura::Window* window);

  // For DesktopMediaID of type TYPE_AURA_WINDOW returns the |window| that was
  // previously registered using RegisterAuraWindow().
  static aura::Window* GetAuraWindowById(const DesktopMediaID& id);
#endif  // defined(USE_AURA)

  static DesktopMediaID Parse(const std::string& str);

  DesktopMediaID()
      : type(TYPE_NONE),
        id(0) {
  }
  DesktopMediaID(Type type, Id id)
      : type(type),
        id(id) {
  }

  // Operators so that DesktopMediaID can be used with STL containers.
  bool operator<(const DesktopMediaID& other) const {
    return type < other.type || (type == other.type && id < other.id);
  }
  bool operator==(const DesktopMediaID& other) const {
    return type == other.type && id == other.id;
  }

  bool is_null() { return type == TYPE_NONE; }

  std::string ToString();

  Type type;
  Id id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DESKTOP_MEDIA_ID_H_
