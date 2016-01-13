// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_PROPERTY_H_
#define UI_AURA_WINDOW_PROPERTY_H_

#include "base/basictypes.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

// This header should be included by code that defines WindowProperties. It
// should not be included by code that only gets and sets WindowProperties.
//
// To define a new WindowProperty:
//
//  #include "foo/foo_export.h"
//  #include "ui/aura/window_property.h"
//
//  DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(FOO_EXPORT, MyType);
//  namespace foo {
//    // Use this to define an exported property that is premitive,
//    // or a pointer you don't want automatically deleted.
//    DEFINE_WINDOW_PROPERTY_KEY(MyType, kMyKey, MyDefault);
//
//    // Use this to define an exported property whose value is a heap
//    // allocated object, and has to be owned and freed by the window.
//    DEFINE_OWNED_WINDOW_PROPERTY_KEY(gfx::Rect, kRestoreBoundsKey, NULL);
//
//    // Use this to define a non exported property that is primitive,
//    // or a pointer you don't want to automatically deleted, and is used
//    // only in a specific file. This will define the property in an unnamed
//    // namespace which cannot be accessed from another file.
//    DEFINE_LOCAL_WINDOW_PROPERTY_KEY(MyType, kMyKey, MyDefault);
//
//  }  // foo namespace
//
// To define a new type used for WindowProperty.
//
//  // outside all namespaces:
//  DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(FOO_EXPORT, MyType)
//
// If a property type is not exported, use DECLARE_WINDOW_PROPERTY_TYPE(MyType)
// which is a shorthand for DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(, MyType).

namespace aura {
namespace {

// No single new-style cast works for every conversion to/from int64, so we
// need this helper class. A third specialization is needed for bool because
// MSVC warning C4800 (forcing value to bool) is not suppressed by an explicit
// cast (!).
template<typename T>
class WindowPropertyCaster {
 public:
  static int64 ToInt64(T x) { return static_cast<int64>(x); }
  static T FromInt64(int64 x) { return static_cast<T>(x); }
};
template<typename T>
class WindowPropertyCaster<T*> {
 public:
  static int64 ToInt64(T* x) { return reinterpret_cast<int64>(x); }
  static T* FromInt64(int64 x) { return reinterpret_cast<T*>(x); }
};
template<>
class WindowPropertyCaster<bool> {
 public:
  static int64 ToInt64(bool x) { return static_cast<int64>(x); }
  static bool FromInt64(int64 x) { return x != 0; }
};

}  // namespace

template<typename T>
struct WindowProperty {
  T default_value;
  const char* name;
  Window::PropertyDeallocator deallocator;
};

template<typename T>
void Window::SetProperty(const WindowProperty<T>* property, T value) {
  int64 old = SetPropertyInternal(
      property,
      property->name,
      value == property->default_value ? NULL : property->deallocator,
      WindowPropertyCaster<T>::ToInt64(value),
      WindowPropertyCaster<T>::ToInt64(property->default_value));
  if (property->deallocator &&
      old != WindowPropertyCaster<T>::ToInt64(property->default_value)) {
    (*property->deallocator)(old);
  }
}

template<typename T>
T Window::GetProperty(const WindowProperty<T>* property) const {
  return WindowPropertyCaster<T>::FromInt64(GetPropertyInternal(
      property, WindowPropertyCaster<T>::ToInt64(property->default_value)));
}

template<typename T>
void Window::ClearProperty(const WindowProperty<T>* property) {
  SetProperty(property, property->default_value);
}

}  // namespace aura

// Macros to instantiate the property getter/setter template functions.
#define DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(EXPORT, T)  \
    template EXPORT void aura::Window::SetProperty(         \
        const aura::WindowProperty<T >*, T);                \
    template EXPORT T aura::Window::GetProperty(            \
        const aura::WindowProperty<T >*) const;             \
    template EXPORT void aura::Window::ClearProperty(       \
        const aura::WindowProperty<T >*);
#define DECLARE_WINDOW_PROPERTY_TYPE(T)  \
    DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(, T)

#define DEFINE_WINDOW_PROPERTY_KEY(TYPE, NAME, DEFAULT) \
  COMPILE_ASSERT(sizeof(TYPE) <= sizeof(int64), property_type_too_large);     \
  namespace {                                                                 \
    const aura::WindowProperty<TYPE> NAME ## _Value = {DEFAULT, #NAME, NULL}; \
  }                                                                           \
  const aura::WindowProperty<TYPE>* const NAME = & NAME ## _Value;

#define DEFINE_LOCAL_WINDOW_PROPERTY_KEY(TYPE, NAME, DEFAULT) \
  COMPILE_ASSERT(sizeof(TYPE) <= sizeof(int64), property_type_too_large);     \
  namespace {                                                                 \
    const aura::WindowProperty<TYPE> NAME ## _Value = {DEFAULT, #NAME, NULL}; \
    const aura::WindowProperty<TYPE>* const NAME = & NAME ## _Value;          \
  }

#define DEFINE_OWNED_WINDOW_PROPERTY_KEY(TYPE, NAME, DEFAULT)   \
  namespace {                                                   \
    void Deallocator ## NAME (int64 p) {                        \
      enum { type_must_be_complete = sizeof(TYPE) };            \
      delete aura::WindowPropertyCaster<TYPE*>::FromInt64(p); \
    }                                                           \
    const aura::WindowProperty<TYPE*> NAME ## _Value =          \
        {DEFAULT,#NAME,&Deallocator ## NAME};                   \
  }                                                             \
  const aura::WindowProperty<TYPE*>* const NAME = & NAME ## _Value;

#endif  // UI_AURA_WINDOW_PROPERTY_H_
