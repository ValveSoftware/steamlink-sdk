// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_test_util.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/auth.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socket.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"

// Socket events are easier to debug if you log individual reads and writes.
// Enable these if locally debugging, but they are too noisy for the waterfall.
#if 0
#define NET_TRACE(level, s) DLOG(level) << s << __FUNCTION__ << "() "
#else
#define NET_TRACE(level, s) EAT_STREAM_PARAMETERS
#endif

namespace net {

namespace {

inline char AsciifyHigh(char x) {
  char nybble = static_cast<char>((x >> 4) & 0x0F);
  return nybble + ((nybble < 0x0A) ? '0' : 'A' - 10);
}

inline char AsciifyLow(char x) {
  char nybble = static_cast<char>((x >> 0) & 0x0F);
  return nybble + ((nybble < 0x0A) ? '0' : 'A' - 10);
}

inline char Asciify(char x) {
  if ((x < 0) || !isprint(x))
    return '.';
  return x;
}

void DumpData(const char* data, int data_len) {
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;
  DVLOG(1) << "Length:  " << data_len;
  const char* pfx = "Data:    ";
  if (!data || (data_len <= 0)) {
    DVLOG(1) << pfx << "<None>";
  } else {
    int i;
    for (i = 0; i <= (data_len - 4); i += 4) {
      DVLOG(1) << pfx
               << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
               << AsciifyHigh(data[i + 1]) << AsciifyLow(data[i + 1])
               << AsciifyHigh(data[i + 2]) << AsciifyLow(data[i + 2])
               << AsciifyHigh(data[i + 3]) << AsciifyLow(data[i + 3])
               << "  '"
               << Asciify(data[i + 0])
               << Asciify(data[i + 1])
               << Asciify(data[i + 2])
               << Asciify(data[i + 3])
               << "'";
      pfx = "         ";
    }
    // Take care of any 'trailing' bytes, if data_len was not a multiple of 4.
    switch (data_len - i) {
      case 3:
        DVLOG(1) << pfx
                 << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                 << AsciifyHigh(data[i + 1]) << AsciifyLow(data[i + 1])
                 << AsciifyHigh(data[i + 2]) << AsciifyLow(data[i + 2])
                 << "    '"
                 << Asciify(data[i + 0])
                 << Asciify(data[i + 1])
                 << Asciify(data[i + 2])
                 << " '";
        break;
      case 2:
        DVLOG(1) << pfx
                 << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                 << AsciifyHigh(data[i + 1]) << AsciifyLow(data[i + 1])
                 << "      '"
                 << Asciify(data[i + 0])
                 << Asciify(data[i + 1])
                 << "  '";
        break;
      case 1:
        DVLOG(1) << pfx
                 << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                 << "        '"
                 << Asciify(data[i + 0])
                 << "   '";
        break;
    }
  }
}

template <MockReadWriteType type>
void DumpMockReadWrite(const MockReadWrite<type>& r) {
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;
  DVLOG(1) << "Async:   " << (r.mode == ASYNC)
           << "\nResult:  " << r.result;
  DumpData(r.data, r.data_len);
  const char* stop = (r.sequence_number & MockRead::STOPLOOP) ? " (STOP)" : "";
  DVLOG(1) << "Stage:   " << (r.sequence_number & ~MockRead::STOPLOOP) << stop
           << "\nTime:    " << r.time_stamp.ToInternalValue();
}

}  // namespace

MockConnect::MockConnect() : mode(ASYNC), result(OK) {
  IPAddressNumber ip;
  CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
  peer_addr = IPEndPoint(ip, 0);
}

MockConnect::MockConnect(IoMode io_mode, int r) : mode(io_mode), result(r) {
  IPAddressNumber ip;
  CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
  peer_addr = IPEndPoint(ip, 0);
}

MockConnect::MockConnect(IoMode io_mode, int r, IPEndPoint addr) :
    mode(io_mode),
    result(r),
    peer_addr(addr) {
}

MockConnect::~MockConnect() {}

StaticSocketDataProvider::StaticSocketDataProvider()
    : reads_(NULL),
      read_index_(0),
      read_count_(0),
      writes_(NULL),
      write_index_(0),
      write_count_(0) {
}

StaticSocketDataProvider::StaticSocketDataProvider(MockRead* reads,
                                                   size_t reads_count,
                                                   MockWrite* writes,
                                                   size_t writes_count)
    : reads_(reads),
      read_index_(0),
      read_count_(reads_count),
      writes_(writes),
      write_index_(0),
      write_count_(writes_count) {
}

StaticSocketDataProvider::~StaticSocketDataProvider() {}

const MockRead& StaticSocketDataProvider::PeekRead() const {
  CHECK(!at_read_eof());
  return reads_[read_index_];
}

const MockWrite& StaticSocketDataProvider::PeekWrite() const {
  CHECK(!at_write_eof());
  return writes_[write_index_];
}

const MockRead& StaticSocketDataProvider::PeekRead(size_t index) const {
  CHECK_LT(index, read_count_);
  return reads_[index];
}

const MockWrite& StaticSocketDataProvider::PeekWrite(size_t index) const {
  CHECK_LT(index, write_count_);
  return writes_[index];
}

MockRead StaticSocketDataProvider::GetNextRead() {
  CHECK(!at_read_eof());
  reads_[read_index_].time_stamp = base::Time::Now();
  return reads_[read_index_++];
}

MockWriteResult StaticSocketDataProvider::OnWrite(const std::string& data) {
  if (!writes_) {
    // Not using mock writes; succeed synchronously.
    return MockWriteResult(SYNCHRONOUS, data.length());
  }
  EXPECT_FALSE(at_write_eof());
  if (at_write_eof()) {
    // Show what the extra write actually consists of.
    EXPECT_EQ("<unexpected write>", data);
    return MockWriteResult(SYNCHRONOUS, ERR_UNEXPECTED);
  }

  // Check that what we are writing matches the expectation.
  // Then give the mocked return value.
  MockWrite* w = &writes_[write_index_++];
  w->time_stamp = base::Time::Now();
  int result = w->result;
  if (w->data) {
    // Note - we can simulate a partial write here.  If the expected data
    // is a match, but shorter than the write actually written, that is legal.
    // Example:
    //   Application writes "foobarbaz" (9 bytes)
    //   Expected write was "foo" (3 bytes)
    //   This is a success, and we return 3 to the application.
    std::string expected_data(w->data, w->data_len);
    EXPECT_GE(data.length(), expected_data.length());
    std::string actual_data(data.substr(0, w->data_len));
    EXPECT_EQ(expected_data, actual_data);
    if (expected_data != actual_data)
      return MockWriteResult(SYNCHRONOUS, ERR_UNEXPECTED);
    if (result == OK)
      result = w->data_len;
  }
  return MockWriteResult(w->mode, result);
}

void StaticSocketDataProvider::Reset() {
  read_index_ = 0;
  write_index_ = 0;
}

DynamicSocketDataProvider::DynamicSocketDataProvider()
    : short_read_limit_(0),
      allow_unconsumed_reads_(false) {
}

DynamicSocketDataProvider::~DynamicSocketDataProvider() {}

MockRead DynamicSocketDataProvider::GetNextRead() {
  if (reads_.empty())
    return MockRead(SYNCHRONOUS, ERR_UNEXPECTED);
  MockRead result = reads_.front();
  if (short_read_limit_ == 0 || result.data_len <= short_read_limit_) {
    reads_.pop_front();
  } else {
    result.data_len = short_read_limit_;
    reads_.front().data += result.data_len;
    reads_.front().data_len -= result.data_len;
  }
  return result;
}

void DynamicSocketDataProvider::Reset() {
  reads_.clear();
}

void DynamicSocketDataProvider::SimulateRead(const char* data,
                                             const size_t length) {
  if (!allow_unconsumed_reads_) {
    EXPECT_TRUE(reads_.empty()) << "Unconsumed read: " << reads_.front().data;
  }
  reads_.push_back(MockRead(ASYNC, data, length));
}

SSLSocketDataProvider::SSLSocketDataProvider(IoMode mode, int result)
    : connect(mode, result),
      next_proto_status(SSLClientSocket::kNextProtoUnsupported),
      was_npn_negotiated(false),
      protocol_negotiated(kProtoUnknown),
      client_cert_sent(false),
      cert_request_info(NULL),
      channel_id_sent(false),
      connection_status(0) {
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1_2,
                                &connection_status);
  // Set to TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305
  SSLConnectionStatusSetCipherSuite(0xcc14, &connection_status);
}

SSLSocketDataProvider::~SSLSocketDataProvider() {
}

void SSLSocketDataProvider::SetNextProto(NextProto proto) {
  was_npn_negotiated = true;
  next_proto_status = SSLClientSocket::kNextProtoNegotiated;
  protocol_negotiated = proto;
  next_proto = SSLClientSocket::NextProtoToString(proto);
}

DelayedSocketData::DelayedSocketData(
    int write_delay, MockRead* reads, size_t reads_count,
    MockWrite* writes, size_t writes_count)
    : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
      write_delay_(write_delay),
      read_in_progress_(false),
      weak_factory_(this) {
  DCHECK_GE(write_delay_, 0);
}

DelayedSocketData::DelayedSocketData(
    const MockConnect& connect, int write_delay, MockRead* reads,
    size_t reads_count, MockWrite* writes, size_t writes_count)
    : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
      write_delay_(write_delay),
      read_in_progress_(false),
      weak_factory_(this) {
  DCHECK_GE(write_delay_, 0);
  set_connect_data(connect);
}

DelayedSocketData::~DelayedSocketData() {
}

void DelayedSocketData::ForceNextRead() {
  DCHECK(read_in_progress_);
  write_delay_ = 0;
  CompleteRead();
}

MockRead DelayedSocketData::GetNextRead() {
  MockRead out = MockRead(ASYNC, ERR_IO_PENDING);
  if (write_delay_ <= 0)
    out = StaticSocketDataProvider::GetNextRead();
  read_in_progress_ = (out.result == ERR_IO_PENDING);
  return out;
}

MockWriteResult DelayedSocketData::OnWrite(const std::string& data) {
  MockWriteResult rv = StaticSocketDataProvider::OnWrite(data);
  // Now that our write has completed, we can allow reads to continue.
  if (!--write_delay_ && read_in_progress_)
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DelayedSocketData::CompleteRead,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(100));
  return rv;
}

void DelayedSocketData::Reset() {
  set_socket(NULL);
  read_in_progress_ = false;
  weak_factory_.InvalidateWeakPtrs();
  StaticSocketDataProvider::Reset();
}

void DelayedSocketData::CompleteRead() {
  if (socket() && read_in_progress_)
    socket()->OnReadComplete(GetNextRead());
}

OrderedSocketData::OrderedSocketData(
    MockRead* reads, size_t reads_count, MockWrite* writes, size_t writes_count)
    : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
      sequence_number_(0), loop_stop_stage_(0),
      blocked_(false), weak_factory_(this) {
}

OrderedSocketData::OrderedSocketData(
    const MockConnect& connect,
    MockRead* reads, size_t reads_count,
    MockWrite* writes, size_t writes_count)
    : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
      sequence_number_(0), loop_stop_stage_(0),
      blocked_(false), weak_factory_(this) {
  set_connect_data(connect);
}

void OrderedSocketData::EndLoop() {
  // If we've already stopped the loop, don't do it again until we've advanced
  // to the next sequence_number.
  NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_ << ": EndLoop()";
  if (loop_stop_stage_ > 0) {
    const MockRead& next_read = StaticSocketDataProvider::PeekRead();
    if ((next_read.sequence_number & ~MockRead::STOPLOOP) >
        loop_stop_stage_) {
      NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                                << ": Clearing stop index";
      loop_stop_stage_ = 0;
    } else {
      return;
    }
  }
  // Record the sequence_number at which we stopped the loop.
  NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                            << ": Posting Quit at read " << read_index();
  loop_stop_stage_ = sequence_number_;
}

MockRead OrderedSocketData::GetNextRead() {
  weak_factory_.InvalidateWeakPtrs();
  blocked_ = false;
  const MockRead& next_read = StaticSocketDataProvider::PeekRead();
  if (next_read.sequence_number & MockRead::STOPLOOP)
    EndLoop();
  if ((next_read.sequence_number & ~MockRead::STOPLOOP) <=
      sequence_number_++) {
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_ - 1
                              << ": Read " << read_index();
    DumpMockReadWrite(next_read);
    blocked_ = (next_read.result == ERR_IO_PENDING);
    return StaticSocketDataProvider::GetNextRead();
  }
  NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_ - 1
                            << ": I/O Pending";
  MockRead result = MockRead(ASYNC, ERR_IO_PENDING);
  DumpMockReadWrite(result);
  blocked_ = true;
  return result;
}

MockWriteResult OrderedSocketData::OnWrite(const std::string& data) {
  NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                            << ": Write " << write_index();
  DumpMockReadWrite(PeekWrite());
  ++sequence_number_;
  if (blocked_) {
    // TODO(willchan): This 100ms delay seems to work around some weirdness.  We
    // should probably fix the weirdness.  One example is in SpdyStream,
    // DoSendRequest() will return ERR_IO_PENDING, and there's a race.  If the
    // SYN_REPLY causes OnResponseReceived() to get called before
    // SpdyStream::ReadResponseHeaders() is called, we hit a NOTREACHED().
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&OrderedSocketData::CompleteRead,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(100));
  }
  return StaticSocketDataProvider::OnWrite(data);
}

void OrderedSocketData::Reset() {
  NET_TRACE(INFO, "  *** ") << "Stage "
                            << sequence_number_ << ": Reset()";
  sequence_number_ = 0;
  loop_stop_stage_ = 0;
  set_socket(NULL);
  weak_factory_.InvalidateWeakPtrs();
  StaticSocketDataProvider::Reset();
}

void OrderedSocketData::CompleteRead() {
  if (socket() && blocked_) {
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_;
    socket()->OnReadComplete(GetNextRead());
  }
}

OrderedSocketData::~OrderedSocketData() {}

DeterministicSocketData::DeterministicSocketData(MockRead* reads,
    size_t reads_count, MockWrite* writes, size_t writes_count)
    : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
      sequence_number_(0),
      current_read_(),
      current_write_(),
      stopping_sequence_number_(0),
      stopped_(false),
      print_debug_(false),
      is_running_(false) {
  VerifyCorrectSequenceNumbers(reads, reads_count, writes, writes_count);
}

DeterministicSocketData::~DeterministicSocketData() {}

void DeterministicSocketData::Run() {
  DCHECK(!is_running_);
  is_running_ = true;

  SetStopped(false);
  int counter = 0;
  // Continue to consume data until all data has run out, or the stopped_ flag
  // has been set. Consuming data requires two separate operations -- running
  // the tasks in the message loop, and explicitly invoking the read/write
  // callbacks (simulating network I/O). We check our conditions between each,
  // since they can change in either.
  while ((!at_write_eof() || !at_read_eof()) && !stopped()) {
    if (counter % 2 == 0)
      base::RunLoop().RunUntilIdle();
    if (counter % 2 == 1) {
      InvokeCallbacks();
    }
    counter++;
  }
  // We're done consuming new data, but it is possible there are still some
  // pending callbacks which we expect to complete before returning.
  while (delegate_.get() &&
         (delegate_->WritePending() || delegate_->ReadPending()) &&
         !stopped()) {
    InvokeCallbacks();
    base::RunLoop().RunUntilIdle();
  }
  SetStopped(false);
  is_running_ = false;
}

void DeterministicSocketData::RunFor(int steps) {
  StopAfter(steps);
  Run();
}

void DeterministicSocketData::SetStop(int seq) {
  DCHECK_LT(sequence_number_, seq);
  stopping_sequence_number_ = seq;
  stopped_ = false;
}

void DeterministicSocketData::StopAfter(int seq) {
  SetStop(sequence_number_ + seq);
}

MockRead DeterministicSocketData::GetNextRead() {
  current_read_ = StaticSocketDataProvider::PeekRead();

  // Synchronous read while stopped is an error
  if (stopped() && current_read_.mode == SYNCHRONOUS) {
    LOG(ERROR) << "Unable to perform synchronous IO while stopped";
    return MockRead(SYNCHRONOUS, ERR_UNEXPECTED);
  }

  // Async read which will be called back in a future step.
  if (sequence_number_ < current_read_.sequence_number) {
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                              << ": I/O Pending";
    MockRead result = MockRead(SYNCHRONOUS, ERR_IO_PENDING);
    if (current_read_.mode == SYNCHRONOUS) {
      LOG(ERROR) << "Unable to perform synchronous read: "
          << current_read_.sequence_number
          << " at stage: " << sequence_number_;
      result = MockRead(SYNCHRONOUS, ERR_UNEXPECTED);
    }
    if (print_debug_)
      DumpMockReadWrite(result);
    return result;
  }

  NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                            << ": Read " << read_index();
  if (print_debug_)
    DumpMockReadWrite(current_read_);

  // Increment the sequence number if IO is complete
  if (current_read_.mode == SYNCHRONOUS)
    NextStep();

  DCHECK_NE(ERR_IO_PENDING, current_read_.result);
  StaticSocketDataProvider::GetNextRead();

  return current_read_;
}

MockWriteResult DeterministicSocketData::OnWrite(const std::string& data) {
  const MockWrite& next_write = StaticSocketDataProvider::PeekWrite();
  current_write_ = next_write;

  // Synchronous write while stopped is an error
  if (stopped() && next_write.mode == SYNCHRONOUS) {
    LOG(ERROR) << "Unable to perform synchronous IO while stopped";
    return MockWriteResult(SYNCHRONOUS, ERR_UNEXPECTED);
  }

  // Async write which will be called back in a future step.
  if (sequence_number_ < next_write.sequence_number) {
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                              << ": I/O Pending";
    if (next_write.mode == SYNCHRONOUS) {
      LOG(ERROR) << "Unable to perform synchronous write: "
          << next_write.sequence_number << " at stage: " << sequence_number_;
      return MockWriteResult(SYNCHRONOUS, ERR_UNEXPECTED);
    }
  } else {
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                              << ": Write " << write_index();
  }

  if (print_debug_)
    DumpMockReadWrite(next_write);

  // Move to the next step if I/O is synchronous, since the operation will
  // complete when this method returns.
  if (next_write.mode == SYNCHRONOUS)
    NextStep();

  // This is either a sync write for this step, or an async write.
  return StaticSocketDataProvider::OnWrite(data);
}

void DeterministicSocketData::Reset() {
  NET_TRACE(INFO, "  *** ") << "Stage "
                            << sequence_number_ << ": Reset()";
  sequence_number_ = 0;
  StaticSocketDataProvider::Reset();
  NOTREACHED();
}

void DeterministicSocketData::InvokeCallbacks() {
  if (delegate_.get() && delegate_->WritePending() &&
      (current_write().sequence_number == sequence_number())) {
    NextStep();
    delegate_->CompleteWrite();
    return;
  }
  if (delegate_.get() && delegate_->ReadPending() &&
      (current_read().sequence_number == sequence_number())) {
    NextStep();
    delegate_->CompleteRead();
    return;
  }
}

void DeterministicSocketData::NextStep() {
  // Invariant: Can never move *past* the stopping step.
  DCHECK_LT(sequence_number_, stopping_sequence_number_);
  sequence_number_++;
  if (sequence_number_ == stopping_sequence_number_)
    SetStopped(true);
}

void DeterministicSocketData::VerifyCorrectSequenceNumbers(
    MockRead* reads, size_t reads_count,
    MockWrite* writes, size_t writes_count) {
  size_t read = 0;
  size_t write = 0;
  int expected = 0;
  while (read < reads_count || write < writes_count) {
    // Check to see that we have a read or write at the expected
    // state.
    if (read < reads_count  && reads[read].sequence_number == expected) {
      ++read;
      ++expected;
      continue;
    }
    if (write < writes_count && writes[write].sequence_number == expected) {
      ++write;
      ++expected;
      continue;
    }
    NOTREACHED() << "Missing sequence number: " << expected;
    return;
  }
  DCHECK_EQ(read, reads_count);
  DCHECK_EQ(write, writes_count);
}

MockClientSocketFactory::MockClientSocketFactory() {}

MockClientSocketFactory::~MockClientSocketFactory() {}

void MockClientSocketFactory::AddSocketDataProvider(
    SocketDataProvider* data) {
  mock_data_.Add(data);
}

void MockClientSocketFactory::AddSSLSocketDataProvider(
    SSLSocketDataProvider* data) {
  mock_ssl_data_.Add(data);
}

void MockClientSocketFactory::ResetNextMockIndexes() {
  mock_data_.ResetNextIndex();
  mock_ssl_data_.ResetNextIndex();
}

scoped_ptr<DatagramClientSocket>
MockClientSocketFactory::CreateDatagramClientSocket(
    DatagramSocket::BindType bind_type,
    const RandIntCallback& rand_int_cb,
    net::NetLog* net_log,
    const net::NetLog::Source& source) {
  SocketDataProvider* data_provider = mock_data_.GetNext();
  scoped_ptr<MockUDPClientSocket> socket(
      new MockUDPClientSocket(data_provider, net_log));
  data_provider->set_socket(socket.get());
  if (bind_type == DatagramSocket::RANDOM_BIND)
    socket->set_source_port(rand_int_cb.Run(1025, 65535));
  return socket.PassAs<DatagramClientSocket>();
}

scoped_ptr<StreamSocket> MockClientSocketFactory::CreateTransportClientSocket(
    const AddressList& addresses,
    net::NetLog* net_log,
    const net::NetLog::Source& source) {
  SocketDataProvider* data_provider = mock_data_.GetNext();
  scoped_ptr<MockTCPClientSocket> socket(
      new MockTCPClientSocket(addresses, net_log, data_provider));
  data_provider->set_socket(socket.get());
  return socket.PassAs<StreamSocket>();
}

scoped_ptr<SSLClientSocket> MockClientSocketFactory::CreateSSLClientSocket(
    scoped_ptr<ClientSocketHandle> transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    const SSLClientSocketContext& context) {
  return scoped_ptr<SSLClientSocket>(
      new MockSSLClientSocket(transport_socket.Pass(),
                              host_and_port, ssl_config,
                              mock_ssl_data_.GetNext()));
}

void MockClientSocketFactory::ClearSSLSessionCache() {
}

const char MockClientSocket::kTlsUnique[] = "MOCK_TLSUNIQ";

MockClientSocket::MockClientSocket(const BoundNetLog& net_log)
    : connected_(false),
      net_log_(net_log),
      weak_factory_(this) {
  IPAddressNumber ip;
  CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
  peer_addr_ = IPEndPoint(ip, 0);
}

int MockClientSocket::SetReceiveBufferSize(int32 size) {
  return OK;
}

int MockClientSocket::SetSendBufferSize(int32 size) {
  return OK;
}

void MockClientSocket::Disconnect() {
  connected_ = false;
}

bool MockClientSocket::IsConnected() const {
  return connected_;
}

bool MockClientSocket::IsConnectedAndIdle() const {
  return connected_;
}

int MockClientSocket::GetPeerAddress(IPEndPoint* address) const {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  *address = peer_addr_;
  return OK;
}

int MockClientSocket::GetLocalAddress(IPEndPoint* address) const {
  IPAddressNumber ip;
  bool rv = ParseIPLiteralToNumber("192.0.2.33", &ip);
  CHECK(rv);
  *address = IPEndPoint(ip, 123);
  return OK;
}

const BoundNetLog& MockClientSocket::NetLog() const {
  return net_log_;
}

void MockClientSocket::GetSSLCertRequestInfo(
  SSLCertRequestInfo* cert_request_info) {
}

int MockClientSocket::ExportKeyingMaterial(const base::StringPiece& label,
                                           bool has_context,
                                           const base::StringPiece& context,
                                           unsigned char* out,
                                           unsigned int outlen) {
  memset(out, 'A', outlen);
  return OK;
}

int MockClientSocket::GetTLSUniqueChannelBinding(std::string* out) {
  out->assign(MockClientSocket::kTlsUnique);
  return OK;
}

ServerBoundCertService* MockClientSocket::GetServerBoundCertService() const {
  NOTREACHED();
  return NULL;
}

SSLClientSocket::NextProtoStatus
MockClientSocket::GetNextProto(std::string* proto, std::string* server_protos) {
  proto->clear();
  server_protos->clear();
  return SSLClientSocket::kNextProtoUnsupported;
}

scoped_refptr<X509Certificate>
MockClientSocket::GetUnverifiedServerCertificateChain() const {
  NOTREACHED();
  return NULL;
}

MockClientSocket::~MockClientSocket() {}

void MockClientSocket::RunCallbackAsync(const CompletionCallback& callback,
                                        int result) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MockClientSocket::RunCallback,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 result));
}

void MockClientSocket::RunCallback(const net::CompletionCallback& callback,
                                   int result) {
  if (!callback.is_null())
    callback.Run(result);
}

MockTCPClientSocket::MockTCPClientSocket(const AddressList& addresses,
                                         net::NetLog* net_log,
                                         SocketDataProvider* data)
    : MockClientSocket(BoundNetLog::Make(net_log, net::NetLog::SOURCE_NONE)),
      addresses_(addresses),
      data_(data),
      read_offset_(0),
      read_data_(SYNCHRONOUS, ERR_UNEXPECTED),
      need_read_data_(true),
      peer_closed_connection_(false),
      pending_buf_(NULL),
      pending_buf_len_(0),
      was_used_to_convey_data_(false) {
  DCHECK(data_);
  peer_addr_ = data->connect_data().peer_addr;
  data_->Reset();
}

MockTCPClientSocket::~MockTCPClientSocket() {}

int MockTCPClientSocket::Read(IOBuffer* buf, int buf_len,
                              const CompletionCallback& callback) {
  if (!connected_)
    return ERR_UNEXPECTED;

  // If the buffer is already in use, a read is already in progress!
  DCHECK(pending_buf_ == NULL);

  // Store our async IO data.
  pending_buf_ = buf;
  pending_buf_len_ = buf_len;
  pending_callback_ = callback;

  if (need_read_data_) {
    read_data_ = data_->GetNextRead();
    if (read_data_.result == ERR_CONNECTION_CLOSED) {
      // This MockRead is just a marker to instruct us to set
      // peer_closed_connection_.
      peer_closed_connection_ = true;
    }
    if (read_data_.result == ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ) {
      // This MockRead is just a marker to instruct us to set
      // peer_closed_connection_.  Skip it and get the next one.
      read_data_ = data_->GetNextRead();
      peer_closed_connection_ = true;
    }
    // ERR_IO_PENDING means that the SocketDataProvider is taking responsibility
    // to complete the async IO manually later (via OnReadComplete).
    if (read_data_.result == ERR_IO_PENDING) {
      // We need to be using async IO in this case.
      DCHECK(!callback.is_null());
      return ERR_IO_PENDING;
    }
    need_read_data_ = false;
  }

  return CompleteRead();
}

int MockTCPClientSocket::Write(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  if (!connected_)
    return ERR_UNEXPECTED;

  std::string data(buf->data(), buf_len);
  MockWriteResult write_result = data_->OnWrite(data);

  was_used_to_convey_data_ = true;

  if (write_result.mode == ASYNC) {
    RunCallbackAsync(callback, write_result.result);
    return ERR_IO_PENDING;
  }

  return write_result.result;
}

int MockTCPClientSocket::Connect(const CompletionCallback& callback) {
  if (connected_)
    return OK;
  connected_ = true;
  peer_closed_connection_ = false;
  if (data_->connect_data().mode == ASYNC) {
    if (data_->connect_data().result == ERR_IO_PENDING)
      pending_callback_ = callback;
    else
      RunCallbackAsync(callback, data_->connect_data().result);
    return ERR_IO_PENDING;
  }
  return data_->connect_data().result;
}

void MockTCPClientSocket::Disconnect() {
  MockClientSocket::Disconnect();
  pending_callback_.Reset();
}

bool MockTCPClientSocket::IsConnected() const {
  return connected_ && !peer_closed_connection_;
}

bool MockTCPClientSocket::IsConnectedAndIdle() const {
  return IsConnected();
}

int MockTCPClientSocket::GetPeerAddress(IPEndPoint* address) const {
  if (addresses_.empty())
    return MockClientSocket::GetPeerAddress(address);

  *address = addresses_[0];
  return OK;
}

bool MockTCPClientSocket::WasEverUsed() const {
  return was_used_to_convey_data_;
}

bool MockTCPClientSocket::UsingTCPFastOpen() const {
  return false;
}

bool MockTCPClientSocket::WasNpnNegotiated() const {
  return false;
}

bool MockTCPClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  return false;
}

void MockTCPClientSocket::OnReadComplete(const MockRead& data) {
  // There must be a read pending.
  DCHECK(pending_buf_);
  // You can't complete a read with another ERR_IO_PENDING status code.
  DCHECK_NE(ERR_IO_PENDING, data.result);
  // Since we've been waiting for data, need_read_data_ should be true.
  DCHECK(need_read_data_);

  read_data_ = data;
  need_read_data_ = false;

  // The caller is simulating that this IO completes right now.  Don't
  // let CompleteRead() schedule a callback.
  read_data_.mode = SYNCHRONOUS;

  CompletionCallback callback = pending_callback_;
  int rv = CompleteRead();
  RunCallback(callback, rv);
}

void MockTCPClientSocket::OnConnectComplete(const MockConnect& data) {
  CompletionCallback callback = pending_callback_;
  RunCallback(callback, data.result);
}

int MockTCPClientSocket::CompleteRead() {
  DCHECK(pending_buf_);
  DCHECK(pending_buf_len_ > 0);

  was_used_to_convey_data_ = true;

  // Save the pending async IO data and reset our |pending_| state.
  scoped_refptr<IOBuffer> buf = pending_buf_;
  int buf_len = pending_buf_len_;
  CompletionCallback callback = pending_callback_;
  pending_buf_ = NULL;
  pending_buf_len_ = 0;
  pending_callback_.Reset();

  int result = read_data_.result;
  DCHECK(result != ERR_IO_PENDING);

  if (read_data_.data) {
    if (read_data_.data_len - read_offset_ > 0) {
      result = std::min(buf_len, read_data_.data_len - read_offset_);
      memcpy(buf->data(), read_data_.data + read_offset_, result);
      read_offset_ += result;
      if (read_offset_ == read_data_.data_len) {
        need_read_data_ = true;
        read_offset_ = 0;
      }
    } else {
      result = 0;  // EOF
    }
  }

  if (read_data_.mode == ASYNC) {
    DCHECK(!callback.is_null());
    RunCallbackAsync(callback, result);
    return ERR_IO_PENDING;
  }
  return result;
}

DeterministicSocketHelper::DeterministicSocketHelper(
    net::NetLog* net_log,
    DeterministicSocketData* data)
    : write_pending_(false),
      write_result_(0),
      read_data_(),
      read_buf_(NULL),
      read_buf_len_(0),
      read_pending_(false),
      data_(data),
      was_used_to_convey_data_(false),
      peer_closed_connection_(false),
      net_log_(BoundNetLog::Make(net_log, net::NetLog::SOURCE_NONE)) {
}

DeterministicSocketHelper::~DeterministicSocketHelper() {}

void DeterministicSocketHelper::CompleteWrite() {
  was_used_to_convey_data_ = true;
  write_pending_ = false;
  write_callback_.Run(write_result_);
}

int DeterministicSocketHelper::CompleteRead() {
  DCHECK_GT(read_buf_len_, 0);
  DCHECK_LE(read_data_.data_len, read_buf_len_);
  DCHECK(read_buf_);

  was_used_to_convey_data_ = true;

  if (read_data_.result == ERR_IO_PENDING)
    read_data_ = data_->GetNextRead();
  DCHECK_NE(ERR_IO_PENDING, read_data_.result);
  // If read_data_.mode is ASYNC, we do not need to wait, since this is already
  // the callback. Therefore we don't even bother to check it.
  int result = read_data_.result;

  if (read_data_.data_len > 0) {
    DCHECK(read_data_.data);
    result = std::min(read_buf_len_, read_data_.data_len);
    memcpy(read_buf_->data(), read_data_.data, result);
  }

  if (read_pending_) {
    read_pending_ = false;
    read_callback_.Run(result);
  }

  return result;
}

int DeterministicSocketHelper::Write(
    IOBuffer* buf, int buf_len, const CompletionCallback& callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  std::string data(buf->data(), buf_len);
  MockWriteResult write_result = data_->OnWrite(data);

  if (write_result.mode == ASYNC) {
    write_callback_ = callback;
    write_result_ = write_result.result;
    DCHECK(!write_callback_.is_null());
    write_pending_ = true;
    return ERR_IO_PENDING;
  }

  was_used_to_convey_data_ = true;
  write_pending_ = false;
  return write_result.result;
}

int DeterministicSocketHelper::Read(
    IOBuffer* buf, int buf_len, const CompletionCallback& callback) {

  read_data_ = data_->GetNextRead();
  // The buffer should always be big enough to contain all the MockRead data. To
  // use small buffers, split the data into multiple MockReads.
  DCHECK_LE(read_data_.data_len, buf_len);

  if (read_data_.result == ERR_CONNECTION_CLOSED) {
    // This MockRead is just a marker to instruct us to set
    // peer_closed_connection_.
    peer_closed_connection_ = true;
  }
  if (read_data_.result == ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ) {
    // This MockRead is just a marker to instruct us to set
    // peer_closed_connection_.  Skip it and get the next one.
    read_data_ = data_->GetNextRead();
    peer_closed_connection_ = true;
  }

  read_buf_ = buf;
  read_buf_len_ = buf_len;
  read_callback_ = callback;

  if (read_data_.mode == ASYNC || (read_data_.result == ERR_IO_PENDING)) {
    read_pending_ = true;
    DCHECK(!read_callback_.is_null());
    return ERR_IO_PENDING;
  }

  was_used_to_convey_data_ = true;
  return CompleteRead();
}

DeterministicMockUDPClientSocket::DeterministicMockUDPClientSocket(
    net::NetLog* net_log,
    DeterministicSocketData* data)
    : connected_(false),
      helper_(net_log, data),
      source_port_(123) {
}

DeterministicMockUDPClientSocket::~DeterministicMockUDPClientSocket() {}

bool DeterministicMockUDPClientSocket::WritePending() const {
  return helper_.write_pending();
}

bool DeterministicMockUDPClientSocket::ReadPending() const {
  return helper_.read_pending();
}

void DeterministicMockUDPClientSocket::CompleteWrite() {
  helper_.CompleteWrite();
}

int DeterministicMockUDPClientSocket::CompleteRead() {
  return helper_.CompleteRead();
}

int DeterministicMockUDPClientSocket::Connect(const IPEndPoint& address) {
  if (connected_)
    return OK;
  connected_ = true;
  peer_address_ = address;
  return helper_.data()->connect_data().result;
};

int DeterministicMockUDPClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  if (!connected_)
    return ERR_UNEXPECTED;

  return helper_.Write(buf, buf_len, callback);
}

int DeterministicMockUDPClientSocket::Read(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  if (!connected_)
    return ERR_UNEXPECTED;

  return helper_.Read(buf, buf_len, callback);
}

int DeterministicMockUDPClientSocket::SetReceiveBufferSize(int32 size) {
  return OK;
}

int DeterministicMockUDPClientSocket::SetSendBufferSize(int32 size) {
  return OK;
}

void DeterministicMockUDPClientSocket::Close() {
  connected_ = false;
}

int DeterministicMockUDPClientSocket::GetPeerAddress(
    IPEndPoint* address) const {
  *address = peer_address_;
  return OK;
}

int DeterministicMockUDPClientSocket::GetLocalAddress(
    IPEndPoint* address) const {
  IPAddressNumber ip;
  bool rv = ParseIPLiteralToNumber("192.0.2.33", &ip);
  CHECK(rv);
  *address = IPEndPoint(ip, source_port_);
  return OK;
}

const BoundNetLog& DeterministicMockUDPClientSocket::NetLog() const {
  return helper_.net_log();
}

void DeterministicMockUDPClientSocket::OnReadComplete(const MockRead& data) {}

void DeterministicMockUDPClientSocket::OnConnectComplete(
    const MockConnect& data) {
  NOTIMPLEMENTED();
}

DeterministicMockTCPClientSocket::DeterministicMockTCPClientSocket(
    net::NetLog* net_log,
    DeterministicSocketData* data)
    : MockClientSocket(BoundNetLog::Make(net_log, net::NetLog::SOURCE_NONE)),
      helper_(net_log, data) {
  peer_addr_ = data->connect_data().peer_addr;
}

DeterministicMockTCPClientSocket::~DeterministicMockTCPClientSocket() {}

bool DeterministicMockTCPClientSocket::WritePending() const {
  return helper_.write_pending();
}

bool DeterministicMockTCPClientSocket::ReadPending() const {
  return helper_.read_pending();
}

void DeterministicMockTCPClientSocket::CompleteWrite() {
  helper_.CompleteWrite();
}

int DeterministicMockTCPClientSocket::CompleteRead() {
  return helper_.CompleteRead();
}

int DeterministicMockTCPClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  if (!connected_)
    return ERR_UNEXPECTED;

  return helper_.Write(buf, buf_len, callback);
}

int DeterministicMockTCPClientSocket::Read(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  if (!connected_)
    return ERR_UNEXPECTED;

  return helper_.Read(buf, buf_len, callback);
}

// TODO(erikchen): Support connect sequencing.
int DeterministicMockTCPClientSocket::Connect(
    const CompletionCallback& callback) {
  if (connected_)
    return OK;
  connected_ = true;
  if (helper_.data()->connect_data().mode == ASYNC) {
    RunCallbackAsync(callback, helper_.data()->connect_data().result);
    return ERR_IO_PENDING;
  }
  return helper_.data()->connect_data().result;
}

void DeterministicMockTCPClientSocket::Disconnect() {
  MockClientSocket::Disconnect();
}

bool DeterministicMockTCPClientSocket::IsConnected() const {
  return connected_ && !helper_.peer_closed_connection();
}

bool DeterministicMockTCPClientSocket::IsConnectedAndIdle() const {
  return IsConnected();
}

bool DeterministicMockTCPClientSocket::WasEverUsed() const {
  return helper_.was_used_to_convey_data();
}

bool DeterministicMockTCPClientSocket::UsingTCPFastOpen() const {
  return false;
}

bool DeterministicMockTCPClientSocket::WasNpnNegotiated() const {
  return false;
}

bool DeterministicMockTCPClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  return false;
}

void DeterministicMockTCPClientSocket::OnReadComplete(const MockRead& data) {}

void DeterministicMockTCPClientSocket::OnConnectComplete(
    const MockConnect& data) {}

// static
void MockSSLClientSocket::ConnectCallback(
    MockSSLClientSocket* ssl_client_socket,
    const CompletionCallback& callback,
    int rv) {
  if (rv == OK)
    ssl_client_socket->connected_ = true;
  callback.Run(rv);
}

MockSSLClientSocket::MockSSLClientSocket(
    scoped_ptr<ClientSocketHandle> transport_socket,
    const HostPortPair& host_port_pair,
    const SSLConfig& ssl_config,
    SSLSocketDataProvider* data)
    : MockClientSocket(
         // Have to use the right BoundNetLog for LoadTimingInfo regression
         // tests.
         transport_socket->socket()->NetLog()),
      transport_(transport_socket.Pass()),
      data_(data),
      is_npn_state_set_(false),
      new_npn_value_(false),
      is_protocol_negotiated_set_(false),
      protocol_negotiated_(kProtoUnknown) {
  DCHECK(data_);
  peer_addr_ = data->connect.peer_addr;
}

MockSSLClientSocket::~MockSSLClientSocket() {
  Disconnect();
}

int MockSSLClientSocket::Read(IOBuffer* buf, int buf_len,
                              const CompletionCallback& callback) {
  return transport_->socket()->Read(buf, buf_len, callback);
}

int MockSSLClientSocket::Write(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) {
  return transport_->socket()->Write(buf, buf_len, callback);
}

int MockSSLClientSocket::Connect(const CompletionCallback& callback) {
  int rv = transport_->socket()->Connect(
      base::Bind(&ConnectCallback, base::Unretained(this), callback));
  if (rv == OK) {
    if (data_->connect.result == OK)
      connected_ = true;
    if (data_->connect.mode == ASYNC) {
      RunCallbackAsync(callback, data_->connect.result);
      return ERR_IO_PENDING;
    }
    return data_->connect.result;
  }
  return rv;
}

void MockSSLClientSocket::Disconnect() {
  MockClientSocket::Disconnect();
  if (transport_->socket() != NULL)
    transport_->socket()->Disconnect();
}

bool MockSSLClientSocket::IsConnected() const {
  return transport_->socket()->IsConnected();
}

bool MockSSLClientSocket::WasEverUsed() const {
  return transport_->socket()->WasEverUsed();
}

bool MockSSLClientSocket::UsingTCPFastOpen() const {
  return transport_->socket()->UsingTCPFastOpen();
}

int MockSSLClientSocket::GetPeerAddress(IPEndPoint* address) const {
  return transport_->socket()->GetPeerAddress(address);
}

bool MockSSLClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  ssl_info->Reset();
  ssl_info->cert = data_->cert;
  ssl_info->client_cert_sent = data_->client_cert_sent;
  ssl_info->channel_id_sent = data_->channel_id_sent;
  ssl_info->connection_status = data_->connection_status;
  return true;
}

void MockSSLClientSocket::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  DCHECK(cert_request_info);
  if (data_->cert_request_info) {
    cert_request_info->host_and_port =
        data_->cert_request_info->host_and_port;
    cert_request_info->client_certs = data_->cert_request_info->client_certs;
  } else {
    cert_request_info->Reset();
  }
}

SSLClientSocket::NextProtoStatus MockSSLClientSocket::GetNextProto(
    std::string* proto, std::string* server_protos) {
  *proto = data_->next_proto;
  *server_protos = data_->server_protos;
  return data_->next_proto_status;
}

bool MockSSLClientSocket::set_was_npn_negotiated(bool negotiated) {
  is_npn_state_set_ = true;
  return new_npn_value_ = negotiated;
}

bool MockSSLClientSocket::WasNpnNegotiated() const {
  if (is_npn_state_set_)
    return new_npn_value_;
  return data_->was_npn_negotiated;
}

NextProto MockSSLClientSocket::GetNegotiatedProtocol() const {
  if (is_protocol_negotiated_set_)
    return protocol_negotiated_;
  return data_->protocol_negotiated;
}

void MockSSLClientSocket::set_protocol_negotiated(
    NextProto protocol_negotiated) {
  is_protocol_negotiated_set_ = true;
  protocol_negotiated_ = protocol_negotiated;
}

bool MockSSLClientSocket::WasChannelIDSent() const {
  return data_->channel_id_sent;
}

void MockSSLClientSocket::set_channel_id_sent(bool channel_id_sent) {
  data_->channel_id_sent = channel_id_sent;
}

ServerBoundCertService* MockSSLClientSocket::GetServerBoundCertService() const {
  return data_->server_bound_cert_service;
}

void MockSSLClientSocket::OnReadComplete(const MockRead& data) {
  NOTIMPLEMENTED();
}

void MockSSLClientSocket::OnConnectComplete(const MockConnect& data) {
  NOTIMPLEMENTED();
}

MockUDPClientSocket::MockUDPClientSocket(SocketDataProvider* data,
                                         net::NetLog* net_log)
    : connected_(false),
      data_(data),
      read_offset_(0),
      read_data_(SYNCHRONOUS, ERR_UNEXPECTED),
      need_read_data_(true),
      source_port_(123),
      pending_buf_(NULL),
      pending_buf_len_(0),
      net_log_(BoundNetLog::Make(net_log, net::NetLog::SOURCE_NONE)),
      weak_factory_(this) {
  DCHECK(data_);
  data_->Reset();
  peer_addr_ = data->connect_data().peer_addr;
}

MockUDPClientSocket::~MockUDPClientSocket() {}

int MockUDPClientSocket::Read(IOBuffer* buf,
                              int buf_len,
                              const CompletionCallback& callback) {
  if (!connected_)
    return ERR_UNEXPECTED;

  // If the buffer is already in use, a read is already in progress!
  DCHECK(pending_buf_ == NULL);

  // Store our async IO data.
  pending_buf_ = buf;
  pending_buf_len_ = buf_len;
  pending_callback_ = callback;

  if (need_read_data_) {
    read_data_ = data_->GetNextRead();
    // ERR_IO_PENDING means that the SocketDataProvider is taking responsibility
    // to complete the async IO manually later (via OnReadComplete).
    if (read_data_.result == ERR_IO_PENDING) {
      // We need to be using async IO in this case.
      DCHECK(!callback.is_null());
      return ERR_IO_PENDING;
    }
    need_read_data_ = false;
  }

  return CompleteRead();
}

int MockUDPClientSocket::Write(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  if (!connected_)
    return ERR_UNEXPECTED;

  std::string data(buf->data(), buf_len);
  MockWriteResult write_result = data_->OnWrite(data);

  if (write_result.mode == ASYNC) {
    RunCallbackAsync(callback, write_result.result);
    return ERR_IO_PENDING;
  }
  return write_result.result;
}

int MockUDPClientSocket::SetReceiveBufferSize(int32 size) {
  return OK;
}

int MockUDPClientSocket::SetSendBufferSize(int32 size) {
  return OK;
}

void MockUDPClientSocket::Close() {
  connected_ = false;
}

int MockUDPClientSocket::GetPeerAddress(IPEndPoint* address) const {
  *address = peer_addr_;
  return OK;
}

int MockUDPClientSocket::GetLocalAddress(IPEndPoint* address) const {
  IPAddressNumber ip;
  bool rv = ParseIPLiteralToNumber("192.0.2.33", &ip);
  CHECK(rv);
  *address = IPEndPoint(ip, source_port_);
  return OK;
}

const BoundNetLog& MockUDPClientSocket::NetLog() const {
  return net_log_;
}

int MockUDPClientSocket::Connect(const IPEndPoint& address) {
  connected_ = true;
  peer_addr_ = address;
  return data_->connect_data().result;
}

void MockUDPClientSocket::OnReadComplete(const MockRead& data) {
  // There must be a read pending.
  DCHECK(pending_buf_);
  // You can't complete a read with another ERR_IO_PENDING status code.
  DCHECK_NE(ERR_IO_PENDING, data.result);
  // Since we've been waiting for data, need_read_data_ should be true.
  DCHECK(need_read_data_);

  read_data_ = data;
  need_read_data_ = false;

  // The caller is simulating that this IO completes right now.  Don't
  // let CompleteRead() schedule a callback.
  read_data_.mode = SYNCHRONOUS;

  net::CompletionCallback callback = pending_callback_;
  int rv = CompleteRead();
  RunCallback(callback, rv);
}

void MockUDPClientSocket::OnConnectComplete(const MockConnect& data) {
  NOTIMPLEMENTED();
}

int MockUDPClientSocket::CompleteRead() {
  DCHECK(pending_buf_);
  DCHECK(pending_buf_len_ > 0);

  // Save the pending async IO data and reset our |pending_| state.
  scoped_refptr<IOBuffer> buf = pending_buf_;
  int buf_len = pending_buf_len_;
  CompletionCallback callback = pending_callback_;
  pending_buf_ = NULL;
  pending_buf_len_ = 0;
  pending_callback_.Reset();

  int result = read_data_.result;
  DCHECK(result != ERR_IO_PENDING);

  if (read_data_.data) {
    if (read_data_.data_len - read_offset_ > 0) {
      result = std::min(buf_len, read_data_.data_len - read_offset_);
      memcpy(buf->data(), read_data_.data + read_offset_, result);
      read_offset_ += result;
      if (read_offset_ == read_data_.data_len) {
        need_read_data_ = true;
        read_offset_ = 0;
      }
    } else {
      result = 0;  // EOF
    }
  }

  if (read_data_.mode == ASYNC) {
    DCHECK(!callback.is_null());
    RunCallbackAsync(callback, result);
    return ERR_IO_PENDING;
  }
  return result;
}

void MockUDPClientSocket::RunCallbackAsync(const CompletionCallback& callback,
                                           int result) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MockUDPClientSocket::RunCallback,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 result));
}

void MockUDPClientSocket::RunCallback(const CompletionCallback& callback,
                                      int result) {
  if (!callback.is_null())
    callback.Run(result);
}

TestSocketRequest::TestSocketRequest(
    std::vector<TestSocketRequest*>* request_order, size_t* completion_count)
    : request_order_(request_order),
      completion_count_(completion_count),
      callback_(base::Bind(&TestSocketRequest::OnComplete,
                           base::Unretained(this))) {
  DCHECK(request_order);
  DCHECK(completion_count);
}

TestSocketRequest::~TestSocketRequest() {
}

void TestSocketRequest::OnComplete(int result) {
  SetResult(result);
  (*completion_count_)++;
  request_order_->push_back(this);
}

// static
const int ClientSocketPoolTest::kIndexOutOfBounds = -1;

// static
const int ClientSocketPoolTest::kRequestNotFound = -2;

ClientSocketPoolTest::ClientSocketPoolTest() : completion_count_(0) {}
ClientSocketPoolTest::~ClientSocketPoolTest() {}

int ClientSocketPoolTest::GetOrderOfRequest(size_t index) const {
  index--;
  if (index >= requests_.size())
    return kIndexOutOfBounds;

  for (size_t i = 0; i < request_order_.size(); i++)
    if (requests_[index] == request_order_[i])
      return i + 1;

  return kRequestNotFound;
}

bool ClientSocketPoolTest::ReleaseOneConnection(KeepAlive keep_alive) {
  ScopedVector<TestSocketRequest>::iterator i;
  for (i = requests_.begin(); i != requests_.end(); ++i) {
    if ((*i)->handle()->is_initialized()) {
      if (keep_alive == NO_KEEP_ALIVE)
        (*i)->handle()->socket()->Disconnect();
      (*i)->handle()->Reset();
      base::RunLoop().RunUntilIdle();
      return true;
    }
  }
  return false;
}

void ClientSocketPoolTest::ReleaseAllConnections(KeepAlive keep_alive) {
  bool released_one;
  do {
    released_one = ReleaseOneConnection(keep_alive);
  } while (released_one);
}

MockTransportClientSocketPool::MockConnectJob::MockConnectJob(
    scoped_ptr<StreamSocket> socket,
    ClientSocketHandle* handle,
    const CompletionCallback& callback)
    : socket_(socket.Pass()),
      handle_(handle),
      user_callback_(callback) {
}

MockTransportClientSocketPool::MockConnectJob::~MockConnectJob() {}

int MockTransportClientSocketPool::MockConnectJob::Connect() {
  int rv = socket_->Connect(base::Bind(&MockConnectJob::OnConnect,
                                       base::Unretained(this)));
  if (rv == OK) {
    user_callback_.Reset();
    OnConnect(OK);
  }
  return rv;
}

bool MockTransportClientSocketPool::MockConnectJob::CancelHandle(
    const ClientSocketHandle* handle) {
  if (handle != handle_)
    return false;
  socket_.reset();
  handle_ = NULL;
  user_callback_.Reset();
  return true;
}

void MockTransportClientSocketPool::MockConnectJob::OnConnect(int rv) {
  if (!socket_.get())
    return;
  if (rv == OK) {
    handle_->SetSocket(socket_.Pass());

    // Needed for socket pool tests that layer other sockets on top of mock
    // sockets.
    LoadTimingInfo::ConnectTiming connect_timing;
    base::TimeTicks now = base::TimeTicks::Now();
    connect_timing.dns_start = now;
    connect_timing.dns_end = now;
    connect_timing.connect_start = now;
    connect_timing.connect_end = now;
    handle_->set_connect_timing(connect_timing);
  } else {
    socket_.reset();
  }

  handle_ = NULL;

  if (!user_callback_.is_null()) {
    CompletionCallback callback = user_callback_;
    user_callback_.Reset();
    callback.Run(rv);
  }
}

MockTransportClientSocketPool::MockTransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    ClientSocketPoolHistograms* histograms,
    ClientSocketFactory* socket_factory)
    : TransportClientSocketPool(max_sockets, max_sockets_per_group, histograms,
                                NULL, NULL, NULL),
      client_socket_factory_(socket_factory),
      last_request_priority_(DEFAULT_PRIORITY),
      release_count_(0),
      cancel_count_(0) {
}

MockTransportClientSocketPool::~MockTransportClientSocketPool() {}

int MockTransportClientSocketPool::RequestSocket(
    const std::string& group_name, const void* socket_params,
    RequestPriority priority, ClientSocketHandle* handle,
    const CompletionCallback& callback, const BoundNetLog& net_log) {
  last_request_priority_ = priority;
  scoped_ptr<StreamSocket> socket =
      client_socket_factory_->CreateTransportClientSocket(
          AddressList(), net_log.net_log(), net::NetLog::Source());
  MockConnectJob* job = new MockConnectJob(socket.Pass(), handle, callback);
  job_list_.push_back(job);
  handle->set_pool_id(1);
  return job->Connect();
}

void MockTransportClientSocketPool::CancelRequest(const std::string& group_name,
                                                  ClientSocketHandle* handle) {
  std::vector<MockConnectJob*>::iterator i;
  for (i = job_list_.begin(); i != job_list_.end(); ++i) {
    if ((*i)->CancelHandle(handle)) {
      cancel_count_++;
      break;
    }
  }
}

void MockTransportClientSocketPool::ReleaseSocket(
    const std::string& group_name,
    scoped_ptr<StreamSocket> socket,
    int id) {
  EXPECT_EQ(1, id);
  release_count_++;
}

DeterministicMockClientSocketFactory::DeterministicMockClientSocketFactory() {}

DeterministicMockClientSocketFactory::~DeterministicMockClientSocketFactory() {}

void DeterministicMockClientSocketFactory::AddSocketDataProvider(
    DeterministicSocketData* data) {
  mock_data_.Add(data);
}

void DeterministicMockClientSocketFactory::AddSSLSocketDataProvider(
    SSLSocketDataProvider* data) {
  mock_ssl_data_.Add(data);
}

void DeterministicMockClientSocketFactory::ResetNextMockIndexes() {
  mock_data_.ResetNextIndex();
  mock_ssl_data_.ResetNextIndex();
}

MockSSLClientSocket* DeterministicMockClientSocketFactory::
    GetMockSSLClientSocket(size_t index) const {
  DCHECK_LT(index, ssl_client_sockets_.size());
  return ssl_client_sockets_[index];
}

scoped_ptr<DatagramClientSocket>
DeterministicMockClientSocketFactory::CreateDatagramClientSocket(
    DatagramSocket::BindType bind_type,
    const RandIntCallback& rand_int_cb,
    net::NetLog* net_log,
    const NetLog::Source& source) {
  DeterministicSocketData* data_provider = mock_data().GetNext();
  scoped_ptr<DeterministicMockUDPClientSocket> socket(
      new DeterministicMockUDPClientSocket(net_log, data_provider));
  data_provider->set_delegate(socket->AsWeakPtr());
  udp_client_sockets().push_back(socket.get());
  if (bind_type == DatagramSocket::RANDOM_BIND)
    socket->set_source_port(rand_int_cb.Run(1025, 65535));
  return socket.PassAs<DatagramClientSocket>();
}

scoped_ptr<StreamSocket>
DeterministicMockClientSocketFactory::CreateTransportClientSocket(
    const AddressList& addresses,
    net::NetLog* net_log,
    const net::NetLog::Source& source) {
  DeterministicSocketData* data_provider = mock_data().GetNext();
  scoped_ptr<DeterministicMockTCPClientSocket> socket(
      new DeterministicMockTCPClientSocket(net_log, data_provider));
  data_provider->set_delegate(socket->AsWeakPtr());
  tcp_client_sockets().push_back(socket.get());
  return socket.PassAs<StreamSocket>();
}

scoped_ptr<SSLClientSocket>
DeterministicMockClientSocketFactory::CreateSSLClientSocket(
    scoped_ptr<ClientSocketHandle> transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    const SSLClientSocketContext& context) {
  scoped_ptr<MockSSLClientSocket> socket(
      new MockSSLClientSocket(transport_socket.Pass(),
                              host_and_port, ssl_config,
                              mock_ssl_data_.GetNext()));
  ssl_client_sockets_.push_back(socket.get());
  return socket.PassAs<SSLClientSocket>();
}

void DeterministicMockClientSocketFactory::ClearSSLSessionCache() {
}

MockSOCKSClientSocketPool::MockSOCKSClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    ClientSocketPoolHistograms* histograms,
    TransportClientSocketPool* transport_pool)
    : SOCKSClientSocketPool(max_sockets, max_sockets_per_group, histograms,
                            NULL, transport_pool, NULL),
      transport_pool_(transport_pool) {
}

MockSOCKSClientSocketPool::~MockSOCKSClientSocketPool() {}

int MockSOCKSClientSocketPool::RequestSocket(
    const std::string& group_name, const void* socket_params,
    RequestPriority priority, ClientSocketHandle* handle,
    const CompletionCallback& callback, const BoundNetLog& net_log) {
  return transport_pool_->RequestSocket(
      group_name, socket_params, priority, handle, callback, net_log);
}

void MockSOCKSClientSocketPool::CancelRequest(
    const std::string& group_name,
    ClientSocketHandle* handle) {
  return transport_pool_->CancelRequest(group_name, handle);
}

void MockSOCKSClientSocketPool::ReleaseSocket(const std::string& group_name,
                                              scoped_ptr<StreamSocket> socket,
                                              int id) {
  return transport_pool_->ReleaseSocket(group_name, socket.Pass(), id);
}

const char kSOCKS5GreetRequest[] = { 0x05, 0x01, 0x00 };
const int kSOCKS5GreetRequestLength = arraysize(kSOCKS5GreetRequest);

const char kSOCKS5GreetResponse[] = { 0x05, 0x00 };
const int kSOCKS5GreetResponseLength = arraysize(kSOCKS5GreetResponse);

const char kSOCKS5OkRequest[] =
    { 0x05, 0x01, 0x00, 0x03, 0x04, 'h', 'o', 's', 't', 0x00, 0x50 };
const int kSOCKS5OkRequestLength = arraysize(kSOCKS5OkRequest);

const char kSOCKS5OkResponse[] =
    { 0x05, 0x00, 0x00, 0x01, 127, 0, 0, 1, 0x00, 0x50 };
const int kSOCKS5OkResponseLength = arraysize(kSOCKS5OkResponse);

}  // namespace net
