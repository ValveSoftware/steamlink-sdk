// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_TRANSLATE_THREAD_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_TRANSLATE_THREAD_H_

#include <deque>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

#include "ppapi/cpp/completion_callback.h"

#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"
#include "ppapi/native_client/src/trusted/plugin/service_runtime.h"

struct PP_PNaClOptions;

namespace nacl {
class DescWrapper;
}


namespace plugin {

class NaClSubprocess;
class Plugin;
class PnaclCoordinator;
class PnaclResources;
class TempFile;

class PnaclTranslateThread {
 public:
  PnaclTranslateThread();
  ~PnaclTranslateThread();

  // Start the translation process. It will continue to run and consume data
  // as it is passed in with PutBytes.
  void RunTranslate(const pp::CompletionCallback& finish_callback,
                    const std::vector<TempFile*>* obj_files,
                    TempFile* nexe_file,
                    nacl::DescWrapper* invalid_desc_wrapper,
                    ErrorInfo* error_info,
                    PnaclResources* resources,
                    PP_PNaClOptions* pnacl_options,
                    const nacl::string &architecture_attributes,
                    PnaclCoordinator* coordinator,
                    Plugin* plugin);

  // Kill the llc and/or ld subprocesses. This happens by closing the command
  // channel on the plugin side, which causes the trusted code in the nexe to
  // exit, which will cause any pending SRPCs to error. Because this is called
  // on the main thread, the translation thread must not use the subprocess
  // objects without the lock, other than InvokeSrpcMethod, which does not
  // race with service runtime shutdown.
  void AbortSubprocesses();

  // Send bitcode bytes to the translator. Called from the main thread.
  void PutBytes(std::vector<char>* data, int count);

  int64_t GetCompileTime() const { return compile_time_; }

 private:
  // Helper thread entry point for translation. Takes a pointer to
  // PnaclTranslateThread and calls DoTranslate().
  static void WINAPI DoTranslateThread(void* arg);
  // Runs the streaming translation. Called from the helper thread.
  void DoTranslate() ;
  // Signal that Pnacl translation failed, from the translation thread only.
  void TranslateFailed(PP_NaClError err_code,
                       const nacl::string& error_string);
  // Run the LD subprocess, returning true on success.
  // On failure, it returns false and runs the callback.
  bool RunLdSubprocess();


  // Callback to run when tasks are completed or an error has occurred.
  pp::CompletionCallback report_translate_finished_;

  nacl::scoped_ptr<NaClThread> translate_thread_;

  // Used to guard llc_subprocess and ld_subprocess
  struct NaClMutex subprocess_mu_;
  nacl::scoped_ptr<NaClSubprocess> llc_subprocess_;
  nacl::scoped_ptr<NaClSubprocess> ld_subprocess_;
  // Used to ensure the subprocesses don't get shutdown more than once.
  bool llc_subprocess_active_;
  bool ld_subprocess_active_;

  bool subprocesses_aborted_;

  // Condition variable to synchronize communication with the SRPC thread.
  // SRPC thread waits on this condvar if data_buffers_ is empty (meaning
  // there is no bitcode to send to the translator), and the main thread
  // appends to data_buffers_ and signals it when it receives bitcode.
  struct NaClCondVar buffer_cond_;
  // Mutex for buffer_cond_.
  struct NaClMutex cond_mu_;
  // Data buffers from FileDownloader are enqueued here to pass from the
  // main thread to the SRPC thread. Protected by cond_mu_
  std::deque<std::vector<char> > data_buffers_;
  // Whether all data has been downloaded and copied to translation thread.
  // Associated with buffer_cond_
  bool done_;

  int64_t compile_time_;

  // Data about the translation files, owned by the coordinator
  const std::vector<TempFile*>* obj_files_;
  TempFile* nexe_file_;
  nacl::DescWrapper* invalid_desc_wrapper_;
  ErrorInfo* coordinator_error_info_;
  PnaclResources* resources_;
  PP_PNaClOptions* pnacl_options_;
  nacl::string architecture_attributes_;
  PnaclCoordinator* coordinator_;
  Plugin* plugin_;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclTranslateThread);
};

}
#endif // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_TRANSLATE_THREAD_H_
