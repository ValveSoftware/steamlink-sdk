// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_sorter_posix.h"

#include "base/bind.h"
#include "base/logging.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/udp/datagram_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// Used to map destination address to source address.
typedef std::map<IPAddressNumber, IPAddressNumber> AddressMapping;

IPAddressNumber ParseIP(const std::string& str) {
  IPAddressNumber addr;
  CHECK(ParseIPLiteralToNumber(str, &addr));
  return addr;
}

// A mock socket which binds to source address according to AddressMapping.
class TestUDPClientSocket : public DatagramClientSocket {
 public:
  explicit TestUDPClientSocket(const AddressMapping* mapping)
      : mapping_(mapping), connected_(false)  {}

  virtual ~TestUDPClientSocket() {}

  virtual int Read(IOBuffer*, int, const CompletionCallback&) OVERRIDE {
    NOTIMPLEMENTED();
    return OK;
  }
  virtual int Write(IOBuffer*, int, const CompletionCallback&) OVERRIDE {
    NOTIMPLEMENTED();
    return OK;
  }
  virtual int SetReceiveBufferSize(int32) OVERRIDE {
    return OK;
  }
  virtual int SetSendBufferSize(int32) OVERRIDE {
    return OK;
  }

  virtual void Close() OVERRIDE {}
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE {
    NOTIMPLEMENTED();
    return OK;
  }
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE {
    if (!connected_)
      return ERR_UNEXPECTED;
    *address = local_endpoint_;
    return OK;
  }

  virtual int Connect(const IPEndPoint& remote) OVERRIDE {
    if (connected_)
      return ERR_UNEXPECTED;
    AddressMapping::const_iterator it = mapping_->find(remote.address());
    if (it == mapping_->end())
      return ERR_FAILED;
    connected_ = true;
    local_endpoint_ = IPEndPoint(it->second, 39874 /* arbitrary port */);
    return OK;
  }

  virtual const BoundNetLog& NetLog() const OVERRIDE {
    return net_log_;
  }

 private:
  BoundNetLog net_log_;
  const AddressMapping* mapping_;
  bool connected_;
  IPEndPoint local_endpoint_;

  DISALLOW_COPY_AND_ASSIGN(TestUDPClientSocket);
};

// Creates TestUDPClientSockets and maintains an AddressMapping.
class TestSocketFactory : public ClientSocketFactory {
 public:
  TestSocketFactory() {}
  virtual ~TestSocketFactory() {}

  virtual scoped_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType,
      const RandIntCallback&,
      NetLog*,
      const NetLog::Source&) OVERRIDE {
    return scoped_ptr<DatagramClientSocket>(new TestUDPClientSocket(&mapping_));
  }
  virtual scoped_ptr<StreamSocket> CreateTransportClientSocket(
      const AddressList&,
      NetLog*,
      const NetLog::Source&) OVERRIDE {
    NOTIMPLEMENTED();
    return scoped_ptr<StreamSocket>();
  }
  virtual scoped_ptr<SSLClientSocket> CreateSSLClientSocket(
      scoped_ptr<ClientSocketHandle>,
      const HostPortPair&,
      const SSLConfig&,
      const SSLClientSocketContext&) OVERRIDE {
    NOTIMPLEMENTED();
    return scoped_ptr<SSLClientSocket>();
  }
  virtual void ClearSSLSessionCache() OVERRIDE {
    NOTIMPLEMENTED();
  }

  void AddMapping(const IPAddressNumber& dst, const IPAddressNumber& src) {
    mapping_[dst] = src;
  }

 private:
  AddressMapping mapping_;

  DISALLOW_COPY_AND_ASSIGN(TestSocketFactory);
};

void OnSortComplete(AddressList* result_buf,
                    const CompletionCallback& callback,
                    bool success,
                    const AddressList& result) {
  EXPECT_TRUE(success);
  if (success)
    *result_buf = result;
  callback.Run(OK);
}

}  // namespace

class AddressSorterPosixTest : public testing::Test {
 protected:
  AddressSorterPosixTest() : sorter_(&socket_factory_) {}

  void AddMapping(const std::string& dst, const std::string& src) {
    socket_factory_.AddMapping(ParseIP(dst), ParseIP(src));
  }

  AddressSorterPosix::SourceAddressInfo* GetSourceInfo(
      const std::string& addr) {
    IPAddressNumber address = ParseIP(addr);
    AddressSorterPosix::SourceAddressInfo* info = &sorter_.source_map_[address];
    if (info->scope == AddressSorterPosix::SCOPE_UNDEFINED)
      sorter_.FillPolicy(address, info);
    return info;
  }

  // Verify that NULL-terminated |addresses| matches (-1)-terminated |order|
  // after sorting.
  void Verify(const char* addresses[], const int order[]) {
    AddressList list;
    for (const char** addr = addresses; *addr != NULL; ++addr)
      list.push_back(IPEndPoint(ParseIP(*addr), 80));
    for (size_t i = 0; order[i] >= 0; ++i)
      CHECK_LT(order[i], static_cast<int>(list.size()));

    AddressList result;
    TestCompletionCallback callback;
    sorter_.Sort(list, base::Bind(&OnSortComplete, &result,
                                  callback.callback()));
    callback.WaitForResult();

    for (size_t i = 0; (i < result.size()) || (order[i] >= 0); ++i) {
      IPEndPoint expected = order[i] >= 0 ? list[order[i]] : IPEndPoint();
      IPEndPoint actual = i < result.size() ? result[i] : IPEndPoint();
      EXPECT_TRUE(expected.address() == actual.address()) <<
          "Address out of order at position " << i << "\n" <<
          "  Actual: " << actual.ToStringWithoutPort() << "\n" <<
          "Expected: " << expected.ToStringWithoutPort();
    }
  }

  TestSocketFactory socket_factory_;
  AddressSorterPosix sorter_;
};

// Rule 1: Avoid unusable destinations.
TEST_F(AddressSorterPosixTest, Rule1) {
  AddMapping("10.0.0.231", "10.0.0.1");
  const char* addresses[] = { "::1", "10.0.0.231", "127.0.0.1", NULL };
  const int order[] = { 1, -1 };
  Verify(addresses, order);
}

// Rule 2: Prefer matching scope.
TEST_F(AddressSorterPosixTest, Rule2) {
  AddMapping("3002::1", "4000::10");      // matching global
  AddMapping("ff32::1", "fe81::10");      // matching link-local
  AddMapping("fec1::1", "fec1::10");      // matching node-local
  AddMapping("3002::2", "::1");           // global vs. link-local
  AddMapping("fec1::2", "fe81::10");      // site-local vs. link-local
  AddMapping("8.0.0.1", "169.254.0.10");  // global vs. link-local
  // In all three cases, matching scope is preferred.
  const int order[] = { 1, 0, -1 };
  const char* addresses1[] = { "3002::2", "3002::1", NULL };
  Verify(addresses1, order);
  const char* addresses2[] = { "fec1::2", "ff32::1", NULL };
  Verify(addresses2, order);
  const char* addresses3[] = { "8.0.0.1", "fec1::1", NULL };
  Verify(addresses3, order);
}

// Rule 3: Avoid deprecated addresses.
TEST_F(AddressSorterPosixTest, Rule3) {
  // Matching scope.
  AddMapping("3002::1", "4000::10");
  GetSourceInfo("4000::10")->deprecated = true;
  AddMapping("3002::2", "4000::20");
  const char* addresses[] = { "3002::1", "3002::2", NULL };
  const int order[] = { 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 4: Prefer home addresses.
TEST_F(AddressSorterPosixTest, Rule4) {
  AddMapping("3002::1", "4000::10");
  AddMapping("3002::2", "4000::20");
  GetSourceInfo("4000::20")->home = true;
  const char* addresses[] = { "3002::1", "3002::2", NULL };
  const int order[] = { 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 5: Prefer matching label.
TEST_F(AddressSorterPosixTest, Rule5) {
  AddMapping("::1", "::1");                       // matching loopback
  AddMapping("::ffff:1234:1", "::ffff:1234:10");  // matching IPv4-mapped
  AddMapping("2001::1", "::ffff:1234:10");        // Teredo vs. IPv4-mapped
  AddMapping("2002::1", "2001::10");              // 6to4 vs. Teredo
  const int order[] = { 1, 0, -1 };
  {
    const char* addresses[] = { "2001::1", "::1", NULL };
    Verify(addresses, order);
  }
  {
    const char* addresses[] = { "2002::1", "::ffff:1234:1", NULL };
    Verify(addresses, order);
  }
}

// Rule 6: Prefer higher precedence.
TEST_F(AddressSorterPosixTest, Rule6) {
  AddMapping("::1", "::1");                       // loopback
  AddMapping("ff32::1", "fe81::10");              // multicast
  AddMapping("::ffff:1234:1", "::ffff:1234:10");  // IPv4-mapped
  AddMapping("2001::1", "2001::10");              // Teredo
  const char* addresses[] = { "2001::1", "::ffff:1234:1", "ff32::1", "::1",
                              NULL };
  const int order[] = { 3, 2, 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 7: Prefer native transport.
TEST_F(AddressSorterPosixTest, Rule7) {
  AddMapping("3002::1", "4000::10");
  AddMapping("3002::2", "4000::20");
  GetSourceInfo("4000::20")->native = true;
  const char* addresses[] = { "3002::1", "3002::2", NULL };
  const int order[] = { 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 8: Prefer smaller scope.
TEST_F(AddressSorterPosixTest, Rule8) {
  // Matching scope. Should precede the others by Rule 2.
  AddMapping("fe81::1", "fe81::10");  // link-local
  AddMapping("3000::1", "4000::10");  // global
  // Mismatched scope.
  AddMapping("ff32::1", "4000::10");  // link-local
  AddMapping("ff35::1", "4000::10");  // site-local
  AddMapping("ff38::1", "4000::10");  // org-local
  const char* addresses[] = { "ff38::1", "3000::1", "ff35::1", "ff32::1",
                              "fe81::1", NULL };
  const int order[] = { 4, 1, 3, 2, 0, -1 };
  Verify(addresses, order);
}

// Rule 9: Use longest matching prefix.
TEST_F(AddressSorterPosixTest, Rule9) {
  AddMapping("3000::1", "3000:ffff::10");  // 16 bit match
  GetSourceInfo("3000:ffff::10")->prefix_length = 16;
  AddMapping("4000::1", "4000::10");       // 123 bit match, limited to 15
  GetSourceInfo("4000::10")->prefix_length = 15;
  AddMapping("4002::1", "4000::10");       // 14 bit match
  AddMapping("4080::1", "4000::10");       // 8 bit match
  const char* addresses[] = { "4080::1", "4002::1", "4000::1", "3000::1",
                              NULL };
  const int order[] = { 3, 2, 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 10: Leave the order unchanged.
TEST_F(AddressSorterPosixTest, Rule10) {
  AddMapping("4000::1", "4000::10");
  AddMapping("4000::2", "4000::10");
  AddMapping("4000::3", "4000::10");
  const char* addresses[] = { "4000::1", "4000::2", "4000::3", NULL };
  const int order[] = { 0, 1, 2, -1 };
  Verify(addresses, order);
}

TEST_F(AddressSorterPosixTest, MultipleRules) {
  AddMapping("::1", "::1");           // loopback
  AddMapping("ff32::1", "fe81::10");  // link-local multicast
  AddMapping("ff3e::1", "4000::10");  // global multicast
  AddMapping("4000::1", "4000::10");  // global unicast
  AddMapping("ff32::2", "fe81::20");  // deprecated link-local multicast
  GetSourceInfo("fe81::20")->deprecated = true;
  const char* addresses[] = { "ff3e::1", "ff32::2", "4000::1", "ff32::1", "::1",
                              "8.0.0.1", NULL };
  const int order[] = { 4, 3, 0, 2, 1, -1 };
  Verify(addresses, order);
}

}  // namespace net
