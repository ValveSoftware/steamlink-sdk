// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This clang plugin checks various invariants of the Blink garbage
// collection infrastructure.
//
// Errors are described at:
// http://www.chromium.org/developers/blink-gc-plugin-errors

#include "Config.h"
#include "JsonWriter.h"
#include "RecordInfo.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

using namespace clang;
using std::string;

namespace {

const char kClassMustLeftMostlyDeriveGC[] =
    "[blink-gc] Class %0 must derive its GC base in the left-most position.";

const char kClassRequiresTraceMethod[] =
    "[blink-gc] Class %0 requires a trace method.";

const char kBaseRequiresTracing[] =
    "[blink-gc] Base class %0 of derived class %1 requires tracing.";

const char kBaseRequiresTracingNote[] =
    "[blink-gc] Untraced base class %0 declared here:";

const char kFieldsRequireTracing[] =
    "[blink-gc] Class %0 has untraced fields that require tracing.";

const char kFieldRequiresTracingNote[] =
    "[blink-gc] Untraced field %0 declared here:";

const char kClassContainsInvalidFields[] =
    "[blink-gc] Class %0 contains invalid fields.";

const char kClassContainsGCRoot[] =
    "[blink-gc] Class %0 contains GC root in field %1.";

const char kClassRequiresFinalization[] =
    "[blink-gc] Class %0 requires finalization.";

const char kFinalizerAccessesFinalizedField[] =
    "[blink-gc] Finalizer %0 accesses potentially finalized field %1.";

const char kRawPtrToGCManagedClassNote[] =
    "[blink-gc] Raw pointer field %0 to a GC managed class declared here:";

const char kRefPtrToGCManagedClassNote[] =
    "[blink-gc] RefPtr field %0 to a GC managed class declared here:";

const char kOwnPtrToGCManagedClassNote[] =
    "[blink-gc] OwnPtr field %0 to a GC managed class declared here:";

const char kStackAllocatedFieldNote[] =
    "[blink-gc] Stack-allocated field %0 declared here:";

const char kMemberInUnmanagedClassNote[] =
    "[blink-gc] Member field %0 in unmanaged class declared here:";

const char kPartObjectToGCDerivedClassNote[] =
    "[blink-gc] Part-object field %0 to a GC derived class declared here:";

const char kPartObjectContainsGCRootNote[] =
    "[blink-gc] Field %0 with embedded GC root in %1 declared here:";

const char kFieldContainsGCRootNote[] =
    "[blink-gc] Field %0 defining a GC root declared here:";

const char kOverriddenNonVirtualTrace[] =
    "[blink-gc] Class %0 overrides non-virtual trace of base class %1.";

const char kOverriddenNonVirtualTraceNote[] =
    "[blink-gc] Non-virtual trace method declared here:";

const char kMissingTraceDispatchMethod[] =
    "[blink-gc] Class %0 is missing manual trace dispatch.";

const char kMissingFinalizeDispatchMethod[] =
    "[blink-gc] Class %0 is missing manual finalize dispatch.";

const char kVirtualAndManualDispatch[] =
    "[blink-gc] Class %0 contains or inherits virtual methods"
    " but implements manual dispatching.";

const char kMissingTraceDispatch[] =
    "[blink-gc] Missing dispatch to class %0 in manual trace dispatch.";

const char kMissingFinalizeDispatch[] =
    "[blink-gc] Missing dispatch to class %0 in manual finalize dispatch.";

const char kFinalizedFieldNote[] =
    "[blink-gc] Potentially finalized field %0 declared here:";

const char kUserDeclaredDestructorNote[] =
    "[blink-gc] User-declared destructor declared here:";

const char kUserDeclaredFinalizerNote[] =
    "[blink-gc] User-declared finalizer declared here:";

const char kBaseRequiresFinalizationNote[] =
    "[blink-gc] Base class %0 requiring finalization declared here:";

const char kFieldRequiresFinalizationNote[] =
    "[blink-gc] Field %0 requiring finalization declared here:";

const char kManualDispatchMethodNote[] =
    "[blink-gc] Manual dispatch %0 declared here:";

const char kDerivesNonStackAllocated[] =
    "[blink-gc] Stack-allocated class %0 derives class %1"
    " which is not stack allocated.";

const char kClassOverridesNew[] =
    "[blink-gc] Garbage collected class %0"
    " is not permitted to override its new operator.";

const char kClassDeclaresPureVirtualTrace[] =
    "[blink-gc] Garbage collected class %0"
    " is not permitted to declare a pure-virtual trace method.";

struct BlinkGCPluginOptions {
  BlinkGCPluginOptions() : enable_oilpan(false), dump_graph(false) {}
  bool enable_oilpan;
  bool dump_graph;
  std::set<std::string> ignored_classes;
  std::set<std::string> checked_namespaces;
  std::vector<std::string> ignored_directories;
};

typedef std::vector<CXXRecordDecl*> RecordVector;
typedef std::vector<CXXMethodDecl*> MethodVector;

// Test if a template specialization is an instantiation.
static bool IsTemplateInstantiation(CXXRecordDecl* record) {
  ClassTemplateSpecializationDecl* spec =
      dyn_cast<ClassTemplateSpecializationDecl>(record);
  if (!spec)
    return false;
  switch (spec->getTemplateSpecializationKind()) {
    case TSK_ImplicitInstantiation:
    case TSK_ExplicitInstantiationDefinition:
      return true;
    case TSK_Undeclared:
    case TSK_ExplicitSpecialization:
      return false;
    // TODO: unsupported cases.
    case TSK_ExplicitInstantiationDeclaration:
      return false;
  }
  assert(false && "Unknown template specialization kind");
}

// This visitor collects the entry points for the checker.
class CollectVisitor : public RecursiveASTVisitor<CollectVisitor> {
 public:
  CollectVisitor() {}

  RecordVector& record_decls() { return record_decls_; }
  MethodVector& trace_decls() { return trace_decls_; }

  bool shouldVisitTemplateInstantiations() { return false; }

  // Collect record declarations, including nested declarations.
  bool VisitCXXRecordDecl(CXXRecordDecl* record) {
    if (record->hasDefinition() && record->isCompleteDefinition())
      record_decls_.push_back(record);
    return true;
  }

  // Collect tracing method definitions, but don't traverse method bodies.
  bool TraverseCXXMethodDecl(CXXMethodDecl* method) {
    if (method->isThisDeclarationADefinition() && Config::IsTraceMethod(method))
      trace_decls_.push_back(method);
    return true;
  }

 private:
  RecordVector record_decls_;
  MethodVector trace_decls_;
};

// This visitor checks that a finalizer method does not have invalid access to
// fields that are potentially finalized. A potentially finalized field is
// either a Member, a heap-allocated collection or an off-heap collection that
// contains Members.  Invalid uses are currently identified as passing the field
// as the argument of a procedure call or using the -> or [] operators on it.
class CheckFinalizerVisitor
    : public RecursiveASTVisitor<CheckFinalizerVisitor> {
 private:
  // Simple visitor to determine if the content of a field might be collected
  // during finalization.
  class MightBeCollectedVisitor : public EdgeVisitor {
   public:
    MightBeCollectedVisitor() : might_be_collected_(false) {}
    bool might_be_collected() { return might_be_collected_; }
    void VisitMember(Member* edge) override { might_be_collected_ = true; }
    void VisitCollection(Collection* edge) override {
      if (edge->on_heap()) {
        might_be_collected_ = !edge->is_root();
      } else {
        edge->AcceptMembers(this);
      }
    }

   private:
    bool might_be_collected_;
  };

 public:
  typedef std::vector<std::pair<MemberExpr*, FieldPoint*> > Errors;

  CheckFinalizerVisitor(RecordCache* cache)
      : blacklist_context_(false), cache_(cache) {}

  Errors& finalized_fields() { return finalized_fields_; }

  bool WalkUpFromCXXOperatorCallExpr(CXXOperatorCallExpr* expr) {
    // Only continue the walk-up if the operator is a blacklisted one.
    switch (expr->getOperator()) {
      case OO_Arrow:
      case OO_Subscript:
        this->WalkUpFromCallExpr(expr);
      default:
        return true;
    }
  }

  // We consider all non-operator calls to be blacklisted contexts.
  bool WalkUpFromCallExpr(CallExpr* expr) {
    bool prev_blacklist_context = blacklist_context_;
    blacklist_context_ = true;
    for (size_t i = 0; i < expr->getNumArgs(); ++i)
      this->TraverseStmt(expr->getArg(i));
    blacklist_context_ = prev_blacklist_context;
    return true;
  }

  bool VisitMemberExpr(MemberExpr* member) {
    FieldDecl* field = dyn_cast<FieldDecl>(member->getMemberDecl());
    if (!field)
      return true;

    RecordInfo* info = cache_->Lookup(field->getParent());
    if (!info)
      return true;

    RecordInfo::Fields::iterator it = info->GetFields().find(field);
    if (it == info->GetFields().end())
      return true;

    if (blacklist_context_ && MightBeCollected(&it->second))
      finalized_fields_.push_back(std::make_pair(member, &it->second));
    return true;
  }

  bool MightBeCollected(FieldPoint* point) {
    MightBeCollectedVisitor visitor;
    point->edge()->Accept(&visitor);
    return visitor.might_be_collected();
  }

 private:
  bool blacklist_context_;
  Errors finalized_fields_;
  RecordCache* cache_;
};

// This visitor checks that a method contains within its body, a call to a
// method on the provided receiver class. This is used to check manual
// dispatching for trace and finalize methods.
class CheckDispatchVisitor : public RecursiveASTVisitor<CheckDispatchVisitor> {
 public:
  CheckDispatchVisitor(RecordInfo* receiver)
      : receiver_(receiver), dispatched_to_receiver_(false) {}

  bool dispatched_to_receiver() { return dispatched_to_receiver_; }

  bool VisitMemberExpr(MemberExpr* member) {
    if (CXXMethodDecl* fn = dyn_cast<CXXMethodDecl>(member->getMemberDecl())) {
      if (fn->getParent() == receiver_->record())
        dispatched_to_receiver_ = true;
    }
    return true;
  }

 private:
  RecordInfo* receiver_;
  bool dispatched_to_receiver_;
};

// This visitor checks a tracing method by traversing its body.
// - A member field is considered traced if it is referenced in the body.
// - A base is traced if a base-qualified call to a trace method is found.
class CheckTraceVisitor : public RecursiveASTVisitor<CheckTraceVisitor> {
 public:
  CheckTraceVisitor(CXXMethodDecl* trace, RecordInfo* info)
      : trace_(trace), info_(info) {}

  // Allow recursive traversal by using VisitMemberExpr.
  bool VisitMemberExpr(MemberExpr* member) {
    // If this member expression references a field decl, mark it as traced.
    if (FieldDecl* field = dyn_cast<FieldDecl>(member->getMemberDecl())) {
      if (IsTemplateInstantiation(info_->record())) {
        // Pointer equality on fields does not work for template instantiations.
        // The trace method refers to fields of the template definition which
        // are different from the instantiated fields that need to be traced.
        const string& name = field->getNameAsString();
        for (RecordInfo::Fields::iterator it = info_->GetFields().begin();
             it != info_->GetFields().end();
             ++it) {
          if (it->first->getNameAsString() == name) {
            MarkTraced(it);
            break;
          }
        }
      } else {
        RecordInfo::Fields::iterator it = info_->GetFields().find(field);
        if (it != info_->GetFields().end())
          MarkTraced(it);
      }
      return true;
    }

    // If this is a weak callback function we only check field tracing.
    if (IsWeakCallback())
      return true;

    // For method calls, check tracing of bases and other special GC methods.
    if (CXXMethodDecl* fn = dyn_cast<CXXMethodDecl>(member->getMemberDecl())) {
      const string& name = fn->getNameAsString();
      // Check weak callbacks.
      if (name == kRegisterWeakMembersName) {
        if (fn->isTemplateInstantiation()) {
          const TemplateArgumentList& args =
              *fn->getTemplateSpecializationInfo()->TemplateArguments;
          // The second template argument is the callback method.
          if (args.size() > 1 &&
              args[1].getKind() == TemplateArgument::Declaration) {
            if (FunctionDecl* callback =
                    dyn_cast<FunctionDecl>(args[1].getAsDecl())) {
              if (callback->hasBody()) {
                CheckTraceVisitor nested_visitor(info_);
                nested_visitor.TraverseStmt(callback->getBody());
              }
            }
          }
        }
        return true;
      }

      // Currently, a manually dispatched class cannot have mixin bases (having
      // one would add a vtable which we explicitly check against). This means
      // that we can only make calls to a trace method of the same name. Revisit
      // this if our mixin/vtable assumption changes.
      if (Config::IsTraceMethod(fn) &&
          fn->getName() == trace_->getName() &&
          member->hasQualifier()) {
        if (const Type* type = member->getQualifier()->getAsType()) {
          if (CXXRecordDecl* decl = type->getAsCXXRecordDecl()) {
            RecordInfo::Bases::iterator it = info_->GetBases().find(decl);
            if (it != info_->GetBases().end())
              it->second.MarkTraced();
          }
        }
      }
    }
    return true;
  }

 private:
  // Nested checking for weak callbacks.
  CheckTraceVisitor(RecordInfo* info) : trace_(0), info_(info) {}

  bool IsWeakCallback() { return !trace_; }

  void MarkTraced(RecordInfo::Fields::iterator it) {
    // In a weak callback we can't mark strong fields as traced.
    if (IsWeakCallback() && !it->second.edge()->IsWeakMember())
      return;
    it->second.MarkTraced();
  }

  CXXMethodDecl* trace_;
  RecordInfo* info_;
};

// This visitor checks that the fields of a class and the fields of
// its part objects don't define GC roots.
class CheckGCRootsVisitor : public RecursiveEdgeVisitor {
 public:
  typedef std::vector<FieldPoint*> RootPath;
  typedef std::vector<RootPath> Errors;

  CheckGCRootsVisitor() {}

  Errors& gc_roots() { return gc_roots_; }

  bool ContainsGCRoots(RecordInfo* info) {
    for (RecordInfo::Fields::iterator it = info->GetFields().begin();
         it != info->GetFields().end();
         ++it) {
      current_.push_back(&it->second);
      it->second.edge()->Accept(this);
      current_.pop_back();
    }
    return !gc_roots_.empty();
  }

  void VisitValue(Value* edge) override {
    // TODO: what should we do to check unions?
    if (edge->value()->record()->isUnion())
      return;

    // If the value is a part object, then continue checking for roots.
    for (Context::iterator it = context().begin();
         it != context().end();
         ++it) {
      if (!(*it)->IsCollection())
        return;
    }
    ContainsGCRoots(edge->value());
  }

  void VisitPersistent(Persistent* edge) override {
    gc_roots_.push_back(current_);
  }

  void AtCollection(Collection* edge) override {
    if (edge->is_root())
      gc_roots_.push_back(current_);
  }

 protected:
  RootPath current_;
  Errors gc_roots_;
};

// This visitor checks that the fields of a class are "well formed".
// - OwnPtr, RefPtr and RawPtr must not point to a GC derived types.
// - Part objects must not be GC derived types.
// - An on-heap class must never contain GC roots.
// - Only stack-allocated types may point to stack-allocated types.
class CheckFieldsVisitor : public RecursiveEdgeVisitor {
 public:

  enum Error {
    kRawPtrToGCManaged,
    kRefPtrToGCManaged,
    kOwnPtrToGCManaged,
    kMemberInUnmanaged,
    kPtrFromHeapToStack,
    kGCDerivedPartObject
  };

  typedef std::vector<std::pair<FieldPoint*, Error> > Errors;

  CheckFieldsVisitor(const BlinkGCPluginOptions& options)
      : options_(options), current_(0), stack_allocated_host_(false) {}

  Errors& invalid_fields() { return invalid_fields_; }

  bool ContainsInvalidFields(RecordInfo* info) {
    stack_allocated_host_ = info->IsStackAllocated();
    managed_host_ = stack_allocated_host_ ||
                    info->IsGCAllocated() ||
                    info->IsNonNewable() ||
                    info->IsOnlyPlacementNewable();
    for (RecordInfo::Fields::iterator it = info->GetFields().begin();
         it != info->GetFields().end();
         ++it) {
      context().clear();
      current_ = &it->second;
      current_->edge()->Accept(this);
    }
    return !invalid_fields_.empty();
  }

  void AtMember(Member* edge) override {
    if (managed_host_)
      return;
    // A member is allowed to appear in the context of a root.
    for (Context::iterator it = context().begin();
         it != context().end();
         ++it) {
      if ((*it)->Kind() == Edge::kRoot)
        return;
    }
    invalid_fields_.push_back(std::make_pair(current_, kMemberInUnmanaged));
  }

  void AtValue(Value* edge) override {
    // TODO: what should we do to check unions?
    if (edge->value()->record()->isUnion())
      return;

    if (!stack_allocated_host_ && edge->value()->IsStackAllocated()) {
      invalid_fields_.push_back(std::make_pair(current_, kPtrFromHeapToStack));
      return;
    }

    if (!Parent() &&
        edge->value()->IsGCDerived() &&
        !edge->value()->IsGCMixin()) {
      invalid_fields_.push_back(std::make_pair(current_, kGCDerivedPartObject));
      return;
    }

    if (!Parent() || !edge->value()->IsGCAllocated())
      return;

    // In transition mode, disallow  OwnPtr<T>, RawPtr<T> to GC allocated T's,
    // also disallow T* in stack-allocated types.
    if (options_.enable_oilpan) {
      if (Parent()->IsOwnPtr() ||
          Parent()->IsRawPtrClass() ||
          (stack_allocated_host_ && Parent()->IsRawPtr())) {
        invalid_fields_.push_back(std::make_pair(
            current_, InvalidSmartPtr(Parent())));
        return;
      }

      return;
    }

    if (Parent()->IsRawPtr() || Parent()->IsRefPtr() || Parent()->IsOwnPtr()) {
      invalid_fields_.push_back(std::make_pair(
          current_, InvalidSmartPtr(Parent())));
      return;
    }
  }

  void AtCollection(Collection* edge) override {
    if (edge->on_heap() && Parent() && Parent()->IsOwnPtr())
      invalid_fields_.push_back(std::make_pair(current_, kOwnPtrToGCManaged));
  }

 private:
  Error InvalidSmartPtr(Edge* ptr) {
    if (ptr->IsRawPtr())
      return kRawPtrToGCManaged;
    if (ptr->IsRefPtr())
      return kRefPtrToGCManaged;
    if (ptr->IsOwnPtr())
      return kOwnPtrToGCManaged;
    assert(false && "Unknown smart pointer kind");
  }

  const BlinkGCPluginOptions& options_;
  FieldPoint* current_;
  bool stack_allocated_host_;
  bool managed_host_;
  Errors invalid_fields_;
};

// Main class containing checks for various invariants of the Blink
// garbage collection infrastructure.
class BlinkGCPluginConsumer : public ASTConsumer {
 public:
  BlinkGCPluginConsumer(CompilerInstance& instance,
                        const BlinkGCPluginOptions& options)
      : instance_(instance),
        diagnostic_(instance.getDiagnostics()),
        options_(options),
        json_(0) {

    // Only check structures in the blink, WebCore and WebKit namespaces.
    options_.checked_namespaces.insert("blink");
    options_.checked_namespaces.insert("WebCore");
    options_.checked_namespaces.insert("WebKit");

    // Ignore GC implementation files.
    options_.ignored_directories.push_back("/heap/");

    // Register warning/error messages.
    diag_class_must_left_mostly_derive_gc_ = diagnostic_.getCustomDiagID(
        getErrorLevel(), kClassMustLeftMostlyDeriveGC);
    diag_class_requires_trace_method_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kClassRequiresTraceMethod);
    diag_base_requires_tracing_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kBaseRequiresTracing);
    diag_fields_require_tracing_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kFieldsRequireTracing);
    diag_class_contains_invalid_fields_ = diagnostic_.getCustomDiagID(
        getErrorLevel(), kClassContainsInvalidFields);
    diag_class_contains_gc_root_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kClassContainsGCRoot);
    diag_class_requires_finalization_ = diagnostic_.getCustomDiagID(
        getErrorLevel(), kClassRequiresFinalization);
    diag_finalizer_accesses_finalized_field_ = diagnostic_.getCustomDiagID(
        getErrorLevel(), kFinalizerAccessesFinalizedField);
    diag_overridden_non_virtual_trace_ = diagnostic_.getCustomDiagID(
        getErrorLevel(), kOverriddenNonVirtualTrace);
    diag_missing_trace_dispatch_method_ = diagnostic_.getCustomDiagID(
        getErrorLevel(), kMissingTraceDispatchMethod);
    diag_missing_finalize_dispatch_method_ = diagnostic_.getCustomDiagID(
        getErrorLevel(), kMissingFinalizeDispatchMethod);
    diag_virtual_and_manual_dispatch_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kVirtualAndManualDispatch);
    diag_missing_trace_dispatch_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kMissingTraceDispatch);
    diag_missing_finalize_dispatch_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kMissingFinalizeDispatch);
    diag_derives_non_stack_allocated_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kDerivesNonStackAllocated);
    diag_class_overrides_new_ =
        diagnostic_.getCustomDiagID(getErrorLevel(), kClassOverridesNew);
    diag_class_declares_pure_virtual_trace_ = diagnostic_.getCustomDiagID(
        getErrorLevel(), kClassDeclaresPureVirtualTrace);

    // Register note messages.
    diag_base_requires_tracing_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kBaseRequiresTracingNote);
    diag_field_requires_tracing_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kFieldRequiresTracingNote);
    diag_raw_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kRawPtrToGCManagedClassNote);
    diag_ref_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kRefPtrToGCManagedClassNote);
    diag_own_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kOwnPtrToGCManagedClassNote);
    diag_stack_allocated_field_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kStackAllocatedFieldNote);
    diag_member_in_unmanaged_class_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kMemberInUnmanagedClassNote);
    diag_part_object_to_gc_derived_class_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kPartObjectToGCDerivedClassNote);
    diag_part_object_contains_gc_root_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kPartObjectContainsGCRootNote);
    diag_field_contains_gc_root_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kFieldContainsGCRootNote);
    diag_finalized_field_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kFinalizedFieldNote);
    diag_user_declared_destructor_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kUserDeclaredDestructorNote);
    diag_user_declared_finalizer_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kUserDeclaredFinalizerNote);
    diag_base_requires_finalization_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kBaseRequiresFinalizationNote);
    diag_field_requires_finalization_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kFieldRequiresFinalizationNote);
    diag_overridden_non_virtual_trace_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kOverriddenNonVirtualTraceNote);
    diag_manual_dispatch_method_note_ = diagnostic_.getCustomDiagID(
        DiagnosticsEngine::Note, kManualDispatchMethodNote);
  }

  void HandleTranslationUnit(ASTContext& context) override {
    CollectVisitor visitor;
    visitor.TraverseDecl(context.getTranslationUnitDecl());

    if (options_.dump_graph) {
      string err;
      // TODO: Make createDefaultOutputFile or a shorter createOutputFile work.
      json_ = JsonWriter::from(instance_.createOutputFile(
          "",                                      // OutputPath
          err,                                     // Errors
          true,                                    // Binary
          true,                                    // RemoveFileOnSignal
          instance_.getFrontendOpts().OutputFile,  // BaseInput
          "graph.json",                            // Extension
          false,                                   // UseTemporary
          false,                                   // CreateMissingDirectories
          0,                                       // ResultPathName
          0));                                     // TempPathName
      if (err.empty() && json_) {
        json_->OpenList();
      } else {
        json_ = 0;
        llvm::errs()
            << "[blink-gc] "
            << "Failed to create an output file for the object graph.\n";
      }
    }

    for (RecordVector::iterator it = visitor.record_decls().begin();
         it != visitor.record_decls().end();
         ++it) {
      CheckRecord(cache_.Lookup(*it));
    }

    for (MethodVector::iterator it = visitor.trace_decls().begin();
         it != visitor.trace_decls().end();
         ++it) {
      CheckTracingMethod(*it);
    }

    if (json_) {
      json_->CloseList();
      delete json_;
      json_ = 0;
    }
  }

  // Main entry for checking a record declaration.
  void CheckRecord(RecordInfo* info) {
    if (IsIgnored(info))
      return;

    CXXRecordDecl* record = info->record();

    // TODO: what should we do to check unions?
    if (record->isUnion())
      return;

    // If this is the primary template declaration, check its specializations.
    if (record->isThisDeclarationADefinition() &&
        record->getDescribedClassTemplate()) {
      ClassTemplateDecl* tmpl = record->getDescribedClassTemplate();
      for (ClassTemplateDecl::spec_iterator it = tmpl->spec_begin();
           it != tmpl->spec_end();
           ++it) {
        CheckClass(cache_.Lookup(*it));
      }
      return;
    }

    CheckClass(info);
  }

  // Check a class-like object (eg, class, specialization, instantiation).
  void CheckClass(RecordInfo* info) {
    if (!info)
      return;

    // Check consistency of stack-allocated hierarchies.
    if (info->IsStackAllocated()) {
      for (RecordInfo::Bases::iterator it = info->GetBases().begin();
           it != info->GetBases().end();
           ++it) {
        if (!it->second.info()->IsStackAllocated())
          ReportDerivesNonStackAllocated(info, &it->second);
      }
    }

    if (CXXMethodDecl* trace = info->GetTraceMethod()) {
      if (trace->isPure())
        ReportClassDeclaresPureVirtualTrace(info, trace);
    } else if (info->RequiresTraceMethod()) {
      ReportClassRequiresTraceMethod(info);
    }

    {
      CheckFieldsVisitor visitor(options_);
      if (visitor.ContainsInvalidFields(info))
        ReportClassContainsInvalidFields(info, &visitor.invalid_fields());
    }

    if (info->IsGCDerived()) {

      if (!info->IsGCMixin()) {
        CheckLeftMostDerived(info);
        CheckDispatch(info);
        if (CXXMethodDecl* newop = info->DeclaresNewOperator())
          ReportClassOverridesNew(info, newop);
      }

      {
        CheckGCRootsVisitor visitor;
        if (visitor.ContainsGCRoots(info))
          ReportClassContainsGCRoots(info, &visitor.gc_roots());
      }

      if (info->NeedsFinalization())
        CheckFinalization(info);
    }

    DumpClass(info);
  }

  void CheckLeftMostDerived(RecordInfo* info) {
    CXXRecordDecl* left_most = info->record();
    CXXRecordDecl::base_class_iterator it = left_most->bases_begin();
    while (it != left_most->bases_end()) {
      left_most = it->getType()->getAsCXXRecordDecl();
      it = left_most->bases_begin();
    }
    if (!Config::IsGCBase(left_most->getName()))
      ReportClassMustLeftMostlyDeriveGC(info);
  }

  void CheckDispatch(RecordInfo* info) {
    bool finalized = info->IsGCFinalized();
    CXXMethodDecl* trace_dispatch = info->GetTraceDispatchMethod();
    CXXMethodDecl* finalize_dispatch = info->GetFinalizeDispatchMethod();
    if (!trace_dispatch && !finalize_dispatch)
      return;

    CXXRecordDecl* base = trace_dispatch ? trace_dispatch->getParent()
                                         : finalize_dispatch->getParent();

    // Check that dispatch methods are defined at the base.
    if (base == info->record()) {
      if (!trace_dispatch)
        ReportMissingTraceDispatchMethod(info);
      if (finalized && !finalize_dispatch)
        ReportMissingFinalizeDispatchMethod(info);
      if (!finalized && finalize_dispatch) {
        ReportClassRequiresFinalization(info);
        NoteUserDeclaredFinalizer(finalize_dispatch);
      }
    }

    // Check that classes implementing manual dispatch do not have vtables.
    if (info->record()->isPolymorphic())
      ReportVirtualAndManualDispatch(
          info, trace_dispatch ? trace_dispatch : finalize_dispatch);

    // If this is a non-abstract class check that it is dispatched to.
    // TODO: Create a global variant of this local check. We can only check if
    // the dispatch body is known in this compilation unit.
    if (info->IsConsideredAbstract())
      return;

    const FunctionDecl* defn;

    if (trace_dispatch && trace_dispatch->isDefined(defn)) {
      CheckDispatchVisitor visitor(info);
      visitor.TraverseStmt(defn->getBody());
      if (!visitor.dispatched_to_receiver())
        ReportMissingTraceDispatch(defn, info);
    }

    if (finalized && finalize_dispatch && finalize_dispatch->isDefined(defn)) {
      CheckDispatchVisitor visitor(info);
      visitor.TraverseStmt(defn->getBody());
      if (!visitor.dispatched_to_receiver())
        ReportMissingFinalizeDispatch(defn, info);
    }
  }

  // TODO: Should we collect destructors similar to trace methods?
  void CheckFinalization(RecordInfo* info) {
    CXXDestructorDecl* dtor = info->record()->getDestructor();

    // For finalized classes, check the finalization method if possible.
    if (info->IsGCFinalized()) {
      if (dtor && dtor->hasBody()) {
        CheckFinalizerVisitor visitor(&cache_);
        visitor.TraverseCXXMethodDecl(dtor);
        if (!visitor.finalized_fields().empty()) {
          ReportFinalizerAccessesFinalizedFields(
              dtor, &visitor.finalized_fields());
        }
      }
      return;
    }

    // Don't require finalization of a mixin that has not yet been "mixed in".
    if (info->IsGCMixin())
      return;

    // Report the finalization error, and proceed to print possible causes for
    // the finalization requirement.
    ReportClassRequiresFinalization(info);

    if (dtor && dtor->isUserProvided())
      NoteUserDeclaredDestructor(dtor);

    for (RecordInfo::Bases::iterator it = info->GetBases().begin();
         it != info->GetBases().end();
         ++it) {
      if (it->second.info()->NeedsFinalization())
        NoteBaseRequiresFinalization(&it->second);
    }

    for (RecordInfo::Fields::iterator it = info->GetFields().begin();
         it != info->GetFields().end();
         ++it) {
      if (it->second.edge()->NeedsFinalization())
        NoteField(&it->second, diag_field_requires_finalization_note_);
    }
  }

  // This is the main entry for tracing method definitions.
  void CheckTracingMethod(CXXMethodDecl* method) {
    RecordInfo* parent = cache_.Lookup(method->getParent());
    if (IsIgnored(parent))
      return;

    // Check templated tracing methods by checking the template instantiations.
    // Specialized templates are handled as ordinary classes.
    if (ClassTemplateDecl* tmpl =
            parent->record()->getDescribedClassTemplate()) {
      for (ClassTemplateDecl::spec_iterator it = tmpl->spec_begin();
           it != tmpl->spec_end();
           ++it) {
        // Check trace using each template instantiation as the holder.
        if (IsTemplateInstantiation(*it))
          CheckTraceOrDispatchMethod(cache_.Lookup(*it), method);
      }
      return;
    }

    CheckTraceOrDispatchMethod(parent, method);
  }

  // Determine what type of tracing method this is (dispatch or trace).
  void CheckTraceOrDispatchMethod(RecordInfo* parent, CXXMethodDecl* method) {
    bool isTraceAfterDispatch;
    if (Config::IsTraceMethod(method, &isTraceAfterDispatch)) {
      if (isTraceAfterDispatch || !parent->GetTraceDispatchMethod()) {
        CheckTraceMethod(parent, method, isTraceAfterDispatch);
      }
      // Dispatch methods are checked when we identify subclasses.
    }
  }

  // Check an actual trace method.
  void CheckTraceMethod(RecordInfo* parent,
                        CXXMethodDecl* trace,
                        bool isTraceAfterDispatch) {
    // A non-virtual trace method must not override another trace.
    if (!isTraceAfterDispatch && !trace->isVirtual()) {
      for (RecordInfo::Bases::iterator it = parent->GetBases().begin();
           it != parent->GetBases().end();
           ++it) {
        RecordInfo* base = it->second.info();
        // We allow mixin bases to contain a non-virtual trace since it will
        // never be used for dispatching.
        if (base->IsGCMixin())
          continue;
        if (CXXMethodDecl* other = base->InheritsNonVirtualTrace())
          ReportOverriddenNonVirtualTrace(parent, trace, other);
      }
    }

    CheckTraceVisitor visitor(trace, parent);
    visitor.TraverseCXXMethodDecl(trace);

    for (RecordInfo::Bases::iterator it = parent->GetBases().begin();
         it != parent->GetBases().end();
         ++it) {
      if (!it->second.IsProperlyTraced())
        ReportBaseRequiresTracing(parent, trace, it->first);
    }

    for (RecordInfo::Fields::iterator it = parent->GetFields().begin();
         it != parent->GetFields().end();
         ++it) {
      if (!it->second.IsProperlyTraced()) {
        // Discontinue once an untraced-field error is found.
        ReportFieldsRequireTracing(parent, trace);
        break;
      }
    }
  }

  void DumpClass(RecordInfo* info) {
    if (!json_)
      return;

    json_->OpenObject();
    json_->Write("name", info->record()->getQualifiedNameAsString());
    json_->Write("loc", GetLocString(info->record()->getLocStart()));
    json_->CloseObject();

    class DumpEdgeVisitor : public RecursiveEdgeVisitor {
     public:
      DumpEdgeVisitor(JsonWriter* json) : json_(json) {}
      void DumpEdge(RecordInfo* src,
                    RecordInfo* dst,
                    const string& lbl,
                    const Edge::LivenessKind& kind,
                    const string& loc) {
        json_->OpenObject();
        json_->Write("src", src->record()->getQualifiedNameAsString());
        json_->Write("dst", dst->record()->getQualifiedNameAsString());
        json_->Write("lbl", lbl);
        json_->Write("kind", kind);
        json_->Write("loc", loc);
        json_->Write("ptr",
                     !Parent() ? "val" :
                     Parent()->IsRawPtr() ? "raw" :
                     Parent()->IsRefPtr() ? "ref" :
                     Parent()->IsOwnPtr() ? "own" :
                     (Parent()->IsMember() ||
                      Parent()->IsWeakMember()) ? "mem" :
                     "val");
        json_->CloseObject();
      }

      void DumpField(RecordInfo* src, FieldPoint* point, const string& loc) {
        src_ = src;
        point_ = point;
        loc_ = loc;
        point_->edge()->Accept(this);
      }

      void AtValue(Value* e) override {
        // The liveness kind of a path from the point to this value
        // is given by the innermost place that is non-strong.
        Edge::LivenessKind kind = Edge::kStrong;
        if (Config::IsIgnoreCycleAnnotated(point_->field())) {
          kind = Edge::kWeak;
        } else {
          for (Context::iterator it = context().begin();
               it != context().end();
               ++it) {
            Edge::LivenessKind pointer_kind = (*it)->Kind();
            if (pointer_kind != Edge::kStrong) {
              kind = pointer_kind;
              break;
            }
          }
        }
        DumpEdge(
            src_, e->value(), point_->field()->getNameAsString(), kind, loc_);
      }

     private:
      JsonWriter* json_;
      RecordInfo* src_;
      FieldPoint* point_;
      string loc_;
    };

    DumpEdgeVisitor visitor(json_);

    RecordInfo::Bases& bases = info->GetBases();
    for (RecordInfo::Bases::iterator it = bases.begin();
         it != bases.end();
         ++it) {
      visitor.DumpEdge(info,
                       it->second.info(),
                       "<super>",
                       Edge::kStrong,
                       GetLocString(it->second.spec().getLocStart()));
    }

    RecordInfo::Fields& fields = info->GetFields();
    for (RecordInfo::Fields::iterator it = fields.begin();
         it != fields.end();
         ++it) {
      visitor.DumpField(info,
                        &it->second,
                        GetLocString(it->second.field()->getLocStart()));
    }
  }

  // Adds either a warning or error, based on the current handling of -Werror.
  DiagnosticsEngine::Level getErrorLevel() {
    return diagnostic_.getWarningsAsErrors() ? DiagnosticsEngine::Error
                                             : DiagnosticsEngine::Warning;
  }

  const string GetLocString(SourceLocation loc) {
    const SourceManager& source_manager = instance_.getSourceManager();
    PresumedLoc ploc = source_manager.getPresumedLoc(loc);
    if (ploc.isInvalid())
      return "";
    string loc_str;
    llvm::raw_string_ostream OS(loc_str);
    OS << ploc.getFilename()
       << ":" << ploc.getLine()
       << ":" << ploc.getColumn();
    return OS.str();
  }

  bool IsIgnored(RecordInfo* record) {
    return !record ||
           !InCheckedNamespace(record) ||
           IsIgnoredClass(record) ||
           InIgnoredDirectory(record);
  }

  bool IsIgnoredClass(RecordInfo* info) {
    // Ignore any class prefixed by SameSizeAs. These are used in
    // Blink to verify class sizes and don't need checking.
    const string SameSizeAs = "SameSizeAs";
    if (info->name().compare(0, SameSizeAs.size(), SameSizeAs) == 0)
      return true;
    return options_.ignored_classes.find(info->name()) !=
           options_.ignored_classes.end();
  }

  bool InIgnoredDirectory(RecordInfo* info) {
    string filename;
    if (!GetFilename(info->record()->getLocStart(), &filename))
      return false;  // TODO: should we ignore non-existing file locations?
    std::vector<string>::iterator it = options_.ignored_directories.begin();
    for (; it != options_.ignored_directories.end(); ++it)
      if (filename.find(*it) != string::npos)
        return true;
    return false;
  }

  bool InCheckedNamespace(RecordInfo* info) {
    if (!info)
      return false;
    for (DeclContext* context = info->record()->getDeclContext();
         !context->isTranslationUnit();
         context = context->getParent()) {
      if (NamespaceDecl* decl = dyn_cast<NamespaceDecl>(context)) {
        if (options_.checked_namespaces.find(decl->getNameAsString()) !=
            options_.checked_namespaces.end()) {
          return true;
        }
      }
    }
    return false;
  }

  bool GetFilename(SourceLocation loc, string* filename) {
    const SourceManager& source_manager = instance_.getSourceManager();
    SourceLocation spelling_location = source_manager.getSpellingLoc(loc);
    PresumedLoc ploc = source_manager.getPresumedLoc(spelling_location);
    if (ploc.isInvalid()) {
      // If we're in an invalid location, we're looking at things that aren't
      // actually stated in the source.
      return false;
    }
    *filename = ploc.getFilename();
    return true;
  }

  void ReportClassMustLeftMostlyDeriveGC(RecordInfo* info) {
    SourceLocation loc = info->record()->getInnerLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_class_must_left_mostly_derive_gc_)
        << info->record();
  }

  void ReportClassRequiresTraceMethod(RecordInfo* info) {
    SourceLocation loc = info->record()->getInnerLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_class_requires_trace_method_)
        << info->record();

    for (RecordInfo::Bases::iterator it = info->GetBases().begin();
         it != info->GetBases().end();
         ++it) {
      if (it->second.NeedsTracing().IsNeeded())
        NoteBaseRequiresTracing(&it->second);
    }

    for (RecordInfo::Fields::iterator it = info->GetFields().begin();
         it != info->GetFields().end();
         ++it) {
      if (!it->second.IsProperlyTraced())
        NoteFieldRequiresTracing(info, it->first);
    }
  }

  void ReportBaseRequiresTracing(RecordInfo* derived,
                                 CXXMethodDecl* trace,
                                 CXXRecordDecl* base) {
    SourceLocation loc = trace->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_base_requires_tracing_)
        << base << derived->record();
  }

  void ReportFieldsRequireTracing(RecordInfo* info, CXXMethodDecl* trace) {
    SourceLocation loc = trace->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_fields_require_tracing_)
        << info->record();
    for (RecordInfo::Fields::iterator it = info->GetFields().begin();
         it != info->GetFields().end();
         ++it) {
      if (!it->second.IsProperlyTraced())
        NoteFieldRequiresTracing(info, it->first);
    }
  }

  void ReportClassContainsInvalidFields(RecordInfo* info,
                                        CheckFieldsVisitor::Errors* errors) {
    SourceLocation loc = info->record()->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_class_contains_invalid_fields_)
        << info->record();
    for (CheckFieldsVisitor::Errors::iterator it = errors->begin();
         it != errors->end();
         ++it) {
      unsigned error;
      if (it->second == CheckFieldsVisitor::kRawPtrToGCManaged) {
        error = diag_raw_ptr_to_gc_managed_class_note_;
      } else if (it->second == CheckFieldsVisitor::kRefPtrToGCManaged) {
        error = diag_ref_ptr_to_gc_managed_class_note_;
      } else if (it->second == CheckFieldsVisitor::kOwnPtrToGCManaged) {
        error = diag_own_ptr_to_gc_managed_class_note_;
      } else if (it->second == CheckFieldsVisitor::kMemberInUnmanaged) {
        error = diag_member_in_unmanaged_class_note_;
      } else if (it->second == CheckFieldsVisitor::kPtrFromHeapToStack) {
        error = diag_stack_allocated_field_note_;
      } else if (it->second == CheckFieldsVisitor::kGCDerivedPartObject) {
        error = diag_part_object_to_gc_derived_class_note_;
      } else {
        assert(false && "Unknown field error");
      }
      NoteField(it->first, error);
    }
  }

  void ReportClassContainsGCRoots(RecordInfo* info,
                                  CheckGCRootsVisitor::Errors* errors) {
    SourceLocation loc = info->record()->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    for (CheckGCRootsVisitor::Errors::iterator it = errors->begin();
         it != errors->end();
         ++it) {
      CheckGCRootsVisitor::RootPath::iterator path = it->begin();
      FieldPoint* point = *path;
      diagnostic_.Report(full_loc, diag_class_contains_gc_root_)
          << info->record() << point->field();
      while (++path != it->end()) {
        NotePartObjectContainsGCRoot(point);
        point = *path;
      }
      NoteFieldContainsGCRoot(point);
    }
  }

  void ReportFinalizerAccessesFinalizedFields(
      CXXMethodDecl* dtor,
      CheckFinalizerVisitor::Errors* fields) {
    for (CheckFinalizerVisitor::Errors::iterator it = fields->begin();
         it != fields->end();
         ++it) {
      SourceLocation loc = it->first->getLocStart();
      SourceManager& manager = instance_.getSourceManager();
      FullSourceLoc full_loc(loc, manager);
      diagnostic_.Report(full_loc, diag_finalizer_accesses_finalized_field_)
          << dtor << it->second->field();
      NoteField(it->second, diag_finalized_field_note_);
    }
  }

  void ReportClassRequiresFinalization(RecordInfo* info) {
    SourceLocation loc = info->record()->getInnerLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_class_requires_finalization_)
        << info->record();
  }

  void ReportOverriddenNonVirtualTrace(RecordInfo* info,
                                       CXXMethodDecl* trace,
                                       CXXMethodDecl* overridden) {
    SourceLocation loc = trace->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_overridden_non_virtual_trace_)
        << info->record() << overridden->getParent();
    NoteOverriddenNonVirtualTrace(overridden);
  }

  void ReportMissingTraceDispatchMethod(RecordInfo* info) {
    ReportMissingDispatchMethod(info, diag_missing_trace_dispatch_method_);
  }

  void ReportMissingFinalizeDispatchMethod(RecordInfo* info) {
    ReportMissingDispatchMethod(info, diag_missing_finalize_dispatch_method_);
  }

  void ReportMissingDispatchMethod(RecordInfo* info, unsigned error) {
    SourceLocation loc = info->record()->getInnerLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, error) << info->record();
  }

  void ReportVirtualAndManualDispatch(RecordInfo* info,
                                      CXXMethodDecl* dispatch) {
    SourceLocation loc = info->record()->getInnerLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_virtual_and_manual_dispatch_)
        << info->record();
    NoteManualDispatchMethod(dispatch);
  }

  void ReportMissingTraceDispatch(const FunctionDecl* dispatch,
                                  RecordInfo* receiver) {
    ReportMissingDispatch(dispatch, receiver, diag_missing_trace_dispatch_);
  }

  void ReportMissingFinalizeDispatch(const FunctionDecl* dispatch,
                                     RecordInfo* receiver) {
    ReportMissingDispatch(dispatch, receiver, diag_missing_finalize_dispatch_);
  }

  void ReportMissingDispatch(const FunctionDecl* dispatch,
                             RecordInfo* receiver,
                             unsigned error) {
    SourceLocation loc = dispatch->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, error) << receiver->record();
  }

  void ReportDerivesNonStackAllocated(RecordInfo* info, BasePoint* base) {
    SourceLocation loc = base->spec().getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_derives_non_stack_allocated_)
        << info->record() << base->info()->record();
  }

  void ReportClassOverridesNew(RecordInfo* info, CXXMethodDecl* newop) {
    SourceLocation loc = newop->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_class_overrides_new_) << info->record();
  }

  void ReportClassDeclaresPureVirtualTrace(RecordInfo* info,
                                           CXXMethodDecl* trace) {
    SourceLocation loc = trace->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_class_declares_pure_virtual_trace_)
        << info->record();
  }

  void NoteManualDispatchMethod(CXXMethodDecl* dispatch) {
    SourceLocation loc = dispatch->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_manual_dispatch_method_note_) << dispatch;
  }

  void NoteBaseRequiresTracing(BasePoint* base) {
    SourceLocation loc = base->spec().getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_base_requires_tracing_note_)
        << base->info()->record();
  }

  void NoteFieldRequiresTracing(RecordInfo* holder, FieldDecl* field) {
    NoteField(field, diag_field_requires_tracing_note_);
  }

  void NotePartObjectContainsGCRoot(FieldPoint* point) {
    FieldDecl* field = point->field();
    SourceLocation loc = field->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_part_object_contains_gc_root_note_)
        << field << field->getParent();
  }

  void NoteFieldContainsGCRoot(FieldPoint* point) {
    NoteField(point, diag_field_contains_gc_root_note_);
  }

  void NoteUserDeclaredDestructor(CXXMethodDecl* dtor) {
    SourceLocation loc = dtor->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_user_declared_destructor_note_);
  }

  void NoteUserDeclaredFinalizer(CXXMethodDecl* dtor) {
    SourceLocation loc = dtor->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_user_declared_finalizer_note_);
  }

  void NoteBaseRequiresFinalization(BasePoint* base) {
    SourceLocation loc = base->spec().getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_base_requires_finalization_note_)
        << base->info()->record();
  }

  void NoteField(FieldPoint* point, unsigned note) {
    NoteField(point->field(), note);
  }

  void NoteField(FieldDecl* field, unsigned note) {
    SourceLocation loc = field->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, note) << field;
  }

  void NoteOverriddenNonVirtualTrace(CXXMethodDecl* overridden) {
    SourceLocation loc = overridden->getLocStart();
    SourceManager& manager = instance_.getSourceManager();
    FullSourceLoc full_loc(loc, manager);
    diagnostic_.Report(full_loc, diag_overridden_non_virtual_trace_note_)
        << overridden;
  }

  unsigned diag_class_must_left_mostly_derive_gc_;
  unsigned diag_class_requires_trace_method_;
  unsigned diag_base_requires_tracing_;
  unsigned diag_fields_require_tracing_;
  unsigned diag_class_contains_invalid_fields_;
  unsigned diag_class_contains_gc_root_;
  unsigned diag_class_requires_finalization_;
  unsigned diag_finalizer_accesses_finalized_field_;
  unsigned diag_overridden_non_virtual_trace_;
  unsigned diag_missing_trace_dispatch_method_;
  unsigned diag_missing_finalize_dispatch_method_;
  unsigned diag_virtual_and_manual_dispatch_;
  unsigned diag_missing_trace_dispatch_;
  unsigned diag_missing_finalize_dispatch_;
  unsigned diag_derives_non_stack_allocated_;
  unsigned diag_class_overrides_new_;
  unsigned diag_class_declares_pure_virtual_trace_;

  unsigned diag_base_requires_tracing_note_;
  unsigned diag_field_requires_tracing_note_;
  unsigned diag_raw_ptr_to_gc_managed_class_note_;
  unsigned diag_ref_ptr_to_gc_managed_class_note_;
  unsigned diag_own_ptr_to_gc_managed_class_note_;
  unsigned diag_stack_allocated_field_note_;
  unsigned diag_member_in_unmanaged_class_note_;
  unsigned diag_part_object_to_gc_derived_class_note_;
  unsigned diag_part_object_contains_gc_root_note_;
  unsigned diag_field_contains_gc_root_note_;
  unsigned diag_finalized_field_note_;
  unsigned diag_user_declared_destructor_note_;
  unsigned diag_user_declared_finalizer_note_;
  unsigned diag_base_requires_finalization_note_;
  unsigned diag_field_requires_finalization_note_;
  unsigned diag_overridden_non_virtual_trace_note_;
  unsigned diag_manual_dispatch_method_note_;

  CompilerInstance& instance_;
  DiagnosticsEngine& diagnostic_;
  BlinkGCPluginOptions options_;
  RecordCache cache_;
  JsonWriter* json_;
};

class BlinkGCPluginAction : public PluginASTAction {
 public:
  BlinkGCPluginAction() {}

 protected:
  // Overridden from PluginASTAction:
  virtual ASTConsumer* CreateASTConsumer(CompilerInstance& instance,
                                         llvm::StringRef ref) {
    return new BlinkGCPluginConsumer(instance, options_);
  }

  virtual bool ParseArgs(const CompilerInstance& instance,
                         const std::vector<string>& args) {
    bool parsed = true;

    for (size_t i = 0; i < args.size() && parsed; ++i) {
      if (args[i] == "enable-oilpan") {
        options_.enable_oilpan = true;
      } else if (args[i] == "dump-graph") {
        options_.dump_graph = true;
      } else {
        parsed = false;
        llvm::errs() << "Unknown blink-gc-plugin argument: " << args[i] << "\n";
      }
    }

    return parsed;
  }

 private:
  BlinkGCPluginOptions options_;
};

}  // namespace

static FrontendPluginRegistry::Add<BlinkGCPluginAction> X(
    "blink-gc-plugin",
    "Check Blink GC invariants");
