// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_UTILITY_CLIENT_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_UTILITY_CLIENT_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"

// Writes a disk image to a device inside the utility process.
class ImageWriterUtilityClient : public content::UtilityProcessHostClient {
 public:
  typedef base::Callback<void()> CancelCallback;
  typedef base::Callback<void()> SuccessCallback;
  typedef base::Callback<void(int64_t)> ProgressCallback;
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  ImageWriterUtilityClient();

  // Starts the write.
  // |progress_callback|: Called periodically with the count of bytes processed.
  // |success_callback|: Called at successful completion.
  // |error_callback|: Called with an error message on failure.
  // |source|: The path to the source file to read data from.
  // |target|: The path to the target device to write the image file to.
  virtual void Write(const ProgressCallback& progress_callback,
                     const SuccessCallback& success_callback,
                     const ErrorCallback& error_callback,
                     const base::FilePath& source,
                     const base::FilePath& target);

  // Starts a verification.
  // |progress_callback|: Called periodically with the count of bytes processed.
  // |success_callback|: Called at successful completion.
  // |error_callback|: Called with an error message on failure.
  // |source|: The path to the source file to read data from.
  // |target|: The path to the target device to write the image file to.
  virtual void Verify(const ProgressCallback& progress_callback,
                      const SuccessCallback& success_callback,
                      const ErrorCallback& error_callback,
                      const base::FilePath& source,
                      const base::FilePath& target);

  // Cancels any pending write or verification.
  // |cancel_callback|: Called when the cancel has actually occurred.
  virtual void Cancel(const CancelCallback& cancel_callback);

  // Shuts down the Utility thread that may have been created.
  virtual void Shutdown();

 protected:
  // It's a reference-counted object, so destructor is not public.
  ~ImageWriterUtilityClient() override;

 private:
  // Ensures the UtilityProcessHost has been created.
  void StartHost();

  // UtilityProcessHostClient implementation.
  void OnProcessCrashed(int exit_code) override;
  void OnProcessLaunchFailed(int error_code) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  virtual bool Send(IPC::Message* msg);

  // IPC message handlers.
  void OnWriteImageSucceeded();
  void OnWriteImageCancelled();
  void OnWriteImageFailed(const std::string& message);
  void OnWriteImageProgress(int64_t progress);

  CancelCallback cancel_callback_;
  ProgressCallback progress_callback_;
  SuccessCallback success_callback_;
  ErrorCallback error_callback_;

  base::WeakPtr<content::UtilityProcessHost> utility_process_host_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ImageWriterUtilityClient);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_UTILITY_CLIENT_H_
