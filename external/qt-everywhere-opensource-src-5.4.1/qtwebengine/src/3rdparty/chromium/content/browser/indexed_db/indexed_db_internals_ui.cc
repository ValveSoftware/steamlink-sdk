// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_internals_ui.h"

#include <string>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "grit/content_resources.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/text/bytes_formatting.h"
#include "webkit/common/database/database_identifier.h"

namespace content {

IndexedDBInternalsUI::IndexedDBInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->RegisterMessageCallback(
      "getAllOrigins",
      base::Bind(&IndexedDBInternalsUI::GetAllOrigins, base::Unretained(this)));

  web_ui->RegisterMessageCallback(
      "downloadOriginData",
      base::Bind(&IndexedDBInternalsUI::DownloadOriginData,
                 base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "forceClose",
      base::Bind(&IndexedDBInternalsUI::ForceCloseOrigin,
                 base::Unretained(this)));

  WebUIDataSource* source =
      WebUIDataSource::Create(kChromeUIIndexedDBInternalsHost);
  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");
  source->AddResourcePath("indexeddb_internals.js",
                          IDR_INDEXED_DB_INTERNALS_JS);
  source->AddResourcePath("indexeddb_internals.css",
                          IDR_INDEXED_DB_INTERNALS_CSS);
  source->SetDefaultResource(IDR_INDEXED_DB_INTERNALS_HTML);

  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, source);
}

IndexedDBInternalsUI::~IndexedDBInternalsUI() {}

void IndexedDBInternalsUI::AddContextFromStoragePartition(
    StoragePartition* partition) {
  scoped_refptr<IndexedDBContext> context = partition->GetIndexedDBContext();
  context->TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&IndexedDBInternalsUI::GetAllOriginsOnIndexedDBThread,
                 base::Unretained(this),
                 context,
                 partition->GetPath()));
}

void IndexedDBInternalsUI::GetAllOrigins(const base::ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  BrowserContext::StoragePartitionCallback cb =
      base::Bind(&IndexedDBInternalsUI::AddContextFromStoragePartition,
                 base::Unretained(this));
  BrowserContext::ForEachStoragePartition(browser_context, cb);
}

void IndexedDBInternalsUI::GetAllOriginsOnIndexedDBThread(
    scoped_refptr<IndexedDBContext> context,
    const base::FilePath& context_path) {
  DCHECK(context->TaskRunner()->RunsTasksOnCurrentThread());

  scoped_ptr<base::ListValue> info_list(static_cast<IndexedDBContextImpl*>(
      context.get())->GetAllOriginsDetails());

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&IndexedDBInternalsUI::OnOriginsReady,
                                     base::Unretained(this),
                                     base::Passed(&info_list),
                                     context_path));
}

void IndexedDBInternalsUI::OnOriginsReady(scoped_ptr<base::ListValue> origins,
                                          const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  web_ui()->CallJavascriptFunction(
      "indexeddb.onOriginsReady", *origins, base::StringValue(path.value()));
}

static void FindContext(const base::FilePath& partition_path,
                        StoragePartition** result_partition,
                        scoped_refptr<IndexedDBContextImpl>* result_context,
                        StoragePartition* storage_partition) {
  if (storage_partition->GetPath() == partition_path) {
    *result_partition = storage_partition;
    *result_context = static_cast<IndexedDBContextImpl*>(
        storage_partition->GetIndexedDBContext());
  }
}

bool IndexedDBInternalsUI::GetOriginData(
    const base::ListValue* args,
    base::FilePath* partition_path,
    GURL* origin_url,
    scoped_refptr<IndexedDBContextImpl>* context) {
  base::FilePath::StringType path_string;
  if (!args->GetString(0, &path_string))
    return false;
  *partition_path = base::FilePath(path_string);

  std::string url_string;
  if (!args->GetString(1, &url_string))
    return false;

  *origin_url = GURL(url_string);

  return GetOriginContext(*partition_path, *origin_url, context);
}

bool IndexedDBInternalsUI::GetOriginContext(
    const base::FilePath& path,
    const GURL& origin_url,
    scoped_refptr<IndexedDBContextImpl>* context) {
  // search the origins to find the right context
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  StoragePartition* result_partition;
  BrowserContext::StoragePartitionCallback cb =
      base::Bind(&FindContext, path, &result_partition, context);
  BrowserContext::ForEachStoragePartition(browser_context, cb);

  if (!result_partition || !(*context))
    return false;

  return true;
}

void IndexedDBInternalsUI::DownloadOriginData(const base::ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::FilePath partition_path;
  GURL origin_url;
  scoped_refptr<IndexedDBContextImpl> context;
  if (!GetOriginData(args, &partition_path, &origin_url, &context))
    return;

  DCHECK(context);
  context->TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&IndexedDBInternalsUI::DownloadOriginDataOnIndexedDBThread,
                 base::Unretained(this),
                 partition_path,
                 context,
                 origin_url));
}

void IndexedDBInternalsUI::ForceCloseOrigin(const base::ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::FilePath partition_path;
  GURL origin_url;
  scoped_refptr<IndexedDBContextImpl> context;
  if (!GetOriginData(args, &partition_path, &origin_url, &context))
    return;

  context->TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&IndexedDBInternalsUI::ForceCloseOriginOnIndexedDBThread,
                 base::Unretained(this),
                 partition_path,
                 context,
                 origin_url));
}

void IndexedDBInternalsUI::DownloadOriginDataOnIndexedDBThread(
    const base::FilePath& partition_path,
    const scoped_refptr<IndexedDBContextImpl> context,
    const GURL& origin_url) {
  DCHECK(context->TaskRunner()->RunsTasksOnCurrentThread());

  // Make sure the database hasn't been deleted since the page was loaded.
  if (!context->IsInOriginSet(origin_url))
    return;

  context->ForceClose(origin_url,
                      IndexedDBContextImpl::FORCE_CLOSE_INTERNALS_PAGE);
  size_t connection_count = context->GetConnectionCount(origin_url);

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return;

  // This will get cleaned up on the File thread after the download
  // has completed.
  base::FilePath temp_path = temp_dir.Take();

  std::string origin_id = webkit_database::GetIdentifierFromOrigin(origin_url);
  base::FilePath zip_path =
      temp_path.AppendASCII(origin_id).AddExtension(FILE_PATH_LITERAL("zip"));

  // This happens on the "webkit" thread (which is really just the IndexedDB
  // thread) as a simple way to avoid another script reopening the origin
  // while we are zipping.
  zip::Zip(context->GetFilePath(origin_url), zip_path, true);

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&IndexedDBInternalsUI::OnDownloadDataReady,
                                     base::Unretained(this),
                                     partition_path,
                                     origin_url,
                                     temp_path,
                                     zip_path,
                                     connection_count));
}

void IndexedDBInternalsUI::ForceCloseOriginOnIndexedDBThread(
    const base::FilePath& partition_path,
    const scoped_refptr<IndexedDBContextImpl> context,
    const GURL& origin_url) {
  DCHECK(context->TaskRunner()->RunsTasksOnCurrentThread());

  // Make sure the database hasn't been deleted since the page was loaded.
  if (!context->IsInOriginSet(origin_url))
    return;

  context->ForceClose(origin_url,
                      IndexedDBContextImpl::FORCE_CLOSE_INTERNALS_PAGE);
  size_t connection_count = context->GetConnectionCount(origin_url);

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&IndexedDBInternalsUI::OnForcedClose,
                                     base::Unretained(this),
                                     partition_path,
                                     origin_url,
                                     connection_count));
}

void IndexedDBInternalsUI::OnForcedClose(const base::FilePath& partition_path,
                                         const GURL& origin_url,
                                         size_t connection_count) {
  web_ui()->CallJavascriptFunction(
      "indexeddb.onForcedClose",
      base::StringValue(partition_path.value()),
      base::StringValue(origin_url.spec()),
      base::FundamentalValue(static_cast<double>(connection_count)));
}

void IndexedDBInternalsUI::OnDownloadDataReady(
    const base::FilePath& partition_path,
    const GURL& origin_url,
    const base::FilePath temp_path,
    const base::FilePath zip_path,
    size_t connection_count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const GURL url = GURL(FILE_PATH_LITERAL("file://") + zip_path.value());
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  scoped_ptr<DownloadUrlParameters> dl_params(
      DownloadUrlParameters::FromWebContents(web_ui()->GetWebContents(), url));
  DownloadManager* dlm = BrowserContext::GetDownloadManager(browser_context);

  const GURL referrer(web_ui()->GetWebContents()->GetLastCommittedURL());
  dl_params->set_referrer(
      content::Referrer(referrer, blink::WebReferrerPolicyDefault));

  // This is how to watch for the download to finish: first wait for it
  // to start, then attach a DownloadItem::Observer to observe the
  // state change to the finished state.
  dl_params->set_callback(base::Bind(&IndexedDBInternalsUI::OnDownloadStarted,
                                     base::Unretained(this),
                                     partition_path,
                                     origin_url,
                                     temp_path,
                                     connection_count));
  dlm->DownloadUrl(dl_params.Pass());
}

// The entire purpose of this class is to delete the temp file after
// the download is complete.
class FileDeleter : public DownloadItem::Observer {
 public:
  explicit FileDeleter(const base::FilePath& temp_dir) : temp_dir_(temp_dir) {}
  virtual ~FileDeleter();

  virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(DownloadItem* item) OVERRIDE {}
  virtual void OnDownloadRemoved(DownloadItem* item) OVERRIDE {}
  virtual void OnDownloadDestroyed(DownloadItem* item) OVERRIDE {}

 private:
  const base::FilePath temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(FileDeleter);
};

void FileDeleter::OnDownloadUpdated(DownloadItem* item) {
  switch (item->GetState()) {
    case DownloadItem::IN_PROGRESS:
      break;
    case DownloadItem::COMPLETE:
    case DownloadItem::CANCELLED:
    case DownloadItem::INTERRUPTED: {
      item->RemoveObserver(this);
      BrowserThread::DeleteOnFileThread::Destruct(this);
      break;
    }
    default:
      NOTREACHED();
  }
}

FileDeleter::~FileDeleter() {
  base::ScopedTempDir path;
  bool will_delete ALLOW_UNUSED = path.Set(temp_dir_);
  DCHECK(will_delete);
}

void IndexedDBInternalsUI::OnDownloadStarted(
    const base::FilePath& partition_path,
    const GURL& origin_url,
    const base::FilePath& temp_path,
    size_t connection_count,
    DownloadItem* item,
    DownloadInterruptReason interrupt_reason) {

  if (interrupt_reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    LOG(ERROR) << "Error downloading database dump: "
               << DownloadInterruptReasonToString(interrupt_reason);
    return;
  }

  item->AddObserver(new FileDeleter(temp_path));
  web_ui()->CallJavascriptFunction(
      "indexeddb.onOriginDownloadReady",
      base::StringValue(partition_path.value()),
      base::StringValue(origin_url.spec()),
      base::FundamentalValue(static_cast<double>(connection_count)));
}

}  // namespace content
