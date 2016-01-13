// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Config.h"
#include "RecordInfo.h"

using namespace clang;
using std::string;

RecordInfo::RecordInfo(CXXRecordDecl* record, RecordCache* cache)
    : cache_(cache),
      record_(record),
      name_(record->getName()),
      fields_need_tracing_(TracingStatus::Unknown()),
      bases_(0),
      fields_(0),
      is_stack_allocated_(kNotComputed),
      is_non_newable_(kNotComputed),
      is_only_placement_newable_(kNotComputed),
      determined_trace_methods_(false),
      trace_method_(0),
      trace_dispatch_method_(0),
      finalize_dispatch_method_(0),
      is_gc_derived_(false),
      base_paths_(0) {}

RecordInfo::~RecordInfo() {
  delete fields_;
  delete bases_;
  delete base_paths_;
}

// Get |count| number of template arguments. Returns false if there
// are fewer than |count| arguments or any of the arguments are not
// of a valid Type structure. If |count| is non-positive, all
// arguments are collected.
bool RecordInfo::GetTemplateArgs(size_t count, TemplateArgs* output_args) {
  ClassTemplateSpecializationDecl* tmpl =
      dyn_cast<ClassTemplateSpecializationDecl>(record_);
  if (!tmpl)
    return false;
  const TemplateArgumentList& args = tmpl->getTemplateArgs();
  if (args.size() < count)
    return false;
  if (count <= 0)
    count = args.size();
  for (unsigned i = 0; i < count; ++i) {
    TemplateArgument arg = args[i];
    if (arg.getKind() == TemplateArgument::Type && !arg.getAsType().isNull()) {
      output_args->push_back(arg.getAsType().getTypePtr());
    } else {
      return false;
    }
  }
  return true;
}

// Test if a record is a HeapAllocated collection.
bool RecordInfo::IsHeapAllocatedCollection() {
  if (!Config::IsGCCollection(name_) && !Config::IsWTFCollection(name_))
    return false;

  TemplateArgs args;
  if (GetTemplateArgs(0, &args)) {
    for (TemplateArgs::iterator it = args.begin(); it != args.end(); ++it) {
      if (CXXRecordDecl* decl = (*it)->getAsCXXRecordDecl())
        if (decl->getName() == kHeapAllocatorName)
          return true;
    }
  }

  return Config::IsGCCollection(name_);
}

static bool IsGCBaseCallback(const CXXBaseSpecifier* specifier,
                             CXXBasePath& path,
                             void* data) {
  if (CXXRecordDecl* record = specifier->getType()->getAsCXXRecordDecl())
    return Config::IsGCBase(record->getName());
  return false;
}

// Test if a record is derived from a garbage collected base.
bool RecordInfo::IsGCDerived() {
  // If already computed, return the known result.
  if (base_paths_)
    return is_gc_derived_;

  base_paths_ = new CXXBasePaths(true, true, false);

  if (!record_->hasDefinition())
    return false;

  // The base classes are not themselves considered garbage collected objects.
  if (Config::IsGCBase(name_))
    return false;

  // Walk the inheritance tree to find GC base classes.
  is_gc_derived_ = record_->lookupInBases(IsGCBaseCallback, 0, *base_paths_);
  return is_gc_derived_;
}

bool RecordInfo::IsGCFinalized() {
  if (!IsGCDerived())
    return false;
  for (CXXBasePaths::paths_iterator it = base_paths_->begin();
       it != base_paths_->end();
       ++it) {
    const CXXBasePathElement& elem = (*it)[it->size() - 1];
    CXXRecordDecl* base = elem.Base->getType()->getAsCXXRecordDecl();
    if (Config::IsGCFinalizedBase(base->getName()))
      return true;
  }
  return false;
}

// A GC mixin is a class that inherits from a GC mixin base and has
// not yet been "mixed in" with another GC base class.
bool RecordInfo::IsGCMixin() {
  if (!IsGCDerived() || base_paths_->begin() == base_paths_->end())
    return false;
  for (CXXBasePaths::paths_iterator it = base_paths_->begin();
       it != base_paths_->end();
       ++it) {
      // Get the last element of the path.
      const CXXBasePathElement& elem = (*it)[it->size() - 1];
      CXXRecordDecl* base = elem.Base->getType()->getAsCXXRecordDecl();
      // If it is not a mixin base we are done.
      if (!Config::IsGCMixinBase(base->getName()))
          return false;
  }
  // This is a mixin if all GC bases are mixins.
  return true;
}

// Test if a record is allocated on the managed heap.
bool RecordInfo::IsGCAllocated() {
  return IsGCDerived() || IsHeapAllocatedCollection();
}

RecordInfo* RecordCache::Lookup(CXXRecordDecl* record) {
  // Ignore classes annotated with the GC_PLUGIN_IGNORE macro.
  if (!record || Config::IsIgnoreAnnotated(record))
    return 0;
  Cache::iterator it = cache_.find(record);
  if (it != cache_.end())
    return &it->second;
  return &cache_.insert(std::make_pair(record, RecordInfo(record, this)))
              .first->second;
}

bool RecordInfo::IsStackAllocated() {
  if (is_stack_allocated_ == kNotComputed) {
    is_stack_allocated_ = kFalse;
    for (Bases::iterator it = GetBases().begin();
         it != GetBases().end();
         ++it) {
      if (it->second.info()->IsStackAllocated()) {
        is_stack_allocated_ = kTrue;
        return is_stack_allocated_;
      }
    }
    for (CXXRecordDecl::method_iterator it = record_->method_begin();
         it != record_->method_end();
         ++it) {
      if (it->getNameAsString() == kNewOperatorName &&
          it->isDeleted() &&
          Config::IsStackAnnotated(*it)) {
        is_stack_allocated_ = kTrue;
        return is_stack_allocated_;
      }
    }
  }
  return is_stack_allocated_;
}

bool RecordInfo::IsNonNewable() {
  if (is_non_newable_ == kNotComputed) {
    bool deleted = false;
    bool all_deleted = true;
    for (CXXRecordDecl::method_iterator it = record_->method_begin();
         it != record_->method_end();
         ++it) {
      if (it->getNameAsString() == kNewOperatorName) {
        deleted = it->isDeleted();
        all_deleted = all_deleted && deleted;
      }
    }
    is_non_newable_ = (deleted && all_deleted) ? kTrue : kFalse;
  }
  return is_non_newable_;
}

bool RecordInfo::IsOnlyPlacementNewable() {
  if (is_only_placement_newable_ == kNotComputed) {
    bool placement = false;
    bool new_deleted = false;
    for (CXXRecordDecl::method_iterator it = record_->method_begin();
         it != record_->method_end();
         ++it) {
      if (it->getNameAsString() == kNewOperatorName) {
        if (it->getNumParams() == 1) {
          new_deleted = it->isDeleted();
        } else if (it->getNumParams() == 2) {
          placement = !it->isDeleted();
        }
      }
    }
    is_only_placement_newable_ = (placement && new_deleted) ? kTrue : kFalse;
  }
  return is_only_placement_newable_;
}

CXXMethodDecl* RecordInfo::DeclaresNewOperator() {
  for (CXXRecordDecl::method_iterator it = record_->method_begin();
       it != record_->method_end();
       ++it) {
    if (it->getNameAsString() == kNewOperatorName && it->getNumParams() == 1)
      return *it;
  }
  return 0;
}

// An object requires a tracing method if it has any fields that need tracing
// or if it inherits from multiple bases that need tracing.
bool RecordInfo::RequiresTraceMethod() {
  if (IsStackAllocated())
    return false;
  unsigned bases_with_trace = 0;
  for (Bases::iterator it = GetBases().begin(); it != GetBases().end(); ++it) {
    if (it->second.NeedsTracing().IsNeeded())
      ++bases_with_trace;
  }
  if (bases_with_trace > 1)
    return true;
  GetFields();
  return fields_need_tracing_.IsNeeded();
}

// Get the actual tracing method (ie, can be traceAfterDispatch if there is a
// dispatch method).
CXXMethodDecl* RecordInfo::GetTraceMethod() {
  DetermineTracingMethods();
  return trace_method_;
}

// Get the static trace dispatch method.
CXXMethodDecl* RecordInfo::GetTraceDispatchMethod() {
  DetermineTracingMethods();
  return trace_dispatch_method_;
}

CXXMethodDecl* RecordInfo::GetFinalizeDispatchMethod() {
  DetermineTracingMethods();
  return finalize_dispatch_method_;
}

RecordInfo::Bases& RecordInfo::GetBases() {
  if (!bases_)
    bases_ = CollectBases();
  return *bases_;
}

bool RecordInfo::InheritsTrace() {
  if (GetTraceMethod())
    return true;
  for (Bases::iterator it = GetBases().begin(); it != GetBases().end(); ++it) {
    if (it->second.info()->InheritsTrace())
      return true;
  }
  return false;
}

CXXMethodDecl* RecordInfo::InheritsNonVirtualTrace() {
  if (CXXMethodDecl* trace = GetTraceMethod())
    return trace->isVirtual() ? 0 : trace;
  for (Bases::iterator it = GetBases().begin(); it != GetBases().end(); ++it) {
    if (CXXMethodDecl* trace = it->second.info()->InheritsNonVirtualTrace())
      return trace;
  }
  return 0;
}

// A (non-virtual) class is considered abstract in Blink if it has
// no public constructors and no create methods.
bool RecordInfo::IsConsideredAbstract() {
  for (CXXRecordDecl::ctor_iterator it = record_->ctor_begin();
       it != record_->ctor_end();
       ++it) {
    if (!it->isCopyOrMoveConstructor() && it->getAccess() == AS_public)
      return false;
  }
  for (CXXRecordDecl::method_iterator it = record_->method_begin();
       it != record_->method_end();
       ++it) {
    if (it->getNameAsString() == kCreateName)
      return false;
  }
  return true;
}

RecordInfo::Bases* RecordInfo::CollectBases() {
  // Compute the collection locally to avoid inconsistent states.
  Bases* bases = new Bases;
  if (!record_->hasDefinition())
    return bases;
  for (CXXRecordDecl::base_class_iterator it = record_->bases_begin();
       it != record_->bases_end();
       ++it) {
    const CXXBaseSpecifier& spec = *it;
    RecordInfo* info = cache_->Lookup(spec.getType());
    if (!info)
      continue;
    CXXRecordDecl* base = info->record();
    TracingStatus status = info->InheritsTrace()
                               ? TracingStatus::Needed()
                               : TracingStatus::Unneeded();
    bases->insert(std::make_pair(base, BasePoint(spec, info, status)));
  }
  return bases;
}

RecordInfo::Fields& RecordInfo::GetFields() {
  if (!fields_)
    fields_ = CollectFields();
  return *fields_;
}

RecordInfo::Fields* RecordInfo::CollectFields() {
  // Compute the collection locally to avoid inconsistent states.
  Fields* fields = new Fields;
  if (!record_->hasDefinition())
    return fields;
  TracingStatus fields_status = TracingStatus::Unneeded();
  for (RecordDecl::field_iterator it = record_->field_begin();
       it != record_->field_end();
       ++it) {
    FieldDecl* field = *it;
    // Ignore fields annotated with the GC_PLUGIN_IGNORE macro.
    if (Config::IsIgnoreAnnotated(field))
      continue;
    if (Edge* edge = CreateEdge(field->getType().getTypePtrOrNull())) {
      fields_status = fields_status.LUB(edge->NeedsTracing(Edge::kRecursive));
      fields->insert(std::make_pair(field, FieldPoint(field, edge)));
    }
  }
  fields_need_tracing_ = fields_status;
  return fields;
}

void RecordInfo::DetermineTracingMethods() {
  if (determined_trace_methods_)
    return;
  determined_trace_methods_ = true;
  if (Config::IsGCBase(name_))
    return;
  CXXMethodDecl* trace = 0;
  CXXMethodDecl* traceAfterDispatch = 0;
  bool isTraceAfterDispatch;
  for (CXXRecordDecl::method_iterator it = record_->method_begin();
       it != record_->method_end();
       ++it) {
    if (Config::IsTraceMethod(*it, &isTraceAfterDispatch)) {
      if (isTraceAfterDispatch) {
        traceAfterDispatch = *it;
      } else {
        trace = *it;
      }
    } else if (it->getNameAsString() == kFinalizeName) {
      finalize_dispatch_method_ = *it;
    }
  }
  if (traceAfterDispatch) {
    trace_method_ = traceAfterDispatch;
    trace_dispatch_method_ = trace;
  } else {
    // TODO: Can we never have a dispatch method called trace without the same
    // class defining a traceAfterDispatch method?
    trace_method_ = trace;
    trace_dispatch_method_ = 0;
  }
  if (trace_dispatch_method_ && finalize_dispatch_method_)
    return;
  // If this class does not define dispatching methods inherit them.
  for (Bases::iterator it = GetBases().begin(); it != GetBases().end(); ++it) {
    // TODO: Does it make sense to inherit multiple dispatch methods?
    if (CXXMethodDecl* dispatch = it->second.info()->GetTraceDispatchMethod()) {
      assert(!trace_dispatch_method_ && "Multiple trace dispatching methods");
      trace_dispatch_method_ = dispatch;
    }
    if (CXXMethodDecl* dispatch =
            it->second.info()->GetFinalizeDispatchMethod()) {
      assert(!finalize_dispatch_method_ &&
             "Multiple finalize dispatching methods");
      finalize_dispatch_method_ = dispatch;
    }
  }
}

// TODO: Add classes with a finalize() method that specialize FinalizerTrait.
bool RecordInfo::NeedsFinalization() {
  return record_->hasNonTrivialDestructor();
}

// A class needs tracing if:
// - it is allocated on the managed heap,
// - it is derived from a class that needs tracing, or
// - it contains fields that need tracing.
// TODO: Defining NeedsTracing based on whether a class defines a trace method
// (of the proper signature) over approximates too much. The use of transition
// types causes some classes to have trace methods without them needing to be
// traced.
TracingStatus RecordInfo::NeedsTracing(Edge::NeedsTracingOption option) {
  if (IsGCAllocated())
    return TracingStatus::Needed();

  if (IsStackAllocated())
    return TracingStatus::Unneeded();

  for (Bases::iterator it = GetBases().begin(); it != GetBases().end(); ++it) {
    if (it->second.info()->NeedsTracing(option).IsNeeded())
      return TracingStatus::Needed();
  }

  if (option == Edge::kRecursive)
    GetFields();

  return fields_need_tracing_;
}

Edge* RecordInfo::CreateEdge(const Type* type) {
  if (!type) {
    return 0;
  }

  if (type->isPointerType()) {
    if (Edge* ptr = CreateEdge(type->getPointeeType().getTypePtrOrNull()))
      return new RawPtr(ptr, false);
    return 0;
  }

  RecordInfo* info = cache_->Lookup(type);

  // If the type is neither a pointer or a C++ record we ignore it.
  if (!info) {
    return 0;
  }

  TemplateArgs args;

  if (Config::IsRawPtr(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new RawPtr(ptr, true);
    return 0;
  }

  if (Config::IsRefPtr(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new RefPtr(ptr);
    return 0;
  }

  if (Config::IsOwnPtr(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new OwnPtr(ptr);
    return 0;
  }

  if (Config::IsMember(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new Member(ptr);
    return 0;
  }

  if (Config::IsWeakMember(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new WeakMember(ptr);
    return 0;
  }

  if (Config::IsPersistent(info->name())) {
    // Persistent might refer to v8::Persistent, so check the name space.
    // TODO: Consider using a more canonical identification than names.
    NamespaceDecl* ns =
        dyn_cast<NamespaceDecl>(info->record()->getDeclContext());
    if (!ns || ns->getName() != "WebCore")
      return 0;
    if (!info->GetTemplateArgs(1, &args))
      return 0;
    if (Edge* ptr = CreateEdge(args[0]))
      return new Persistent(ptr);
    return 0;
  }

  if (Config::IsGCCollection(info->name()) ||
      Config::IsWTFCollection(info->name())) {
    bool is_root = Config::IsPersistentGCCollection(info->name());
    bool on_heap = is_root || info->IsHeapAllocatedCollection();
    size_t count = Config::CollectionDimension(info->name());
    if (!info->GetTemplateArgs(count, &args))
      return 0;
    Collection* edge = new Collection(info, on_heap, is_root);
    for (TemplateArgs::iterator it = args.begin(); it != args.end(); ++it) {
      if (Edge* member = CreateEdge(*it)) {
        edge->members().push_back(member);
      }
      // TODO: Handle the case where we fail to create an edge (eg, if the
      // argument is a primitive type or just not fully known yet).
    }
    return edge;
  }

  return new Value(info);
}
