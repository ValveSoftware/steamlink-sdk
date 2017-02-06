// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_openssl_types.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Return;
using testing::_;

namespace {

void IgnoreResult(const base::Closure& callback, const base::Value* value) {
  callback.Run();
}

void StoreBool(bool* result,
               const base::Closure& callback,
               const base::Value* value) {
  value->GetAsBoolean(result);
  callback.Run();
}

void StoreString(std::string* result,
                 const base::Closure& callback,
                 const base::Value* value) {
  value->GetAsString(result);
  callback.Run();
}

void StoreDigest(std::vector<uint8_t>* digest,
                 const base::Closure& callback,
                 const base::Value* value) {
  const base::BinaryValue* binary = nullptr;
  const bool is_binary = value->GetAsBinary(&binary);
  EXPECT_TRUE(is_binary) << "Unexpected value in StoreDigest";
  if (is_binary) {
    const uint8_t* const binary_begin =
        reinterpret_cast<const uint8_t*>(binary->GetBuffer());
    digest->assign(binary_begin, binary_begin + binary->GetSize());
  }

  callback.Run();
}

// See net::SSLPrivateKey::SignDigest for the expected padding and DigestInfo
// prefixing.
bool RsaSign(const std::vector<uint8_t>& digest,
             crypto::RSAPrivateKey* key,
             std::vector<uint8_t>* signature) {
  crypto::ScopedRSA rsa_key(EVP_PKEY_get1_RSA(key->key()));
  if (!rsa_key)
    return false;

  uint8_t* prefixed_digest = nullptr;
  size_t prefixed_digest_len = 0;
  int is_alloced = 0;
  if (!RSA_add_pkcs1_prefix(&prefixed_digest, &prefixed_digest_len, &is_alloced,
                            NID_sha1, digest.data(), digest.size())) {
    return false;
  }
  size_t len = 0;
  signature->resize(RSA_size(rsa_key.get()));
  const int rv =
      RSA_sign_raw(rsa_key.get(), &len, signature->data(), signature->size(),
                   prefixed_digest, prefixed_digest_len, RSA_PKCS1_PADDING);
  if (is_alloced)
    free(prefixed_digest);

  if (rv) {
    signature->resize(len);
    return true;
  } else {
    signature->clear();
    return false;
  }
}

// Create a string that if evaluated in JavaScript returns a Uint8Array with
// |bytes| as content.
std::string JsUint8Array(const std::vector<uint8_t>& bytes) {
  std::string res = "new Uint8Array([";
  for (const uint8_t byte : bytes) {
    res += base::UintToString(byte);
    res += ", ";
  }
  res += "])";
  return res;
}

class CertificateProviderApiTest : public ExtensionApiTest {
 public:
  CertificateProviderApiTest() {}

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    // Set up the AutoSelectCertificateForUrls policy to avoid the client
    // certificate selection dialog.
    const std::string autoselect_pattern =
        "{\"pattern\": \"*\", \"filter\": {\"ISSUER\": {\"CN\": \"root\"}}}";

    std::unique_ptr<base::ListValue> autoselect_policy(new base::ListValue);
    autoselect_policy->AppendString(autoselect_pattern);

    policy::PolicyMap policy;
    policy.Set(policy::key::kAutoSelectCertificateForUrls,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, std::move(autoselect_policy),
               nullptr);
    provider_.UpdateChromePolicy(policy);

    content::RunAllPendingInMessageLoop();
  }

 protected:
  policy::MockConfigurationPolicyProvider provider_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(CertificateProviderApiTest, Basic) {
  // Start an HTTPS test server that requests a client certificate.
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  net::SpawnedTestServer https_server(net::SpawnedTestServer::TYPE_HTTPS,
                                      ssl_options, base::FilePath());
  ASSERT_TRUE(https_server.Start());

  extensions::ResultCatcher catcher;

  const base::FilePath extension_path =
      test_data_dir_.AppendASCII("certificate_provider");
  const extensions::Extension* const extension = LoadExtension(extension_path);
  ui_test_utils::NavigateToURL(browser(),
                               extension->GetResourceURL("basic.html"));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  VLOG(1) << "Extension registered. Navigate to the test https page.";

  content::WebContents* const extension_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver navigation_observer(
      nullptr /* no WebContents */);
  navigation_observer.StartWatchingNewWebContents();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server.GetURL("client-cert"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  content::WebContents* const https_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  VLOG(1) << "Wait for the extension to respond to the certificates request.";
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  VLOG(1) << "Wait for the extension to receive the sign request.";
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  VLOG(1) << "Fetch the digest from the sign request.";
  std::vector<uint8_t> request_digest;
  {
    base::RunLoop run_loop;
    extension_contents->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("signDigestRequest.digest;"),
        base::Bind(&StoreDigest, &request_digest, run_loop.QuitClosure()));
    run_loop.Run();
  }

  VLOG(1) << "Sign the digest using the private key.";
  std::string key_pk8;
  base::ReadFileToString(extension_path.AppendASCII("l1_leaf.pk8"), &key_pk8);

  const uint8_t* const key_pk8_begin =
      reinterpret_cast<const uint8_t*>(key_pk8.data());
  std::unique_ptr<crypto::RSAPrivateKey> key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(
          std::vector<uint8_t>(key_pk8_begin, key_pk8_begin + key_pk8.size())));
  ASSERT_TRUE(key);

  std::vector<uint8_t> signature;
  EXPECT_TRUE(RsaSign(request_digest, key.get(), &signature));

  VLOG(1) << "Inject the signature back to the extension and let it reply.";
  {
    base::RunLoop run_loop;
    const std::string code =
        "replyWithSignature(" + JsUint8Array(signature) + ");";
    extension_contents->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(code),
        base::Bind(&IgnoreResult, run_loop.QuitClosure()));
    run_loop.Run();
  }

  VLOG(1) << "Wait for the https navigation to finish.";
  navigation_observer.Wait();

  VLOG(1) << "Check whether the server acknowledged that a client certificate "
             "was presented.";
  {
    base::RunLoop run_loop;
    std::string https_reply;
    https_contents->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("document.body.textContent;"),
        base::Bind(&StoreString, &https_reply, run_loop.QuitClosure()));
    run_loop.Run();
    // Expect the server to return the fingerprint of the client cert that we
    // presented, which should be the fingerprint of 'l1_leaf.der'.
    // The fingerprint can be calculated independently using:
    // openssl x509 -inform DER -noout -fingerprint -in
    //   chrome/test/data/extensions/api_test/certificate_provider/l1_leaf.der
    ASSERT_EQ(
        "got client cert with fingerprint: "
        "2ab3f55e06eb8b36a741fe285a769da45edb2695",
        https_reply);
  }

  // Replying to the same signature request a second time must fail.
  {
    base::RunLoop run_loop;
    const std::string code = "replyWithSignatureSecondTime();";
    bool result = false;
    extension_contents->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(code),
        base::Bind(&StoreBool, &result, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(result);
  }
}
