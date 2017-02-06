// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SANDBOXED_UNPACKER_H_
#define EXTENSIONS_BROWSER_SANDBOXED_UNPACKER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_message_loop.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host_client.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/common/manifest.h"

class SkBitmap;

namespace base {
class DictionaryValue;
class SequencedTaskRunner;
}

namespace content {
class UtilityProcessHost;
}

namespace crypto {
class SecureHash;
}

namespace extensions {
class Extension;

class SandboxedUnpackerClient
    : public base::RefCountedDeleteOnMessageLoop<SandboxedUnpackerClient> {
 public:
  // Initialize the ref-counted base to always delete on the UI thread. Note
  // the constructor call must also happen on the UI thread.
  SandboxedUnpackerClient();

  // temp_dir - A temporary directory containing the results of the extension
  // unpacking. The client is responsible for deleting this directory.
  //
  // extension_root - The path to the extension root inside of temp_dir.
  //
  // original_manifest - The parsed but unmodified version of the manifest,
  // with no modifications such as localization, etc.
  //
  // extension - The extension that was unpacked. The client is responsible
  // for deleting this memory.
  //
  // install_icon - The icon we will display in the installation UI, if any.
  virtual void OnUnpackSuccess(const base::FilePath& temp_dir,
                               const base::FilePath& extension_root,
                               const base::DictionaryValue* original_manifest,
                               const Extension* extension,
                               const SkBitmap& install_icon) = 0;
  virtual void OnUnpackFailure(const CrxInstallError& error) = 0;

 protected:
  friend class base::RefCountedDeleteOnMessageLoop<SandboxedUnpackerClient>;
  friend class base::DeleteHelper<SandboxedUnpackerClient>;

  virtual ~SandboxedUnpackerClient() {}
};

// SandboxedUnpacker does work to optionally unpack and then validate/sanitize
// an extension, either starting from a crx file or an already unzipped
// directory (eg from differential update). This is done in a sandboxed
// subprocess to protect the browser process from parsing complex formats like
// JPEG or JSON from untrusted sources.
//
// Unpacking an extension using this class makes minor changes to its source,
// such as transcoding all images to PNG, parsing all message catalogs
// and rewriting the manifest JSON. As such, it should not be used when the
// output is not intended to be given back to the author.
//
//
// Lifetime management:
//
// This class is ref-counted by each call it makes to itself on another thread,
// and by UtilityProcessHost.
//
// Additionally, we hold a reference to our own client so that it lives at least
// long enough to receive the result of unpacking.
//
//
// NOTE: This class should only be used on the file thread.
class SandboxedUnpacker : public content::UtilityProcessHostClient {
 public:
  // Creates a SanboxedUnpacker that will do work to unpack an extension,
  // passing the |location| and |creation_flags| to Extension::Create. The
  // |extensions_dir| parameter should specify the directory under which we'll
  // create a subdirectory to write the unpacked extension contents.
  SandboxedUnpacker(
      Manifest::Location location,
      int creation_flags,
      const base::FilePath& extensions_dir,
      const scoped_refptr<base::SequencedTaskRunner>& unpacker_io_task_runner,
      SandboxedUnpackerClient* client);

  // Start processing the extension, either from a CRX file or already unzipped
  // in a directory. The client is called with the results. The directory form
  // requires the id and base64-encoded public key (for insertion into the
  // 'key' field of the manifest.json file).
  void StartWithCrx(const CRXFileInfo& crx_info);
  void StartWithDirectory(const std::string& extension_id,
                          const std::string& public_key_base64,
                          const base::FilePath& directory);

 private:
  class ProcessHostClient;

  // Enumerate all the ways unpacking can fail.  Calls to ReportFailure()
  // take a failure reason as an argument, and put it in histogram
  // Extensions.SandboxUnpackFailureReason.
  enum FailureReason {
    // SandboxedUnpacker::CreateTempDirectory()
    COULD_NOT_GET_TEMP_DIRECTORY,
    COULD_NOT_CREATE_TEMP_DIRECTORY,

    // SandboxedUnpacker::Start()
    FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY,
    COULD_NOT_GET_SANDBOX_FRIENDLY_PATH,

    // SandboxedUnpacker::OnUnpackExtensionSucceeded()
    COULD_NOT_LOCALIZE_EXTENSION,
    INVALID_MANIFEST,

    // SandboxedUnpacker::OnUnpackExtensionFailed()
    UNPACKER_CLIENT_FAILED,

    // SandboxedUnpacker::OnProcessCrashed()
    UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL,

    // SandboxedUnpacker::ValidateSignature()
    CRX_FILE_NOT_READABLE,
    CRX_HEADER_INVALID,
    CRX_MAGIC_NUMBER_INVALID,
    CRX_VERSION_NUMBER_INVALID,
    CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE,
    CRX_ZERO_KEY_LENGTH,
    CRX_ZERO_SIGNATURE_LENGTH,
    CRX_PUBLIC_KEY_INVALID,
    CRX_SIGNATURE_INVALID,
    CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED,
    CRX_SIGNATURE_VERIFICATION_FAILED,

    // SandboxedUnpacker::RewriteManifestFile()
    ERROR_SERIALIZING_MANIFEST_JSON,
    ERROR_SAVING_MANIFEST_JSON,

    // SandboxedUnpacker::RewriteImageFiles()
    COULD_NOT_READ_IMAGE_DATA_FROM_DISK,
    DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST,
    INVALID_PATH_FOR_BROWSER_IMAGE,
    ERROR_REMOVING_OLD_IMAGE_FILE,
    INVALID_PATH_FOR_BITMAP_IMAGE,
    ERROR_RE_ENCODING_THEME_IMAGE,
    ERROR_SAVING_THEME_IMAGE,
    ABORTED_DUE_TO_SHUTDOWN,

    // SandboxedUnpacker::RewriteCatalogFiles()
    COULD_NOT_READ_CATALOG_DATA_FROM_DISK,
    INVALID_CATALOG_DATA,
    INVALID_PATH_FOR_CATALOG,
    ERROR_SERIALIZING_CATALOG,
    ERROR_SAVING_CATALOG,

    // SandboxedUnpacker::ValidateSignature()
    CRX_HASH_VERIFICATION_FAILED,

    UNZIP_FAILED,
    DIRECTORY_MOVE_FAILED,
    COULD_NOT_START_UTILITY_PROCESS,

    NUM_FAILURE_REASONS
  };

  friend class ProcessHostClient;
  friend class SandboxedUnpackerTest;

  ~SandboxedUnpacker() override;

  // Set |temp_dir_| as a temporary directory to unpack the extension in.
  // Return true on success.
  virtual bool CreateTempDirectory();

  // Helper functions to simplify calls to ReportFailure.
  base::string16 FailureReasonToString16(FailureReason reason);
  void FailWithPackageError(FailureReason reason);

  // Validates the signature of the extension and extract the key to
  // |public_key_|. Returns true if the signature validates, false otherwise.
  bool ValidateSignature(const base::FilePath& crx_path,
                         const std::string& expected_hash);

  void StartUnzipOnIOThread(const base::FilePath& crx_path);
  void StartUnpackOnIOThread(const base::FilePath& directory_path);

  // UtilityProcessHostClient
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnProcessCrashed(int exit_code) override;

  // IPC message handlers.
  void OnUnzipToDirSucceeded(const base::FilePath& directory);
  void OnUnzipToDirFailed(const std::string& error);
  void OnUnpackExtensionSucceeded(const base::DictionaryValue& manifest);
  void OnUnpackExtensionFailed(const base::string16& error_message);

  void ReportFailure(FailureReason reason, const base::string16& message);
  void ReportSuccess(const base::DictionaryValue& original_manifest,
                     const SkBitmap& install_icon);

  // Overwrites original manifest with safe result from utility process.
  // Returns NULL on error. Caller owns the returned object.
  base::DictionaryValue* RewriteManifestFile(
      const base::DictionaryValue& manifest);

  // Overwrites original files with safe results from utility process.
  // Reports error and returns false if it fails.
  bool RewriteImageFiles(SkBitmap* install_icon);
  bool RewriteCatalogFiles();

  // Cleans up temp directory artifacts.
  void Cleanup();

  // This is a helper class to make it easier to keep track of the lifecycle of
  // a UtilityProcessHost, including automatic begin and end of batch mode.
  class UtilityHostWrapper : public base::RefCountedThreadSafe<
                                 UtilityHostWrapper,
                                 content::BrowserThread::DeleteOnIOThread> {
   public:
    UtilityHostWrapper();

    // Start up the utility process if it is not already started, putting it
    // into batch mode and giving it access to |exposed_dir|. This should only
    // be called on the IO thread. Returns false if there was an error starting
    // the utility process or putting it into batch mode.
    bool StartIfNeeded(
        const base::FilePath& exposed_dir,
        const scoped_refptr<UtilityProcessHostClient>& client,
        const scoped_refptr<base::SequencedTaskRunner>& client_task_runner);

    // This should only be called on the IO thread.
    content::UtilityProcessHost* host() const;

   private:
    friend struct content::BrowserThread::DeleteOnThread<
        content::BrowserThread::IO>;
    friend class base::DeleteHelper<UtilityHostWrapper>;
    ~UtilityHostWrapper();

    // Should only be used on the IO thread.
    base::WeakPtr<content::UtilityProcessHost> utility_host_;

    DISALLOW_COPY_AND_ASSIGN(UtilityHostWrapper);
  };

  // If we unpacked a crx file, we hold on to the path for use in various
  // histograms.
  base::FilePath crx_path_for_histograms_;

  // Our client.
  scoped_refptr<SandboxedUnpackerClient> client_;

  // The Extensions directory inside the profile.
  base::FilePath extensions_dir_;

  // A temporary directory to use for unpacking.
  base::ScopedTempDir temp_dir_;

  // The root directory of the unpacked extension. This is a child of temp_dir_.
  base::FilePath extension_root_;

  // Represents the extension we're unpacking.
  scoped_refptr<Extension> extension_;

  // Whether we've received a response from the utility process yet.
  bool got_response_;

  // The public key that was extracted from the CRX header.
  std::string public_key_;

  // The extension's ID. This will be calculated from the public key in the crx
  // header.
  std::string extension_id_;

  // If we unpacked a .crx file, the time at which unpacking started. Used to
  // compute the time unpacking takes.
  base::TimeTicks crx_unpack_start_time_;

  // Location to use for the unpacked extension.
  Manifest::Location location_;

  // Creation flags to use for the extension.  These flags will be used
  // when calling Extenion::Create() by the crx installer.
  int creation_flags_;

  // Sequenced task runner where file I/O operations will be performed at.
  scoped_refptr<base::SequencedTaskRunner> unpacker_io_task_runner_;

  // Used for sending tasks to the utility process.
  scoped_refptr<UtilityHostWrapper> utility_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(SandboxedUnpacker);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SANDBOXED_UNPACKER_H_
