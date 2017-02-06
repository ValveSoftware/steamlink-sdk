// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRUCT_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRUCT_TRAITS_H_

namespace mojo {

// This must be specialized for any type |T| to be serialized/deserialized as
// a mojom struct of type |MojomType|.
//
// Each specialization needs to implement a few things:
//   1. Static getters for each field in the Mojom type. These should be
//      of the form:
//
//        static <return type> <field name>(const T& input);
//
//      and should return a serializable form of the named field as extracted
//      from |input|.
//
//      Serializable form of a field:
//        Value or reference of the same type used in |MojomType|, or the
//        following alternatives:
//        - string:
//          Value or reference of any type that has a StringTraits defined.
//          Supported by default: base::StringPiece, std::string.
//
//        - array:
//          Value or reference of any type that has an ArrayTraits defined.
//          Supported by default: std::vector, WTF::Vector (in blink), CArray.
//
//        - map:
//          Value or reference of any type that has a MapTraits defined.
//          Supported by default: std::map.
//
//        - struct:
//          Value or reference of any type that has a StructTraits defined.
//
//        - enum:
//          Value of any type that has an EnumTraits defined.
//
//      During serialization, getters for string/struct/array/map/union fields
//      are called twice (one for size calculation and one for actual
//      serialization). If you want to return a value (as opposed to a
//      reference) from these getters, you have to be sure that constructing and
//      copying the returned object is really cheap.
//
//      Getters for fields of other types are called once.
//
//   2. A static Read() method to set the contents of a |T| instance from a
//      |MojomType|DataView (e.g., if |MojomType| is test::Example, the data
//      view will be test::ExampleDataView).
//
//        static bool Read(|MojomType|DataView data, T* output);
//
//      The generated |MojomType|DataView type provides a convenient,
//      inexpensive view of a serialized struct's field data.
//
//      Returning false indicates invalid incoming data and causes the message
//      pipe receiving it to be disconnected. Therefore, you can do custom
//      validation for |T| in this method.
//
//   3. [Optional] A static IsNull() method indicating whether a given |T|
//      instance is null:
//
//        static bool IsNull(const T& input);
//
//      If this method returns true, it is guaranteed that none of the getters
//      (described in section 1) will be called for the same |input|. So you
//      don't have to check whether |input| is null in those getters.
//
//      If it is not defined, |T| instances are always considered non-null.
//
//      [Optional] A static SetToNull() method to set the contents of a given
//      |T| instance to null.
//
//        static void SetToNull(T* output);
//
//      When a null serialized struct is received, the deserialization code
//      calls this method instead of Read().
//
//      NOTE: It is to set |*output|'s contents to a null state, not to set the
//      |output| pointer itself to null. "Null state" means whatever state you
//      think it makes sense to map a null serialized struct to.
//
//      If it is not defined, null is not allowed to be converted to |T|. In
//      that case, an incoming null value is considered invalid and causes the
//      message pipe to be disconnected.
//
//   4. [Optional] As mentioned above, getters for string/struct/array/map/union
//      fields are called multiple times (twice to be exact). If you need to do
//      some expensive calculation/conversion, you probably want to cache the
//      result across multiple calls. You can introduce an arbitrary context
//      object by adding two optional methods:
//        static void* SetUpContext(const T& input);
//        static void TearDownContext(const T& input, void* context);
//
//      And then you append a second parameter, void* context, to getters:
//        static <return type> <field name>(const T& input, void* context);
//
//      If a T instance is not null, the serialization code will call
//      SetUpContext() at the beginning, and pass the resulting context pointer
//      to getters. After serialization is done, it calls TearDownContext() so
//      that you can do any necessary cleanup.
//
// In the description above, methods having an |input| parameter define it as
// const reference of T. Actually, it can be a non-const reference of T too.
// E.g., if T contains Mojo handles or interfaces whose ownership needs to be
// transferred. Correspondingly, it requies you to always give non-const T
// reference/value to the Mojo bindings for serialization:
//    - if T is used in the "type_mappings" section of a typemap config file,
//      you need to declare it as pass-by-value:
//        type_mappings = [ "MojomType=T(pass_by_value)" ]
//    - if another type U's StructTraits has a getter for T, it needs to return
//      non-const reference/value.
//
// EXAMPLE:
//
// Mojom definition:
//   struct Bar {};
//   struct Foo {
//     int32 f_integer;
//     string f_string;
//     array<string> f_string_array;
//     Bar f_bar;
//   };
//
// StructTraits for Foo:
//   template <>
//   struct StructTraits<Foo, CustomFoo> {
//     // Optional methods dealing with null:
//     static bool IsNull(const CustomFoo& input);
//     static void SetToNull(CustomFoo* output);
//
//     // Field getters:
//     static int32_t f_integer(const CustomFoo& input);
//     static const std::string& f_string(const CustomFoo& input);
//     static const std::vector<std::string>& f_string_array(
//         const CustomFoo& input);
//     // Assuming there is a StructTraits<Bar, CustomBar> defined.
//     static const CustomBar& f_bar(const CustomFoo& input);
//
//     static bool Read(FooDataView data, CustomFoo* output);
//   };
//
template <typename MojomType, typename T>
struct StructTraits;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRUCT_TRAITS_H_
