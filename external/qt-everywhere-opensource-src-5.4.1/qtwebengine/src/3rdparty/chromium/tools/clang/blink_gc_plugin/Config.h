// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the names used by GC infrastructure.

#ifndef TOOLS_BLINK_GC_PLUGIN_CONFIG_H_
#define TOOLS_BLINK_GC_PLUGIN_CONFIG_H_

#include "clang/AST/AST.h"
#include "clang/AST/Attr.h"

const char kNewOperatorName[] = "operator new";
const char kCreateName[] = "create";
const char kTraceName[] = "trace";
const char kFinalizeName[] = "finalizeGarbageCollectedObject";
const char kTraceAfterDispatchName[] = "traceAfterDispatch";
const char kRegisterWeakMembersName[] = "registerWeakMembers";
const char kHeapAllocatorName[] = "HeapAllocator";

class Config {
 public:
  static bool IsMember(const std::string& name) {
    return name == "Member";
  }

  static bool IsWeakMember(const std::string& name) {
    return name == "WeakMember";
  }

  static bool IsMemberHandle(const std::string& name) {
    return IsMember(name) ||
           IsWeakMember(name);
  }

  static bool IsPersistent(const std::string& name) {
    return name == "Persistent";
  }

  static bool IsPersistentHandle(const std::string& name) {
    return IsPersistent(name) ||
           IsPersistentGCCollection(name);
  }

  static bool IsRawPtr(const std::string& name) {
    return name == "RawPtr";
  }

  static bool IsRefPtr(const std::string& name) {
    return name == "RefPtr";
  }

  static bool IsOwnPtr(const std::string& name) {
    return name == "OwnPtr";
  }

  static bool IsWTFCollection(const std::string& name) {
    return name == "Vector" ||
           name == "Deque" ||
           name == "HashSet" ||
           name == "ListHashSet" ||
           name == "LinkedHashSet" ||
           name == "HashCountedSet" ||
           name == "HashMap";
  }

  static bool IsGCCollection(const std::string& name) {
    return name == "HeapVector" ||
           name == "HeapDeque" ||
           name == "HeapHashSet" ||
           name == "HeapListHashSet" ||
           name == "HeapLinkedHashSet" ||
           name == "HeapHashCountedSet" ||
           name == "HeapHashMap" ||
           IsPersistentGCCollection(name);
  }

  static bool IsPersistentGCCollection(const std::string& name) {
    return name == "PersistentHeapVector" ||
           name == "PersistentHeapDeque" ||
           name == "PersistentHeapHashSet" ||
           name == "PersistentHeapListHashSet" ||
           name == "PersistentHeapLinkedHashSet" ||
           name == "PersistentHeapHashCountedSet" ||
           name == "PersistentHeapHashMap";
  }

  static bool IsHashMap(const std::string& name) {
    return name == "HashMap" ||
           name == "HeapHashMap" ||
           name == "PersistentHeapHashMap";
  }

  // Assumes name is a valid collection name.
  static size_t CollectionDimension(const std::string& name) {
    return (IsHashMap(name) || name == "pair") ? 2 : 1;
  }

  static bool IsGCMixinBase(const std::string& name) {
    return name == "GarbageCollectedMixin";
  }

  static bool IsGCFinalizedBase(const std::string& name) {
    return name == "GarbageCollectedFinalized" ||
           name == "RefCountedGarbageCollected";
  }

  static bool IsGCBase(const std::string& name) {
    return name == "GarbageCollected" ||
           IsGCFinalizedBase(name) ||
           IsGCMixinBase(name);
  }

  static bool IsAnnotated(clang::Decl* decl, const std::string& anno) {
    clang::AnnotateAttr* attr = decl->getAttr<clang::AnnotateAttr>();
    return attr && (attr->getAnnotation() == anno);
  }

  static bool IsStackAnnotated(clang::Decl* decl) {
    return IsAnnotated(decl, "blink_stack_allocated");
  }

  static bool IsIgnoreAnnotated(clang::Decl* decl) {
    return IsAnnotated(decl, "blink_gc_plugin_ignore");
  }

  static bool IsIgnoreCycleAnnotated(clang::Decl* decl) {
    return IsAnnotated(decl, "blink_gc_plugin_ignore_cycle") ||
           IsIgnoreAnnotated(decl);
  }

  static bool IsVisitor(const std::string& name) { return name == "Visitor"; }

  static bool IsTraceMethod(clang::CXXMethodDecl* method,
                            bool* isTraceAfterDispatch = 0) {
    if (method->getNumParams() != 1)
      return false;

    const std::string& name = method->getNameAsString();
    if (name != kTraceName && name != kTraceAfterDispatchName)
      return false;

    const clang::QualType& formal_type = method->getParamDecl(0)->getType();
    if (!formal_type->isPointerType())
      return false;

    clang::CXXRecordDecl* pointee_type =
        formal_type->getPointeeType()->getAsCXXRecordDecl();
    if (!pointee_type)
      return false;

    if (!IsVisitor(pointee_type->getName()))
      return false;

    if (isTraceAfterDispatch)
      *isTraceAfterDispatch = (name == kTraceAfterDispatchName);
    return true;
  }

  static bool StartsWith(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size())
      return false;
    return str.compare(0, prefix.size(), prefix) == 0;
  }

  static bool EndsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size())
      return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  }
};

#endif  // TOOLS_BLINK_GC_PLUGIN_CONFIG_H_
