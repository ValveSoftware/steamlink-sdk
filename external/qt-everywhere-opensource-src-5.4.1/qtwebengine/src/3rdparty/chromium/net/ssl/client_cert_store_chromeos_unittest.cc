// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_chromeos.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "net/cert/nss_cert_database.h"
#include "net/ssl/client_cert_store_unittest-inl.h"

namespace net {

class ClientCertStoreChromeOSTestDelegate {
 public:
  ClientCertStoreChromeOSTestDelegate()
      : store_("usernamehash",
               ClientCertStoreChromeOS::PasswordDelegateFactory()) {
    store_.InitForTesting(
        crypto::ScopedPK11Slot(crypto::GetPublicNSSKeySlot()),
        crypto::ScopedPK11Slot(crypto::GetPrivateNSSKeySlot()));
  }

  bool SelectClientCerts(const CertificateList& input_certs,
                         const SSLCertRequestInfo& cert_request_info,
                         CertificateList* selected_certs) {
    return store_.SelectClientCertsForTesting(
        input_certs, cert_request_info, selected_certs);
  }

 private:
  ClientCertStoreChromeOS store_;
};

class ClientCertStoreChromeOSTest : public ::testing::Test {
 public:
  scoped_refptr<X509Certificate> ImportCertForUser(
      const std::string& username_hash,
      const std::string& filename,
      const std::string& password) {
    crypto::ScopedPK11Slot slot(
        crypto::GetPublicSlotForChromeOSUser(username_hash));
    EXPECT_TRUE(slot.get());
    if (!slot.get())
      return NULL;

    net::CertificateList cert_list;

    base::FilePath p12_path = GetTestCertsDirectory().AppendASCII(filename);
    std::string p12_data;
    if (!base::ReadFileToString(p12_path, &p12_data)) {
      EXPECT_TRUE(false);
      return NULL;
    }

    scoped_refptr<net::CryptoModule> module(
        net::CryptoModule::CreateFromHandle(slot.get()));
    int rv = NSSCertDatabase::GetInstance()->ImportFromPKCS12(
        module.get(), p12_data, base::UTF8ToUTF16(password), false, &cert_list);

    EXPECT_EQ(0, rv);
    EXPECT_EQ(1U, cert_list.size());
    if (rv || cert_list.size() != 1)
      return NULL;

    return cert_list[0];
  }
};

// TODO(mattm): Do better testing of cert_authorities matching below. Update
// net/data/ssl/scripts/generate-client-certificates.sh so that it actually
// saves the .p12 files, and regenerate them.

TEST_F(ClientCertStoreChromeOSTest, WaitForNSSInit) {
  crypto::ScopedTestNSSChromeOSUser user("scopeduser");
  ASSERT_TRUE(user.constructed_successfully());
  ClientCertStoreChromeOS store(
      user.username_hash(), ClientCertStoreChromeOS::PasswordDelegateFactory());
  scoped_refptr<X509Certificate> cert_1(
      ImportCertForUser(user.username_hash(), "client.p12", "12345"));
  scoped_refptr<X509Certificate> cert_2(
      ImportCertForUser(user.username_hash(), "websocket_client_cert.p12", ""));

  std::vector<std::string> authority_1(
      1,
      std::string(reinterpret_cast<const char*>(kAuthority1DN),
                  sizeof(kAuthority1DN)));
  scoped_refptr<SSLCertRequestInfo> request_1(new SSLCertRequestInfo());
  request_1->cert_authorities = authority_1;

  scoped_refptr<SSLCertRequestInfo> request_all(new SSLCertRequestInfo());

  base::RunLoop run_loop_1;
  base::RunLoop run_loop_all;
  store.GetClientCerts(
      *request_1, &request_1->client_certs, run_loop_1.QuitClosure());
  store.GetClientCerts(
      *request_all, &request_all->client_certs, run_loop_all.QuitClosure());

  // Callbacks won't be run until nss_util init finishes for the user.
  user.FinishInit();

  run_loop_1.Run();
  run_loop_all.Run();

  ASSERT_EQ(0u, request_1->client_certs.size());
  ASSERT_EQ(2u, request_all->client_certs.size());
}

TEST_F(ClientCertStoreChromeOSTest, NSSAlreadyInitialized) {
  crypto::ScopedTestNSSChromeOSUser user("scopeduser");
  ASSERT_TRUE(user.constructed_successfully());
  user.FinishInit();

  ClientCertStoreChromeOS store(
      user.username_hash(), ClientCertStoreChromeOS::PasswordDelegateFactory());
  scoped_refptr<X509Certificate> cert_1(
      ImportCertForUser(user.username_hash(), "client.p12", "12345"));
  scoped_refptr<X509Certificate> cert_2(
      ImportCertForUser(user.username_hash(), "websocket_client_cert.p12", ""));

  std::vector<std::string> authority_1(
      1,
      std::string(reinterpret_cast<const char*>(kAuthority1DN),
                  sizeof(kAuthority1DN)));
  scoped_refptr<SSLCertRequestInfo> request_1(new SSLCertRequestInfo());
  request_1->cert_authorities = authority_1;

  scoped_refptr<SSLCertRequestInfo> request_all(new SSLCertRequestInfo());

  base::RunLoop run_loop_1;
  base::RunLoop run_loop_all;
  store.GetClientCerts(
      *request_1, &request_1->client_certs, run_loop_1.QuitClosure());
  store.GetClientCerts(
      *request_all, &request_all->client_certs, run_loop_all.QuitClosure());

  run_loop_1.Run();
  run_loop_all.Run();

  ASSERT_EQ(0u, request_1->client_certs.size());
  ASSERT_EQ(2u, request_all->client_certs.size());
}

TEST_F(ClientCertStoreChromeOSTest, TwoUsers) {
  crypto::ScopedTestNSSChromeOSUser user1("scopeduser1");
  ASSERT_TRUE(user1.constructed_successfully());
  crypto::ScopedTestNSSChromeOSUser user2("scopeduser2");
  ASSERT_TRUE(user2.constructed_successfully());
  ClientCertStoreChromeOS store1(
      user1.username_hash(),
      ClientCertStoreChromeOS::PasswordDelegateFactory());
  ClientCertStoreChromeOS store2(
      user2.username_hash(),
      ClientCertStoreChromeOS::PasswordDelegateFactory());
  scoped_refptr<X509Certificate> cert_1(
      ImportCertForUser(user1.username_hash(), "client.p12", "12345"));
  scoped_refptr<X509Certificate> cert_2(ImportCertForUser(
      user2.username_hash(), "websocket_client_cert.p12", ""));

  scoped_refptr<SSLCertRequestInfo> request_1(new SSLCertRequestInfo());
  scoped_refptr<SSLCertRequestInfo> request_2(new SSLCertRequestInfo());

  base::RunLoop run_loop_1;
  base::RunLoop run_loop_2;
  store1.GetClientCerts(
      *request_1, &request_1->client_certs, run_loop_1.QuitClosure());
  store2.GetClientCerts(
      *request_2, &request_2->client_certs, run_loop_2.QuitClosure());

  // Callbacks won't be run until nss_util init finishes for the user.
  user1.FinishInit();
  user2.FinishInit();

  run_loop_1.Run();
  run_loop_2.Run();

  ASSERT_EQ(1u, request_1->client_certs.size());
  EXPECT_TRUE(cert_1->Equals(request_1->client_certs[0]));
  // TODO(mattm): Request for second user will have zero results due to
  // crbug.com/315285.  Update the test once that is fixed.
}

}  // namespace net
