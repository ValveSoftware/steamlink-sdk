// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_PROPERTY_CONVERTER_H_
#define UI_AURA_MUS_PROPERTY_CONVERTER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

namespace gfx {
class Rect;
}

namespace aura {

// PropertyConverter is used to convert Window properties for transport to the
// mus window server and back. Any time a property changes from one side it is
// mapped to the other using this class. Not all Window properties need to map
// to server properties, and similarly not all transport properties need map to
// Window properties.
class AURA_EXPORT PropertyConverter {
 public:
  PropertyConverter();
  ~PropertyConverter();

  // Maps a property on the Window to a property pushed to the server. Return
  // true if the property should be sent to the server, false if the property
  // is only used locally.
  bool ConvertPropertyForTransport(
      Window* window,
      const void* key,
      std::string* transport_name,
      std::unique_ptr<std::vector<uint8_t>>* transport_value);

  // Returns the transport name for a Window property.
  std::string GetTransportNameForPropertyKey(const void* key);

  // Applies a value from the server to |window|. |transport_name| is the
  // name of the property and |transport_data| the value. |transport_data| may
  // be null.
  void SetPropertyFromTransportValue(
      Window* window,
      const std::string& transport_name,
      const std::vector<uint8_t>* transport_data);

  // Register a property to support conversion between mus and aura.
  template<typename T>
  void RegisterProperty(const WindowProperty<T>* property,
                        const char* transport_name) {
    primitive_properties_[property] =
        PropertyNames(property->name, transport_name);
  }

  // Specializations for properties to pointer types supporting mojo conversion.
  void RegisterProperty(const WindowProperty<gfx::Rect*>* property,
                        const char* transport_name);
  void RegisterProperty(const WindowProperty<std::string*>* property,
                        const char* transport_name);

 private:
  // A pair with the aura::WindowProperty::name and the mus property name.
  using PropertyNames = std::pair<const char*, const char*>;
  // A map of aura::WindowProperty<T> to its aura and mus property names.
  // This supports the internal codepaths for primitive types, eg. T=bool.
  std::map<const void*, PropertyNames> primitive_properties_;

  // Maps of aura::WindowProperty<T> to their mus property names.
  // This supports types that can be serialized for Mojo, eg. T=std::string*.
  std::map<const WindowProperty<gfx::Rect*>*, const char*> rect_properties_;
  std::map<const WindowProperty<std::string*>*, const char*> string_properties_;

  DISALLOW_COPY_AND_ASSIGN(PropertyConverter);
};

}  // namespace aura

#endif  // UI_AURA_MUS_PROPERTY_CONVERTER_H_
