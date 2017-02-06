// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_PROPERTY_H_
#define UI_AURA_WINDOW_PROPERTY_H_

#include <stdint.h>

#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

// This header should be included by code that defines WindowProperties.
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

// No single new-style cast works for every conversion to/from int64_t, so we
// need this helper class. A third specialization is needed for bool because
// MSVC warning C4800 (forcing value to bool) is not suppressed by an explicit
// cast (!).
template<typename T>
class WindowPropertyCaster {
 public:
  static int64_t ToInt64(T x) { return static_cast<int64_t>(x); }
  static T FromInt64(int64_t x) { return static_cast<T>(x); }
};
template<typename T>
class WindowPropertyCaster<T*> {
 public:
  static int64_t ToInt64(T* x) { return reinterpret_cast<int64_t>(x); }
  static T* FromInt64(int64_t x) { return reinterpret_cast<T*>(x); }
};
template<>
class WindowPropertyCaster<bool> {
 public:
  static int64_t ToInt64(bool x) { return static_cast<int64_t>(x); }
  static bool FromInt64(int64_t x) { return x != 0; }
};

}  // namespace

template<typename T>
struct WindowProperty {
  T default_value;
  const char* name;
  Window::PropertyDeallocator deallocator;
};

namespace subtle {

class AURA_EXPORT PropertyHelper {
 public:
  template<typename T>
  static void Set(Window* window, const WindowProperty<T>* property, T value) {
    int64_t old = window->SetPropertyInternal(
        property, property->name,
        value == property->default_value ? nullptr : property->deallocator,
        WindowPropertyCaster<T>::ToInt64(value),
        WindowPropertyCaster<T>::ToInt64(property->default_value));
    if (property->deallocator &&
        old != WindowPropertyCaster<T>::ToInt64(property->default_value)) {
      (*property->deallocator)(old);
    }
  }
  template<typename T>
  static T Get(const Window* window, const WindowProperty<T>* property) {
    return WindowPropertyCaster<T>::FromInt64(window->GetPropertyInternal(
        property, WindowPropertyCaster<T>::ToInt64(property->default_value)));
  }
  template<typename T>
  static void Clear(Window* window, const WindowProperty<T>* property) {
    window->SetProperty(property, property->default_value);       \
  }
};

}  // namespace subtle

}  // namespace aura

// Macros to instantiate the property getter/setter template functions.
#define DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(EXPORT, T)  \
    namespace aura {                                            \
      template<> EXPORT void aura::Window::SetProperty(         \
          const WindowProperty<T >* property, T value) {        \
        subtle::PropertyHelper::Set<T>(this, property, value);  \
      }                                                         \
      template<> EXPORT T Window::GetProperty(                  \
          const WindowProperty<T >* property) const {           \
        return subtle::PropertyHelper::Get<T>(this, property);  \
      }                                                         \
      template<> EXPORT void Window::ClearProperty(             \
          const WindowProperty<T >* property) {                 \
        subtle::PropertyHelper::Clear<T>(this, property);       \
      }                                                         \
    }
#define DECLARE_WINDOW_PROPERTY_TYPE(T)  \
    DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(, T)

#define DEFINE_WINDOW_PROPERTY_KEY(TYPE, NAME, DEFAULT)                      \
  static_assert(sizeof(TYPE) <= sizeof(int64_t), "property type too large"); \
  namespace {                                                                \
  const aura::WindowProperty<TYPE> NAME##_Value = {DEFAULT, #NAME, nullptr}; \
  }                                                                          \
  const aura::WindowProperty<TYPE>* const NAME = &NAME##_Value;

#define DEFINE_LOCAL_WINDOW_PROPERTY_KEY(TYPE, NAME, DEFAULT)                \
  static_assert(sizeof(TYPE) <= sizeof(int64_t), "property type too large"); \
  namespace {                                                                \
  const aura::WindowProperty<TYPE> NAME##_Value = {DEFAULT, #NAME, nullptr}; \
  const aura::WindowProperty<TYPE>* const NAME = &NAME##_Value;              \
  }

#define DEFINE_OWNED_WINDOW_PROPERTY_KEY(TYPE, NAME, DEFAULT)            \
  namespace {                                                            \
  void Deallocator##NAME(int64_t p) {                                    \
    enum { type_must_be_complete = sizeof(TYPE) };                       \
    delete aura::WindowPropertyCaster<TYPE*>::FromInt64(p);              \
  }                                                                      \
  const aura::WindowProperty<TYPE*> NAME##_Value = {DEFAULT, #NAME,      \
                                                    &Deallocator##NAME}; \
  }                                                                      \
  const aura::WindowProperty<TYPE*>* const NAME = &NAME##_Value;

#endif  // UI_AURA_WINDOW_PROPERTY_H_
