// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_PROPERTY_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_PROPERTY_H_

#include <stdint.h>

// To define a new WindowProperty:
//
//  #include "components/mus/public/cpp/window_property.h"
//
//  MUS_DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(FOO_EXPORT, MyType);
//  namespace foo {
//    // Use this to define an exported property that is primitive,
//    // or a pointer you don't want automatically deleted.
//    MUS_DEFINE_WINDOW_PROPERTY_KEY(MyType, kMyKey, MyDefault);
//
//    // Use this to define an exported property whose value is a heap
//    // allocated object, and has to be owned and freed by the window.
//    MUS_DEFINE_OWNED_WINDOW_PROPERTY_KEY(gfx::Rect, kRestoreBoundsKey,
//        nullptr);
//
//    // Use this to define a non exported property that is primitive,
//    // or a pointer you don't want to automatically deleted, and is used
//    // only in a specific file. This will define the property in an unnamed
//    // namespace which cannot be accessed from another file.
//    MUS_DEFINE_LOCAL_WINDOW_PROPERTY_KEY(MyType, kMyKey, MyDefault);
//
//  }  // foo namespace
//
// To define a new type used for WindowProperty.
//
//  // outside all namespaces:
//  MUS_DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(FOO_EXPORT, MyType)
//
// If a property type is not exported, use
// MUS_DECLARE_WINDOW_PROPERTY_TYPE(MyType) which is a shorthand for
// MUS_DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(, MyType).

namespace mus {

template <typename T>
void Window::SetSharedProperty(const std::string& name, const T& data) {
  const std::vector<uint8_t> bytes =
      mojo::TypeConverter<std::vector<uint8_t>, T>::Convert(data);
  SetSharedPropertyInternal(name, &bytes);
}

template <typename T>
T Window::GetSharedProperty(const std::string& name) const {
  DCHECK(HasSharedProperty(name));
  auto it = properties_.find(name);
  return mojo::TypeConverter<T, std::vector<uint8_t>>::Convert(it->second);
}

namespace {

// No single new-style cast works for every conversion to/from int64_t, so we
// need this helper class. A third specialization is needed for bool because
// MSVC warning C4800 (forcing value to bool) is not suppressed by an explicit
// cast (!).
template <typename T>
class WindowPropertyCaster {
 public:
  static int64_t ToInt64(T x) { return static_cast<int64_t>(x); }
  static T FromInt64(int64_t x) { return static_cast<T>(x); }
};
template <typename T>
class WindowPropertyCaster<T*> {
 public:
  static int64_t ToInt64(T* x) { return reinterpret_cast<int64_t>(x); }
  static T* FromInt64(int64_t x) { return reinterpret_cast<T*>(x); }
};
template <>
class WindowPropertyCaster<bool> {
 public:
  static int64_t ToInt64(bool x) { return static_cast<int64_t>(x); }
  static bool FromInt64(int64_t x) { return x != 0; }
};

}  // namespace

template <typename T>
struct WindowProperty {
  T default_value;
  const char* name;
  Window::PropertyDeallocator deallocator;
};

template <typename T>
void Window::SetLocalProperty(const WindowProperty<T>* property, T value) {
  int64_t old = SetLocalPropertyInternal(
      property, property->name,
      value == property->default_value ? nullptr : property->deallocator,
      WindowPropertyCaster<T>::ToInt64(value),
      WindowPropertyCaster<T>::ToInt64(property->default_value));
  if (property->deallocator &&
      old != WindowPropertyCaster<T>::ToInt64(property->default_value)) {
    (*property->deallocator)(old);
  }
}

template <typename T>
T Window::GetLocalProperty(const WindowProperty<T>* property) const {
  return WindowPropertyCaster<T>::FromInt64(GetLocalPropertyInternal(
      property, WindowPropertyCaster<T>::ToInt64(property->default_value)));
}

template <typename T>
void Window::ClearLocalProperty(const WindowProperty<T>* property) {
  SetLocalProperty(property, property->default_value);
}

}  // namespace mus

// Macros to instantiate the property getter/setter template functions.
#define MUS_DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(EXPORT, T) \
  template EXPORT void mus::Window::SetLocalProperty(        \
      const mus::WindowProperty<T>*, T);                     \
  template EXPORT T mus::Window::GetLocalProperty(           \
      const mus::WindowProperty<T>*) const;                  \
  template EXPORT void mus::Window::ClearLocalProperty(      \
      const mus::WindowProperty<T>*);
#define MUS_DECLARE_WINDOW_PROPERTY_TYPE(T) \
  MUS_DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(, T)

#define MUS_DEFINE_WINDOW_PROPERTY_KEY(TYPE, NAME, DEFAULT)                 \
  static_assert(sizeof(TYPE) <= sizeof(int64_t),                            \
                "Property type must fit in 64 bits");                       \
  namespace {                                                               \
  const mus::WindowProperty<TYPE> NAME##_Value = {DEFAULT, #NAME, nullptr}; \
  }                                                                         \
  const mus::WindowProperty<TYPE>* const NAME = &NAME##_Value;

#define MUS_DEFINE_LOCAL_WINDOW_PROPERTY_KEY(TYPE, NAME, DEFAULT)           \
  static_assert(sizeof(TYPE) <= sizeof(int64_t),                            \
                "Property type must fit in 64 bits");                       \
  namespace {                                                               \
  const mus::WindowProperty<TYPE> NAME##_Value = {DEFAULT, #NAME, nullptr}; \
  const mus::WindowProperty<TYPE>* const NAME = &NAME##_Value;              \
  }

#define MUS_DEFINE_OWNED_WINDOW_PROPERTY_KEY(TYPE, NAME, DEFAULT)       \
  namespace {                                                           \
  void Deallocator##NAME(int64_t p) {                                   \
    enum { type_must_be_complete = sizeof(TYPE) };                      \
    delete mus::WindowPropertyCaster<TYPE*>::FromInt64(p);              \
  }                                                                     \
  const mus::WindowProperty<TYPE*> NAME##_Value = {DEFAULT, #NAME,      \
                                                   &Deallocator##NAME}; \
  }                                                                     \
  const mus::WindowProperty<TYPE*>* const NAME = &NAME##_Value;

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_PROPERTY_H_
