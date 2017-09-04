// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLOB_STORAGE_WEBBLOBREGISTRY_IMPL_H_
#define CONTENT_CHILD_BLOB_STORAGE_WEBBLOBREGISTRY_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "storage/common/blob_storage/blob_storage_constants.h"
#include "storage/common/data_element.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"

namespace blink {
class WebThreadSafeData;
}  // namespace blink

namespace content {
class BlobConsolidation;
class ThreadSafeSender;

class WebBlobRegistryImpl : public blink::WebBlobRegistry {
 public:
  WebBlobRegistryImpl(scoped_refptr<base::SingleThreadTaskRunner> io_runner,
                      scoped_refptr<base::SingleThreadTaskRunner> main_runner,
                      scoped_refptr<ThreadSafeSender> sender);
  ~WebBlobRegistryImpl() override;

  // TODO(dmurph): remove this after moving to createBuilder. crbug.com/504583
  void registerBlobData(const blink::WebString& uuid,
                        const blink::WebBlobData& data) override;

  std::unique_ptr<blink::WebBlobRegistry::Builder> createBuilder(
      const blink::WebString& uuid,
      const blink::WebString& content_type) override;

  void addBlobDataRef(const blink::WebString& uuid) override;
  void removeBlobDataRef(const blink::WebString& uuid) override;
  void registerPublicBlobURL(const blink::WebURL&,
                             const blink::WebString& uuid) override;
  void revokePublicBlobURL(const blink::WebURL&) override;

  // Additional support for Streams.
  void registerStreamURL(const blink::WebURL& url,
                         const blink::WebString& content_type) override;
  void registerStreamURL(const blink::WebURL& url,
                         const blink::WebURL& src_url) override;
  void addDataToStream(const blink::WebURL& url,
                       const char* data,
                       size_t length) override;
  void flushStream(const blink::WebURL& url) override;
  void finalizeStream(const blink::WebURL& url) override;
  void abortStream(const blink::WebURL& url) override;
  void unregisterStreamURL(const blink::WebURL& url) override;

 private:
  // Handles all of the IPCs sent for building a blob.
  class BuilderImpl : public blink::WebBlobRegistry::Builder {
   public:
    BuilderImpl(const blink::WebString& uuid,
                const blink::WebString& content_type,
                ThreadSafeSender* sender,
                scoped_refptr<base::SingleThreadTaskRunner> io_runner,
                scoped_refptr<base::SingleThreadTaskRunner> main_runner);
    ~BuilderImpl() override;

    void appendData(const blink::WebThreadSafeData&) override;
    void appendFile(const blink::WebString& path,
                    uint64_t offset,
                    uint64_t length,
                    double expected_modification_time) override;
    void appendBlob(const blink::WebString& uuid,
                    uint64_t offset,
                    uint64_t length) override;
    void appendFileSystemURL(const blink::WebURL&,
                             uint64_t offset,
                             uint64_t length,
                             double expected_modification_time) override;

    void build() override;

   private:
    const std::string uuid_;
    const std::string content_type_;
    scoped_refptr<BlobConsolidation> consolidation_;
    scoped_refptr<ThreadSafeSender> sender_;
    scoped_refptr<base::SingleThreadTaskRunner> io_runner_;
    scoped_refptr<base::SingleThreadTaskRunner> main_runner_;
  };

  storage::BlobStorageLimits limits_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_runner_;
  scoped_refptr<ThreadSafeSender> sender_;
};

}  // namespace content

#endif  // CONTENT_CHILD_BLOB_STORAGE_WEBBLOBREGISTRY_IMPL_H_
