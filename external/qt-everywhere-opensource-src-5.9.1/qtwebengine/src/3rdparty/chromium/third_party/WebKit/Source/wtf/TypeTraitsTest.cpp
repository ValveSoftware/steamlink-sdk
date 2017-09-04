/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "wtf/TypeTraits.h"

#include "wtf/Noncopyable.h"

// No gtest tests; only static_assert checks.

namespace WTF {

namespace {

struct VirtualClass {
  virtual void A() {}
};
static_assert(!IsTriviallyMoveAssignable<VirtualClass>::value,
              "VirtualClass should not be trivially move assignable");

struct DestructorClass {
  ~DestructorClass() {}
};
static_assert(IsTriviallyMoveAssignable<DestructorClass>::value,
              "DestructorClass should be trivially move assignable");
static_assert(IsTriviallyCopyAssignable<DestructorClass>::value,
              "DestructorClass should be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<DestructorClass>::value,
              "DestructorClass should have a trivial default constructor");

struct MixedPrivate {
  int M2() { return m2; }
  int m1;

 private:
  int m2;
};
static_assert(IsTriviallyMoveAssignable<MixedPrivate>::value,
              "MixedPrivate should be trivially move assignable");
static_assert(IsTriviallyCopyAssignable<MixedPrivate>::value,
              "MixedPrivate should be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<MixedPrivate>::value,
              "MixedPrivate should have a trivial default constructor");
struct JustPrivate {
  int M2() { return m2; }

 private:
  int m2;
};
static_assert(IsTriviallyMoveAssignable<JustPrivate>::value,
              "JustPrivate should be trivially move assignable");
static_assert(IsTriviallyCopyAssignable<JustPrivate>::value,
              "JustPrivate should be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<JustPrivate>::value,
              "JustPrivate should have a trivial default constructor");
struct JustPublic {
  int m2;
};
static_assert(IsTriviallyMoveAssignable<JustPublic>::value,
              "JustPublic should be trivially move assignable");
static_assert(IsTriviallyCopyAssignable<JustPublic>::value,
              "JustPublic should be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<JustPublic>::value,
              "JustPublic should have a trivial default constructor");
struct NestedInherited : public JustPublic, JustPrivate {
  float m3;
};
static_assert(IsTriviallyMoveAssignable<NestedInherited>::value,
              "NestedInherited should be trivially move assignable");
static_assert(IsTriviallyCopyAssignable<NestedInherited>::value,
              "NestedInherited should be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<NestedInherited>::value,
              "NestedInherited should have a trivial default constructor");
struct NestedOwned {
  JustPublic m1;
  JustPrivate m2;
  float m3;
};

static_assert(IsTriviallyMoveAssignable<NestedOwned>::value,
              "NestedOwned should be trivially move assignable");
static_assert(IsTriviallyCopyAssignable<NestedOwned>::value,
              "NestedOwned should be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<NestedOwned>::value,
              "NestedOwned should have a trivial default constructor");

class NonCopyableClass {
  WTF_MAKE_NONCOPYABLE(NonCopyableClass);
};
#if 0   // Compilers don't get this "right" yet if using = delete.
static_assert(!IsTriviallyMoveAssignable<NonCopyableClass>::value, "NonCopyableClass should not be trivially move assignable");
static_assert(!IsTriviallyCopyAssignable<NonCopyableClass>::value, "NonCopyableClass should not be trivially copy assignable");
static_assert(IsTriviallyDefaultConstructible<NonCopyableClass>::value, "NonCopyableClass should have a trivial default constructor");
#endif  // 0

template <typename T>
class TestBaseClass {};

class TestDerivedClass : public TestBaseClass<int> {};

static_assert((IsSubclass<TestDerivedClass, TestBaseClass<int>>::value),
              "Derived class should be a subclass of its base");
static_assert((!IsSubclass<TestBaseClass<int>, TestDerivedClass>::value),
              "Base class should not be a sublass of a derived class");
static_assert((IsSubclassOfTemplate<TestDerivedClass, TestBaseClass>::value),
              "Derived class should be a subclass of template from its base");

typedef int IntArray[];
typedef int IntArraySized[4];

#if !COMPILER(MSVC) || COMPILER(CLANG)

class AssignmentDeleted final {
 private:
  AssignmentDeleted& operator=(const AssignmentDeleted&) = delete;
};

static_assert(!IsCopyAssignable<AssignmentDeleted>::value,
              "AssignmentDeleted isn't copy assignable.");
static_assert(!IsMoveAssignable<AssignmentDeleted>::value,
              "AssignmentDeleted isn't move assignable.");

class AssignmentPrivate final {
 private:
  AssignmentPrivate& operator=(const AssignmentPrivate&);
};

static_assert(!IsCopyAssignable<AssignmentPrivate>::value,
              "AssignmentPrivate isn't copy assignable.");
static_assert(!IsMoveAssignable<AssignmentPrivate>::value,
              "AssignmentPrivate isn't move assignable.");

class CopyAssignmentDeleted final {
 public:
  CopyAssignmentDeleted& operator=(CopyAssignmentDeleted&&);

 private:
  CopyAssignmentDeleted& operator=(const CopyAssignmentDeleted&) = delete;
};

static_assert(!IsCopyAssignable<CopyAssignmentDeleted>::value,
              "CopyAssignmentDeleted isn't copy assignable.");
static_assert(IsMoveAssignable<CopyAssignmentDeleted>::value,
              "CopyAssignmentDeleted is move assignable.");

class CopyAssignmentPrivate final {
 public:
  CopyAssignmentPrivate& operator=(CopyAssignmentPrivate&&);

 private:
  CopyAssignmentPrivate& operator=(const CopyAssignmentPrivate&);
};

static_assert(!IsCopyAssignable<CopyAssignmentPrivate>::value,
              "CopyAssignmentPrivate isn't copy assignable.");
static_assert(IsMoveAssignable<CopyAssignmentPrivate>::value,
              "CopyAssignmentPrivate is move assignable.");

class CopyAssignmentUndeclared final {
 public:
  CopyAssignmentUndeclared& operator=(CopyAssignmentUndeclared&&);
};

static_assert(!IsCopyAssignable<CopyAssignmentUndeclared>::value,
              "CopyAssignmentUndeclared isn't copy assignable.");
static_assert(IsMoveAssignable<CopyAssignmentUndeclared>::value,
              "CopyAssignmentUndeclared is move assignable.");

class Assignable final {
 public:
  Assignable& operator=(const Assignable&);
};

static_assert(IsCopyAssignable<Assignable>::value,
              "Assignable is copy assignable.");
static_assert(IsMoveAssignable<Assignable>::value,
              "Assignable is move assignable.");

class AssignableImplicit final {};

static_assert(IsCopyAssignable<AssignableImplicit>::value,
              "AssignableImplicit is copy assignable.");
static_assert(IsMoveAssignable<AssignableImplicit>::value,
              "AssignableImplicit is move assignable.");

#endif  // !COMPILER(MSVC) || COMPILER(CLANG)

class DefaultConstructorDeleted final {
 private:
  DefaultConstructorDeleted() = delete;
};

class DestructorDeleted final {
 private:
  ~DestructorDeleted() = delete;
};

static_assert(
    !IsTriviallyDefaultConstructible<DefaultConstructorDeleted>::value,
    "DefaultConstructorDeleted must not be trivially default constructible.");

static_assert(!IsTriviallyDestructible<DestructorDeleted>::value,
              "DestructorDeleted must not be trivially destructible.");

template <typename T>
class Wrapper {
 public:
  template <typename U>
  Wrapper(const Wrapper<U>&, EnsurePtrConvertibleArgDecl(U, T)) {}
};

class ForwardDeclarationOnlyClass;

static_assert(std::is_convertible<Wrapper<TestDerivedClass>,
                                  Wrapper<TestDerivedClass>>::value,
              "EnsurePtrConvertibleArgDecl<T, T> should pass");

static_assert(std::is_convertible<Wrapper<TestDerivedClass>,
                                  Wrapper<const TestDerivedClass>>::value,
              "EnsurePtrConvertibleArgDecl<T, const T> should pass");

static_assert(!std::is_convertible<Wrapper<const TestDerivedClass>,
                                   Wrapper<TestDerivedClass>>::value,
              "EnsurePtrConvertibleArgDecl<const T, T> should not pass");

static_assert(std::is_convertible<Wrapper<ForwardDeclarationOnlyClass>,
                                  Wrapper<ForwardDeclarationOnlyClass>>::value,
              "EnsurePtrConvertibleArgDecl<T, T> should pass if T is not a "
              "complete type");

static_assert(
    std::is_convertible<Wrapper<ForwardDeclarationOnlyClass>,
                        Wrapper<const ForwardDeclarationOnlyClass>>::value,
    "EnsurePtrConvertibleArgDecl<T, const T> should pass if T is not a "
    "complete type");

static_assert(!std::is_convertible<Wrapper<const ForwardDeclarationOnlyClass>,
                                   Wrapper<ForwardDeclarationOnlyClass>>::value,
              "EnsurePtrConvertibleArgDecl<const T, T> should not pass if T is "
              "not a complete type");

static_assert(
    std::is_convertible<Wrapper<TestDerivedClass>,
                        Wrapper<TestBaseClass<int>>>::value,
    "EnsurePtrConvertibleArgDecl<U, T> should pass if U is a subclass of T");

static_assert(!std::is_convertible<Wrapper<TestBaseClass<int>>,
                                   Wrapper<TestDerivedClass>>::value,
              "EnsurePtrConvertibleArgDecl<U, T> should not pass if U is a "
              "base class of T");

}  // anonymous namespace

}  // namespace WTF
