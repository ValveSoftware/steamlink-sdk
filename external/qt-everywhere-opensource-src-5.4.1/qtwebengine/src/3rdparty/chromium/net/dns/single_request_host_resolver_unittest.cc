// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/single_request_host_resolver.h"

#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Helper class used by SingleRequestHostResolverTest.Cancel test.
// It checks that only one request is outstanding at a time, and that
// it is cancelled before the class is destroyed.
class HangingHostResolver : public HostResolver {
 public:
  HangingHostResolver() : outstanding_request_(NULL) {}

  virtual ~HangingHostResolver() {
    EXPECT_TRUE(!has_outstanding_request());
  }

  bool has_outstanding_request() const {
    return outstanding_request_ != NULL;
  }

  virtual int Resolve(const RequestInfo& info,
                      RequestPriority priority,
                      AddressList* addresses,
                      const CompletionCallback& callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log) OVERRIDE {
    EXPECT_FALSE(has_outstanding_request());
    outstanding_request_ = reinterpret_cast<RequestHandle>(0x1234);
    *out_req = outstanding_request_;

    // Never complete this request! Caller is expected to cancel it
    // before destroying the resolver.
    return ERR_IO_PENDING;
  }

  virtual int ResolveFromCache(const RequestInfo& info,
                               AddressList* addresses,
                               const BoundNetLog& net_log) OVERRIDE {
    NOTIMPLEMENTED();
    return ERR_UNEXPECTED;
  }

  virtual void CancelRequest(RequestHandle req) OVERRIDE {
    EXPECT_TRUE(has_outstanding_request());
    EXPECT_EQ(req, outstanding_request_);
    outstanding_request_ = NULL;
  }

 private:
  RequestHandle outstanding_request_;

  DISALLOW_COPY_AND_ASSIGN(HangingHostResolver);
};

// Test that a regular end-to-end lookup returns the expected result.
TEST(SingleRequestHostResolverTest, NormalResolve) {
  // Create a host resolver dependency that returns address "199.188.1.166"
  // for resolutions of "watsup".
  MockHostResolver resolver;
  resolver.rules()->AddIPLiteralRule("watsup", "199.188.1.166", std::string());

  SingleRequestHostResolver single_request_resolver(&resolver);

  // Resolve "watsup:90" using our SingleRequestHostResolver.
  AddressList addrlist;
  TestCompletionCallback callback;
  HostResolver::RequestInfo request(HostPortPair("watsup", 90));
  int rv = single_request_resolver.Resolve(
      request, DEFAULT_PRIORITY, &addrlist, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  // Verify that the result is what we specified in the MockHostResolver.
  ASSERT_FALSE(addrlist.empty());
  EXPECT_EQ("199.188.1.166", addrlist.front().ToStringWithoutPort());
}

// Test that the Cancel() method cancels any outstanding request.
TEST(SingleRequestHostResolverTest, Cancel) {
  HangingHostResolver resolver;

  {
    SingleRequestHostResolver single_request_resolver(&resolver);

    // Resolve "watsup:90" using our SingleRequestHostResolver.
    AddressList addrlist;
    TestCompletionCallback callback;
    HostResolver::RequestInfo request(HostPortPair("watsup", 90));
    int rv = single_request_resolver.Resolve(request,
                                             DEFAULT_PRIORITY,
                                             &addrlist,
                                             callback.callback(),
                                             BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_TRUE(resolver.has_outstanding_request());
  }

  // Now that the SingleRequestHostResolver has been destroyed, the
  // in-progress request should have been aborted.
  EXPECT_FALSE(resolver.has_outstanding_request());
}

// Test that the Cancel() method is a no-op when there is no outstanding
// request.
TEST(SingleRequestHostResolverTest, CancelWhileNoPendingRequest) {
  HangingHostResolver resolver;
  SingleRequestHostResolver single_request_resolver(&resolver);
  single_request_resolver.Cancel();

  // To pass, HangingHostResolver should not have received a cancellation
  // request (since there is nothing to cancel). If it does, it will crash.
}

} // namespace

}  // namespace net
