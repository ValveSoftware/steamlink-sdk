// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/page_capture/page_capture_api.h"

#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mhtml_generation_params.h"
#include "extensions/common/extension_messages.h"

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::WebContents;
using extensions::PageCaptureSaveAsMHTMLFunction;
using storage::ShareableFileReference;

namespace SaveAsMHTML = extensions::api::page_capture::SaveAsMHTML;

namespace {

const char kFileTooBigError[] = "The MHTML file generated is too big.";
const char kMHTMLGenerationFailedError[] = "Failed to generate MHTML.";
const char kTemporaryFileError[] = "Failed to create a temporary file.";
const char kTabClosedError[] = "Cannot find the tab for this request.";

}  // namespace

static PageCaptureSaveAsMHTMLFunction::TestDelegate* test_delegate_ = NULL;

PageCaptureSaveAsMHTMLFunction::PageCaptureSaveAsMHTMLFunction() {
}

PageCaptureSaveAsMHTMLFunction::~PageCaptureSaveAsMHTMLFunction() {
  if (mhtml_file_.get()) {
    storage::ShareableFileReference* to_release = mhtml_file_.get();
    to_release->AddRef();
    mhtml_file_ = NULL;
    BrowserThread::ReleaseSoon(BrowserThread::IO, FROM_HERE, to_release);
  }
}

void PageCaptureSaveAsMHTMLFunction::SetTestDelegate(TestDelegate* delegate) {
  test_delegate_ = delegate;
}

bool PageCaptureSaveAsMHTMLFunction::RunAsync() {
  params_ = SaveAsMHTML::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  AddRef();  // Balanced in ReturnFailure/ReturnSuccess()

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&PageCaptureSaveAsMHTMLFunction::CreateTemporaryFile, this));
  return true;
}

bool PageCaptureSaveAsMHTMLFunction::OnMessageReceived(
    const IPC::Message& message) {
  if (message.type() != ExtensionHostMsg_ResponseAck::ID)
    return false;

  int message_request_id;
  base::PickleIterator iter(message);
  if (!iter.ReadInt(&message_request_id)) {
    NOTREACHED() << "malformed extension message";
    return true;
  }

  if (message_request_id != request_id())
    return false;

  // The extension process has processed the response and has created a
  // reference to the blob, it is safe for us to go away.
  Release();  // Balanced in Run()

  return true;
}

void PageCaptureSaveAsMHTMLFunction::CreateTemporaryFile() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  bool success = base::CreateTemporaryFile(&mhtml_path_);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PageCaptureSaveAsMHTMLFunction::TemporaryFileCreated, this,
                 success));
}

void PageCaptureSaveAsMHTMLFunction::TemporaryFileCreated(bool success) {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    if (success) {
      // Setup a ShareableFileReference so the temporary file gets deleted
      // once it is no longer used.
      mhtml_file_ = ShareableFileReference::GetOrCreate(
          mhtml_path_,
          ShareableFileReference::DELETE_ON_FINAL_RELEASE,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)
              .get());
    }
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&PageCaptureSaveAsMHTMLFunction::TemporaryFileCreated, this,
                   success));
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success) {
    ReturnFailure(kTemporaryFileError);
    return;
  }

  if (test_delegate_)
    test_delegate_->OnTemporaryFileCreated(mhtml_path_);

  WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    ReturnFailure(kTabClosedError);
    return;
  }

  web_contents->GenerateMHTML(
      content::MHTMLGenerationParams(mhtml_path_),
      base::Bind(&PageCaptureSaveAsMHTMLFunction::MHTMLGenerated, this));
}

void PageCaptureSaveAsMHTMLFunction::MHTMLGenerated(int64_t mhtml_file_size) {
  if (mhtml_file_size <= 0) {
    ReturnFailure(kMHTMLGenerationFailedError);
    return;
  }

  if (mhtml_file_size > std::numeric_limits<int>::max()) {
    ReturnFailure(kFileTooBigError);
    return;
  }

  ReturnSuccess(mhtml_file_size);
}

void PageCaptureSaveAsMHTMLFunction::ReturnFailure(const std::string& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  error_ = error;

  SendResponse(false);

  // Must not Release() here, OnMessageReceived will call it eventually.
}

void PageCaptureSaveAsMHTMLFunction::ReturnSuccess(int64_t file_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = GetWebContents();
  if (!web_contents || !render_frame_host()) {
    ReturnFailure(kTabClosedError);
    return;
  }

  int child_id = render_frame_host()->GetProcess()->GetID();
  ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(
      child_id, mhtml_path_);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("mhtmlFilePath", mhtml_path_.value());
  dict->SetInteger("mhtmlFileLength", file_size);
  SetResult(std::move(dict));

  SendResponse(true);

  // Note that we'll wait for a response ack message received in
  // OnMessageReceived before we call Release() (to prevent the blob file from
  // being deleted).
}

WebContents* PageCaptureSaveAsMHTMLFunction::GetWebContents() {
  Browser* browser = NULL;
  content::WebContents* web_contents = NULL;

  if (!ExtensionTabUtil::GetTabById(params_->details.tab_id,
                                    GetProfile(),
                                    include_incognito(),
                                    &browser,
                                    NULL,
                                    &web_contents,
                                    NULL)) {
    return NULL;
  }
  return web_contents;
}
