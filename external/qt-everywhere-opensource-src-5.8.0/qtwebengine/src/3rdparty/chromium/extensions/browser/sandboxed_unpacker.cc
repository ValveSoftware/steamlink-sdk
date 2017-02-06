// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/sandboxed_unpacker.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <tuple>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"
#include "components/crx_file/crx_file.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/common/common_param_traits.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_utility_messages.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/switches.h"
#include "grit/extensions_strings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"

using base::ASCIIToUTF16;
using content::BrowserThread;
using content::UtilityProcessHost;
using crx_file::CrxFile;

// The following macro makes histograms that record the length of paths
// in this file much easier to read.
// Windows has a short max path length. If the path length to a
// file being unpacked from a CRX exceeds the max length, we might
// fail to install. To see if this is happening, see how long the
// path to the temp unpack directory is. See crbug.com/69693 .
#define PATH_LENGTH_HISTOGRAM(name, path) \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, path.value().length(), 0, 500, 100)

// Record a rate (kB per second) at which extensions are unpacked.
// Range from 1kB/s to 100mB/s.
#define UNPACK_RATE_HISTOGRAM(name, rate) \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, rate, 1, 100000, 100);

namespace extensions {
namespace {

void RecordSuccessfulUnpackTimeHistograms(const base::FilePath& crx_path,
                                          const base::TimeDelta unpack_time) {
  const int64_t kBytesPerKb = 1024;
  const int64_t kBytesPerMb = 1024 * 1024;

  UMA_HISTOGRAM_TIMES("Extensions.SandboxUnpackSuccessTime", unpack_time);

  // To get a sense of how CRX size impacts unpack time, record unpack
  // time for several increments of CRX size.
  int64_t crx_file_size;
  if (!base::GetFileSize(crx_path, &crx_file_size)) {
    UMA_HISTOGRAM_COUNTS("Extensions.SandboxUnpackSuccessCantGetCrxSize", 1);
    return;
  }

  // Cast is safe as long as the number of bytes in the CRX is less than
  // 2^31 * 2^10.
  int crx_file_size_kb = static_cast<int>(crx_file_size / kBytesPerKb);
  UMA_HISTOGRAM_COUNTS("Extensions.SandboxUnpackSuccessCrxSize",
                       crx_file_size_kb);

  // We have time in seconds and file size in bytes.  We want the rate bytes are
  // unpacked in kB/s.
  double file_size_kb =
      static_cast<double>(crx_file_size) / static_cast<double>(kBytesPerKb);
  int unpack_rate_kb_per_s =
      static_cast<int>(file_size_kb / unpack_time.InSecondsF());
  UNPACK_RATE_HISTOGRAM("Extensions.SandboxUnpackRate", unpack_rate_kb_per_s);

  if (crx_file_size < 50.0 * kBytesPerKb) {
    UNPACK_RATE_HISTOGRAM("Extensions.SandboxUnpackRateUnder50kB",
                          unpack_rate_kb_per_s);

  } else if (crx_file_size < 1 * kBytesPerMb) {
    UNPACK_RATE_HISTOGRAM("Extensions.SandboxUnpackRate50kBTo1mB",
                          unpack_rate_kb_per_s);

  } else if (crx_file_size < 2 * kBytesPerMb) {
    UNPACK_RATE_HISTOGRAM("Extensions.SandboxUnpackRate1To2mB",
                          unpack_rate_kb_per_s);

  } else if (crx_file_size < 5 * kBytesPerMb) {
    UNPACK_RATE_HISTOGRAM("Extensions.SandboxUnpackRate2To5mB",
                          unpack_rate_kb_per_s);

  } else if (crx_file_size < 10 * kBytesPerMb) {
    UNPACK_RATE_HISTOGRAM("Extensions.SandboxUnpackRate5To10mB",
                          unpack_rate_kb_per_s);

  } else {
    UNPACK_RATE_HISTOGRAM("Extensions.SandboxUnpackRateOver10mB",
                          unpack_rate_kb_per_s);
  }
}

// Work horse for FindWritableTempLocation. Creates a temp file in the folder
// and uses NormalizeFilePath to check if the path is junction free.
bool VerifyJunctionFreeLocation(base::FilePath* temp_dir) {
  if (temp_dir->empty())
    return false;

  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(*temp_dir, &temp_file)) {
    LOG(ERROR) << temp_dir->value() << " is not writable";
    return false;
  }
  // NormalizeFilePath requires a non-empty file, so write some data.
  // If you change the exit points of this function please make sure all
  // exit points delete this temp file!
  if (base::WriteFile(temp_file, ".", 1) != 1)
    return false;

  base::FilePath normalized_temp_file;
  bool normalized = base::NormalizeFilePath(temp_file, &normalized_temp_file);
  if (!normalized) {
    // If |temp_file| contains a link, the sandbox will block al file system
    // operations, and the install will fail.
    LOG(ERROR) << temp_dir->value() << " seem to be on remote drive.";
  } else {
    *temp_dir = normalized_temp_file.DirName();
  }
  // Clean up the temp file.
  base::DeleteFile(temp_file, false);

  return normalized;
}

// This function tries to find a location for unpacking the extension archive
// that is writable and does not lie on a shared drive so that the sandboxed
// unpacking process can write there. If no such location exists we can not
// proceed and should fail.
// The result will be written to |temp_dir|. The function will write to this
// parameter even if it returns false.
bool FindWritableTempLocation(const base::FilePath& extensions_dir,
                              base::FilePath* temp_dir) {
// On ChromeOS, we will only attempt to unpack extension in cryptohome (profile)
// directory to provide additional security/privacy and speed up the rest of
// the extension install process.
#if !defined(OS_CHROMEOS)
  PathService::Get(base::DIR_TEMP, temp_dir);
  if (VerifyJunctionFreeLocation(temp_dir))
    return true;
#endif

  *temp_dir = file_util::GetInstallTempDir(extensions_dir);
  if (VerifyJunctionFreeLocation(temp_dir))
    return true;
  // Neither paths is link free chances are good installation will fail.
  LOG(ERROR) << "Both the %TEMP% folder and the profile seem to be on "
             << "remote drives or read-only. Installation can not complete!";
  return false;
}

// Read the decoded images back from the file we saved them to.
// |extension_path| is the path to the extension we unpacked that wrote the
// data. Returns true on success.
bool ReadImagesFromFile(const base::FilePath& extension_path,
                        DecodedImages* images) {
  base::FilePath path = extension_path.AppendASCII(kDecodedImagesFilename);
  std::string file_str;
  if (!base::ReadFileToString(path, &file_str))
    return false;

  IPC::Message pickle(file_str.data(), file_str.size());
  base::PickleIterator iter(pickle);
  return IPC::ReadParam(&pickle, &iter, images);
}

// Read the decoded message catalogs back from the file we saved them to.
// |extension_path| is the path to the extension we unpacked that wrote the
// data. Returns true on success.
bool ReadMessageCatalogsFromFile(const base::FilePath& extension_path,
                                 base::DictionaryValue* catalogs) {
  base::FilePath path =
      extension_path.AppendASCII(kDecodedMessageCatalogsFilename);
  std::string file_str;
  if (!base::ReadFileToString(path, &file_str))
    return false;

  IPC::Message pickle(file_str.data(), file_str.size());
  base::PickleIterator iter(pickle);
  return IPC::ReadParam(&pickle, &iter, catalogs);
}

}  // namespace

SandboxedUnpackerClient::SandboxedUnpackerClient()
    : RefCountedDeleteOnMessageLoop<SandboxedUnpackerClient>(
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::UI)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

SandboxedUnpacker::SandboxedUnpacker(
    Manifest::Location location,
    int creation_flags,
    const base::FilePath& extensions_dir,
    const scoped_refptr<base::SequencedTaskRunner>& unpacker_io_task_runner,
    SandboxedUnpackerClient* client)
    : client_(client),
      extensions_dir_(extensions_dir),
      got_response_(false),
      location_(location),
      creation_flags_(creation_flags),
      unpacker_io_task_runner_(unpacker_io_task_runner),
      utility_wrapper_(new UtilityHostWrapper) {}

bool SandboxedUnpacker::CreateTempDirectory() {
  CHECK(unpacker_io_task_runner_->RunsTasksOnCurrentThread());

  base::FilePath temp_dir;
  if (!FindWritableTempLocation(extensions_dir_, &temp_dir)) {
    ReportFailure(COULD_NOT_GET_TEMP_DIRECTORY,
                  l10n_util::GetStringFUTF16(
                      IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                      ASCIIToUTF16("COULD_NOT_GET_TEMP_DIRECTORY")));
    return false;
  }

  if (!temp_dir_.CreateUniqueTempDirUnderPath(temp_dir)) {
    ReportFailure(COULD_NOT_CREATE_TEMP_DIRECTORY,
                  l10n_util::GetStringFUTF16(
                      IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                      ASCIIToUTF16("COULD_NOT_CREATE_TEMP_DIRECTORY")));
    return false;
  }

  return true;
}

void SandboxedUnpacker::StartWithCrx(const CRXFileInfo& crx_info) {
  // We assume that we are started on the thread that the client wants us to do
  // file IO on.
  CHECK(unpacker_io_task_runner_->RunsTasksOnCurrentThread());

  crx_unpack_start_time_ = base::TimeTicks::Now();
  std::string expected_hash;
  if (!crx_info.expected_hash.empty() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          extensions::switches::kEnableCrxHashCheck)) {
    expected_hash = base::ToLowerASCII(crx_info.expected_hash);
  }

  PATH_LENGTH_HISTOGRAM("Extensions.SandboxUnpackInitialCrxPathLength",
                        crx_info.path);
  if (!CreateTempDirectory())
    return;  // ReportFailure() already called.

  // Initialize the path that will eventually contain the unpacked extension.
  extension_root_ = temp_dir_.path().AppendASCII(kTempExtensionName);
  PATH_LENGTH_HISTOGRAM("Extensions.SandboxUnpackUnpackedCrxPathLength",
                        extension_root_);

  // Extract the public key and validate the package.
  if (!ValidateSignature(crx_info.path, expected_hash))
    return;  // ValidateSignature() already reported the error.

  // Copy the crx file into our working directory.
  base::FilePath temp_crx_path =
      temp_dir_.path().Append(crx_info.path.BaseName());
  PATH_LENGTH_HISTOGRAM("Extensions.SandboxUnpackTempCrxPathLength",
                        temp_crx_path);

  if (!base::CopyFile(crx_info.path, temp_crx_path)) {
    // Failed to copy extension file to temporary directory.
    ReportFailure(
        FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY")));
    return;
  }

  // The utility process will have access to the directory passed to
  // SandboxedUnpacker.  That directory should not contain a symlink or NTFS
  // reparse point.  When the path is used, following the link/reparse point
  // will cause file system access outside the sandbox path, and the sandbox
  // will deny the operation.
  base::FilePath link_free_crx_path;
  if (!base::NormalizeFilePath(temp_crx_path, &link_free_crx_path)) {
    LOG(ERROR) << "Could not get the normalized path of "
               << temp_crx_path.value();
    ReportFailure(COULD_NOT_GET_SANDBOX_FRIENDLY_PATH,
                  l10n_util::GetStringUTF16(IDS_EXTENSION_UNPACK_FAILED));
    return;
  }
  PATH_LENGTH_HISTOGRAM("Extensions.SandboxUnpackLinkFreeCrxPathLength",
                        link_free_crx_path);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SandboxedUnpacker::StartUnzipOnIOThread,
                                     this, link_free_crx_path));
}

void SandboxedUnpacker::StartWithDirectory(const std::string& extension_id,
                                           const std::string& public_key,
                                           const base::FilePath& directory) {
  extension_id_ = extension_id;
  public_key_ = public_key;
  if (!CreateTempDirectory())
    return;  // ReportFailure() already called.

  extension_root_ = temp_dir_.path().AppendASCII(kTempExtensionName);

  if (!base::Move(directory, extension_root_)) {
    LOG(ERROR) << "Could not move " << directory.value() << " to "
               << extension_root_.value();
    ReportFailure(
        DIRECTORY_MOVE_FAILED,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                   ASCIIToUTF16("DIRECTORY_MOVE_FAILED")));
    return;
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SandboxedUnpacker::StartUnpackOnIOThread,
                                     this, extension_root_));
}

SandboxedUnpacker::~SandboxedUnpacker() {
  // To avoid blocking shutdown, don't delete temporary directory here if it
  // hasn't been cleaned up or passed on to another owner yet.
  temp_dir_.Take();
}

bool SandboxedUnpacker::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SandboxedUnpacker, message)
    IPC_MESSAGE_HANDLER(ExtensionUtilityHostMsg_UnzipToDir_Succeeded,
                        OnUnzipToDirSucceeded)
    IPC_MESSAGE_HANDLER(ExtensionUtilityHostMsg_UnzipToDir_Failed,
                        OnUnzipToDirFailed)
    IPC_MESSAGE_HANDLER(ExtensionUtilityHostMsg_UnpackExtension_Succeeded,
                        OnUnpackExtensionSucceeded)
    IPC_MESSAGE_HANDLER(ExtensionUtilityHostMsg_UnpackExtension_Failed,
                        OnUnpackExtensionFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SandboxedUnpacker::OnProcessCrashed(int exit_code) {
  // Don't report crashes if they happen after we got a response.
  if (got_response_)
    return;

  // Utility process crashed while trying to install.
  ReportFailure(
      UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL,
      l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
          ASCIIToUTF16("UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL")) +
          ASCIIToUTF16(". ") +
          l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_PROCESS_CRASHED));
}

void SandboxedUnpacker::StartUnzipOnIOThread(const base::FilePath& crx_path) {
  if (!utility_wrapper_->StartIfNeeded(temp_dir_.path(), this,
                                       unpacker_io_task_runner_)) {
    ReportFailure(
        COULD_NOT_START_UTILITY_PROCESS,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            FailureReasonToString16(COULD_NOT_START_UTILITY_PROCESS)));
    return;
  }
  DCHECK(crx_path.DirName() == temp_dir_.path());
  base::FilePath unzipped_dir =
      crx_path.DirName().AppendASCII(kTempExtensionName);
  utility_wrapper_->host()->Send(
      new ExtensionUtilityMsg_UnzipToDir(crx_path, unzipped_dir));
}

void SandboxedUnpacker::StartUnpackOnIOThread(
    const base::FilePath& directory_path) {
  if (!utility_wrapper_->StartIfNeeded(temp_dir_.path(), this,
                                       unpacker_io_task_runner_)) {
    ReportFailure(
        COULD_NOT_START_UTILITY_PROCESS,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            FailureReasonToString16(COULD_NOT_START_UTILITY_PROCESS)));
    return;
  }
  DCHECK(directory_path.DirName() == temp_dir_.path());
  utility_wrapper_->host()->Send(new ExtensionUtilityMsg_UnpackExtension(
      directory_path, extension_id_, location_, creation_flags_));
}

void SandboxedUnpacker::OnUnzipToDirSucceeded(const base::FilePath& directory) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SandboxedUnpacker::StartUnpackOnIOThread, this, directory));
}

void SandboxedUnpacker::OnUnzipToDirFailed(const std::string& error) {
  got_response_ = true;
  utility_wrapper_ = nullptr;
  ReportFailure(UNZIP_FAILED,
                l10n_util::GetStringUTF16(IDS_EXTENSION_PACKAGE_UNZIP_ERROR));
}

void SandboxedUnpacker::OnUnpackExtensionSucceeded(
    const base::DictionaryValue& manifest) {
  CHECK(unpacker_io_task_runner_->RunsTasksOnCurrentThread());
  got_response_ = true;
  utility_wrapper_ = nullptr;

  std::unique_ptr<base::DictionaryValue> final_manifest(
      RewriteManifestFile(manifest));
  if (!final_manifest)
    return;

  // Create an extension object that refers to the temporary location the
  // extension was unpacked to. We use this until the extension is finally
  // installed. For example, the install UI shows images from inside the
  // extension.

  // Localize manifest now, so confirm UI gets correct extension name.

  // TODO(rdevlin.cronin): Continue removing std::string errors and replacing
  // with base::string16
  std::string utf8_error;
  if (!extension_l10n_util::LocalizeExtension(
          extension_root_, final_manifest.get(), &utf8_error)) {
    ReportFailure(
        COULD_NOT_LOCALIZE_EXTENSION,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_ERROR_MESSAGE,
                                   base::UTF8ToUTF16(utf8_error)));
    return;
  }

  extension_ =
      Extension::Create(extension_root_, location_, *final_manifest,
                        Extension::REQUIRE_KEY | creation_flags_, &utf8_error);

  if (!extension_.get()) {
    ReportFailure(INVALID_MANIFEST,
                  ASCIIToUTF16("Manifest is invalid: " + utf8_error));
    return;
  }

  SkBitmap install_icon;
  if (!RewriteImageFiles(&install_icon))
    return;

  if (!RewriteCatalogFiles())
    return;

  ReportSuccess(manifest, install_icon);
}

void SandboxedUnpacker::OnUnpackExtensionFailed(const base::string16& error) {
  CHECK(unpacker_io_task_runner_->RunsTasksOnCurrentThread());
  got_response_ = true;
  utility_wrapper_ = nullptr;
  ReportFailure(
      UNPACKER_CLIENT_FAILED,
      l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_ERROR_MESSAGE, error));
}

base::string16 SandboxedUnpacker::FailureReasonToString16(
    FailureReason reason) {
  switch (reason) {
    case COULD_NOT_GET_TEMP_DIRECTORY:
      return ASCIIToUTF16("COULD_NOT_GET_TEMP_DIRECTORY");
    case COULD_NOT_CREATE_TEMP_DIRECTORY:
      return ASCIIToUTF16("COULD_NOT_CREATE_TEMP_DIRECTORY");
    case FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY:
      return ASCIIToUTF16("FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY");
    case COULD_NOT_GET_SANDBOX_FRIENDLY_PATH:
      return ASCIIToUTF16("COULD_NOT_GET_SANDBOX_FRIENDLY_PATH");
    case COULD_NOT_LOCALIZE_EXTENSION:
      return ASCIIToUTF16("COULD_NOT_LOCALIZE_EXTENSION");
    case INVALID_MANIFEST:
      return ASCIIToUTF16("INVALID_MANIFEST");
    case UNPACKER_CLIENT_FAILED:
      return ASCIIToUTF16("UNPACKER_CLIENT_FAILED");
    case UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL:
      return ASCIIToUTF16("UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL");

    case CRX_FILE_NOT_READABLE:
      return ASCIIToUTF16("CRX_FILE_NOT_READABLE");
    case CRX_HEADER_INVALID:
      return ASCIIToUTF16("CRX_HEADER_INVALID");
    case CRX_MAGIC_NUMBER_INVALID:
      return ASCIIToUTF16("CRX_MAGIC_NUMBER_INVALID");
    case CRX_VERSION_NUMBER_INVALID:
      return ASCIIToUTF16("CRX_VERSION_NUMBER_INVALID");
    case CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE:
      return ASCIIToUTF16("CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE");
    case CRX_ZERO_KEY_LENGTH:
      return ASCIIToUTF16("CRX_ZERO_KEY_LENGTH");
    case CRX_ZERO_SIGNATURE_LENGTH:
      return ASCIIToUTF16("CRX_ZERO_SIGNATURE_LENGTH");
    case CRX_PUBLIC_KEY_INVALID:
      return ASCIIToUTF16("CRX_PUBLIC_KEY_INVALID");
    case CRX_SIGNATURE_INVALID:
      return ASCIIToUTF16("CRX_SIGNATURE_INVALID");
    case CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED:
      return ASCIIToUTF16("CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED");
    case CRX_SIGNATURE_VERIFICATION_FAILED:
      return ASCIIToUTF16("CRX_SIGNATURE_VERIFICATION_FAILED");

    case ERROR_SERIALIZING_MANIFEST_JSON:
      return ASCIIToUTF16("ERROR_SERIALIZING_MANIFEST_JSON");
    case ERROR_SAVING_MANIFEST_JSON:
      return ASCIIToUTF16("ERROR_SAVING_MANIFEST_JSON");

    case COULD_NOT_READ_IMAGE_DATA_FROM_DISK:
      return ASCIIToUTF16("COULD_NOT_READ_IMAGE_DATA_FROM_DISK");
    case DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST:
      return ASCIIToUTF16("DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST");
    case INVALID_PATH_FOR_BROWSER_IMAGE:
      return ASCIIToUTF16("INVALID_PATH_FOR_BROWSER_IMAGE");
    case ERROR_REMOVING_OLD_IMAGE_FILE:
      return ASCIIToUTF16("ERROR_REMOVING_OLD_IMAGE_FILE");
    case INVALID_PATH_FOR_BITMAP_IMAGE:
      return ASCIIToUTF16("INVALID_PATH_FOR_BITMAP_IMAGE");
    case ERROR_RE_ENCODING_THEME_IMAGE:
      return ASCIIToUTF16("ERROR_RE_ENCODING_THEME_IMAGE");
    case ERROR_SAVING_THEME_IMAGE:
      return ASCIIToUTF16("ERROR_SAVING_THEME_IMAGE");
    case ABORTED_DUE_TO_SHUTDOWN:
      return ASCIIToUTF16("ABORTED_DUE_TO_SHUTDOWN");

    case COULD_NOT_READ_CATALOG_DATA_FROM_DISK:
      return ASCIIToUTF16("COULD_NOT_READ_CATALOG_DATA_FROM_DISK");
    case INVALID_CATALOG_DATA:
      return ASCIIToUTF16("INVALID_CATALOG_DATA");
    case INVALID_PATH_FOR_CATALOG:
      return ASCIIToUTF16("INVALID_PATH_FOR_CATALOG");
    case ERROR_SERIALIZING_CATALOG:
      return ASCIIToUTF16("ERROR_SERIALIZING_CATALOG");
    case ERROR_SAVING_CATALOG:
      return ASCIIToUTF16("ERROR_SAVING_CATALOG");

    case CRX_HASH_VERIFICATION_FAILED:
      return ASCIIToUTF16("CRX_HASH_VERIFICATION_FAILED");

    case UNZIP_FAILED:
      return ASCIIToUTF16("UNZIP_FAILED");
    case DIRECTORY_MOVE_FAILED:
      return ASCIIToUTF16("DIRECTORY_MOVE_FAILED");
    case COULD_NOT_START_UTILITY_PROCESS:
      return ASCIIToUTF16("COULD_NOT_START_UTILITY_PROCESS");

    case NUM_FAILURE_REASONS:
      NOTREACHED();
      return base::string16();
  }
  NOTREACHED();
  return base::string16();
}

void SandboxedUnpacker::FailWithPackageError(FailureReason reason) {
  ReportFailure(reason,
                l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_ERROR_CODE,
                                           FailureReasonToString16(reason)));
}

bool SandboxedUnpacker::ValidateSignature(const base::FilePath& crx_path,
                                          const std::string& expected_hash) {
  CrxFile::ValidateError error = CrxFile::ValidateSignature(
      crx_path, expected_hash, &public_key_, &extension_id_, nullptr);

  switch (error) {
    case CrxFile::ValidateError::NONE: {
      if (!expected_hash.empty())
        UMA_HISTOGRAM_BOOLEAN("Extensions.SandboxUnpackHashCheck", true);
      return true;
    }

    case CrxFile::ValidateError::CRX_FILE_NOT_READABLE:
      FailWithPackageError(CRX_FILE_NOT_READABLE);
      break;
    case CrxFile::ValidateError::CRX_HEADER_INVALID:
      FailWithPackageError(CRX_HEADER_INVALID);
      break;
    case CrxFile::ValidateError::CRX_MAGIC_NUMBER_INVALID:
      FailWithPackageError(CRX_MAGIC_NUMBER_INVALID);
      break;
    case CrxFile::ValidateError::CRX_VERSION_NUMBER_INVALID:
      FailWithPackageError(CRX_VERSION_NUMBER_INVALID);
      break;
    case CrxFile::ValidateError::CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE:
      FailWithPackageError(CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE);
      break;
    case CrxFile::ValidateError::CRX_ZERO_KEY_LENGTH:
      FailWithPackageError(CRX_ZERO_KEY_LENGTH);
      break;
    case CrxFile::ValidateError::CRX_ZERO_SIGNATURE_LENGTH:
      FailWithPackageError(CRX_ZERO_SIGNATURE_LENGTH);
      break;
    case CrxFile::ValidateError::CRX_PUBLIC_KEY_INVALID:
      FailWithPackageError(CRX_PUBLIC_KEY_INVALID);
      break;
    case CrxFile::ValidateError::CRX_SIGNATURE_INVALID:
      FailWithPackageError(CRX_SIGNATURE_INVALID);
      break;
    case CrxFile::ValidateError::
        CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED:
      FailWithPackageError(CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED);
      break;
    case CrxFile::ValidateError::CRX_SIGNATURE_VERIFICATION_FAILED:
      FailWithPackageError(CRX_SIGNATURE_VERIFICATION_FAILED);
      break;
    case CrxFile::ValidateError::CRX_HASH_VERIFICATION_FAILED:
      // We should never get this result unless we had specifically asked for
      // verification of the crx file's hash.
      CHECK(!expected_hash.empty());
      UMA_HISTOGRAM_BOOLEAN("Extensions.SandboxUnpackHashCheck", false);
      FailWithPackageError(CRX_HASH_VERIFICATION_FAILED);
      break;
  }
  return false;
}

void SandboxedUnpacker::ReportFailure(FailureReason reason,
                                      const base::string16& error) {
  utility_wrapper_ = nullptr;
  UMA_HISTOGRAM_ENUMERATION("Extensions.SandboxUnpackFailureReason", reason,
                            NUM_FAILURE_REASONS);
  if (!crx_unpack_start_time_.is_null())
    UMA_HISTOGRAM_TIMES("Extensions.SandboxUnpackFailureTime",
                        base::TimeTicks::Now() - crx_unpack_start_time_);
  Cleanup();

  CrxInstallError error_info(reason == CRX_HASH_VERIFICATION_FAILED
                                 ? CrxInstallError::ERROR_HASH_MISMATCH
                                 : CrxInstallError::ERROR_OTHER,
                             error);

  client_->OnUnpackFailure(error_info);
}

void SandboxedUnpacker::ReportSuccess(
    const base::DictionaryValue& original_manifest,
    const SkBitmap& install_icon) {
  utility_wrapper_ = nullptr;
  UMA_HISTOGRAM_COUNTS("Extensions.SandboxUnpackSuccess", 1);

  if (!crx_unpack_start_time_.is_null())
    RecordSuccessfulUnpackTimeHistograms(
        crx_path_for_histograms_,
        base::TimeTicks::Now() - crx_unpack_start_time_);
  DCHECK(!temp_dir_.path().empty());

  // Client takes ownership of temporary directory and extension.
  client_->OnUnpackSuccess(temp_dir_.Take(), extension_root_,
                           &original_manifest, extension_.get(), install_icon);
  extension_ = NULL;
}

base::DictionaryValue* SandboxedUnpacker::RewriteManifestFile(
    const base::DictionaryValue& manifest) {
  // Add the public key extracted earlier to the parsed manifest and overwrite
  // the original manifest. We do this to ensure the manifest doesn't contain an
  // exploitable bug that could be used to compromise the browser.
  DCHECK(!public_key_.empty());
  std::unique_ptr<base::DictionaryValue> final_manifest(manifest.DeepCopy());
  final_manifest->SetString(manifest_keys::kPublicKey, public_key_);

  std::string manifest_json;
  JSONStringValueSerializer serializer(&manifest_json);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(*final_manifest)) {
    // Error serializing manifest.json.
    ReportFailure(ERROR_SERIALIZING_MANIFEST_JSON,
                  l10n_util::GetStringFUTF16(
                      IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                      ASCIIToUTF16("ERROR_SERIALIZING_MANIFEST_JSON")));
    return NULL;
  }

  base::FilePath manifest_path = extension_root_.Append(kManifestFilename);
  int size = base::checked_cast<int>(manifest_json.size());
  if (base::WriteFile(manifest_path, manifest_json.data(), size) != size) {
    // Error saving manifest.json.
    ReportFailure(
        ERROR_SAVING_MANIFEST_JSON,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                   ASCIIToUTF16("ERROR_SAVING_MANIFEST_JSON")));
    return NULL;
  }

  return final_manifest.release();
}

bool SandboxedUnpacker::RewriteImageFiles(SkBitmap* install_icon) {
  DCHECK(!temp_dir_.path().empty());
  DecodedImages images;
  if (!ReadImagesFromFile(temp_dir_.path(), &images)) {
    // Couldn't read image data from disk.
    ReportFailure(COULD_NOT_READ_IMAGE_DATA_FROM_DISK,
                  l10n_util::GetStringFUTF16(
                      IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                      ASCIIToUTF16("COULD_NOT_READ_IMAGE_DATA_FROM_DISK")));
    return false;
  }

  // Delete any images that may be used by the browser.  We're going to write
  // out our own versions of the parsed images, and we want to make sure the
  // originals are gone for good.
  std::set<base::FilePath> image_paths =
      ExtensionsClient::Get()->GetBrowserImagePaths(extension_.get());
  if (image_paths.size() != images.size()) {
    // Decoded images don't match what's in the manifest.
    ReportFailure(
        DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST")));
    return false;
  }

  for (std::set<base::FilePath>::iterator it = image_paths.begin();
       it != image_paths.end(); ++it) {
    base::FilePath path = *it;
    if (path.IsAbsolute() || path.ReferencesParent()) {
      // Invalid path for browser image.
      ReportFailure(INVALID_PATH_FOR_BROWSER_IMAGE,
                    l10n_util::GetStringFUTF16(
                        IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                        ASCIIToUTF16("INVALID_PATH_FOR_BROWSER_IMAGE")));
      return false;
    }
    if (!base::DeleteFile(extension_root_.Append(path), false)) {
      // Error removing old image file.
      ReportFailure(ERROR_REMOVING_OLD_IMAGE_FILE,
                    l10n_util::GetStringFUTF16(
                        IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                        ASCIIToUTF16("ERROR_REMOVING_OLD_IMAGE_FILE")));
      return false;
    }
  }

  const std::string& install_icon_path =
      IconsInfo::GetIcons(extension_.get())
          .Get(extension_misc::EXTENSION_ICON_LARGE,
               ExtensionIconSet::MATCH_BIGGER);

  // Write our parsed images back to disk as well.
  for (size_t i = 0; i < images.size(); ++i) {
    if (BrowserThread::GetBlockingPool()->IsShutdownInProgress()) {
      // Abort package installation if shutdown was initiated, crbug.com/235525
      ReportFailure(
          ABORTED_DUE_TO_SHUTDOWN,
          l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                     ASCIIToUTF16("ABORTED_DUE_TO_SHUTDOWN")));
      return false;
    }

    const SkBitmap& image = std::get<0>(images[i]);
    base::FilePath path_suffix = std::get<1>(images[i]);
    if (path_suffix.MaybeAsASCII() == install_icon_path)
      *install_icon = image;

    if (path_suffix.IsAbsolute() || path_suffix.ReferencesParent()) {
      // Invalid path for bitmap image.
      ReportFailure(INVALID_PATH_FOR_BITMAP_IMAGE,
                    l10n_util::GetStringFUTF16(
                        IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                        ASCIIToUTF16("INVALID_PATH_FOR_BITMAP_IMAGE")));
      return false;
    }
    base::FilePath path = extension_root_.Append(path_suffix);

    std::vector<unsigned char> image_data;
    // TODO(mpcomplete): It's lame that we're encoding all images as PNG, even
    // though they may originally be .jpg, etc.  Figure something out.
    // http://code.google.com/p/chromium/issues/detail?id=12459
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(image, false, &image_data)) {
      // Error re-encoding theme image.
      ReportFailure(ERROR_RE_ENCODING_THEME_IMAGE,
                    l10n_util::GetStringFUTF16(
                        IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                        ASCIIToUTF16("ERROR_RE_ENCODING_THEME_IMAGE")));
      return false;
    }

    // Note: we're overwriting existing files that the utility process wrote,
    // so we can be sure the directory exists.
    const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
    int size = base::checked_cast<int>(image_data.size());
    if (base::WriteFile(path, image_data_ptr, size) != size) {
      // Error saving theme image.
      ReportFailure(
          ERROR_SAVING_THEME_IMAGE,
          l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                     ASCIIToUTF16("ERROR_SAVING_THEME_IMAGE")));
      return false;
    }
  }

  return true;
}

bool SandboxedUnpacker::RewriteCatalogFiles() {
  base::DictionaryValue catalogs;
  if (!ReadMessageCatalogsFromFile(temp_dir_.path(), &catalogs)) {
    // Could not read catalog data from disk.
    ReportFailure(COULD_NOT_READ_CATALOG_DATA_FROM_DISK,
                  l10n_util::GetStringFUTF16(
                      IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                      ASCIIToUTF16("COULD_NOT_READ_CATALOG_DATA_FROM_DISK")));
    return false;
  }

  // Write our parsed catalogs back to disk.
  for (base::DictionaryValue::Iterator it(catalogs); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* catalog = NULL;
    if (!it.value().GetAsDictionary(&catalog)) {
      // Invalid catalog data.
      ReportFailure(
          INVALID_CATALOG_DATA,
          l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                     ASCIIToUTF16("INVALID_CATALOG_DATA")));
      return false;
    }

    base::FilePath relative_path = base::FilePath::FromUTF8Unsafe(it.key());
    relative_path = relative_path.Append(kMessagesFilename);
    if (relative_path.IsAbsolute() || relative_path.ReferencesParent()) {
      // Invalid path for catalog.
      ReportFailure(
          INVALID_PATH_FOR_CATALOG,
          l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                     ASCIIToUTF16("INVALID_PATH_FOR_CATALOG")));
      return false;
    }
    base::FilePath path = extension_root_.Append(relative_path);

    std::string catalog_json;
    JSONStringValueSerializer serializer(&catalog_json);
    serializer.set_pretty_print(true);
    if (!serializer.Serialize(*catalog)) {
      // Error serializing catalog.
      ReportFailure(ERROR_SERIALIZING_CATALOG,
                    l10n_util::GetStringFUTF16(
                        IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                        ASCIIToUTF16("ERROR_SERIALIZING_CATALOG")));
      return false;
    }

    // Note: we're overwriting existing files that the utility process read,
    // so we can be sure the directory exists.
    int size = base::checked_cast<int>(catalog_json.size());
    if (base::WriteFile(path, catalog_json.c_str(), size) != size) {
      // Error saving catalog.
      ReportFailure(
          ERROR_SAVING_CATALOG,
          l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                     ASCIIToUTF16("ERROR_SAVING_CATALOG")));
      return false;
    }
  }

  return true;
}

void SandboxedUnpacker::Cleanup() {
  DCHECK(unpacker_io_task_runner_->RunsTasksOnCurrentThread());
  if (!temp_dir_.Delete()) {
    LOG(WARNING) << "Can not delete temp directory at "
                 << temp_dir_.path().value();
  }
}

SandboxedUnpacker::UtilityHostWrapper::UtilityHostWrapper() {}

bool SandboxedUnpacker::UtilityHostWrapper::StartIfNeeded(
    const base::FilePath& exposed_dir,
    const scoped_refptr<UtilityProcessHostClient>& client,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!utility_host_) {
    utility_host_ =
        UtilityProcessHost::Create(client, client_task_runner)->AsWeakPtr();
    utility_host_->SetName(
        l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_EXTENSION_UNPACKER_NAME));

    // Grant the subprocess access to our temp dir so it can write out files.
    DCHECK(!exposed_dir.empty());
    utility_host_->SetExposedDir(exposed_dir);
    if (!utility_host_->StartBatchMode()) {
      utility_host_.reset();
      return false;
    }
  }
  return true;
}

content::UtilityProcessHost* SandboxedUnpacker::UtilityHostWrapper::host()
    const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return utility_host_.get();
}

SandboxedUnpacker::UtilityHostWrapper::~UtilityHostWrapper() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (utility_host_) {
    utility_host_->EndBatchMode();
    utility_host_.reset();
  }
}

}  // namespace extensions
