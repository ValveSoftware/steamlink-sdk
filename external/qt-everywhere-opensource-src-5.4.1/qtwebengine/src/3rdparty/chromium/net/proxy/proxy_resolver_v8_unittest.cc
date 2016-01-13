// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/net_log_unittest.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {
namespace {

// Javascript bindings for ProxyResolverV8, which returns mock values.
// Each time one of the bindings is called into, we push the input into a
// list, for later verification.
class MockJSBindings : public ProxyResolverV8::JSBindings {
 public:
  MockJSBindings() : my_ip_address_count(0), my_ip_address_ex_count(0),
                     should_terminate(false) {}

  virtual void Alert(const base::string16& message) OVERRIDE {
    VLOG(1) << "PAC-alert: " << message;  // Helpful when debugging.
    alerts.push_back(base::UTF16ToUTF8(message));
  }

  virtual bool ResolveDns(const std::string& host,
                          ResolveDnsOperation op,
                          std::string* output,
                          bool* terminate) OVERRIDE {
    *terminate = should_terminate;

    if (op == MY_IP_ADDRESS) {
      my_ip_address_count++;
      *output = my_ip_address_result;
      return !my_ip_address_result.empty();
    }

    if (op == MY_IP_ADDRESS_EX) {
      my_ip_address_ex_count++;
      *output = my_ip_address_ex_result;
      return !my_ip_address_ex_result.empty();
    }

    if (op == DNS_RESOLVE) {
      dns_resolves.push_back(host);
      *output = dns_resolve_result;
      return !dns_resolve_result.empty();
    }

    if (op == DNS_RESOLVE_EX) {
      dns_resolves_ex.push_back(host);
      *output = dns_resolve_ex_result;
      return !dns_resolve_ex_result.empty();
    }

    CHECK(false);
    return false;
  }

  virtual void OnError(int line_number,
                       const base::string16& message) OVERRIDE {
    // Helpful when debugging.
    VLOG(1) << "PAC-error: [" << line_number << "] " << message;

    errors.push_back(base::UTF16ToUTF8(message));
    errors_line_number.push_back(line_number);
  }

  // Mock values to return.
  std::string my_ip_address_result;
  std::string my_ip_address_ex_result;
  std::string dns_resolve_result;
  std::string dns_resolve_ex_result;

  // Inputs we got called with.
  std::vector<std::string> alerts;
  std::vector<std::string> errors;
  std::vector<int> errors_line_number;
  std::vector<std::string> dns_resolves;
  std::vector<std::string> dns_resolves_ex;
  int my_ip_address_count;
  int my_ip_address_ex_count;

  // Whether ResolveDns() should terminate script execution.
  bool should_terminate;
};

// This is the same as ProxyResolverV8, but it uses mock bindings in place of
// the default bindings, and has a helper function to load PAC scripts from
// disk.
class ProxyResolverV8WithMockBindings : public ProxyResolverV8 {
 public:
  ProxyResolverV8WithMockBindings() {
    set_js_bindings(&mock_js_bindings_);
  }

  virtual ~ProxyResolverV8WithMockBindings() {
  }

  MockJSBindings* mock_js_bindings() {
    return &mock_js_bindings_;
  }

  // Initialize with the PAC script data at |filename|.
  int SetPacScriptFromDisk(const char* filename) {
    base::FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("net");
    path = path.AppendASCII("data");
    path = path.AppendASCII("proxy_resolver_v8_unittest");
    path = path.AppendASCII(filename);

    // Try to read the file from disk.
    std::string file_contents;
    bool ok = base::ReadFileToString(path, &file_contents);

    // If we can't load the file from disk, something is misconfigured.
    if (!ok) {
      LOG(ERROR) << "Failed to read file: " << path.value();
      return ERR_UNEXPECTED;
    }

    // Load the PAC script into the ProxyResolver.
    return SetPacScript(ProxyResolverScriptData::FromUTF8(file_contents),
                        CompletionCallback());
  }

 private:
  MockJSBindings mock_js_bindings_;
};

// Doesn't really matter what these values are for many of the tests.
const GURL kQueryUrl("http://www.google.com");
const GURL kPacUrl;

TEST(ProxyResolverV8Test, Direct) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("direct.js");
  EXPECT_EQ(OK, result);

  ProxyInfo proxy_info;
  CapturingBoundNetLog log;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, log.bound());

  EXPECT_EQ(OK, result);
  EXPECT_TRUE(proxy_info.is_direct());

  EXPECT_EQ(0U, resolver.mock_js_bindings()->alerts.size());
  EXPECT_EQ(0U, resolver.mock_js_bindings()->errors.size());

  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  // No bindings were called, so no log entries.
  EXPECT_EQ(0u, entries.size());
}

TEST(ProxyResolverV8Test, ReturnEmptyString) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("return_empty_string.js");
  EXPECT_EQ(OK, result);

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

  EXPECT_EQ(OK, result);
  EXPECT_TRUE(proxy_info.is_direct());

  EXPECT_EQ(0U, resolver.mock_js_bindings()->alerts.size());
  EXPECT_EQ(0U, resolver.mock_js_bindings()->errors.size());
}

TEST(ProxyResolverV8Test, Basic) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("passthrough.js");
  EXPECT_EQ(OK, result);

  // The "FindProxyForURL" of this PAC script simply concatenates all of the
  // arguments into a pseudo-host. The purpose of this test is to verify that
  // the correct arguments are being passed to FindProxyForURL().
  {
    ProxyInfo proxy_info;
    result = resolver.GetProxyForURL(GURL("http://query.com/path"), &proxy_info,
                                     CompletionCallback(), NULL, BoundNetLog());
    EXPECT_EQ(OK, result);
    EXPECT_EQ("http.query.com.path.query.com:80",
              proxy_info.proxy_server().ToURI());
  }
  {
    ProxyInfo proxy_info;
    int result = resolver.GetProxyForURL(
        GURL("ftp://query.com:90/path"), &proxy_info, CompletionCallback(),
        NULL, BoundNetLog());
    EXPECT_EQ(OK, result);
    // Note that FindProxyForURL(url, host) does not expect |host| to contain
    // the port number.
    EXPECT_EQ("ftp.query.com.90.path.query.com:80",
              proxy_info.proxy_server().ToURI());

    EXPECT_EQ(0U, resolver.mock_js_bindings()->alerts.size());
    EXPECT_EQ(0U, resolver.mock_js_bindings()->errors.size());
  }
}

TEST(ProxyResolverV8Test, BadReturnType) {
  // These are the filenames of PAC scripts which each return a non-string
  // types for FindProxyForURL(). They should all fail with
  // ERR_PAC_SCRIPT_FAILED.
  static const char* const filenames[] = {
      "return_undefined.js",
      "return_integer.js",
      "return_function.js",
      "return_object.js",
      // TODO(eroman): Should 'null' be considered equivalent to "DIRECT" ?
      "return_null.js"
  };

  for (size_t i = 0; i < arraysize(filenames); ++i) {
    ProxyResolverV8WithMockBindings resolver;
    int result = resolver.SetPacScriptFromDisk(filenames[i]);
    EXPECT_EQ(OK, result);

    ProxyInfo proxy_info;
    result = resolver.GetProxyForURL(
        kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

    EXPECT_EQ(ERR_PAC_SCRIPT_FAILED, result);

    MockJSBindings* bindings = resolver.mock_js_bindings();
    EXPECT_EQ(0U, bindings->alerts.size());
    ASSERT_EQ(1U, bindings->errors.size());
    EXPECT_EQ("FindProxyForURL() did not return a string.",
              bindings->errors[0]);
    EXPECT_EQ(-1, bindings->errors_line_number[0]);
  }
}

// Try using a PAC script which defines no "FindProxyForURL" function.
TEST(ProxyResolverV8Test, NoEntryPoint) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("no_entrypoint.js");
  EXPECT_EQ(ERR_PAC_SCRIPT_FAILED, result);

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

  EXPECT_EQ(ERR_FAILED, result);
}

// Try loading a malformed PAC script.
TEST(ProxyResolverV8Test, ParseError) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("missing_close_brace.js");
  EXPECT_EQ(ERR_PAC_SCRIPT_FAILED, result);

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

  EXPECT_EQ(ERR_FAILED, result);

  MockJSBindings* bindings = resolver.mock_js_bindings();
  EXPECT_EQ(0U, bindings->alerts.size());

  // We get one error during compilation.
  ASSERT_EQ(1U, bindings->errors.size());

  EXPECT_EQ("Uncaught SyntaxError: Unexpected end of input",
            bindings->errors[0]);
  EXPECT_EQ(0, bindings->errors_line_number[0]);
}

// Run a PAC script several times, which has side-effects.
TEST(ProxyResolverV8Test, SideEffects) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("side_effects.js");

  // The PAC script increments a counter each time we invoke it.
  for (int i = 0; i < 3; ++i) {
    ProxyInfo proxy_info;
    result = resolver.GetProxyForURL(
        kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());
    EXPECT_EQ(OK, result);
    EXPECT_EQ(base::StringPrintf("sideffect_%d:80", i),
              proxy_info.proxy_server().ToURI());
  }

  // Reload the script -- the javascript environment should be reset, hence
  // the counter starts over.
  result = resolver.SetPacScriptFromDisk("side_effects.js");
  EXPECT_EQ(OK, result);

  for (int i = 0; i < 3; ++i) {
    ProxyInfo proxy_info;
    result = resolver.GetProxyForURL(
        kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());
    EXPECT_EQ(OK, result);
    EXPECT_EQ(base::StringPrintf("sideffect_%d:80", i),
              proxy_info.proxy_server().ToURI());
  }
}

// Execute a PAC script which throws an exception in FindProxyForURL.
TEST(ProxyResolverV8Test, UnhandledException) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("unhandled_exception.js");
  EXPECT_EQ(OK, result);

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

  EXPECT_EQ(ERR_PAC_SCRIPT_FAILED, result);

  MockJSBindings* bindings = resolver.mock_js_bindings();
  EXPECT_EQ(0U, bindings->alerts.size());
  ASSERT_EQ(1U, bindings->errors.size());
  EXPECT_EQ("Uncaught ReferenceError: undefined_variable is not defined",
            bindings->errors[0]);
  EXPECT_EQ(3, bindings->errors_line_number[0]);
}

TEST(ProxyResolverV8Test, ReturnUnicode) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("return_unicode.js");
  EXPECT_EQ(OK, result);

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

  // The result from this resolve was unparseable, because it
  // wasn't ASCII.
  EXPECT_EQ(ERR_PAC_SCRIPT_FAILED, result);
}

// Test the PAC library functions that we expose in the JS environment.
TEST(ProxyResolverV8Test, JavascriptLibrary) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("pac_library_unittest.js");
  EXPECT_EQ(OK, result);

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

  // If the javascript side of this unit-test fails, it will throw a javascript
  // exception. Otherwise it will return "PROXY success:80".
  EXPECT_EQ(OK, result);
  EXPECT_EQ("success:80", proxy_info.proxy_server().ToURI());

  EXPECT_EQ(0U, resolver.mock_js_bindings()->alerts.size());
  EXPECT_EQ(0U, resolver.mock_js_bindings()->errors.size());
}

// Try resolving when SetPacScriptByData() has not been called.
TEST(ProxyResolverV8Test, NoSetPacScript) {
  ProxyResolverV8WithMockBindings resolver;

  ProxyInfo proxy_info;

  // Resolve should fail, as we are not yet initialized with a script.
  int result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());
  EXPECT_EQ(ERR_FAILED, result);

  // Initialize it.
  result = resolver.SetPacScriptFromDisk("direct.js");
  EXPECT_EQ(OK, result);

  // Resolve should now succeed.
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());
  EXPECT_EQ(OK, result);

  // Clear it, by initializing with an empty string.
  resolver.SetPacScript(
      ProxyResolverScriptData::FromUTF16(base::string16()),
      CompletionCallback());

  // Resolve should fail again now.
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());
  EXPECT_EQ(ERR_FAILED, result);

  // Load a good script once more.
  result = resolver.SetPacScriptFromDisk("direct.js");
  EXPECT_EQ(OK, result);
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());
  EXPECT_EQ(OK, result);

  EXPECT_EQ(0U, resolver.mock_js_bindings()->alerts.size());
  EXPECT_EQ(0U, resolver.mock_js_bindings()->errors.size());
}

// Test marshalling/un-marshalling of values between C++/V8.
TEST(ProxyResolverV8Test, V8Bindings) {
  ProxyResolverV8WithMockBindings resolver;
  MockJSBindings* bindings = resolver.mock_js_bindings();
  bindings->dns_resolve_result = "127.0.0.1";
  int result = resolver.SetPacScriptFromDisk("bindings.js");
  EXPECT_EQ(OK, result);

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

  EXPECT_EQ(OK, result);
  EXPECT_TRUE(proxy_info.is_direct());

  EXPECT_EQ(0U, resolver.mock_js_bindings()->errors.size());

  // Alert was called 5 times.
  ASSERT_EQ(5U, bindings->alerts.size());
  EXPECT_EQ("undefined", bindings->alerts[0]);
  EXPECT_EQ("null", bindings->alerts[1]);
  EXPECT_EQ("undefined", bindings->alerts[2]);
  EXPECT_EQ("[object Object]", bindings->alerts[3]);
  EXPECT_EQ("exception from calling toString()", bindings->alerts[4]);

  // DnsResolve was called 8 times, however only 2 of those were string
  // parameters. (so 6 of them failed immediately).
  ASSERT_EQ(2U, bindings->dns_resolves.size());
  EXPECT_EQ("", bindings->dns_resolves[0]);
  EXPECT_EQ("arg1", bindings->dns_resolves[1]);

  // MyIpAddress was called two times.
  EXPECT_EQ(2, bindings->my_ip_address_count);

  // MyIpAddressEx was called once.
  EXPECT_EQ(1, bindings->my_ip_address_ex_count);

  // DnsResolveEx was called 2 times.
  ASSERT_EQ(2U, bindings->dns_resolves_ex.size());
  EXPECT_EQ("is_resolvable", bindings->dns_resolves_ex[0]);
  EXPECT_EQ("foobar", bindings->dns_resolves_ex[1]);
}

// Test calling a binding (myIpAddress()) from the script's global scope.
// http://crbug.com/40026
TEST(ProxyResolverV8Test, BindingCalledDuringInitialization) {
  ProxyResolverV8WithMockBindings resolver;

  int result = resolver.SetPacScriptFromDisk("binding_from_global.js");
  EXPECT_EQ(OK, result);

  MockJSBindings* bindings = resolver.mock_js_bindings();

  // myIpAddress() got called during initialization of the script.
  EXPECT_EQ(1, bindings->my_ip_address_count);

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

  EXPECT_EQ(OK, result);
  EXPECT_FALSE(proxy_info.is_direct());
  EXPECT_EQ("127.0.0.1:80", proxy_info.proxy_server().ToURI());

  // Check that no other bindings were called.
  EXPECT_EQ(0U, bindings->errors.size());
  ASSERT_EQ(0U, bindings->alerts.size());
  ASSERT_EQ(0U, bindings->dns_resolves.size());
  EXPECT_EQ(0, bindings->my_ip_address_ex_count);
  ASSERT_EQ(0U, bindings->dns_resolves_ex.size());
}

// Try loading a PAC script that ends with a comment and has no terminal
// newline. This should not cause problems with the PAC utility functions
// that we add to the script's environment.
// http://crbug.com/22864
TEST(ProxyResolverV8Test, EndsWithCommentNoNewline) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("ends_with_comment.js");
  EXPECT_EQ(OK, result);

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

  EXPECT_EQ(OK, result);
  EXPECT_FALSE(proxy_info.is_direct());
  EXPECT_EQ("success:80", proxy_info.proxy_server().ToURI());
}

// Try loading a PAC script that ends with a statement and has no terminal
// newline. This should not cause problems with the PAC utility functions
// that we add to the script's environment.
// http://crbug.com/22864
TEST(ProxyResolverV8Test, EndsWithStatementNoNewline) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk(
      "ends_with_statement_no_semicolon.js");
  EXPECT_EQ(OK, result);

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

  EXPECT_EQ(OK, result);
  EXPECT_FALSE(proxy_info.is_direct());
  EXPECT_EQ("success:3", proxy_info.proxy_server().ToURI());
}

// Test the return values from myIpAddress(), myIpAddressEx(), dnsResolve(),
// dnsResolveEx(), isResolvable(), isResolvableEx(), when the the binding
// returns empty string (failure). This simulates the return values from
// those functions when the underlying DNS resolution fails.
TEST(ProxyResolverV8Test, DNSResolutionFailure) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("dns_fail.js");
  EXPECT_EQ(OK, result);

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

  EXPECT_EQ(OK, result);
  EXPECT_FALSE(proxy_info.is_direct());
  EXPECT_EQ("success:80", proxy_info.proxy_server().ToURI());
}

TEST(ProxyResolverV8Test, DNSResolutionOfInternationDomainName) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("international_domain_names.js");
  EXPECT_EQ(OK, result);

  // Execute FindProxyForURL().
  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      kQueryUrl, &proxy_info, CompletionCallback(), NULL, BoundNetLog());

  EXPECT_EQ(OK, result);
  EXPECT_TRUE(proxy_info.is_direct());

  // Check that the international domain name was converted to punycode
  // before passing it onto the bindings layer.
  MockJSBindings* bindings = resolver.mock_js_bindings();

  ASSERT_EQ(1u, bindings->dns_resolves.size());
  EXPECT_EQ("xn--bcher-kva.ch", bindings->dns_resolves[0]);

  ASSERT_EQ(1u, bindings->dns_resolves_ex.size());
  EXPECT_EQ("xn--bcher-kva.ch", bindings->dns_resolves_ex[0]);
}

// Test that when resolving a URL which contains an IPv6 string literal, the
// brackets are removed from the host before passing it down to the PAC script.
// If we don't do this, then subsequent calls to dnsResolveEx(host) will be
// doomed to fail since it won't correspond with a valid name.
TEST(ProxyResolverV8Test, IPv6HostnamesNotBracketed) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("resolve_host.js");
  EXPECT_EQ(OK, result);

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      GURL("http://[abcd::efff]:99/watsupdawg"), &proxy_info,
      CompletionCallback(), NULL, BoundNetLog());

  EXPECT_EQ(OK, result);
  EXPECT_TRUE(proxy_info.is_direct());

  // We called dnsResolveEx() exactly once, by passing through the "host"
  // argument to FindProxyForURL(). The brackets should have been stripped.
  ASSERT_EQ(1U, resolver.mock_js_bindings()->dns_resolves_ex.size());
  EXPECT_EQ("abcd::efff", resolver.mock_js_bindings()->dns_resolves_ex[0]);
}

// Test that terminating a script within DnsResolve() leads to eventual
// termination of the script. Also test that repeatedly calling terminate is
// safe, and running the script again after termination still works.
TEST(ProxyResolverV8Test, Terminate) {
  ProxyResolverV8WithMockBindings resolver;
  int result = resolver.SetPacScriptFromDisk("terminate.js");
  EXPECT_EQ(OK, result);

  MockJSBindings* bindings = resolver.mock_js_bindings();

  // Terminate script execution upon reaching dnsResolve(). Note that
  // termination may not take effect right away (so the subsequent dnsResolve()
  // and alert() may be run).
  bindings->should_terminate = true;

  ProxyInfo proxy_info;
  result = resolver.GetProxyForURL(
      GURL("http://hang/"), &proxy_info,
      CompletionCallback(), NULL, BoundNetLog());

  // The script execution was terminated.
  EXPECT_EQ(ERR_PAC_SCRIPT_FAILED, result);

  EXPECT_EQ(1U, resolver.mock_js_bindings()->dns_resolves.size());
  EXPECT_GE(2U, resolver.mock_js_bindings()->dns_resolves_ex.size());
  EXPECT_GE(1U, bindings->alerts.size());

  EXPECT_EQ(1U, bindings->errors.size());

  // Termination shows up as an uncaught exception without any message.
  EXPECT_EQ("", bindings->errors[0]);

  bindings->errors.clear();

  // Try running the script again, this time with a different input which won't
  // cause a termination+hang.
  result = resolver.GetProxyForURL(
      GURL("http://kittens/"), &proxy_info,
      CompletionCallback(), NULL, BoundNetLog());

  EXPECT_EQ(OK, result);
  EXPECT_EQ(0u, bindings->errors.size());
  EXPECT_EQ("kittens:88", proxy_info.proxy_server().ToURI());
}

}  // namespace
}  // namespace net
