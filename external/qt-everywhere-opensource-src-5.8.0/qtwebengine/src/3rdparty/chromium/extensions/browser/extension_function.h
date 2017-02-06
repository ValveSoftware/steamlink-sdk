// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FUNCTION_H_
#define EXTENSIONS_BROWSER_EXTENSION_FUNCTION_H_

#include <stddef.h>

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/console_message_level.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "ipc/ipc_message.h"

class ExtensionFunction;
class UIThreadExtensionFunction;
class IOThreadExtensionFunction;

namespace base {
class ListValue;
class Value;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
class RenderViewHost;
class WebContents;
}

namespace extensions {
class ExtensionFunctionDispatcher;
class IOThreadExtensionMessageFilter;
class QuotaLimitHeuristic;
}

namespace IPC {
class Sender;
}

#ifdef NDEBUG
#define EXTENSION_FUNCTION_VALIDATE(test) \
  do {                                    \
    if (!(test)) {                        \
      this->bad_message_ = true;          \
      return ValidationFailure(this);     \
    }                                     \
  } while (0)
#else   // NDEBUG
#define EXTENSION_FUNCTION_VALIDATE(test) CHECK(test)
#endif  // NDEBUG

#ifdef NDEBUG
#define EXTENSION_FUNCTION_PRERUN_VALIDATE(test) \
  do {                                           \
    if (!(test)) {                               \
      this->bad_message_ = true;                 \
      return false;                              \
    }                                            \
  } while (0)
#else  // NDEBUG
#define EXTENSION_FUNCTION_PRERUN_VALIDATE(test) CHECK(test)
#endif  // NDEBUG

#define EXTENSION_FUNCTION_ERROR(error) \
  do {                                  \
    error_ = error;                     \
    this->bad_message_ = true;          \
    return ValidationFailure(this);     \
  } while (0)

// Declares a callable extension function with the given |name|. You must also
// supply a unique |histogramvalue| used for histograms of extension function
// invocation (add new ones at the end of the enum in
// extension_function_histogram_value.h).
#define DECLARE_EXTENSION_FUNCTION(name, histogramvalue) \
  public: static const char* function_name() { return name; } \
  public: static extensions::functions::HistogramValue histogram_value() \
    { return extensions::functions::histogramvalue; }

// Traits that describe how ExtensionFunction should be deleted. This just calls
// the virtual "Destruct" method on ExtensionFunction, allowing derived classes
// to override the behavior.
struct ExtensionFunctionDeleteTraits {
 public:
  static void Destruct(const ExtensionFunction* x);
};

// Abstract base class for extension functions the ExtensionFunctionDispatcher
// knows how to dispatch to.
class ExtensionFunction
    : public base::RefCountedThreadSafe<ExtensionFunction,
                                        ExtensionFunctionDeleteTraits> {
 public:
  enum ResponseType {
    // The function has succeeded.
    SUCCEEDED,
    // The function has failed.
    FAILED,
    // The input message is malformed.
    BAD_MESSAGE
  };

  using ResponseCallback = base::Callback<void(
      ResponseType type,
      const base::ListValue& results,
      const std::string& error,
      extensions::functions::HistogramValue histogram_value)>;

  ExtensionFunction();

  virtual UIThreadExtensionFunction* AsUIThreadExtensionFunction();
  virtual IOThreadExtensionFunction* AsIOThreadExtensionFunction();

  // Returns true if the function has permission to run.
  //
  // The default implementation is to check the Extension's permissions against
  // what this function requires to run, but some APIs may require finer
  // grained control, such as tabs.executeScript being allowed for active tabs.
  //
  // This will be run after the function has been set up but before Run().
  virtual bool HasPermission();

  // The result of a function call.
  //
  // Use NoArguments(), OneArgument(), ArgumentList(), or Error()
  // rather than this class directly.
  class ResponseValueObject {
   public:
    virtual ~ResponseValueObject() {}

    // Returns true for success, false for failure.
    virtual bool Apply() = 0;
  };
  typedef std::unique_ptr<ResponseValueObject> ResponseValue;

  // The action to use when returning from RunAsync.
  //
  // Use RespondNow() or RespondLater() rather than this class directly.
  class ResponseActionObject {
   public:
    virtual ~ResponseActionObject() {}

    virtual void Execute() = 0;
  };
  typedef std::unique_ptr<ResponseActionObject> ResponseAction;

  // Helper class for tests to force all ExtensionFunction::user_gesture()
  // calls to return true as long as at least one instance of this class
  // exists.
  class ScopedUserGestureForTests {
   public:
    ScopedUserGestureForTests();
    ~ScopedUserGestureForTests();
  };

  // Called before Run() in order to perform a common verification check so that
  // APIs subclassing this don't have to roll their own RunSafe() variants.
  // If this returns false, then Run() is never called, and the function
  // responds immediately with an error (note that error must be non-empty in
  // this case). If this returns true, execution continues on to Run().
  virtual bool PreRunValidation(std::string* error);

  // Runs the extension function if PreRunValidation() succeeds.
  ResponseAction RunWithValidation();

  // Runs the function and returns the action to take when the caller is ready
  // to respond.
  //
  // Typical return values might be:
  //   * RespondNow(NoArguments())
  //   * RespondNow(OneArgument(42))
  //   * RespondNow(ArgumentList(my_result.ToValue()))
  //   * RespondNow(Error("Warp core breach"))
  //   * RespondNow(Error("Warp core breach on *", GetURL()))
  //   * RespondLater(), then later,
  //     * Respond(NoArguments())
  //     * ... etc.
  //
  //
  // Callers must call Execute() on the return ResponseAction at some point,
  // exactly once.
  //
  // SyncExtensionFunction and AsyncExtensionFunction implement this in terms
  // of SyncExtensionFunction::RunSync and AsyncExtensionFunction::RunAsync,
  // but this is deprecated. ExtensionFunction implementations are encouraged
  // to just implement Run.
  virtual ResponseAction Run() WARN_UNUSED_RESULT = 0;

  // Gets whether quota should be applied to this individual function
  // invocation. This is different to GetQuotaLimitHeuristics which is only
  // invoked once and then cached.
  //
  // Returns false by default.
  virtual bool ShouldSkipQuotaLimiting() const;

  // Optionally adds one or multiple QuotaLimitHeuristic instances suitable for
  // this function to |heuristics|. The ownership of the new QuotaLimitHeuristic
  // instances is passed to the owner of |heuristics|.
  // No quota limiting by default.
  //
  // Only called once per lifetime of the QuotaService.
  virtual void GetQuotaLimitHeuristics(
      extensions::QuotaLimitHeuristics* heuristics) const {}

  // Called when the quota limit has been exceeded. The default implementation
  // returns an error.
  virtual void OnQuotaExceeded(const std::string& violation_error);

  // Specifies the raw arguments to the function, as a JSON value.
  // TODO(dcheng): This should take a const ref.
  virtual void SetArgs(const base::ListValue* args);

  // Sets a single Value as the results of the function.
  void SetResult(std::unique_ptr<base::Value> result);

  // Sets multiple Values as the results of the function.
  void SetResultList(std::unique_ptr<base::ListValue> results);

  // Retrieves the results of the function as a ListValue.
  const base::ListValue* GetResultList() const;

  // Retrieves any error string from the function.
  virtual std::string GetError() const;

  // Sets the function's error string.
  virtual void SetError(const std::string& error);

  // Sets the function's bad message state.
  void set_bad_message(bool bad_message) { bad_message_ = bad_message; }

  // Specifies the name of the function. A long-lived string (such as a string
  // literal) must be provided.
  void set_name(const char* name) { name_ = name; }
  const char* name() const { return name_; }

  void set_profile_id(void* profile_id) { profile_id_ = profile_id; }
  void* profile_id() const { return profile_id_; }

  void set_extension(
      const scoped_refptr<const extensions::Extension>& extension) {
    extension_ = extension;
  }
  const extensions::Extension* extension() const { return extension_.get(); }
  const std::string& extension_id() const {
    DCHECK(extension())
        << "extension_id() called without an Extension. If " << name()
        << " is allowed to be called without any Extension then you should "
        << "check extension() first. If not, there is a bug in the Extension "
        << "platform, so page somebody in extensions/OWNERS";
    return extension_->id();
  }

  void set_request_id(int request_id) { request_id_ = request_id; }
  int request_id() { return request_id_; }

  void set_source_url(const GURL& source_url) { source_url_ = source_url; }
  const GURL& source_url() { return source_url_; }

  void set_has_callback(bool has_callback) { has_callback_ = has_callback; }
  bool has_callback() { return has_callback_; }

  void set_include_incognito(bool include) { include_incognito_ = include; }
  bool include_incognito() const { return include_incognito_; }

  // Note: consider using ScopedUserGestureForTests instead of calling
  // set_user_gesture directly.
  void set_user_gesture(bool user_gesture) { user_gesture_ = user_gesture; }
  bool user_gesture() const;

  void set_histogram_value(
      extensions::functions::HistogramValue histogram_value) {
    histogram_value_ = histogram_value; }
  extensions::functions::HistogramValue histogram_value() const {
    return histogram_value_; }

  void set_response_callback(const ResponseCallback& callback) {
    response_callback_ = callback;
  }

  void set_source_tab_id(int source_tab_id) { source_tab_id_ = source_tab_id; }
  int source_tab_id() const { return source_tab_id_; }

  void set_source_context_type(extensions::Feature::Context type) {
    source_context_type_ = type;
  }
  extensions::Feature::Context source_context_type() const {
    return source_context_type_;
  }

  void set_source_process_id(int source_process_id) {
    source_process_id_ = source_process_id;
  }
  int source_process_id() const {
    return source_process_id_;
  }

  // Sets did_respond_ to true so that the function won't DCHECK if it never
  // sends a response. Typically, this shouldn't be used, even in testing. It's
  // only for when you want to test functionality that doesn't exercise the
  // Run() aspect of an extension function.
  void ignore_did_respond_for_testing() { did_respond_ = true; }
  // Same as above, but global. Yuck. Do not add any more uses of this.
  static bool ignore_all_did_respond_for_testing_do_not_use;

 protected:
  friend struct ExtensionFunctionDeleteTraits;

  // ResponseValues.
  //
  // Success, no arguments to pass to caller.
  ResponseValue NoArguments();
  // Success, a single argument |arg| to pass to caller.
  ResponseValue OneArgument(std::unique_ptr<base::Value> arg);
  // Success, two arguments |arg1| and |arg2| to pass to caller.
  // Note that use of this function may imply you
  // should be using the generated Result struct and ArgumentList.
  ResponseValue TwoArguments(std::unique_ptr<base::Value> arg1,
                             std::unique_ptr<base::Value> arg2);
  // Success, a list of arguments |results| to pass to caller.
  // - a std::unique_ptr<> for convenience, since callers usually get this from
  //   the result of a Create(...) call on the generated Results struct. For
  //   example, alarms::Get::Results::Create(alarm).
  ResponseValue ArgumentList(std::unique_ptr<base::ListValue> results);
  // Error. chrome.runtime.lastError.message will be set to |error|.
  ResponseValue Error(const std::string& error);
  // Error with formatting. Args are processed using
  // ErrorUtils::FormatErrorMessage, that is, each occurence of * is replaced
  // by the corresponding |s*|:
  // Error("Error in *: *", "foo", "bar") <--> Error("Error in foo: bar").
  ResponseValue Error(const std::string& format, const std::string& s1);
  ResponseValue Error(const std::string& format,
                      const std::string& s1,
                      const std::string& s2);
  ResponseValue Error(const std::string& format,
                      const std::string& s1,
                      const std::string& s2,
                      const std::string& s3);
  // Error with a list of arguments |args| to pass to caller.
  // Using this ResponseValue indicates something is wrong with the API.
  // It shouldn't be possible to have both an error *and* some arguments.
  // Some legacy APIs do rely on it though, like webstorePrivate.
  ResponseValue ErrorWithArguments(std::unique_ptr<base::ListValue> args,
                                   const std::string& error);
  // Bad message. A ResponseValue equivalent to EXTENSION_FUNCTION_VALIDATE(),
  // so this will actually kill the renderer and not respond at all.
  ResponseValue BadMessage();

  // ResponseActions.
  //
  // These are exclusively used as return values from Run(). Call Respond(...)
  // to respond at any other time - but as described below, only after Run()
  // has already executed, and only if it returned RespondLater().
  //
  // Respond to the extension immediately with |result|.
  ResponseAction RespondNow(ResponseValue result) WARN_UNUSED_RESULT;
  // Don't respond now, but promise to call Respond(...) later.
  ResponseAction RespondLater() WARN_UNUSED_RESULT;

  // This is the return value of the EXTENSION_FUNCTION_VALIDATE macro, which
  // needs to work from Run(), RunAsync(), and RunSync(). The former of those
  // has a different return type (ResponseAction) than the latter two (bool).
  static ResponseAction ValidationFailure(ExtensionFunction* function)
      WARN_UNUSED_RESULT;

  // If RespondLater() was returned from Run(), functions must at some point
  // call Respond() with |result| as their result.
  //
  // More specifically: call this iff Run() has already executed, it returned
  // RespondLater(), and Respond(...) hasn't already been called.
  void Respond(ResponseValue result);

  virtual ~ExtensionFunction();

  // Helper method for ExtensionFunctionDeleteTraits. Deletes this object.
  virtual void Destruct() const = 0;

  // Do not call this function directly, return the appropriate ResponseAction
  // from Run() instead. If using RespondLater then call Respond().
  //
  // Call with true to indicate success, false to indicate failure, in which
  // case please set |error_|.
  virtual void SendResponse(bool success) = 0;

  // Common implementation for SendResponse.
  void SendResponseImpl(bool success);

  // Return true if the argument to this function at |index| was provided and
  // is non-null.
  bool HasOptionalArgument(size_t index);

  // Id of this request, used to map the response back to the caller.
  int request_id_;

  // The id of the profile of this function's extension.
  void* profile_id_;

  // The extension that called this function.
  scoped_refptr<const extensions::Extension> extension_;

  // The name of this function.
  const char* name_;

  // The URL of the frame which is making this request
  GURL source_url_;

  // True if the js caller provides a callback function to receive the response
  // of this call.
  bool has_callback_;

  // True if this callback should include information from incognito contexts
  // even if our profile_ is non-incognito. Note that in the case of a "split"
  // mode extension, this will always be false, and we will limit access to
  // data from within the same profile_ (either incognito or not).
  bool include_incognito_;

  // True if the call was made in response of user gesture.
  bool user_gesture_;

  // The arguments to the API. Only non-null if argument were specified.
  std::unique_ptr<base::ListValue> args_;

  // The results of the API. This should be populated by the derived class
  // before SendResponse() is called.
  std::unique_ptr<base::ListValue> results_;

  // Any detailed error from the API. This should be populated by the derived
  // class before Run() returns.
  std::string error_;

  // Any class that gets a malformed message should set this to true before
  // returning.  Usually we want to kill the message sending process.
  bool bad_message_;

  // The sample value to record with the histogram API when the function
  // is invoked.
  extensions::functions::HistogramValue histogram_value_;

  // The callback to run once the function has done execution.
  ResponseCallback response_callback_;

  // The ID of the tab triggered this function call, or -1 if there is no tab.
  int source_tab_id_;

  // The type of the JavaScript context where this call originated.
  extensions::Feature::Context source_context_type_;

  // The process ID of the page that triggered this function call, or -1
  // if unknown.
  int source_process_id_;

  // Whether this function has responded.
  bool did_respond_;

 private:
  base::ElapsedTimer timer_;

  void OnRespondingLater(ResponseValue response);

  DISALLOW_COPY_AND_ASSIGN(ExtensionFunction);
};

// Extension functions that run on the UI thread. Most functions fall into
// this category.
class UIThreadExtensionFunction : public ExtensionFunction {
 public:
  // TODO(yzshen): We should be able to remove this interface now that we
  // support overriding the response callback.
  // A delegate for use in testing, to intercept the call to SendResponse.
  class DelegateForTests {
   public:
    virtual void OnSendResponse(UIThreadExtensionFunction* function,
                                bool success,
                                bool bad_message) = 0;
  };

  UIThreadExtensionFunction();

  UIThreadExtensionFunction* AsUIThreadExtensionFunction() override;

  bool PreRunValidation(std::string* error) override;

  void set_test_delegate(DelegateForTests* delegate) {
    delegate_ = delegate;
  }

  // Called when a message was received.
  // Should return true if it processed the message.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Set the browser context which contains the extension that has originated
  // this function call.
  void set_browser_context(content::BrowserContext* context) {
    context_ = context;
  }
  content::BrowserContext* browser_context() const { return context_; }

  // DEPRECATED: Please use render_frame_host().
  // TODO(devlin): Remove this once all callers are updated to use
  // render_frame_host().
  content::RenderViewHost* render_view_host_do_not_use() const;

  void SetRenderFrameHost(content::RenderFrameHost* render_frame_host);
  content::RenderFrameHost* render_frame_host() const {
    return render_frame_host_;
  }

  void set_dispatcher(const base::WeakPtr<
      extensions::ExtensionFunctionDispatcher>& dispatcher) {
    dispatcher_ = dispatcher;
  }
  extensions::ExtensionFunctionDispatcher* dispatcher() const {
    return dispatcher_.get();
  }

  void set_is_from_service_worker(bool value) {
    is_from_service_worker_ = value;
  }

  // Gets the "current" web contents if any. If there is no associated web
  // contents then defaults to the foremost one.
  // NOTE: "current" can mean different things in different contexts. You
  // probably want to use GetSenderWebContents().
  virtual content::WebContents* GetAssociatedWebContents();

  // Returns the web contents associated with the sending |render_frame_host_|.
  // This can be null.
  content::WebContents* GetSenderWebContents();

 protected:
  // Emits a message to the extension's devtools console.
  void WriteToConsole(content::ConsoleMessageLevel level,
                      const std::string& message);

  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<UIThreadExtensionFunction>;

  ~UIThreadExtensionFunction() override;

  void SendResponse(bool success) override;

  // Sets the Blob UUIDs whose ownership is being transferred to the renderer.
  void SetTransferredBlobUUIDs(const std::vector<std::string>& blob_uuids);

  // The BrowserContext of this function's extension.
  // TODO(devlin): Grr... protected members. Move this to be private.
  content::BrowserContext* context_;

 private:
  class RenderFrameHostTracker;

  void Destruct() const override;

  // The dispatcher that will service this extension function call.
  base::WeakPtr<extensions::ExtensionFunctionDispatcher> dispatcher_;

  // The RenderFrameHost we will send responses to.
  content::RenderFrameHost* render_frame_host_;

  // Whether or not this ExtensionFunction was called by an extension Service
  // Worker.
  bool is_from_service_worker_;

  std::unique_ptr<RenderFrameHostTracker> tracker_;

  DelegateForTests* delegate_;

  // The blobs transferred to the renderer process.
  std::vector<std::string> transferred_blob_uuids_;

  DISALLOW_COPY_AND_ASSIGN(UIThreadExtensionFunction);
};

// Extension functions that run on the IO thread. This type of function avoids
// a roundtrip to and from the UI thread (because communication with the
// extension process happens on the IO thread). It's intended to be used when
// performance is critical (e.g. the webRequest API which can block network
// requests). Generally, UIThreadExtensionFunction is more appropriate and will
// be easier to use and interface with the rest of the browser.
class IOThreadExtensionFunction : public ExtensionFunction {
 public:
  IOThreadExtensionFunction();

  IOThreadExtensionFunction* AsIOThreadExtensionFunction() override;

  void set_ipc_sender(
      base::WeakPtr<extensions::IOThreadExtensionMessageFilter> ipc_sender,
      int routing_id) {
    ipc_sender_ = ipc_sender;
    routing_id_ = routing_id;
  }

  base::WeakPtr<extensions::IOThreadExtensionMessageFilter> ipc_sender_weak()
      const {
    return ipc_sender_;
  }

  int routing_id() const { return routing_id_; }

  void set_extension_info_map(const extensions::InfoMap* extension_info_map) {
    extension_info_map_ = extension_info_map;
  }
  const extensions::InfoMap* extension_info_map() const {
    return extension_info_map_.get();
  }

 protected:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<IOThreadExtensionFunction>;

  ~IOThreadExtensionFunction() override;

  void Destruct() const override;

  void SendResponse(bool success) override;

 private:
  base::WeakPtr<extensions::IOThreadExtensionMessageFilter> ipc_sender_;
  int routing_id_;

  scoped_refptr<const extensions::InfoMap> extension_info_map_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadExtensionFunction);
};

// Base class for an extension function that runs asynchronously *relative to
// the browser's UI thread*.
class AsyncExtensionFunction : public UIThreadExtensionFunction {
 public:
  AsyncExtensionFunction();

 protected:
  ~AsyncExtensionFunction() override;

  // Deprecated: Override UIThreadExtensionFunction and implement Run() instead.
  //
  // AsyncExtensionFunctions implement this method. Return true to indicate that
  // nothing has gone wrong yet; SendResponse must be called later. Return false
  // to respond immediately with an error.
  virtual bool RunAsync() = 0;

  // ValidationFailure override to match RunAsync().
  static bool ValidationFailure(AsyncExtensionFunction* function);

 private:
  // If you're hitting a compile error here due to "final" - great! You're
  // doing the right thing, you just need to extend UIThreadExtensionFunction
  // instead of AsyncExtensionFunction.
  ResponseAction Run() final;

  DISALLOW_COPY_AND_ASSIGN(AsyncExtensionFunction);
};

// A SyncExtensionFunction is an ExtensionFunction that runs synchronously
// *relative to the browser's UI thread*. Note that this has nothing to do with
// running synchronously relative to the extension process. From the extension
// process's point of view, the function is still asynchronous.
//
// This kind of function is convenient for implementing simple APIs that just
// need to interact with things on the browser UI thread.
class SyncExtensionFunction : public UIThreadExtensionFunction {
 public:
  SyncExtensionFunction();

 protected:
  ~SyncExtensionFunction() override;

  // Deprecated: Override UIThreadExtensionFunction and implement Run() instead.
  //
  // SyncExtensionFunctions implement this method. Return true to respond
  // immediately with success, false to respond immediately with an error.
  virtual bool RunSync() = 0;

  // ValidationFailure override to match RunSync().
  static bool ValidationFailure(SyncExtensionFunction* function);

 private:
  // If you're hitting a compile error here due to "final" - great! You're
  // doing the right thing, you just need to extend UIThreadExtensionFunction
  // instead of SyncExtensionFunction.
  ResponseAction Run() final;

  DISALLOW_COPY_AND_ASSIGN(SyncExtensionFunction);
};

class SyncIOThreadExtensionFunction : public IOThreadExtensionFunction {
 public:
  SyncIOThreadExtensionFunction();

 protected:
  ~SyncIOThreadExtensionFunction() override;

  // Deprecated: Override IOThreadExtensionFunction and implement Run() instead.
  //
  // SyncIOThreadExtensionFunctions implement this method. Return true to
  // respond immediately with success, false to respond immediately with an
  // error.
  virtual bool RunSync() = 0;

  // ValidationFailure override to match RunSync().
  static bool ValidationFailure(SyncIOThreadExtensionFunction* function);

 private:
  // If you're hitting a compile error here due to "final" - great! You're
  // doing the right thing, you just need to extend IOThreadExtensionFunction
  // instead of SyncIOExtensionFunction.
  ResponseAction Run() final;

  DISALLOW_COPY_AND_ASSIGN(SyncIOThreadExtensionFunction);
};

#endif  // EXTENSIONS_BROWSER_EXTENSION_FUNCTION_H_
