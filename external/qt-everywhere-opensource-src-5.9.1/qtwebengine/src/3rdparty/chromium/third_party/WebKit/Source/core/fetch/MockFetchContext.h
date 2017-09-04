// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockFetchContext_h
#define MockFetchContext_h

#include "core/fetch/FetchContext.h"
#include "core/fetch/FetchRequest.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/scheduler/test/fake_web_task_runner.h"
#include "wtf/PtrUtil.h"

#include <memory>

namespace blink {

class KURL;
class ResourceRequest;
class WebTaskRunner;
struct ResourceLoaderOptions;

// Mocked FetchContext for testing.
// TODO(toyoshim): Use this class by other unit tests that currently have own
// mocked FetchContext respectively.
class MockFetchContext : public FetchContext {
 public:
  enum LoadPolicy {
    kShouldLoadNewResource,
    kShouldNotLoadNewResource,
  };
  static MockFetchContext* create(LoadPolicy loadPolicy) {
    return new MockFetchContext(loadPolicy);
  }

  ~MockFetchContext() override {}

  bool allowImage(bool imagesEnabled, const KURL&) const override {
    return true;
  }
  bool canRequest(Resource::Type,
                  const ResourceRequest&,
                  const KURL&,
                  const ResourceLoaderOptions&,
                  bool forPreload,
                  FetchRequest::OriginRestriction) const override {
    return true;
  }
  bool shouldLoadNewResource(Resource::Type) const override {
    return m_loadPolicy == kShouldLoadNewResource;
  }
  WebTaskRunner* loadingTaskRunner() const override { return m_runner.get(); }

  void setCachePolicy(CachePolicy policy) { m_policy = policy; }
  CachePolicy getCachePolicy() const override { return m_policy; }
  void setLoadComplete(bool complete) { m_complete = complete; }
  bool isLoadComplete() const override { return m_complete; }

  void addResourceTiming(
      const ResourceTimingInfo& resourceTimingInfo) override {
    m_transferSize = resourceTimingInfo.transferSize();
  }
  long long getTransferSize() const { return m_transferSize; }

 private:
  MockFetchContext(LoadPolicy loadPolicy)
      : m_loadPolicy(loadPolicy),
        m_policy(CachePolicyVerify),
        m_runner(wrapUnique(new scheduler::FakeWebTaskRunner)),
        m_complete(false),
        m_transferSize(-1) {}

  enum LoadPolicy m_loadPolicy;
  CachePolicy m_policy;
  std::unique_ptr<scheduler::FakeWebTaskRunner> m_runner;
  bool m_complete;
  long long m_transferSize;
};

}  // namespace blink

#endif
