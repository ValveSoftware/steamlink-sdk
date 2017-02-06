// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_PROPERTY_H_
#define COMPONENTS_EXO_SURFACE_PROPERTY_H_

#include <stdint.h>

#include "components/exo/surface.h"

// This header should be included by code that defines SurfaceProperties.
//
// To define a new SurfaceProperty:
//
//  #include "foo/foo_export.h"
//  #include "components/exo/surface_property.h"
//
//  DECLARE_EXPORTED_SURFACE_PROPERTY_TYPE(FOO_EXPORT, MyType);
//  namespace foo {
//    // Use this to define an exported property that is premitive,
//    // or a pointer you don't want automatically deleted.
//    DEFINE_SURFACE_PROPERTY_KEY(MyType, kMyKey, MyDefault);
//
//    // Use this to define an exported property whose value is a heap
//    // allocated object, and has to be owned and freed by the surface.
//    DEFINE_OWNED_SURFACE_PROPERTY_KEY(gfx::Rect, kRestoreBoundsKey, NULL);
//
//    // Use this to define a non exported property that is primitive,
//    // or a pointer you don't want to automatically deleted, and is used
//    // only in a specific file. This will define the property in an unnamed
//    // namespace which cannot be accessed from another file.
//    DEFINE_LOCAL_SURFACE_PROPERTY_KEY(MyType, kMyKey, MyDefault);
//
//  }  // foo namespace
//
// To define a new type used for SurfaceProperty.
//
//  // outside all namespaces:
//  DECLARE_EXPORTED_SURFACE_PROPERTY_TYPE(FOO_EXPORT, MyType)
//
// If a property type is not exported, use DECLARE_SURFACE_PROPERTY_TYPE(MyType)
// which is a shorthand for DECLARE_EXPORTED_SURFACE_PROPERTY_TYPE(, MyType).

namespace exo {
namespace {

// No single new-style cast works for every conversion to/from int64_t, so we
// need this helper class. A third specialization is needed for bool because
// MSVC warning C4800 (forcing value to bool) is not suppressed by an explicit
// cast (!).
template <typename T>
class SurfacePropertyCaster {
 public:
  static int64_t ToInt64(T x) { return static_cast<int64_t>(x); }
  static T FromInt64(int64_t x) { return static_cast<T>(x); }
};
template <typename T>
class SurfacePropertyCaster<T*> {
 public:
  static int64_t ToInt64(T* x) { return reinterpret_cast<int64_t>(x); }
  static T* FromInt64(int64_t x) { return reinterpret_cast<T*>(x); }
};
template <>
class SurfacePropertyCaster<bool> {
 public:
  static int64_t ToInt64(bool x) { return static_cast<int64_t>(x); }
  static bool FromInt64(int64_t x) { return x != 0; }
};

}  // namespace

template <typename T>
struct SurfaceProperty {
  T default_value;
  const char* name;
  Surface::PropertyDeallocator deallocator;
};

namespace subtle {

class PropertyHelper {
 public:
  template <typename T>
  static void Set(Surface* surface,
                  const SurfaceProperty<T>* property,
                  T value) {
    int64_t old = surface->SetPropertyInternal(
        property, property->name,
        value == property->default_value ? nullptr : property->deallocator,
        SurfacePropertyCaster<T>::ToInt64(value),
        SurfacePropertyCaster<T>::ToInt64(property->default_value));
    if (property->deallocator &&
        old != SurfacePropertyCaster<T>::ToInt64(property->default_value)) {
      (*property->deallocator)(old);
    }
  }
  template <typename T>
  static T Get(const Surface* surface, const SurfaceProperty<T>* property) {
    return SurfacePropertyCaster<T>::FromInt64(surface->GetPropertyInternal(
        property, SurfacePropertyCaster<T>::ToInt64(property->default_value)));
  }
  template <typename T>
  static void Clear(Surface* surface, const SurfaceProperty<T>* property) {
    surface->SetProperty(property, property->default_value);
  }
};

}  // namespace subtle

}  // namespace exo

// Macros to instantiate the property getter/setter template functions.
#define DECLARE_EXPORTED_SURFACE_PROPERTY_TYPE(EXPORT, T)                   \
  namespace exo {                                                           \
  template <>                                                               \
  EXPORT void exo::Surface::SetProperty(const SurfaceProperty<T>* property, \
                                        T value) {                          \
    subtle::PropertyHelper::Set<T>(this, property, value);                  \
  }                                                                         \
  template <>                                                               \
  EXPORT T Surface::GetProperty(const SurfaceProperty<T>* property) const { \
    return subtle::PropertyHelper::Get<T>(this, property);                  \
  }                                                                         \
  template <>                                                               \
  EXPORT void Surface::ClearProperty(const SurfaceProperty<T>* property) {  \
    subtle::PropertyHelper::Clear<T>(this, property);                       \
  }                                                                         \
  }
#define DECLARE_SURFACE_PROPERTY_TYPE(T) \
  DECLARE_EXPORTED_SURFACE_PROPERTY_TYPE(, T)

#define DEFINE_SURFACE_PROPERTY_KEY(TYPE, NAME, DEFAULT)                     \
  static_assert(sizeof(TYPE) <= sizeof(int64_t), "property type too large"); \
  namespace {                                                                \
  const exo::SurfaceProperty<TYPE> NAME##_Value = {DEFAULT, #NAME, nullptr}; \
  }                                                                          \
  const exo::SurfaceProperty<TYPE>* const NAME = &NAME##_Value;

#define DEFINE_LOCAL_SURFACE_PROPERTY_KEY(TYPE, NAME, DEFAULT)               \
  static_assert(sizeof(TYPE) <= sizeof(int64_t), "property type too large"); \
  namespace {                                                                \
  const exo::SurfaceProperty<TYPE> NAME##_Value = {DEFAULT, #NAME, nullptr}; \
  const exo::SurfaceProperty<TYPE>* const NAME = &NAME##_Value;              \
  }

#define DEFINE_OWNED_SURFACE_PROPERTY_KEY(TYPE, NAME, DEFAULT)           \
  namespace {                                                            \
  void Deallocator##NAME(int64_t p) {                                    \
    enum { type_must_be_complete = sizeof(TYPE) };                       \
    delete exo::SurfacePropertyCaster<TYPE*>::FromInt64(p);              \
  }                                                                      \
  const exo::SurfaceProperty<TYPE*> NAME##_Value = {DEFAULT, #NAME,      \
                                                    &Deallocator##NAME}; \
  }                                                                      \
  const exo::SurfaceProperty<TYPE*>* const NAME = &NAME##_Value;

#endif  // COMPONENTS_EXO_SURFACE_PROPERTY_H_
