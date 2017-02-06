// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function.h"

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;
using extensions::ErrorUtils;
using extensions::ExtensionAPI;
using extensions::Feature;

namespace {

// Logs UMA about the performance for a given extension function run.
void LogUma(bool success,
            base::TimeDelta elapsed_time,
            extensions::functions::HistogramValue histogram_value) {
  // Note: Certain functions perform actions that are inherently slow - such as
  // anything waiting on user action. As such, we can't always assume that a
  // long execution time equates to a poorly-performing function.
  if (success) {
    if (elapsed_time < base::TimeDelta::FromMilliseconds(1)) {
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Extensions.Functions.SucceededTime.LessThan1ms", histogram_value);
    } else if (elapsed_time < base::TimeDelta::FromMilliseconds(5)) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("Extensions.Functions.SucceededTime.1msTo5ms",
                                  histogram_value);
    } else if (elapsed_time < base::TimeDelta::FromMilliseconds(10)) {
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Extensions.Functions.SucceededTime.5msTo10ms", histogram_value);
    } else {
      UMA_HISTOGRAM_SPARSE_SLOWLY("Extensions.Functions.SucceededTime.Over10ms",
                                  histogram_value);
    }
    UMA_HISTOGRAM_TIMES("Extensions.Functions.SucceededTotalExecutionTime",
                        elapsed_time);
  } else {
    if (elapsed_time < base::TimeDelta::FromMilliseconds(1)) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("Extensions.Functions.FailedTime.LessThan1ms",
                                  histogram_value);
    } else if (elapsed_time < base::TimeDelta::FromMilliseconds(5)) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("Extensions.Functions.FailedTime.1msTo5ms",
                                  histogram_value);
    } else if (elapsed_time < base::TimeDelta::FromMilliseconds(10)) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("Extensions.Functions.FailedTime.5msTo10ms",
                                  histogram_value);
    } else {
      UMA_HISTOGRAM_SPARSE_SLOWLY("Extensions.Functions.FailedTime.Over10ms",
                                  histogram_value);
    }
    UMA_HISTOGRAM_TIMES("Extensions.Functions.FailedTotalExecutionTime",
                        elapsed_time);
  }
}

class ArgumentListResponseValue
    : public ExtensionFunction::ResponseValueObject {
 public:
  ArgumentListResponseValue(const std::string& function_name,
                            const char* title,
                            ExtensionFunction* function,
                            std::unique_ptr<base::ListValue> result)
      : function_name_(function_name), title_(title) {
    if (function->GetResultList()) {
      DCHECK_EQ(function->GetResultList(), result.get())
          << "The result set on this function (" << function_name_ << ") "
          << "either by calling SetResult() or directly modifying |result_| is "
          << "different to the one passed to " << title_ << "(). "
          << "The best way to fix this problem is to exclusively use " << title_
          << "(). SetResult() and |result_| are deprecated.";
    } else {
      function->SetResultList(std::move(result));
    }
    // It would be nice to DCHECK(error.empty()) but some legacy extension
    // function implementations... I'm looking at chrome.input.ime... do this
    // for some reason.
  }

  ~ArgumentListResponseValue() override {}

  bool Apply() override { return true; }

 private:
  std::string function_name_;
  const char* title_;
};

class ErrorWithArgumentsResponseValue : public ArgumentListResponseValue {
 public:
  ErrorWithArgumentsResponseValue(const std::string& function_name,
                                  const char* title,
                                  ExtensionFunction* function,
                                  std::unique_ptr<base::ListValue> result,
                                  const std::string& error)
      : ArgumentListResponseValue(function_name,
                                  title,
                                  function,
                                  std::move(result)) {
    function->SetError(error);
  }

  ~ErrorWithArgumentsResponseValue() override {}

  bool Apply() override { return false; }
};

class ErrorResponseValue : public ExtensionFunction::ResponseValueObject {
 public:
  ErrorResponseValue(ExtensionFunction* function, const std::string& error) {
    // It would be nice to DCHECK(!error.empty()) but too many legacy extension
    // function implementations don't set error but signal failure.
    function->SetError(error);
  }

  ~ErrorResponseValue() override {}

  bool Apply() override { return false; }
};

class BadMessageResponseValue : public ExtensionFunction::ResponseValueObject {
 public:
  explicit BadMessageResponseValue(ExtensionFunction* function) {
    function->set_bad_message(true);
    NOTREACHED() << function->name() << ": bad message";
  }

  ~BadMessageResponseValue() override {}

  bool Apply() override { return false; }
};

class RespondNowAction : public ExtensionFunction::ResponseActionObject {
 public:
  typedef base::Callback<void(bool)> SendResponseCallback;
  RespondNowAction(ExtensionFunction::ResponseValue result,
                   const SendResponseCallback& send_response)
      : result_(std::move(result)), send_response_(send_response) {}
  ~RespondNowAction() override {}

  void Execute() override { send_response_.Run(result_->Apply()); }

 private:
  ExtensionFunction::ResponseValue result_;
  SendResponseCallback send_response_;
};

class RespondLaterAction : public ExtensionFunction::ResponseActionObject {
 public:
  ~RespondLaterAction() override {}

  void Execute() override {}
};

// Used in implementation of ScopedUserGestureForTests.
class UserGestureForTests {
 public:
  static UserGestureForTests* GetInstance();

  // Returns true if there is at least one ScopedUserGestureForTests object
  // alive.
  bool HaveGesture();

  // These should be called when a ScopedUserGestureForTests object is
  // created/destroyed respectively.
  void IncrementCount();
  void DecrementCount();

 private:
  UserGestureForTests();
  friend struct base::DefaultSingletonTraits<UserGestureForTests>;

  base::Lock lock_; // for protecting access to count_
  int count_;
};

// static
UserGestureForTests* UserGestureForTests::GetInstance() {
  return base::Singleton<UserGestureForTests>::get();
}

UserGestureForTests::UserGestureForTests() : count_(0) {}

bool UserGestureForTests::HaveGesture() {
  base::AutoLock autolock(lock_);
  return count_ > 0;
}

void UserGestureForTests::IncrementCount() {
  base::AutoLock autolock(lock_);
  ++count_;
}

void UserGestureForTests::DecrementCount() {
  base::AutoLock autolock(lock_);
  --count_;
}


}  // namespace

// static
bool ExtensionFunction::ignore_all_did_respond_for_testing_do_not_use = false;

// static
void ExtensionFunctionDeleteTraits::Destruct(const ExtensionFunction* x) {
  x->Destruct();
}

// Helper class to track the lifetime of ExtensionFunction's RenderFrameHost and
// notify the function when it is deleted, as well as forwarding any messages
// to the ExtensionFunction.
class UIThreadExtensionFunction::RenderFrameHostTracker
    : public content::WebContentsObserver {
 public:
  explicit RenderFrameHostTracker(UIThreadExtensionFunction* function)
      : content::WebContentsObserver(
            WebContents::FromRenderFrameHost(function->render_frame_host())),
        function_(function) {
  }

 private:
  // content::WebContentsObserver:
  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    if (render_frame_host == function_->render_frame_host())
      function_->SetRenderFrameHost(nullptr);
  }

  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override {
    return render_frame_host == function_->render_frame_host() &&
        function_->OnMessageReceived(message);
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    return function_->OnMessageReceived(message);
  }

  UIThreadExtensionFunction* function_;  // Owns us.

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostTracker);
};

ExtensionFunction::ExtensionFunction()
    : request_id_(-1),
      profile_id_(NULL),
      name_(""),
      has_callback_(false),
      include_incognito_(false),
      user_gesture_(false),
      bad_message_(false),
      histogram_value_(extensions::functions::UNKNOWN),
      source_tab_id_(-1),
      source_context_type_(Feature::UNSPECIFIED_CONTEXT),
      source_process_id_(-1),
      did_respond_(false) {}

ExtensionFunction::~ExtensionFunction() {
}

UIThreadExtensionFunction* ExtensionFunction::AsUIThreadExtensionFunction() {
  return NULL;
}

IOThreadExtensionFunction* ExtensionFunction::AsIOThreadExtensionFunction() {
  return NULL;
}

bool ExtensionFunction::HasPermission() {
  Feature::Availability availability =
      ExtensionAPI::GetSharedInstance()->IsAvailable(
          name_, extension_.get(), source_context_type_, source_url());
  return availability.is_available();
}

void ExtensionFunction::OnQuotaExceeded(const std::string& violation_error) {
  error_ = violation_error;
  SendResponse(false);
}

void ExtensionFunction::SetArgs(const base::ListValue* args) {
  DCHECK(!args_.get());  // Should only be called once.
  args_ = args->CreateDeepCopy();
}

void ExtensionFunction::SetResult(std::unique_ptr<base::Value> result) {
  results_.reset(new base::ListValue());
  results_->Append(std::move(result));
}

void ExtensionFunction::SetResultList(
    std::unique_ptr<base::ListValue> results) {
  results_ = std::move(results);
}

const base::ListValue* ExtensionFunction::GetResultList() const {
  return results_.get();
}

std::string ExtensionFunction::GetError() const {
  return error_;
}

void ExtensionFunction::SetError(const std::string& error) {
  error_ = error;
}

bool ExtensionFunction::user_gesture() const {
  return user_gesture_ || UserGestureForTests::GetInstance()->HaveGesture();
}

ExtensionFunction::ResponseValue ExtensionFunction::NoArguments() {
  return ResponseValue(new ArgumentListResponseValue(
      name(), "NoArguments", this, base::WrapUnique(new base::ListValue())));
}

ExtensionFunction::ResponseValue ExtensionFunction::OneArgument(
    std::unique_ptr<base::Value> arg) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(std::move(arg));
  return ResponseValue(new ArgumentListResponseValue(name(), "OneArgument",
                                                     this, std::move(args)));
}

ExtensionFunction::ResponseValue ExtensionFunction::TwoArguments(
    std::unique_ptr<base::Value> arg1,
    std::unique_ptr<base::Value> arg2) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(std::move(arg1));
  args->Append(std::move(arg2));
  return ResponseValue(new ArgumentListResponseValue(name(), "TwoArguments",
                                                     this, std::move(args)));
}

ExtensionFunction::ResponseValue ExtensionFunction::ArgumentList(
    std::unique_ptr<base::ListValue> args) {
  return ResponseValue(new ArgumentListResponseValue(name(), "ArgumentList",
                                                     this, std::move(args)));
}

ExtensionFunction::ResponseValue ExtensionFunction::Error(
    const std::string& error) {
  return ResponseValue(new ErrorResponseValue(this, error));
}

ExtensionFunction::ResponseValue ExtensionFunction::Error(
    const std::string& format,
    const std::string& s1) {
  return ResponseValue(
      new ErrorResponseValue(this, ErrorUtils::FormatErrorMessage(format, s1)));
}

ExtensionFunction::ResponseValue ExtensionFunction::Error(
    const std::string& format,
    const std::string& s1,
    const std::string& s2) {
  return ResponseValue(new ErrorResponseValue(
      this, ErrorUtils::FormatErrorMessage(format, s1, s2)));
}

ExtensionFunction::ResponseValue ExtensionFunction::Error(
    const std::string& format,
    const std::string& s1,
    const std::string& s2,
    const std::string& s3) {
  return ResponseValue(new ErrorResponseValue(
      this, ErrorUtils::FormatErrorMessage(format, s1, s2, s3)));
}

ExtensionFunction::ResponseValue ExtensionFunction::ErrorWithArguments(
    std::unique_ptr<base::ListValue> args,
    const std::string& error) {
  return ResponseValue(new ErrorWithArgumentsResponseValue(
      name(), "ErrorWithArguments", this, std::move(args), error));
}

ExtensionFunction::ResponseValue ExtensionFunction::BadMessage() {
  return ResponseValue(new BadMessageResponseValue(this));
}

ExtensionFunction::ResponseAction ExtensionFunction::RespondNow(
    ResponseValue result) {
  return ResponseAction(new RespondNowAction(
      std::move(result), base::Bind(&ExtensionFunction::SendResponse, this)));
}

ExtensionFunction::ResponseAction ExtensionFunction::RespondLater() {
  return ResponseAction(new RespondLaterAction());
}

// static
ExtensionFunction::ResponseAction ExtensionFunction::ValidationFailure(
    ExtensionFunction* function) {
  return function->RespondNow(function->BadMessage());
}

void ExtensionFunction::Respond(ResponseValue result) {
  SendResponse(result->Apply());
}

bool ExtensionFunction::PreRunValidation(std::string* error) {
  return true;
}

ExtensionFunction::ResponseAction ExtensionFunction::RunWithValidation() {
  std::string error;
  if (!PreRunValidation(&error)) {
    DCHECK(!error.empty() || bad_message_);
    return bad_message_ ? ValidationFailure(this) : RespondNow(Error(error));
  }
  return Run();
}

bool ExtensionFunction::ShouldSkipQuotaLimiting() const {
  return false;
}

bool ExtensionFunction::HasOptionalArgument(size_t index) {
  base::Value* value;
  return args_->Get(index, &value) && !value->IsType(base::Value::TYPE_NULL);
}

void ExtensionFunction::SendResponseImpl(bool success) {
  DCHECK(!response_callback_.is_null());

  ResponseType type = success ? SUCCEEDED : FAILED;
  if (bad_message_) {
    type = BAD_MESSAGE;
    LOG(ERROR) << "Bad extension message " << name_;
  }

  // If results were never set, we send an empty argument list.
  if (!results_)
    results_.reset(new base::ListValue());

  response_callback_.Run(type, *results_, GetError(), histogram_value());
  LogUma(success, timer_.Elapsed(), histogram_value_);
}

void ExtensionFunction::OnRespondingLater(ResponseValue value) {
  SendResponse(value->Apply());
}

UIThreadExtensionFunction::UIThreadExtensionFunction()
    : context_(nullptr),
      render_frame_host_(nullptr),
      is_from_service_worker_(false),
      delegate_(nullptr) {}

UIThreadExtensionFunction::~UIThreadExtensionFunction() {
  if (dispatcher() && render_frame_host())
    dispatcher()->OnExtensionFunctionCompleted(extension());
  // The extension function should always respond to avoid leaks in the
  // renderer, dangling callbacks, etc. The exception is if the system is
  // shutting down.
  // TODO(devlin): Duplicate this check in IOThreadExtensionFunction. It's
  // tricky because checking IsShuttingDown has to be called from the UI thread.
  DCHECK(extensions::ExtensionsBrowserClient::Get()->IsShuttingDown() ||
         did_respond_ || ignore_all_did_respond_for_testing_do_not_use)
      << name_;
}

UIThreadExtensionFunction*
UIThreadExtensionFunction::AsUIThreadExtensionFunction() {
  return this;
}

bool UIThreadExtensionFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error))
    return false;

  // TODO(crbug.com/625646) This is a partial fix to avoid crashes when certain
  // extension functions run during shutdown. Browser or Notification creation
  // for example create a ScopedKeepAlive, which hit a CHECK if the browser is
  // shutting down. This fixes the current problem as the known issues happen
  // through synchronous calls from Run(), but posted tasks will not be covered.
  // A possible fix would involve refactoring ExtensionFunction: unrefcount
  // here and use weakptrs for the tasks, then have it owned by something that
  // will be destroyed naturally in the course of shut down.
  if (extensions::ExtensionsBrowserClient::Get()->IsShuttingDown()) {
    *error = "The browser is shutting down.";
    return false;
  }

  return true;
}

bool UIThreadExtensionFunction::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void UIThreadExtensionFunction::Destruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

content::RenderViewHost*
UIThreadExtensionFunction::render_view_host_do_not_use() const {
  return render_frame_host_ ? render_frame_host_->GetRenderViewHost() : nullptr;
}

void UIThreadExtensionFunction::SetRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  // An extension function from Service Worker does not have a RenderFrameHost.
  if (is_from_service_worker_) {
    DCHECK(!render_frame_host);
    return;
  }

  DCHECK_NE(render_frame_host_ == nullptr, render_frame_host == nullptr);
  render_frame_host_ = render_frame_host;
  tracker_.reset(
      render_frame_host ? new RenderFrameHostTracker(this) : nullptr);
}

content::WebContents* UIThreadExtensionFunction::GetAssociatedWebContents() {
  content::WebContents* web_contents = NULL;
  if (dispatcher())
    web_contents = dispatcher()->GetAssociatedWebContents();

  return web_contents;
}

content::WebContents* UIThreadExtensionFunction::GetSenderWebContents() {
  return render_frame_host_ ?
      content::WebContents::FromRenderFrameHost(render_frame_host_) : nullptr;
}

void UIThreadExtensionFunction::SendResponse(bool success) {
  DCHECK(!did_respond_) << name_;
  did_respond_ = true;
  if (delegate_)
    delegate_->OnSendResponse(this, success, bad_message_);
  else
    SendResponseImpl(success);

  if (!transferred_blob_uuids_.empty()) {
    DCHECK(!delegate_) << "Blob transfer not supported with test delegate.";
    render_frame_host_->Send(
        new ExtensionMsg_TransferBlobs(transferred_blob_uuids_));
  }
}

void UIThreadExtensionFunction::SetTransferredBlobUUIDs(
    const std::vector<std::string>& blob_uuids) {
  DCHECK(transferred_blob_uuids_.empty());  // Should only be called once.
  transferred_blob_uuids_ = blob_uuids;
}

void UIThreadExtensionFunction::WriteToConsole(
    content::ConsoleMessageLevel level,
    const std::string& message) {
  // Only the main frame handles dev tools messages.
  WebContents::FromRenderFrameHost(render_frame_host_)
      ->GetMainFrame()
      ->AddMessageToConsole(level, message);
}

IOThreadExtensionFunction::IOThreadExtensionFunction()
    : routing_id_(MSG_ROUTING_NONE) {
}

IOThreadExtensionFunction::~IOThreadExtensionFunction() {
}

IOThreadExtensionFunction*
IOThreadExtensionFunction::AsIOThreadExtensionFunction() {
  return this;
}

void IOThreadExtensionFunction::Destruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void IOThreadExtensionFunction::SendResponse(bool success) {
  DCHECK(!did_respond_) << name_;
  did_respond_ = true;
  SendResponseImpl(success);
}

AsyncExtensionFunction::AsyncExtensionFunction() {
}

AsyncExtensionFunction::~AsyncExtensionFunction() {
}

ExtensionFunction::ScopedUserGestureForTests::ScopedUserGestureForTests() {
  UserGestureForTests::GetInstance()->IncrementCount();
}

ExtensionFunction::ScopedUserGestureForTests::~ScopedUserGestureForTests() {
  UserGestureForTests::GetInstance()->DecrementCount();
}

ExtensionFunction::ResponseAction AsyncExtensionFunction::Run() {
  return RunAsync() ? RespondLater() : RespondNow(Error(error_));
}

// static
bool AsyncExtensionFunction::ValidationFailure(
    AsyncExtensionFunction* function) {
  return false;
}

SyncExtensionFunction::SyncExtensionFunction() {
}

SyncExtensionFunction::~SyncExtensionFunction() {
}

ExtensionFunction::ResponseAction SyncExtensionFunction::Run() {
  return RespondNow(RunSync() ? ArgumentList(std::move(results_))
                              : Error(error_));
}

// static
bool SyncExtensionFunction::ValidationFailure(SyncExtensionFunction* function) {
  return false;
}

SyncIOThreadExtensionFunction::SyncIOThreadExtensionFunction() {
}

SyncIOThreadExtensionFunction::~SyncIOThreadExtensionFunction() {
}

ExtensionFunction::ResponseAction SyncIOThreadExtensionFunction::Run() {
  return RespondNow(RunSync() ? ArgumentList(std::move(results_))
                              : Error(error_));
}

// static
bool SyncIOThreadExtensionFunction::ValidationFailure(
    SyncIOThreadExtensionFunction* function) {
  return false;
}
