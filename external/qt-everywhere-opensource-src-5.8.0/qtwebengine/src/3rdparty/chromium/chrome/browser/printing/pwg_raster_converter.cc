// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pwg_raster_converter.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/chrome_utility_printing_messages.h"
#include "chrome/grit/generated_resources.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/printer_description.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"
#include "printing/units.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

using content::BrowserThread;

class FileHandlers {
 public:
  FileHandlers() {}

  ~FileHandlers() {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  }

  void Init(base::RefCountedMemory* data);
  bool IsValid();

  base::FilePath GetPwgPath() const {
    return temp_dir_.path().AppendASCII("output.pwg");
  }

  base::FilePath GetPdfPath() const {
    return temp_dir_.path().AppendASCII("input.pdf");
  }

  IPC::PlatformFileForTransit GetPdfForProcess() {
    DCHECK(pdf_file_.IsValid());
    IPC::PlatformFileForTransit transit =
        IPC::TakePlatformFileForTransit(std::move(pdf_file_));
    return transit;
  }

  IPC::PlatformFileForTransit GetPwgForProcess() {
    DCHECK(pwg_file_.IsValid());
    IPC::PlatformFileForTransit transit =
        IPC::TakePlatformFileForTransit(std::move(pwg_file_));
    return transit;
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::File pdf_file_;
  base::File pwg_file_;
};

void FileHandlers::Init(base::RefCountedMemory* data) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (!temp_dir_.CreateUniqueTempDir()) {
    return;
  }

  if (static_cast<int>(data->size()) !=
      base::WriteFile(GetPdfPath(), data->front_as<char>(), data->size())) {
    return;
  }

  // Reopen in read only mode.
  pdf_file_.Initialize(GetPdfPath(),
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  pwg_file_.Initialize(GetPwgPath(),
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
}

bool FileHandlers::IsValid() {
  return pdf_file_.IsValid() && pwg_file_.IsValid();
}

// Converts PDF into PWG raster.
// Class uses 3 threads: UI, IO and FILE.
// Internal workflow is following:
// 1. Create instance on the UI thread. (files_, settings_,)
// 2. Create file on the FILE thread.
// 3. Start utility process and start conversion on the IO thread.
// 4. Run result callback on the UI thread.
// 5. Instance is destroyed from any thread that has the last reference.
// 6. FileHandlers destroyed on the FILE thread.
//    This step posts |FileHandlers| to be destroyed on the FILE thread.
// All these steps work sequentially, so no data should be accessed
// simultaneously by several threads.
class PwgUtilityProcessHostClient : public content::UtilityProcessHostClient {
 public:
  PwgUtilityProcessHostClient(
      const PdfRenderSettings& settings,
      const PwgRasterSettings& bitmap_settings);

  void Convert(base::RefCountedMemory* data,
               const PWGRasterConverter::ResultCallback& callback);

  // UtilityProcessHostClient implementation.
  void OnProcessCrashed(int exit_code) override;
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~PwgUtilityProcessHostClient() override;

  // Message handlers.
  void OnSucceeded();
  void OnFailed();

  void RunCallback(bool success);

  void StartProcessOnIOThread();

  void RunCallbackOnUIThread(bool success);
  void OnFilesReadyOnUIThread();

  std::unique_ptr<FileHandlers, BrowserThread::DeleteOnFileThread> files_;
  PdfRenderSettings settings_;
  PwgRasterSettings bitmap_settings_;
  PWGRasterConverter::ResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PwgUtilityProcessHostClient);
};

PwgUtilityProcessHostClient::PwgUtilityProcessHostClient(
    const printing::PdfRenderSettings& settings,
    const printing::PwgRasterSettings& bitmap_settings)
    : settings_(settings), bitmap_settings_(bitmap_settings) {}

PwgUtilityProcessHostClient::~PwgUtilityProcessHostClient() {
}

void PwgUtilityProcessHostClient::Convert(
    base::RefCountedMemory* data,
    const PWGRasterConverter::ResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback_ = callback;
  CHECK(!files_);
  files_.reset(new FileHandlers());
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&FileHandlers::Init, base::Unretained(files_.get()),
                 base::RetainedRef(data)),
      base::Bind(&PwgUtilityProcessHostClient::OnFilesReadyOnUIThread, this));
}

void PwgUtilityProcessHostClient::OnProcessCrashed(int exit_code) {
  OnFailed();
}

bool PwgUtilityProcessHostClient::OnMessageReceived(
  const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PwgUtilityProcessHostClient, message)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_RenderPDFPagesToPWGRaster_Succeeded, OnSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_RenderPDFPagesToPWGRaster_Failed,
                        OnFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PwgUtilityProcessHostClient::OnSucceeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  RunCallback(true);
}

void PwgUtilityProcessHostClient::OnFailed() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  RunCallback(false);
}

void PwgUtilityProcessHostClient::OnFilesReadyOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!files_->IsValid()) {
    RunCallbackOnUIThread(false);
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PwgUtilityProcessHostClient::StartProcessOnIOThread, this));
}

void PwgUtilityProcessHostClient::StartProcessOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::UtilityProcessHost* utility_process_host =
      content::UtilityProcessHost::Create(this,
                                          base::ThreadTaskRunnerHandle::Get());
  utility_process_host->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_PWG_RASTER_CONVERTOR_NAME));
  utility_process_host->Send(new ChromeUtilityMsg_RenderPDFPagesToPWGRaster(
      files_->GetPdfForProcess(), settings_, bitmap_settings_,
      files_->GetPwgForProcess()));
}

void PwgUtilityProcessHostClient::RunCallback(bool success) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PwgUtilityProcessHostClient::RunCallbackOnUIThread, this,
                 success));
}

void PwgUtilityProcessHostClient::RunCallbackOnUIThread(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!callback_.is_null()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback_, success,
                                       files_->GetPwgPath()));
    callback_.Reset();
  }
}

class PWGRasterConverterImpl : public PWGRasterConverter {
 public:
  PWGRasterConverterImpl();

  ~PWGRasterConverterImpl() override;

  void Start(base::RefCountedMemory* data,
             const printing::PdfRenderSettings& conversion_settings,
             const printing::PwgRasterSettings& bitmap_settings,
             const ResultCallback& callback) override;

 private:
  scoped_refptr<PwgUtilityProcessHostClient> utility_client_;
  base::CancelableCallback<ResultCallback::RunType> callback_;

  DISALLOW_COPY_AND_ASSIGN(PWGRasterConverterImpl);
};

PWGRasterConverterImpl::PWGRasterConverterImpl() {
}

PWGRasterConverterImpl::~PWGRasterConverterImpl() {
}

void PWGRasterConverterImpl::Start(
    base::RefCountedMemory* data,
    const printing::PdfRenderSettings& conversion_settings,
    const printing::PwgRasterSettings& bitmap_settings,
    const ResultCallback& callback) {
  // Rebind cancelable callback to avoid calling callback if
  // PWGRasterConverterImpl is destroyed.
  callback_.Reset(callback);
  utility_client_ =
      new PwgUtilityProcessHostClient(conversion_settings, bitmap_settings);
  utility_client_->Convert(data, callback_.callback());
}

}  // namespace

// static
std::unique_ptr<PWGRasterConverter> PWGRasterConverter::CreateDefault() {
  return std::unique_ptr<PWGRasterConverter>(new PWGRasterConverterImpl());
}

// static
printing::PdfRenderSettings PWGRasterConverter::GetConversionSettings(
    const cloud_devices::CloudDeviceDescription& printer_capabilities,
    const gfx::Size& page_size) {
  int dpi = printing::kDefaultPdfDpi;
  cloud_devices::printer::DpiCapability dpis;
  if (dpis.LoadFrom(printer_capabilities))
    dpi = std::max(dpis.GetDefault().horizontal, dpis.GetDefault().vertical);

  double scale = dpi;
  scale /= printing::kPointsPerInch;

  // Make vertical rectangle to optimize streaming to printer. Fix orientation
  // by autorotate.
  gfx::Rect area(std::min(page_size.width(), page_size.height()) * scale,
                 std::max(page_size.width(), page_size.height()) * scale);
  return printing::PdfRenderSettings(area, dpi, true /* autorotate */);
}

// static
printing::PwgRasterSettings PWGRasterConverter::GetBitmapSettings(
    const cloud_devices::CloudDeviceDescription& printer_capabilities,
    const cloud_devices::CloudDeviceDescription& ticket) {
  printing::PwgRasterSettings result;
  cloud_devices::printer::PwgRasterConfigCapability raster_capability;
  // If the raster capability fails to load, raster_capability will contain
  // the default value.
  raster_capability.LoadFrom(printer_capabilities);

  cloud_devices::printer::DuplexTicketItem duplex_item;
  cloud_devices::printer::DuplexType duplex_value =
      cloud_devices::printer::NO_DUPLEX;

  cloud_devices::printer::DocumentSheetBack document_sheet_back =
      raster_capability.value().document_sheet_back;

  if (duplex_item.LoadFrom(ticket)) {
    duplex_value = duplex_item.value();
  }

  result.odd_page_transform = printing::TRANSFORM_NORMAL;
  switch (duplex_value) {
    case cloud_devices::printer::NO_DUPLEX:
      result.odd_page_transform = printing::TRANSFORM_NORMAL;
      break;
    case cloud_devices::printer::LONG_EDGE:
      if (document_sheet_back == cloud_devices::printer::ROTATED) {
        result.odd_page_transform = printing::TRANSFORM_ROTATE_180;
      } else if (document_sheet_back == cloud_devices::printer::FLIPPED) {
        result.odd_page_transform = printing::TRANSFORM_FLIP_VERTICAL;
      }
      break;
    case cloud_devices::printer::SHORT_EDGE:
      if (document_sheet_back == cloud_devices::printer::MANUAL_TUMBLE) {
        result.odd_page_transform = printing::TRANSFORM_ROTATE_180;
      } else if (document_sheet_back == cloud_devices::printer::FLIPPED) {
        result.odd_page_transform = printing::TRANSFORM_FLIP_HORIZONTAL;
      }
  }

  result.rotate_all_pages = raster_capability.value().rotate_all_pages;

  result.reverse_page_order = raster_capability.value().reverse_order_streaming;
  return result;
}

}  // namespace printing
