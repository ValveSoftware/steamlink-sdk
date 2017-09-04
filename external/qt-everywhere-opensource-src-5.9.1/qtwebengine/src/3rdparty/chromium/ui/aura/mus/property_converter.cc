// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/property_converter.h"

#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_property.h"

namespace aura {

namespace {

// Get the WindowProperty's value as a byte array. Only supports aura properties
// that point to types with a matching std::vector<uint8_t> mojo::TypeConverter.
template <typename T>
std::unique_ptr<std::vector<uint8_t>> GetArray(Window* window,
                                               const WindowProperty<T>* key) {
  const T value = window->GetProperty(key);
  if (!value)
    return base::MakeUnique<std::vector<uint8_t>>();
  return base::MakeUnique<std::vector<uint8_t>>(
      mojo::ConvertTo<std::vector<uint8_t>>(*value));
}

}  // namespace

PropertyConverter::PropertyConverter() {
  // Add known aura properties with associated mus properties.
  RegisterProperty(client::kAlwaysOnTopKey,
                   ui::mojom::WindowManager::kAlwaysOnTop_Property);
  RegisterProperty(client::kAppIdKey,
                   ui::mojom::WindowManager::kAppID_Property);
  RegisterProperty(client::kExcludeFromMruKey,
                   ui::mojom::WindowManager::kExcludeFromMru_Property);
  RegisterProperty(client::kRestoreBoundsKey,
                   ui::mojom::WindowManager::kRestoreBounds_Property);
}

PropertyConverter::~PropertyConverter() {}

bool PropertyConverter::ConvertPropertyForTransport(
    Window* window,
    const void* key,
    std::string* transport_name,
    std::unique_ptr<std::vector<uint8_t>>* transport_value) {
  *transport_name = GetTransportNameForPropertyKey(key);
  if (transport_name->empty())
    return false;

  auto rect_key = static_cast<const WindowProperty<gfx::Rect*>*>(key);
  if (rect_properties_.count(rect_key) > 0) {
    *transport_value = GetArray(window, rect_key);
    return true;
  }

  auto string_key = static_cast<const WindowProperty<std::string*>*>(key);
  if (string_properties_.count(string_key) > 0) {
    *transport_value = GetArray(window, string_key);
    return true;
  }

  // Handle primitive property types generically.
  DCHECK_GT(primitive_properties_.count(key), 0u);
  // TODO(msw): Using the int64_t accessor is wasteful for smaller types.
  const int64_t value = window->GetPropertyInternal(key, 0);
  *transport_value = base::MakeUnique<std::vector<uint8_t>>(
      mojo::ConvertTo<std::vector<uint8_t>>(value));
  return true;
}

std::string PropertyConverter::GetTransportNameForPropertyKey(const void* key) {
  if (primitive_properties_.count(key) > 0)
    return primitive_properties_[key].second;

  auto rect_key = static_cast<const WindowProperty<gfx::Rect*>*>(key);
  if (rect_properties_.count(rect_key) > 0)
    return rect_properties_[rect_key];

  auto string_key = static_cast<const WindowProperty<std::string*>*>(key);
  if (string_properties_.count(string_key) > 0)
    return string_properties_[string_key];

  return std::string();
}

void PropertyConverter::SetPropertyFromTransportValue(
    Window* window,
    const std::string& transport_name,
    const std::vector<uint8_t>* data) {
  for (const auto& primitive_property : primitive_properties_) {
    if (primitive_property.second.second == transport_name) {
      // aura::Window only supports property types that fit in int64_t.
      if (data->size() != 8u) {
        DVLOG(2) << "Property size mismatch (int64_t): " << transport_name;
        return;
      }
      const int64_t value = mojo::ConvertTo<int64_t>(*data);
      // TODO(msw): Should aura::Window just store all properties by name?
      window->SetPropertyInternal(primitive_property.first,
                                  primitive_property.second.first, nullptr,
                                  value, 0);
      return;
    }
  }

  for (const auto& rect_property : rect_properties_) {
    if (rect_property.second == transport_name) {
      if (data->size() != 16u) {
        DVLOG(2) << "Property size mismatch (gfx::Rect): " << transport_name;
        return;
      }
      const gfx::Rect value = mojo::ConvertTo<gfx::Rect>(*data);
      window->SetProperty(rect_property.first, new gfx::Rect(value));
      return;
    }
  }

  for (const auto& string_property : string_properties_) {
    if (string_property.second == transport_name) {
      // TODO(msw): Validate the data somehow, before trying to convert?
      const std::string value = mojo::ConvertTo<std::string>(*data);
      window->SetProperty(string_property.first, new std::string(value));
      return;
    }
  }

  DVLOG(2) << "Unknown mus property name: " << transport_name;
}

void PropertyConverter::RegisterProperty(
    const WindowProperty<gfx::Rect*>* property,
    const char* transport_name) {
  rect_properties_[property] = transport_name;
}

void PropertyConverter::RegisterProperty(
    const WindowProperty<std::string*>* property,
    const char* transport_name) {
  string_properties_[property] = transport_name;
}

}  // namespace aura
