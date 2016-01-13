// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_COORDINATOR_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_COORDINATOR_H_

#include <set>
#include <map>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

#include "ppapi/cpp/completion_callback.h"

#include "ppapi/native_client/src/trusted/plugin/callback_source.h"
#include "ppapi/native_client/src/trusted/plugin/file_downloader.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_subprocess.h"
#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"
#include "ppapi/native_client/src/trusted/plugin/pnacl_resources.h"

struct PP_PNaClOptions;

namespace plugin {

class Plugin;
class PnaclCoordinator;
class PnaclTranslateThread;
class TempFile;

// A class invoked by Plugin to handle PNaCl client-side translation.
// Usage:
// (1) Invoke the factory method, e.g.,
//     PnaclCoordinator* coord = BitcodeToNative(plugin,
//                                               "http://foo.com/my.pexe",
//                                               pnacl_options,
//                                               TranslateNotifyCallback);
// (2) TranslateNotifyCallback gets invoked when translation is complete.
//     If the translation was successful, the pp_error argument is PP_OK.
//     Other values indicate errors.
// (3) After finish_callback runs, get the file descriptor of the translated
//     nexe, e.g.,
//     fd = coord->ReleaseTranslatedFD();
// (4) Load the nexe from "fd".
// (5) delete coord.
//
// Translation proceeds in two steps:
// (1) llc translates the bitcode in pexe_url_ to an object in obj_file_.
// (2) ld links the object code in obj_file_ and produces a nexe in nexe_file_.
class PnaclCoordinator: public CallbackSource<FileStreamData> {
 public:
  // Maximum number of object files passable to the translator. Cannot be
  // changed without changing the RPC signatures.
  const static size_t kMaxTranslatorObjectFiles = 16;
  virtual ~PnaclCoordinator();

  // The factory method for translations.
  static PnaclCoordinator* BitcodeToNative(
      Plugin* plugin,
      const nacl::string& pexe_url,
      const PP_PNaClOptions& pnacl_options,
      const pp::CompletionCallback& translate_notify_callback);

  // Call this to take ownership of the FD of the translated nexe after
  // BitcodeToNative has completed (and the finish_callback called).
  PP_FileHandle TakeTranslatedFileHandle();

  // Implement FileDownloader's template of the CallbackSource interface.
  // This method returns a callback which will be called by the FileDownloader
  // to stream the bitcode data as it arrives. The callback
  // (BitcodeStreamGotData) passes it to llc over SRPC.
  StreamCallback GetCallback();

  // Return a callback that should be notified when |bytes_compiled| bytes
  // have been compiled.
  pp::CompletionCallback GetCompileProgressCallback(int64_t bytes_compiled);

  // Get the last known load progress.
  void GetCurrentProgress(int64_t* bytes_loaded, int64_t* bytes_total);

  // Return true if we should delay the progress event reporting.
  // This delay approximates:
  // - the size of the buffer of bytes sent but not-yet-compiled by LLC.
  // - the linking time.
  bool ShouldDelayProgressEvent() {
    const uint32_t kProgressEventSlopPct = 5;
    return ((expected_pexe_size_ - pexe_bytes_compiled_) * 100 /
            expected_pexe_size_) < kProgressEventSlopPct;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclCoordinator);

  // BitcodeToNative is the factory method for PnaclCoordinators.
  // Therefore the constructor is private.
  PnaclCoordinator(Plugin* plugin,
                   const nacl::string& pexe_url,
                   const PP_PNaClOptions& pnacl_options,
                   const pp::CompletionCallback& translate_notify_callback);

  // Invoke to issue a GET request for bitcode.
  void OpenBitcodeStream();
  // Invoked when we've started an URL fetch for the pexe to check for
  // caching metadata.
  void BitcodeStreamDidOpen(int32_t pp_error);

  // Callback for when the resource info JSON file has been read.
  void ResourceInfoWasRead(int32_t pp_error);

  // Callback for when llc and ld have been downloaded.
  void ResourcesDidLoad(int32_t pp_error);
  // Invoked when we've gotten a temp FD for the nexe, either with the nexe
  // data, or a writeable fd to save to.
  void NexeFdDidOpen(int32_t pp_error);
  // Invoked when a pexe data chunk arrives (when using streaming translation)
  void BitcodeStreamGotData(int32_t pp_error, FileStreamData data);
  // Invoked when a pexe data chunk is compiled.
  void BitcodeGotCompiled(int32_t pp_error, int64_t bytes_compiled);
  // Invoked when the pexe download finishes (using streaming translation)
  void BitcodeStreamDidFinish(int32_t pp_error);
  // Once llc and ld nexes have been loaded and the two temporary files have
  // been created, this starts the translation.  Translation starts two
  // subprocesses, one for llc and one for ld.
  void RunTranslate(int32_t pp_error);

  // Invoked when translation is finished.
  void TranslateFinished(int32_t pp_error);

  // Invoked when the read descriptor for nexe_file_ is created.
  void NexeReadDidOpen(int32_t pp_error);

  // Bring control back to the plugin by invoking the
  // |translate_notify_callback_|.  This does not set the ErrorInfo report,
  // it is assumed that it was already set.
  void ExitWithError();
  // Run |translate_notify_callback_| with an error condition that is not
  // PPAPI specific.  Also set ErrorInfo report.
  void ReportNonPpapiError(PP_NaClError err, const nacl::string& message);
  // Run when faced with a PPAPI error condition. Bring control back to the
  // plugin by invoking the |translate_notify_callback_|.
  // Also set ErrorInfo report.
  void ReportPpapiError(PP_NaClError err,
                        int32_t pp_error, const nacl::string& message);


  // Keeps track of the pp_error upon entry to TranslateFinished,
  // for inspection after cleanup.
  int32_t translate_finish_error_;

  // The plugin owning the nexe for which we are doing translation.
  Plugin* plugin_;

  pp::CompletionCallback translate_notify_callback_;
  // Set to true when the translation (if applicable) is finished and the nexe
  // file is loaded, (or when there was an error), and the browser has been
  // notified via ReportTranslationFinished. If it is not set before
  // plugin/coordinator destruction, the destructor will call
  // ReportTranslationFinished.
  bool translation_finished_reported_;
  // Threadsafety is required to support file lookups.
  pp::CompletionCallbackFactory<PnaclCoordinator,
                                pp::ThreadSafeThreadTraits> callback_factory_;

  // An auxiliary class that manages downloaded resources (llc and ld nexes).
  nacl::scoped_ptr<PnaclResources> resources_;

  // The URL for the pexe file.
  nacl::string pexe_url_;
  // Options for translation.
  PP_PNaClOptions pnacl_options_;
  // Architecture-specific attributes used for translation. These are
  // supplied by Chrome, not the developer, and are therefore different
  // from PNaCl options.
  nacl::string architecture_attributes_;

  // Object file, produced by the translator and consumed by the linker.
  std::vector<TempFile*> obj_files_;
  nacl::scoped_ptr<nacl::DescWrapper> invalid_desc_wrapper_;
  // Number of split modules (threads) for llc
  int split_module_count_;

  // Translated nexe file, produced by the linker.
  nacl::scoped_ptr<TempFile> temp_nexe_file_;
  // Passed to the browser, which sets it to true if there is a translation
  // cache hit.
  PP_Bool is_cache_hit_;

  // Downloader for streaming translation
  nacl::scoped_ptr<FileDownloader> streaming_downloader_;

  // Used to report information when errors (PPAPI or otherwise) are reported.
  ErrorInfo error_info_;

  // True if an error was already reported, and translate_notify_callback_
  // was already run/consumed.
  bool error_already_reported_;

  // State for timing and size information for UMA stats.
  int64_t pnacl_init_time_;
  int64_t pexe_size_;  // Count as we stream -- will converge to pexe size.
  int64_t pexe_bytes_compiled_;  // Count as we compile.
  int64_t expected_pexe_size_;   // Expected download total (-1 if unknown).

  // The helper thread used to do translations via SRPC.
  // It accesses fields of PnaclCoordinator so it must have a
  // shorter lifetime.
  nacl::scoped_ptr<PnaclTranslateThread> translate_thread_;
};

//----------------------------------------------------------------------

}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_COORDINATOR_H_
