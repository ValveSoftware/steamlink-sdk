// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/indexed_db_context.h"
#include "content/public/browser/web_ui_controller.h"

namespace base {
class ListValue;
}

namespace content {

class DownloadItem;
class IndexedDBContextImpl;
class StoragePartition;

// The implementation for the chrome://indexeddb-internals page.
class IndexedDBInternalsUI : public WebUIController {
 public:
  explicit IndexedDBInternalsUI(WebUI* web_ui);
  virtual ~IndexedDBInternalsUI();

 private:
  void GetAllOrigins(const base::ListValue* args);
  void GetAllOriginsOnIndexedDBThread(scoped_refptr<IndexedDBContext> context,
                                      const base::FilePath& context_path);
  void OnOriginsReady(scoped_ptr<base::ListValue> origins,
                      const base::FilePath& path);

  void AddContextFromStoragePartition(StoragePartition* partition);

  void DownloadOriginData(const base::ListValue* args);
  void DownloadOriginDataOnIndexedDBThread(
      const base::FilePath& partition_path,
      const scoped_refptr<IndexedDBContextImpl> context,
      const GURL& origin_url);
  void OnDownloadDataReady(const base::FilePath& partition_path,
                           const GURL& origin_url,
                           const base::FilePath temp_path,
                           const base::FilePath zip_path,
                           size_t connection_count);
  void OnDownloadStarted(const base::FilePath& partition_path,
                         const GURL& origin_url,
                         const base::FilePath& temp_path,
                         size_t connection_count,
                         DownloadItem* item,
                         DownloadInterruptReason interrupt_reason);

  void ForceCloseOrigin(const base::ListValue* args);
  void ForceCloseOriginOnIndexedDBThread(
      const base::FilePath& partition_path,
      const scoped_refptr<IndexedDBContextImpl> context,
      const GURL& origin_url);
  void OnForcedClose(const base::FilePath& partition_path,
                     const GURL& origin_url,
                     size_t connection_count);
  bool GetOriginContext(const base::FilePath& path,
                        const GURL& origin_url,
                        scoped_refptr<IndexedDBContextImpl>* context);
  bool GetOriginData(const base::ListValue* args,
                     base::FilePath* path,
                     GURL* origin_url,
                     scoped_refptr<IndexedDBContextImpl>* context);

  DISALLOW_COPY_AND_ASSIGN(IndexedDBInternalsUI);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_
