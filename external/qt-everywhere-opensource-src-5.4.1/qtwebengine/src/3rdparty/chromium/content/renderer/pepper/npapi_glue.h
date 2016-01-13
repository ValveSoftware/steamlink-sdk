// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_NPAPI_GLUE_H_
#define CONTENT_RENDERER_PEPPER_NPAPI_GLUE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"

struct NPObject;
typedef struct _NPVariant NPVariant;
typedef void* NPIdentifier;

namespace content {

class PepperPluginInstanceImpl;
class PluginObject;

// Utilities -------------------------------------------------------------------

// Converts the given PP_Var to an NPVariant, returning true on success.
// False means that the given variant is invalid. In this case, the result
// NPVariant will be set to a void one.
//
// The contents of the PP_Var will be copied unless the PP_Var corresponds to
// an object.
bool PPVarToNPVariant(PP_Var var, NPVariant* result);

// Returns a PP_Var that corresponds to the given NPVariant. The contents of
// the NPVariant will be copied unless the NPVariant corresponds to an
// object. This will handle all Variant types including POD, strings, and
// objects.
//
// The returned PP_Var will have a refcount of 1, this passing ownership of
// the reference to the caller. This is suitable for returning to a plugin.
PP_Var NPVariantToPPVar(PepperPluginInstanceImpl* instance,
                        const NPVariant* variant);

// Returns a NPIdentifier that corresponds to the given PP_Var. The contents
// of the PP_Var will be copied. Returns 0 if the given PP_Var is not a a
// string or integer type.
NPIdentifier PPVarToNPIdentifier(PP_Var var);

// Returns a PP_Var corresponding to the given identifier. In the case of
// a string identifier, the returned string will have a reference count of 1.
PP_Var NPIdentifierToPPVar(NPIdentifier id);

// Helper function to create a PP_Var of type object that contains the given
// NPObject for use byt he given module. Calling this function multiple times
// given the same module + NPObject results in the same PP_Var, assuming that
// there is still a PP_Var with a reference open to it from the previous
// call.
//
// The instance is necessary because we can have different instances pointing to
// the same NPObject, and we want to keep their refs separate.
//
// If no ObjectVar currently exists corresponding to the NPObject, one is
// created associated with the given module.
//
// Note: this could easily be changed to take a PP_Instance instead if that
// makes certain calls in the future easier. Currently all callers have a
// PluginInstance so that's what we use here.
CONTENT_EXPORT PP_Var
    NPObjectToPPVar(PepperPluginInstanceImpl* instance, NPObject* object);

// This version creates a default v8::Context rather than using the one from
// the container of |instance|. It is only for use in unit tests, where we don't
// have a real container for |instance|.
CONTENT_EXPORT PP_Var NPObjectToPPVarForTest(PepperPluginInstanceImpl* instance,
                                             NPObject* object);

// PPResultAndExceptionToNPResult ----------------------------------------------

// Convenience object for converting a PPAPI call that can throw an exception
// and optionally return a value, back to the NPAPI layer which expects a
// NPVariant as a result.
//
// Normal usage is that you will pass the result of exception() to the
// PPAPI function as the exception output parameter. Then you will either
// call SetResult with the result of the PPAPI call, or
// CheckExceptionForNoResult if the PPAPI call doesn't return a PP_Var.
//
// Both SetResult and CheckExceptionForNoResult will throw an exception to
// the JavaScript library if the plugin reported an exception. SetResult
// will additionally convert the result to an NPVariant and write it to the
// output parameter given in the constructor.
class PPResultAndExceptionToNPResult {
 public:
  // The object_var parameter is the object to associate any exception with.
  // It may not be NULL.
  //
  // The np_result parameter is the NPAPI result output parameter. This may be
  // NULL if there is no NPVariant result (like for HasProperty). If this is
  // specified, you must call SetResult() to set it. If it is not, you must
  // call CheckExceptionForNoResult to do the exception checking with no result
  // conversion.
  PPResultAndExceptionToNPResult(NPObject* object_var, NPVariant* np_result);

  ~PPResultAndExceptionToNPResult();

  // Returns true if an exception has been set.
  bool has_exception() const { return exception_.type != PP_VARTYPE_UNDEFINED; }

  // Returns a pointer to the exception. You would pass this to the PPAPI
  // function as the exception parameter. If it is set to non-void, this object
  // will take ownership of destroying it.
  PP_Var* exception() { return &exception_; }

  // Returns true if everything succeeded with no exception. This is valid only
  // after calling SetResult/CheckExceptionForNoResult.
  bool success() const { return success_; }

  // Call this with the return value of the PPAPI function. It will convert
  // the result to the NPVariant output parameter and pass any exception on to
  // the JS engine. It will update the success flag and return it.
  bool SetResult(PP_Var result);

  // Call this after calling a PPAPI function that could have set the
  // exception. It will pass the exception on to the JS engine and update
  // the success flag.
  //
  // The success flag will be returned.
  bool CheckExceptionForNoResult();

  // Call this to ignore any exception. This prevents the DCHECK from failing
  // in the destructor.
  void IgnoreException();

 private:
  // Throws the current exception to JS. The exception must be set.
  void ThrowException();

  NPObject* object_var_;  // Non-owning ref (see constructor).
  NPVariant* np_result_;  // Output value, possibly NULL (see constructor).
  PP_Var exception_;  // Exception set by the PPAPI call. We own a ref to it.
  bool success_;      // See the success() function above.
  bool checked_exception_;  // SetResult/CheckExceptionForNoResult was called.

  DISALLOW_COPY_AND_ASSIGN(PPResultAndExceptionToNPResult);
};

// PPVarArrayFromNPVariantArray ------------------------------------------------

// Converts an array of NPVariants to an array of PP_Var, and scopes the
// ownership of the PP_Var. This is used when converting argument lists from
// WebKit to the plugin.
class PPVarArrayFromNPVariantArray {
 public:
  PPVarArrayFromNPVariantArray(PepperPluginInstanceImpl* instance,
                               size_t size,
                               const NPVariant* variants);
  ~PPVarArrayFromNPVariantArray();

  PP_Var* array() { return array_.get(); }

 private:
  size_t size_;
  scoped_ptr<PP_Var[]> array_;

  DISALLOW_COPY_AND_ASSIGN(PPVarArrayFromNPVariantArray);
};

// PPVarFromNPObject -----------------------------------------------------------

// Converts an NPObject tp PP_Var, and scopes the ownership of the PP_Var. This
// is used when converting 'this' pointer from WebKit to the plugin.
class PPVarFromNPObject {
 public:
  PPVarFromNPObject(PepperPluginInstanceImpl* instance, NPObject* object);
  ~PPVarFromNPObject();

  PP_Var var() const { return var_; }

 private:
  const PP_Var var_;

  DISALLOW_COPY_AND_ASSIGN(PPVarFromNPObject);
};

// NPObjectAccessorWithIdentifier ----------------------------------------------

// Helper class for our NPObject wrapper. This converts a call from WebKit
// where it gives us an NPObject and an NPIdentifier to an easily-accessible
// ObjectVar (corresponding to the NPObject) and PP_Var (corresponding to the
// NPIdentifier).
//
// If the NPObject or identifier is invalid, we'll set is_valid() to false.
// The caller should check is_valid() before doing anything with the class.
//
// JS can't have integer functions, so when dealing with these, we don't want
// to allow integer identifiers. The calling code can decode if it wants to
// allow integer identifiers (like for property access) or prohibit them
// (like for method calling) by setting |allow_integer_identifier|. If this
// is false and the identifier is an integer, we'll set is_valid() to false.
//
// Getting an integer identifier in this case should be impossible. V8
// shouldn't be allowing this, and the Pepper Var calls from the plugin are
// supposed to error out before calling into V8 (which will then call us back).
// Aside from an egregious error, the only time this could happen is an NPAPI
// plugin calling us.
class NPObjectAccessorWithIdentifier {
 public:
  NPObjectAccessorWithIdentifier(NPObject* object,
                                 NPIdentifier identifier,
                                 bool allow_integer_identifier);
  ~NPObjectAccessorWithIdentifier();

  // Returns true if both the object and identifier are valid.
  bool is_valid() const {
    return object_ && identifier_.type != PP_VARTYPE_UNDEFINED;
  }

  PluginObject* object() { return object_; }
  PP_Var identifier() const { return identifier_; }

 private:
  PluginObject* object_;
  PP_Var identifier_;

  DISALLOW_COPY_AND_ASSIGN(NPObjectAccessorWithIdentifier);
};

// TryCatch --------------------------------------------------------------------

// Instantiate this object on the stack to catch V8 exceptions and pass them
// to an optional out parameter supplied by the plugin.
class TryCatch {
 public:
  // The given exception may be NULL if the consumer isn't interested in
  // catching exceptions. If non-NULL, the given var will be updated if any
  // exception is thrown (so it must outlive the TryCatch object).
  TryCatch(PP_Var* exception);
  ~TryCatch();

  // Returns true is an exception has been thrown. This can be true immediately
  // after construction if the var passed to the constructor is non-void.
  bool has_exception() const { return has_exception_; }

  // Sets the given exception. If an exception has been previously set, this
  // function will do nothing (normally you want only the first exception).
  void SetException(const char* message);

 private:
  static void Catch(void* self, const char* message);

  // True if an exception has been thrown. Since the exception itself may be
  // NULL if the plugin isn't interested in getting the exception, this will
  // always indicate if SetException has been called, regardless of whether
  // the exception itself has been stored.
  bool has_exception_;

  // May be null if the consumer isn't interesting in catching exceptions.
  PP_Var* exception_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_NPAPI_GLUE_H_
