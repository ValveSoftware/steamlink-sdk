// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_INTERNAL_H_

#include <new>
#include <vector>

#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_serialization.h"
#include "mojo/public/cpp/bindings/lib/bounds_checker.h"
#include "mojo/public/cpp/bindings/lib/buffer.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace mojo {
template <typename T> class Array;
class String;

namespace internal {

template <typename T>
struct ArrayDataTraits {
  typedef T StorageType;
  typedef T& Ref;
  typedef T const& ConstRef;

  static size_t GetStorageSize(size_t num_elements) {
    return sizeof(StorageType) * num_elements;
  }
  static Ref ToRef(StorageType* storage, size_t offset) {
    return storage[offset];
  }
  static ConstRef ToConstRef(const StorageType* storage, size_t offset) {
    return storage[offset];
  }
};

template <typename P>
struct ArrayDataTraits<P*> {
  typedef StructPointer<P> StorageType;
  typedef P*& Ref;
  typedef P* const& ConstRef;

  static size_t GetStorageSize(size_t num_elements) {
    return sizeof(StorageType) * num_elements;
  }
  static Ref ToRef(StorageType* storage, size_t offset) {
    return storage[offset].ptr;
  }
  static ConstRef ToConstRef(const StorageType* storage, size_t offset) {
    return storage[offset].ptr;
  }
};

template <typename T>
struct ArrayDataTraits<Array_Data<T>*> {
  typedef ArrayPointer<T> StorageType;
  typedef Array_Data<T>*& Ref;
  typedef Array_Data<T>* const& ConstRef;

  static size_t GetStorageSize(size_t num_elements) {
    return sizeof(StorageType) * num_elements;
  }
  static Ref ToRef(StorageType* storage, size_t offset) {
    return storage[offset].ptr;
  }
  static ConstRef ToConstRef(const StorageType* storage, size_t offset) {
    return storage[offset].ptr;
  }
};

// Specialization of Arrays for bools, optimized for space. It has the
// following differences from a generalized Array:
// * Each element takes up a single bit of memory.
// * Accessing a non-const single element uses a helper class |BitRef|, which
// emulates a reference to a bool.
template <>
struct ArrayDataTraits<bool> {
  // Helper class to emulate a reference to a bool, used for direct element
  // access.
  class BitRef {
   public:
    ~BitRef();
    BitRef& operator=(bool value);
    BitRef& operator=(const BitRef& value);
    operator bool() const;
   private:
    friend struct ArrayDataTraits<bool>;
    BitRef(uint8_t* storage, uint8_t mask);
    BitRef();
    uint8_t* storage_;
    uint8_t mask_;
  };

  typedef uint8_t StorageType;
  typedef BitRef Ref;
  typedef bool ConstRef;

  static size_t GetStorageSize(size_t num_elements) {
    return ((num_elements + 7) / 8);
  }
  static BitRef ToRef(StorageType* storage, size_t offset) {
    return BitRef(&storage[offset / 8], 1 << (offset % 8));
  }
  static bool ToConstRef(const StorageType* storage, size_t offset) {
    return (storage[offset / 8] & (1 << (offset % 8))) != 0;
  }
};

// What follows is code to support the serialization of Array_Data<T>. There
// are two interesting cases: arrays of primitives and arrays of objects.
// Arrays of objects are represented as arrays of pointers to objects.

template <typename T, bool kIsHandle> struct ArraySerializationHelper;

template <typename T>
struct ArraySerializationHelper<T, false> {
  typedef typename ArrayDataTraits<T>::StorageType ElementType;

  static void EncodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       std::vector<Handle>* handles) {
  }

  static void DecodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       std::vector<Handle>* handles) {
  }

  static bool ValidateElements(const ArrayHeader* header,
                               const ElementType* elements,
                               BoundsChecker* bounds_checker) {
    return true;
  }
};

template <>
struct ArraySerializationHelper<Handle, true> {
  typedef ArrayDataTraits<Handle>::StorageType ElementType;

  static void EncodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       std::vector<Handle>* handles);

  static void DecodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       std::vector<Handle>* handles);

  static bool ValidateElements(const ArrayHeader* header,
                               const ElementType* elements,
                               BoundsChecker* bounds_checker);
};

template <typename H>
struct ArraySerializationHelper<H, true> {
  typedef typename ArrayDataTraits<H>::StorageType ElementType;

  static void EncodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       std::vector<Handle>* handles) {
    ArraySerializationHelper<Handle, true>::EncodePointersAndHandles(
        header, elements, handles);
  }

  static void DecodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       std::vector<Handle>* handles) {
    ArraySerializationHelper<Handle, true>::DecodePointersAndHandles(
        header, elements, handles);
  }

  static bool ValidateElements(const ArrayHeader* header,
                               const ElementType* elements,
                               BoundsChecker* bounds_checker) {
    return ArraySerializationHelper<Handle, true>::ValidateElements(
        header, elements, bounds_checker);
  }
};

template <typename P>
struct ArraySerializationHelper<P*, false> {
  typedef typename ArrayDataTraits<P*>::StorageType ElementType;

  static void EncodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       std::vector<Handle>* handles) {
    for (uint32_t i = 0; i < header->num_elements; ++i)
      Encode(&elements[i], handles);
  }

  static void DecodePointersAndHandles(const ArrayHeader* header,
                                       ElementType* elements,
                                       std::vector<Handle>* handles) {
    for (uint32_t i = 0; i < header->num_elements; ++i)
      Decode(&elements[i], handles);
  }

  static bool ValidateElements(const ArrayHeader* header,
                               const ElementType* elements,
                               BoundsChecker* bounds_checker) {
    for (uint32_t i = 0; i < header->num_elements; ++i) {
      if (!ValidateEncodedPointer(&elements[i].offset)) {
        ReportValidationError(VALIDATION_ERROR_ILLEGAL_POINTER);
        return false;
      }
      if (!P::Validate(DecodePointerRaw(&elements[i].offset), bounds_checker))
        return false;
    }
    return true;
  }
};

template <typename T>
class Array_Data {
 public:
  typedef ArrayDataTraits<T> Traits;
  typedef typename Traits::StorageType StorageType;
  typedef typename Traits::Ref Ref;
  typedef typename Traits::ConstRef ConstRef;
  typedef ArraySerializationHelper<T, IsHandle<T>::value> Helper;

  static Array_Data<T>* New(size_t num_elements, Buffer* buf) {
    size_t num_bytes = sizeof(Array_Data<T>) +
                       Traits::GetStorageSize(num_elements);
    return new (buf->Allocate(num_bytes)) Array_Data<T>(num_bytes,
                                                        num_elements);
  }

  static bool Validate(const void* data, BoundsChecker* bounds_checker) {
    if (!data)
      return true;
    if (!IsAligned(data)) {
      ReportValidationError(VALIDATION_ERROR_MISALIGNED_OBJECT);
      return false;
    }
    if (!bounds_checker->IsValidRange(data, sizeof(ArrayHeader))) {
      ReportValidationError(VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
      return false;
    }
    const ArrayHeader* header = static_cast<const ArrayHeader*>(data);
    if (header->num_bytes < (sizeof(Array_Data<T>) +
                             Traits::GetStorageSize(header->num_elements))) {
      ReportValidationError(VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER);
      return false;
    }
    if (!bounds_checker->ClaimMemory(data, header->num_bytes)) {
      ReportValidationError(VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
      return false;
    }

    const Array_Data<T>* object = static_cast<const Array_Data<T>*>(data);
    return Helper::ValidateElements(&object->header_, object->storage(),
                                    bounds_checker);
  }

  size_t size() const { return header_.num_elements; }

  Ref at(size_t offset) {
    assert(offset < static_cast<size_t>(header_.num_elements));
    return Traits::ToRef(storage(), offset);
  }

  ConstRef at(size_t offset) const {
    assert(offset < static_cast<size_t>(header_.num_elements));
    return Traits::ToConstRef(storage(), offset);
  }

  StorageType* storage() {
    return reinterpret_cast<StorageType*>(
        reinterpret_cast<char*>(this) + sizeof(*this));
  }

  const StorageType* storage() const {
    return reinterpret_cast<const StorageType*>(
        reinterpret_cast<const char*>(this) + sizeof(*this));
  }

  void EncodePointersAndHandles(std::vector<Handle>* handles) {
    Helper::EncodePointersAndHandles(&header_, storage(), handles);
  }

  void DecodePointersAndHandles(std::vector<Handle>* handles) {
    Helper::DecodePointersAndHandles(&header_, storage(), handles);
  }

 private:
  Array_Data(size_t num_bytes, size_t num_elements) {
    header_.num_bytes = static_cast<uint32_t>(num_bytes);
    header_.num_elements = static_cast<uint32_t>(num_elements);
  }
  ~Array_Data() {}

  internal::ArrayHeader header_;

  // Elements of type internal::ArrayDataTraits<T>::StorageType follow.
};
MOJO_COMPILE_ASSERT(sizeof(Array_Data<char>) == 8, bad_sizeof_Array_Data);

// UTF-8 encoded
typedef Array_Data<char> String_Data;

template <typename T, bool kIsMoveOnlyType> struct ArrayTraits {};

template <typename T> struct ArrayTraits<T, false> {
  typedef T StorageType;
  typedef typename std::vector<T>::reference RefType;
  typedef typename std::vector<T>::const_reference ConstRefType;
  typedef ConstRefType ForwardType;
  static inline void Initialize(std::vector<T>* vec) {
  }
  static inline void Finalize(std::vector<T>* vec) {
  }
  static inline ConstRefType at(const std::vector<T>* vec, size_t offset) {
    return vec->at(offset);
  }
  static inline RefType at(std::vector<T>* vec, size_t offset) {
    return vec->at(offset);
  }
  static inline void Resize(std::vector<T>* vec, size_t size) {
    vec->resize(size);
  }
  static inline void PushBack(std::vector<T>* vec, ForwardType value) {
    vec->push_back(value);
  }
};

template <typename T> struct ArrayTraits<T, true> {
  struct StorageType {
    char buf[sizeof(T) + (8 - (sizeof(T) % 8)) % 8];  // Make 8-byte aligned.
  };
  typedef T& RefType;
  typedef const T& ConstRefType;
  typedef T ForwardType;
  static inline void Initialize(std::vector<StorageType>* vec) {
    for (size_t i = 0; i < vec->size(); ++i)
      new (vec->at(i).buf) T();
  }
  static inline void Finalize(std::vector<StorageType>* vec) {
    for (size_t i = 0; i < vec->size(); ++i)
      reinterpret_cast<T*>(vec->at(i).buf)->~T();
  }
  static inline ConstRefType at(const std::vector<StorageType>* vec,
                                size_t offset) {
    return *reinterpret_cast<const T*>(vec->at(offset).buf);
  }
  static inline RefType at(std::vector<StorageType>* vec, size_t offset) {
    return *reinterpret_cast<T*>(vec->at(offset).buf);
  }
  static inline void Resize(std::vector<StorageType>* vec, size_t size) {
    size_t old_size = vec->size();
    for (size_t i = size; i < old_size; i++)
      reinterpret_cast<T*>(vec->at(i).buf)->~T();
    ResizeStorage(vec, size);
    for (size_t i = old_size; i < vec->size(); i++)
      new (vec->at(i).buf) T();
  }
  static inline void PushBack(std::vector<StorageType>* vec, RefType value) {
    size_t old_size = vec->size();
    ResizeStorage(vec, old_size + 1);
    new (vec->at(old_size).buf) T(value.Pass());
  }
  static inline void ResizeStorage(std::vector<StorageType>* vec, size_t size) {
    if (size <= vec->capacity()) {
      vec->resize(size);
      return;
    }
    std::vector<StorageType> new_storage(size);
    for (size_t i = 0; i < vec->size(); i++)
      new (new_storage.at(i).buf) T(at(vec, i).Pass());
    vec->swap(new_storage);
    Finalize(&new_storage);
  }
};

template <> struct WrapperTraits<String, false> {
  typedef String_Data* DataType;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_INTERNAL_H_
