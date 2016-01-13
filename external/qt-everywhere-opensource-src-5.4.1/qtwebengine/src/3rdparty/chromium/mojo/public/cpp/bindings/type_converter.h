// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TYPE_CONVERTER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TYPE_CONVERTER_H_

namespace mojo {

// Specialize the following class:
//   template <typename T, typename U> class TypeConverter;
// to perform type conversion for Mojom-defined structs and arrays. Here, T is
// the Mojom-defined struct or array, and U is some other non-Mojom
// struct or array type.
//
// Specializations should implement the following interface:
//   namespace mojo {
//   template <>
//   class TypeConverter<T, U> {
//    public:
//     static T ConvertFrom(const U& input);
//     static U ConvertTo(const T& input);
//   };
//   }
//
// EXAMPLE:
//
// Suppose you have the following Mojom-defined struct:
//
//   module geometry {
//   struct Point {
//     int32 x;
//     int32 y;
//   };
//   }
//
// Now, imagine you wanted to write a TypeConverter specialization for
// gfx::Point. It might look like this:
//
//   namespace mojo {
//   template <>
//   class TypeConverter<geometry::PointPtr, gfx::Point> {
//    public:
//     static geometry::PointPtr ConvertFrom(const gfx::Point& input) {
//       geometry::PointPtr result;
//       result->x = input.x();
//       result->y = input.y();
//       return result.Pass();
//     }
//     static gfx::Point ConvertTo(const geometry::PointPtr& input) {
//       return input ? gfx::Point(input->x, input->y) : gfx::Point();
//     }
//   };
//   }
//
// With the above TypeConverter defined, it is possible to write code like this:
//
//   void AcceptPoint(const geometry::PointPtr& input) {
//     // With an explicit cast using the .To<> method.
//     gfx::Point pt = input.To<gfx::Point>();
//
//     // With an explicit cast using the static From() method.
//     geometry::PointPtr output = geometry::Point::From(pt);
//   }
//
template <typename T, typename U> class TypeConverter;

// The following specialization is useful when you are converting between
// Array<POD> and std::vector<POD>.
template <typename T> class TypeConverter<T, T> {
 public:
  static T ConvertFrom(const T& obj) {
    return obj;
  }
  static T ConvertTo(const T& obj) {
    return obj;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TYPE_CONVERTER_H_
