// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_network_transaction.h"

#include "build/build_config.h"

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ftp/ftp_network_session.h"
#include "net/ftp/ftp_request_info.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Size we use for IOBuffers used to receive data from the test data socket.
const int kBufferSize = 128;

}  // namespace

namespace net {

class FtpSocketDataProvider : public DynamicSocketDataProvider {
 public:
  enum State {
    NONE,
    PRE_USER,
    PRE_PASSWD,
    PRE_SYST,
    PRE_PWD,
    PRE_TYPE,
    PRE_SIZE,
    PRE_EPSV,
    PRE_PASV,
    PRE_LIST,
    PRE_RETR,
    PRE_RETR_EPSV,
    PRE_RETR_PASV,
    PRE_CWD_EPSV,
    PRE_CWD_PASV,
    PRE_CWD,
    PRE_QUIT,
    PRE_NOPASV,
    QUIT
  };

  FtpSocketDataProvider()
      : failure_injection_state_(NONE),
        multiline_welcome_(false),
        use_epsv_(true),
        data_type_('I') {
    Init();
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_USER:
        return Verify("USER anonymous\r\n", data, PRE_PASSWD,
                      "331 Password needed\r\n");
      case PRE_PASSWD:
        {
          const char* response_one = "230 Welcome\r\n";
          const char* response_multi = "230- One\r\n230- Two\r\n230 Three\r\n";
          return Verify("PASS chrome@example.com\r\n", data, PRE_SYST,
                        multiline_welcome_ ? response_multi : response_one);
        }
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 UNIX\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"/\" is your current location\r\n");
      case PRE_TYPE:
        return Verify(std::string("TYPE ") + data_type_ + "\r\n", data,
                      use_epsv_ ? PRE_EPSV : PRE_PASV,
                      "200 TYPE set successfully\r\n");
      case PRE_EPSV:
        return Verify("EPSV\r\n", data, PRE_SIZE,
                      "227 Entering Extended Passive Mode (|||31744|)\r\n");
      case PRE_CWD_EPSV:
        return Verify("EPSV\r\n", data, PRE_CWD,
                      "227 Entering Extended Passive Mode (|||31744|)\r\n");
      case PRE_RETR_EPSV:
        return Verify("EPSV\r\n", data, PRE_RETR,
                      "227 Entering Extended Passive Mode (|||31744|)\r\n");
      case PRE_CWD_PASV:
        return Verify("PASV\r\n", data, PRE_CWD,
                      "227 Entering Passive Mode 127,0,0,1,123,456\r\n");
      case PRE_RETR_PASV:
        return Verify("PASV\r\n", data, PRE_RETR,
                      "227 Entering Passive Mode 127,0,0,1,123,456\r\n");
      case PRE_PASV:
        return Verify("PASV\r\n", data, PRE_SIZE,
                      "227 Entering Passive Mode 127,0,0,1,123,456\r\n");
      case PRE_NOPASV:
        // Use unallocated 599 FTP error code to make sure it falls into the
        // generic ERR_FTP_FAILED bucket.
        return Verify("PASV\r\n", data, PRE_QUIT,
                      "599 fail\r\n");
      case PRE_QUIT:
        return Verify("QUIT\r\n", data, QUIT, "221 Goodbye.\r\n");
      default:
        NOTREACHED() << "State not handled " << state();
        return MockWriteResult(ASYNC, ERR_UNEXPECTED);
    }
  }

  void InjectFailure(State state, State next_state, const char* response) {
    DCHECK_EQ(NONE, failure_injection_state_);
    DCHECK_NE(NONE, state);
    DCHECK_NE(NONE, next_state);
    DCHECK_NE(state, next_state);
    failure_injection_state_ = state;
    failure_injection_next_state_ = next_state;
    fault_response_ = response;
  }

  State state() const {
    return state_;
  }

  virtual void Reset() OVERRIDE {
    DynamicSocketDataProvider::Reset();
    Init();
  }

  void set_multiline_welcome(bool multiline) { multiline_welcome_ = multiline; }

  bool use_epsv() const { return use_epsv_; }
  void set_use_epsv(bool use_epsv) { use_epsv_ = use_epsv; }

  void set_data_type(char data_type) { data_type_ = data_type; }

 protected:
  void Init() {
    state_ = PRE_USER;
    SimulateRead("220 host TestFTPd\r\n");
  }

  // If protocol fault injection has been requested, adjusts state and mocked
  // read and returns true.
  bool InjectFault() {
    if (state_ != failure_injection_state_)
      return false;
    SimulateRead(fault_response_);
    state_ = failure_injection_next_state_;
    return true;
  }

  MockWriteResult Verify(const std::string& expected,
                         const std::string& data,
                         State next_state,
                         const char* next_read,
                         const size_t next_read_length) {
    EXPECT_EQ(expected, data);
    if (expected == data) {
      state_ = next_state;
      SimulateRead(next_read, next_read_length);
      return MockWriteResult(ASYNC, data.length());
    }
    return MockWriteResult(ASYNC, ERR_UNEXPECTED);
  }

  MockWriteResult Verify(const std::string& expected,
                         const std::string& data,
                         State next_state,
                         const char* next_read) {
    return Verify(expected, data, next_state,
                  next_read, std::strlen(next_read));
  }


 private:
  State state_;
  State failure_injection_state_;
  State failure_injection_next_state_;
  const char* fault_response_;

  // If true, we will send multiple 230 lines as response after PASS.
  bool multiline_welcome_;

  // If true, we will use EPSV command.
  bool use_epsv_;

  // Data type to be used for TYPE command.
  char data_type_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProvider);
};

class FtpSocketDataProviderDirectoryListing : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderDirectoryListing() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /\r\n", data,
                      use_epsv() ? PRE_CWD_EPSV : PRE_CWD_PASV,
                      "550 I can only retrieve regular files\r\n");
      case PRE_CWD:
        return Verify("CWD /\r\n", data, PRE_LIST, "200 OK\r\n");
      case PRE_LIST:
        return Verify("LIST -l\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderDirectoryListing);
};

class FtpSocketDataProviderDirectoryListingWithPasvFallback
    : public FtpSocketDataProviderDirectoryListing {
 public:
  FtpSocketDataProviderDirectoryListingWithPasvFallback() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_EPSV:
        return Verify("EPSV\r\n", data, PRE_PASV,
                      "500 no EPSV for you\r\n");
      case PRE_SIZE:
        return Verify("SIZE /\r\n", data, PRE_CWD_PASV,
                      "550 I can only retrieve regular files\r\n");
      default:
        return FtpSocketDataProviderDirectoryListing::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      FtpSocketDataProviderDirectoryListingWithPasvFallback);
};

class FtpSocketDataProviderDirectoryListingZeroSize
    : public FtpSocketDataProviderDirectoryListing {
 public:
  FtpSocketDataProviderDirectoryListingZeroSize() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /\r\n", data, PRE_CWD, "213 0\r\n");
      default:
        return FtpSocketDataProviderDirectoryListing::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderDirectoryListingZeroSize);
};

class FtpSocketDataProviderVMSDirectoryListing : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderVMSDirectoryListing() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 VMS\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"ANONYMOUS_ROOT:[000000]\"\r\n");
      case PRE_EPSV:
        return Verify("EPSV\r\n", data, PRE_PASV, "500 Invalid command\r\n");
      case PRE_SIZE:
        return Verify("SIZE ANONYMOUS_ROOT:[000000]dir\r\n", data, PRE_CWD_PASV,
                      "550 I can only retrieve regular files\r\n");
      case PRE_CWD:
        return Verify("CWD ANONYMOUS_ROOT:[dir]\r\n", data, PRE_LIST,
                      "200 OK\r\n");
      case PRE_LIST:
        return Verify("LIST *.*;0\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderVMSDirectoryListing);
};

class FtpSocketDataProviderVMSDirectoryListingRootDirectory
    : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderVMSDirectoryListingRootDirectory() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 VMS\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"ANONYMOUS_ROOT:[000000]\"\r\n");
      case PRE_EPSV:
        return Verify("EPSV\r\n", data, PRE_PASV,
                      "500 EPSV command unknown\r\n");
      case PRE_SIZE:
        return Verify("SIZE ANONYMOUS_ROOT\r\n", data, PRE_CWD_PASV,
                      "550 I can only retrieve regular files\r\n");
      case PRE_CWD:
        return Verify("CWD ANONYMOUS_ROOT:[000000]\r\n", data, PRE_LIST,
                      "200 OK\r\n");
      case PRE_LIST:
        return Verify("LIST *.*;0\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      FtpSocketDataProviderVMSDirectoryListingRootDirectory);
};

class FtpSocketDataProviderFileDownloadWithFileTypecode
    : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderFileDownloadWithFileTypecode() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /file\r\n", data, PRE_RETR,
                      "213 18\r\n");
      case PRE_RETR:
        return Verify("RETR /file\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadWithFileTypecode);
};

class FtpSocketDataProviderFileDownload : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderFileDownload() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /file\r\n", data, PRE_CWD,
                      "213 18\r\n");
      case PRE_CWD:
        return Verify("CWD /file\r\n", data,
                      use_epsv() ? PRE_RETR_EPSV : PRE_RETR_PASV,
                      "550 Not a directory\r\n");
      case PRE_RETR:
        return Verify("RETR /file\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownload);
};

class FtpSocketDataProviderFileNotFound : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderFileNotFound() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /file\r\n", data,
                      use_epsv() ? PRE_CWD_EPSV : PRE_CWD_PASV,
                      "550 File Not Found\r\n");
      case PRE_CWD:
        return Verify("CWD /file\r\n", data,
                      use_epsv() ? PRE_RETR_EPSV : PRE_RETR_PASV,
                      "550 File Not Found\r\n");
      case PRE_RETR:
        return Verify("RETR /file\r\n", data, PRE_QUIT,
                      "550 File Not Found\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileNotFound);
};

class FtpSocketDataProviderFileDownloadWithPasvFallback
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadWithPasvFallback() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_EPSV:
        return Verify("EPSV\r\n", data, PRE_PASV,
                      "500 No can do\r\n");
      case PRE_CWD:
        return Verify("CWD /file\r\n", data, PRE_RETR_PASV,
                      "550 Not a directory\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadWithPasvFallback);
};

class FtpSocketDataProviderFileDownloadZeroSize
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadZeroSize() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /file\r\n", data, PRE_CWD,
                      "213 0\r\n");
      case PRE_CWD:
        return Verify("CWD /file\r\n", data,
                      use_epsv() ? PRE_RETR_EPSV : PRE_RETR_PASV,
                      "550 not a directory\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadZeroSize);
};

class FtpSocketDataProviderFileDownloadCWD451
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadCWD451() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_CWD:
        return Verify("CWD /file\r\n", data,
                      use_epsv() ? PRE_RETR_EPSV : PRE_RETR_PASV,
                      "451 not a directory\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadCWD451);
};

class FtpSocketDataProviderVMSFileDownload : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderVMSFileDownload() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 VMS\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"ANONYMOUS_ROOT:[000000]\"\r\n");
      case PRE_EPSV:
        return Verify("EPSV\r\n", data, PRE_PASV,
                      "500 EPSV command unknown\r\n");
      case PRE_SIZE:
        return Verify("SIZE ANONYMOUS_ROOT:[000000]file\r\n", data, PRE_CWD,
                      "213 18\r\n");
      case PRE_CWD:
        return Verify("CWD ANONYMOUS_ROOT:[file]\r\n", data, PRE_RETR_PASV,
                      "550 Not a directory\r\n");
      case PRE_RETR:
        return Verify("RETR ANONYMOUS_ROOT:[000000]file\r\n", data, PRE_QUIT,
                      "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderVMSFileDownload);
};

class FtpSocketDataProviderEscaping : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderEscaping() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE / !\"#$%y\200\201\r\n", data, PRE_CWD,
                      "213 18\r\n");
      case PRE_CWD:
        return Verify("CWD / !\"#$%y\200\201\r\n", data,
                      use_epsv() ? PRE_RETR_EPSV : PRE_RETR_PASV,
                      "550 Not a directory\r\n");
      case PRE_RETR:
        return Verify("RETR / !\"#$%y\200\201\r\n", data, PRE_QUIT,
                      "200 OK\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderEscaping);
};

class FtpSocketDataProviderFileDownloadTransferStarting
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadTransferStarting() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_RETR:
        return Verify("RETR /file\r\n", data, PRE_QUIT,
                      "125-Data connection already open.\r\n"
                      "125  Transfer starting.\r\n"
                      "226 Transfer complete.\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadTransferStarting);
};

class FtpSocketDataProviderDirectoryListingTransferStarting
    : public FtpSocketDataProviderDirectoryListing {
 public:
  FtpSocketDataProviderDirectoryListingTransferStarting() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_LIST:
        return Verify("LIST -l\r\n", data, PRE_QUIT,
                      "125-Data connection already open.\r\n"
                      "125  Transfer starting.\r\n"
                      "226 Transfer complete.\r\n");
      default:
        return FtpSocketDataProviderDirectoryListing::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      FtpSocketDataProviderDirectoryListingTransferStarting);
};

class FtpSocketDataProviderFileDownloadInvalidResponse
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadInvalidResponse() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        // Use unallocated 599 FTP error code to make sure it falls into the
        // generic ERR_FTP_FAILED bucket.
        return Verify("SIZE /file\r\n", data, PRE_QUIT,
                      "599 Evil Response\r\n"
                      "599 More Evil\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadInvalidResponse);
};

class FtpSocketDataProviderEvilEpsv : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderEvilEpsv(const char* epsv_response,
                                State expected_state)
      : epsv_response_(epsv_response),
        epsv_response_length_(std::strlen(epsv_response)),
        expected_state_(expected_state) {}

  FtpSocketDataProviderEvilEpsv(const char* epsv_response,
                               size_t epsv_response_length,
                               State expected_state)
      : epsv_response_(epsv_response),
        epsv_response_length_(epsv_response_length),
        expected_state_(expected_state) {}

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_EPSV:
        return Verify("EPSV\r\n", data, expected_state_,
                      epsv_response_, epsv_response_length_);
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  const char* epsv_response_;
  const size_t epsv_response_length_;
  const State expected_state_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderEvilEpsv);
};

class FtpSocketDataProviderEvilPasv
    : public FtpSocketDataProviderFileDownloadWithPasvFallback {
 public:
  FtpSocketDataProviderEvilPasv(const char* pasv_response, State expected_state)
      : pasv_response_(pasv_response),
        expected_state_(expected_state) {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_PASV:
        return Verify("PASV\r\n", data, expected_state_, pasv_response_);
      default:
        return FtpSocketDataProviderFileDownloadWithPasvFallback::OnWrite(data);
    }
  }

 private:
  const char* pasv_response_;
  const State expected_state_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderEvilPasv);
};

class FtpSocketDataProviderEvilSize : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderEvilSize(const char* size_response, State expected_state)
      : size_response_(size_response),
        expected_state_(expected_state) {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /file\r\n", data, expected_state_, size_response_);
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  const char* size_response_;
  const State expected_state_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderEvilSize);
};

class FtpSocketDataProviderEvilLogin
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderEvilLogin(const char* expected_user,
                                const char* expected_password)
      : expected_user_(expected_user),
        expected_password_(expected_password) {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_USER:
        return Verify(std::string("USER ") + expected_user_ + "\r\n", data,
                      PRE_PASSWD, "331 Password needed\r\n");
      case PRE_PASSWD:
        return Verify(std::string("PASS ") + expected_password_ + "\r\n", data,
                      PRE_SYST, "230 Welcome\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  const char* expected_user_;
  const char* expected_password_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderEvilLogin);
};

class FtpSocketDataProviderCloseConnection : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderCloseConnection() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_USER:
        return Verify("USER anonymous\r\n", data,
                      PRE_QUIT, "");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderCloseConnection);
};

class FtpNetworkTransactionTest
    : public PlatformTest,
      public ::testing::WithParamInterface<int> {
 public:
  FtpNetworkTransactionTest()
      : host_resolver_(new MockHostResolver),
        session_(new FtpNetworkSession(host_resolver_.get())),
        transaction_(session_.get(), &mock_socket_factory_) {
    scoped_refptr<RuleBasedHostResolverProc> rules(
        new RuleBasedHostResolverProc(NULL));
    if (GetFamily() == AF_INET) {
      rules->AddIPLiteralRule("*", "127.0.0.1", "127.0.0.1");
    } else if (GetFamily() == AF_INET6) {
      rules->AddIPLiteralRule("*", "::1", "::1");
    } else {
      NOTREACHED();
    }
    host_resolver_->set_rules(rules.get());
  }

 protected:
  // Accessor to make code refactoring-friendly, e.g. when we change the way
  // parameters are passed (like more parameters).
  int GetFamily() {
    return GetParam();
  }

  FtpRequestInfo GetRequestInfo(const std::string& url) {
    FtpRequestInfo info;
    info.url = GURL(url);
    return info;
  }

  void ExecuteTransaction(FtpSocketDataProvider* ctrl_socket,
                          const char* request,
                          int data_socket,
                          int expected_result) {
    // Expect EPSV usage for non-IPv4 control connections.
    ctrl_socket->set_use_epsv((GetFamily() != AF_INET));

    mock_socket_factory_.AddSocketDataProvider(ctrl_socket);

    std::string mock_data("mock-data");
    MockRead data_reads[] = {
      // Usually FTP servers close the data connection after the entire data has
      // been received.
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(mock_data.c_str()),
    };

    ScopedVector<StaticSocketDataProvider> data_sockets;
    data_sockets.reserve(data_socket);
    for (int i = 0; i < data_socket + 1; i++) {
      // We only read from one data socket, other ones are dummy.
      if (i == data_socket) {
        data_sockets.push_back(new StaticSocketDataProvider(
                                   data_reads, arraysize(data_reads), NULL, 0));
      } else {
        data_sockets.push_back(new StaticSocketDataProvider);
      }
      mock_socket_factory_.AddSocketDataProvider(data_sockets[i]);
    }

    FtpRequestInfo request_info = GetRequestInfo(request);
    EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
    ASSERT_EQ(ERR_IO_PENDING,
              transaction_.Start(&request_info, callback_.callback(),
                                 BoundNetLog()));
    EXPECT_NE(LOAD_STATE_IDLE, transaction_.GetLoadState());
    ASSERT_EQ(expected_result, callback_.WaitForResult());
    if (expected_result == OK) {
      scoped_refptr<IOBuffer> io_buffer(new IOBuffer(kBufferSize));
      memset(io_buffer->data(), 0, kBufferSize);
      ASSERT_EQ(ERR_IO_PENDING,
                transaction_.Read(io_buffer.get(), kBufferSize,
                                  callback_.callback()));
      ASSERT_EQ(static_cast<int>(mock_data.length()),
                callback_.WaitForResult());
      EXPECT_EQ(mock_data, std::string(io_buffer->data(), mock_data.length()));

      // Do another Read to detect that the data socket is now closed.
      int rv = transaction_.Read(io_buffer.get(), kBufferSize,
                                 callback_.callback());
      if (rv == ERR_IO_PENDING) {
        EXPECT_EQ(0, callback_.WaitForResult());
      } else {
        EXPECT_EQ(0, rv);
      }
    }
    EXPECT_EQ(FtpSocketDataProvider::QUIT, ctrl_socket->state());
    EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
  }

  void TransactionFailHelper(FtpSocketDataProvider* ctrl_socket,
                             const char* request,
                             FtpSocketDataProvider::State state,
                             FtpSocketDataProvider::State next_state,
                             const char* response,
                             int expected_result) {
    ctrl_socket->InjectFailure(state, next_state, response);
    ExecuteTransaction(ctrl_socket, request, 1, expected_result);
  }

  scoped_ptr<MockHostResolver> host_resolver_;
  scoped_refptr<FtpNetworkSession> session_;
  MockClientSocketFactory mock_socket_factory_;
  FtpNetworkTransaction transaction_;
  TestCompletionCallback callback_;
};

TEST_P(FtpNetworkTransactionTest, FailedLookup) {
  FtpRequestInfo request_info = GetRequestInfo("ftp://badhost");
  scoped_refptr<RuleBasedHostResolverProc> rules(
      new RuleBasedHostResolverProc(NULL));
  rules->AddSimulatedFailure("badhost");
  host_resolver_->set_rules(rules.get());

  EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Start(&request_info, callback_.callback(),
                               BoundNetLog()));
  ASSERT_EQ(ERR_NAME_NOT_RESOLVED, callback_.WaitForResult());
  EXPECT_EQ(LOAD_STATE_IDLE, transaction_.GetLoadState());
}

// Check that when determining the host, the square brackets decorating IPv6
// literals in URLs are stripped.
TEST_P(FtpNetworkTransactionTest, StripBracketsFromIPv6Literals) {
  // This test only makes sense for IPv6 connections.
  if (GetFamily() != AF_INET6)
    return;

  host_resolver_->rules()->AddSimulatedFailure("[::1]");

  // We start a transaction that is expected to fail with ERR_INVALID_RESPONSE.
  // The important part of this test is to make sure that we don't fail with
  // ERR_NAME_NOT_RESOLVED, since that would mean the decorated hostname
  // was used.
  FtpSocketDataProviderEvilSize ctrl_socket(
      "213 99999999999999999999999999999999\r\n",
      FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://[::1]/file", 1, ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransaction) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", 1, OK);

  EXPECT_TRUE(transaction_.GetResponseInfo()->is_directory_listing);
  EXPECT_EQ(-1, transaction_.GetResponseInfo()->expected_content_size);
  EXPECT_EQ((GetFamily() == AF_INET) ? "127.0.0.1" : "::1",
            transaction_.GetResponseInfo()->socket_address.host());
  EXPECT_EQ(21, transaction_.GetResponseInfo()->socket_address.port());
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionWithPasvFallback) {
  FtpSocketDataProviderDirectoryListingWithPasvFallback ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", 1, OK);

  EXPECT_TRUE(transaction_.GetResponseInfo()->is_directory_listing);
  EXPECT_EQ(-1, transaction_.GetResponseInfo()->expected_content_size);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionWithTypecode) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host;type=d", 1, OK);

  EXPECT_TRUE(transaction_.GetResponseInfo()->is_directory_listing);
  EXPECT_EQ(-1, transaction_.GetResponseInfo()->expected_content_size);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionMultilineWelcome) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.set_multiline_welcome(true);
  ExecuteTransaction(&ctrl_socket, "ftp://host", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionShortReads2) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.set_short_read_limit(2);
  ExecuteTransaction(&ctrl_socket, "ftp://host", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionShortReads5) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionMultilineWelcomeShort) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // The client will not consume all three 230 lines. That's good, we want to
  // test that scenario.
  ctrl_socket.allow_unconsumed_reads(true);
  ctrl_socket.set_multiline_welcome(true);
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host", 1, OK);
}

// Regression test for http://crbug.com/60555.
TEST_P(FtpNetworkTransactionTest, DirectoryTransactionZeroSize) {
  FtpSocketDataProviderDirectoryListingZeroSize ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", 0, OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionVMS) {
  FtpSocketDataProviderVMSDirectoryListing ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/dir", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionVMSRootDirectory) {
  FtpSocketDataProviderVMSDirectoryListingRootDirectory ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionTransferStarting) {
  FtpSocketDataProviderDirectoryListingTransferStarting ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransaction) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, OK);

  // We pass an artificial value of 18 as a response to the SIZE command.
  EXPECT_EQ(18, transaction_.GetResponseInfo()->expected_content_size);
  EXPECT_EQ((GetFamily() == AF_INET) ? "127.0.0.1" : "::1",
            transaction_.GetResponseInfo()->socket_address.host());
  EXPECT_EQ(21, transaction_.GetResponseInfo()->socket_address.port());
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionWithPasvFallback) {
  FtpSocketDataProviderFileDownloadWithPasvFallback ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, OK);

  // We pass an artificial value of 18 as a response to the SIZE command.
  EXPECT_EQ(18, transaction_.GetResponseInfo()->expected_content_size);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionWithTypecodeA) {
  FtpSocketDataProviderFileDownloadWithFileTypecode ctrl_socket;
  ctrl_socket.set_data_type('A');
  ExecuteTransaction(&ctrl_socket, "ftp://host/file;type=a", 0, OK);

  // We pass an artificial value of 18 as a response to the SIZE command.
  EXPECT_EQ(18, transaction_.GetResponseInfo()->expected_content_size);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionWithTypecodeI) {
  FtpSocketDataProviderFileDownloadWithFileTypecode ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file;type=i", 0, OK);

  // We pass an artificial value of 18 as a response to the SIZE command.
  EXPECT_EQ(18, transaction_.GetResponseInfo()->expected_content_size);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionMultilineWelcome) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ctrl_socket.set_multiline_welcome(true);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionShortReads2) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ctrl_socket.set_short_read_limit(2);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionShortReads5) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionZeroSize) {
  FtpSocketDataProviderFileDownloadZeroSize ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionCWD451) {
  FtpSocketDataProviderFileDownloadCWD451 ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionVMS) {
  FtpSocketDataProviderVMSFileDownload ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionTransferStarting) {
  FtpSocketDataProviderFileDownloadTransferStarting ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionInvalidResponse) {
  FtpSocketDataProviderFileDownloadInvalidResponse ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvReallyBadFormat) {
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort1) {
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,1,0,22)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort2) {
  // Still unsafe. 1 * 256 + 2 = 258, which is < 1024.
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,1,1,2)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort3) {
  // Still unsafe. 3 * 256 + 4 = 772, which is < 1024.
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,1,3,4)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort4) {
  // Unsafe. 8 * 256 + 1 = 2049, which is used by nfs.
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,1,8,1)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafeHost) {
  FtpSocketDataProviderEvilPasv ctrl_socket(
      "227 Portscan (10,1,2,3,123,456)\r\n", FtpSocketDataProvider::PRE_SIZE);
  ctrl_socket.set_use_epsv(GetFamily() != AF_INET);
  std::string mock_data("mock-data");
  MockRead data_reads[] = {
    MockRead(mock_data.c_str()),
  };
  StaticSocketDataProvider data_socket1;
  StaticSocketDataProvider data_socket2(data_reads, arraysize(data_reads),
                                        NULL, 0);
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket);
  mock_socket_factory_.AddSocketDataProvider(&data_socket1);
  mock_socket_factory_.AddSocketDataProvider(&data_socket2);
  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  // Start the transaction.
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Start(&request_info, callback_.callback(),
                               BoundNetLog()));
  ASSERT_EQ(OK, callback_.WaitForResult());

  // The transaction fires the callback when we can start reading data. That
  // means that the data socket should be open.
  MockTCPClientSocket* data_socket =
      static_cast<MockTCPClientSocket*>(transaction_.data_socket_.get());
  ASSERT_TRUE(data_socket);
  ASSERT_TRUE(data_socket->IsConnected());

  // Even if the PASV response specified some other address, we connect
  // to the address we used for control connection (which could be 127.0.0.1
  // or ::1 depending on whether we use IPv6).
  for (AddressList::const_iterator it = data_socket->addresses().begin();
      it != data_socket->addresses().end(); ++it) {
    EXPECT_NE("10.1.2.3", it->ToStringWithoutPort());
  }
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat1) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|||22)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat2) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (||\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat3) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat4) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (||||)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat5) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  const char response[] = "227 Portscan (\0\0\031773\0)\r\n";
  FtpSocketDataProviderEvilEpsv ctrl_socket(response, sizeof(response)-1,
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvUnsafePort1) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|||22|)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvUnsafePort2) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|||258|)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvUnsafePort3) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|||772|)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvUnsafePort4) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|||2049|)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvWeirdSep) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan ($$$31744$)\r\n",
                                            FtpSocketDataProvider::PRE_SIZE);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, OK);
}

TEST_P(FtpNetworkTransactionTest,
       DownloadTransactionEvilEpsvWeirdSepUnsafePort) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan ($$$317$)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvIllegalHost) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|2|::1|31744|)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilLoginBadUsername) {
  FtpSocketDataProviderEvilLogin ctrl_socket("hello%0Aworld", "test");
  ExecuteTransaction(&ctrl_socket, "ftp://hello%0Aworld:test@host/file", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilLoginBadPassword) {
  FtpSocketDataProviderEvilLogin ctrl_socket("test", "hello%0Dworld");
  ExecuteTransaction(&ctrl_socket, "ftp://test:hello%0Dworld@host/file", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionSpaceInLogin) {
  FtpSocketDataProviderEvilLogin ctrl_socket("hello world", "test");
  ExecuteTransaction(&ctrl_socket, "ftp://hello%20world:test@host/file", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionSpaceInPassword) {
  FtpSocketDataProviderEvilLogin ctrl_socket("test", "hello world");
  ExecuteTransaction(&ctrl_socket, "ftp://test:hello%20world@host/file", 1, OK);
}

TEST_P(FtpNetworkTransactionTest, EvilRestartUser) {
  FtpSocketDataProvider ctrl_socket1;
  ctrl_socket1.InjectFailure(FtpSocketDataProvider::PRE_PASSWD,
                             FtpSocketDataProvider::PRE_QUIT,
                             "530 Login authentication failed\r\n");
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket1);

  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Start(&request_info, callback_.callback(),
                               BoundNetLog()));
  ASSERT_EQ(ERR_FTP_FAILED, callback_.WaitForResult());

  MockRead ctrl_reads[] = {
    MockRead("220 host TestFTPd\r\n"),
    MockRead("221 Goodbye!\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite ctrl_writes[] = {
    MockWrite("QUIT\r\n"),
  };
  StaticSocketDataProvider ctrl_socket2(ctrl_reads, arraysize(ctrl_reads),
                                        ctrl_writes, arraysize(ctrl_writes));
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket2);
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.RestartWithAuth(
                AuthCredentials(
                    base::ASCIIToUTF16("foo\nownz0red"),
                    base::ASCIIToUTF16("innocent")),
                callback_.callback()));
  EXPECT_EQ(ERR_MALFORMED_IDENTITY, callback_.WaitForResult());
}

TEST_P(FtpNetworkTransactionTest, EvilRestartPassword) {
  FtpSocketDataProvider ctrl_socket1;
  ctrl_socket1.InjectFailure(FtpSocketDataProvider::PRE_PASSWD,
                             FtpSocketDataProvider::PRE_QUIT,
                             "530 Login authentication failed\r\n");
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket1);

  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.Start(&request_info, callback_.callback(),
                               BoundNetLog()));
  ASSERT_EQ(ERR_FTP_FAILED, callback_.WaitForResult());

  MockRead ctrl_reads[] = {
    MockRead("220 host TestFTPd\r\n"),
    MockRead("331 User okay, send password\r\n"),
    MockRead("221 Goodbye!\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite ctrl_writes[] = {
    MockWrite("USER innocent\r\n"),
    MockWrite("QUIT\r\n"),
  };
  StaticSocketDataProvider ctrl_socket2(ctrl_reads, arraysize(ctrl_reads),
                                        ctrl_writes, arraysize(ctrl_writes));
  mock_socket_factory_.AddSocketDataProvider(&ctrl_socket2);
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_.RestartWithAuth(
                AuthCredentials(base::ASCIIToUTF16("innocent"),
                                base::ASCIIToUTF16("foo\nownz0red")),
                callback_.callback()));
  EXPECT_EQ(ERR_MALFORMED_IDENTITY, callback_.WaitForResult());
}

TEST_P(FtpNetworkTransactionTest, Escaping) {
  FtpSocketDataProviderEscaping ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/%20%21%22%23%24%25%79%80%81",
                     1, OK);
}

// Test for http://crbug.com/23794.
TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilSize) {
  // Try to overflow int64 in the response.
  FtpSocketDataProviderEvilSize ctrl_socket(
      "213 99999999999999999999999999999999\r\n",
      FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, ERR_INVALID_RESPONSE);
}

// Test for http://crbug.com/36360.
TEST_P(FtpNetworkTransactionTest, DownloadTransactionBigSize) {
  // Pass a valid, but large file size. The transaction should not fail.
  FtpSocketDataProviderEvilSize ctrl_socket(
      "213 3204427776\r\n",
      FtpSocketDataProvider::PRE_CWD);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 1, OK);
  EXPECT_EQ(3204427776LL,
            transaction_.GetResponseInfo()->expected_content_size);
}

// Regression test for http://crbug.com/25023.
TEST_P(FtpNetworkTransactionTest, CloseConnection) {
  FtpSocketDataProviderCloseConnection ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", 1, ERR_EMPTY_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailUser) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_USER,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailPass) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_PASSWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "530 Login authentication failed\r\n",
                        ERR_FTP_FAILED);
}

// Regression test for http://crbug.com/38707.
TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailPass503) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_PASSWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "503 Bad sequence of commands\r\n",
                        ERR_FTP_BAD_COMMAND_SEQUENCE);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailSyst) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_SYST,
                        FtpSocketDataProvider::PRE_PWD,
                        "599 fail\r\n",
                        OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailPwd) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_PWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailType) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_TYPE,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailEpsv) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_EPSV,
                        FtpSocketDataProvider::PRE_NOPASV,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailCwd) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_CWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailList) {
  FtpSocketDataProviderVMSDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/dir",
                        FtpSocketDataProvider::PRE_LIST,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailUser) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_USER,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailPass) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_PASSWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "530 Login authentication failed\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailSyst) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_SYST,
                        FtpSocketDataProvider::PRE_PWD,
                        "599 fail\r\n",
                        OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailPwd) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_PWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailType) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_TYPE,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailEpsv) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderFileDownload ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_EPSV,
                        FtpSocketDataProvider::PRE_NOPASV,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailRetr) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_RETR,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, FileNotFound) {
  FtpSocketDataProviderFileNotFound ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", 2, ERR_FTP_FAILED);
}

// Test for http://crbug.com/38845.
TEST_P(FtpNetworkTransactionTest, ZeroLengthDirInPWD) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_PWD,
                        FtpSocketDataProvider::PRE_TYPE,
                        "257 \"\"\r\n",
                        OK);
}

INSTANTIATE_TEST_CASE_P(FTP,
                        FtpNetworkTransactionTest,
                        ::testing::Values(AF_INET, AF_INET6));

}  // namespace net
