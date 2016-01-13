// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_v8.h"

#include <algorithm>
#include <cstdio>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "gin/public/isolate_holder.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver_script.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "v8/include/v8.h"

// Notes on the javascript environment:
//
// For the majority of the PAC utility functions, we use the same code
// as Firefox. See the javascript library that proxy_resolver_scipt.h
// pulls in.
//
// In addition, we implement a subset of Microsoft's extensions to PAC.
// - myIpAddressEx()
// - dnsResolveEx()
// - isResolvableEx()
// - isInNetEx()
// - sortIpAddressList()
//
// It is worth noting that the original PAC specification does not describe
// the return values on failure. Consequently, there are compatibility
// differences between browsers on what to return on failure, which are
// illustrated below:
//
// --------------------+-------------+-------------------+--------------
//                     | Firefox3    | InternetExplorer8 |  --> Us <---
// --------------------+-------------+-------------------+--------------
// myIpAddress()       | "127.0.0.1" |  ???              |  "127.0.0.1"
// dnsResolve()        | null        |  false            |  null
// myIpAddressEx()     | N/A         |  ""               |  ""
// sortIpAddressList() | N/A         |  false            |  false
// dnsResolveEx()      | N/A         |  ""               |  ""
// isInNetEx()         | N/A         |  false            |  false
// --------------------+-------------+-------------------+--------------
//
// TODO(eroman): The cell above reading ??? means I didn't test it.
//
// Another difference is in how dnsResolve() and myIpAddress() are
// implemented -- whether they should restrict to IPv4 results, or
// include both IPv4 and IPv6. The following table illustrates the
// differences:
//
// --------------------+-------------+-------------------+--------------
//                     | Firefox3    | InternetExplorer8 |  --> Us <---
// --------------------+-------------+-------------------+--------------
// myIpAddress()       | IPv4/IPv6   |  IPv4             |  IPv4
// dnsResolve()        | IPv4/IPv6   |  IPv4             |  IPv4
// isResolvable()      | IPv4/IPv6   |  IPv4             |  IPv4
// myIpAddressEx()     | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// dnsResolveEx()      | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// sortIpAddressList() | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// isResolvableEx()    | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// isInNetEx()         | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// -----------------+-------------+-------------------+--------------

namespace net {

namespace {

// Pseudo-name for the PAC script.
const char kPacResourceName[] = "proxy-pac-script.js";
// Pseudo-name for the PAC utility script.
const char kPacUtilityResourceName[] = "proxy-pac-utility-script.js";

// External string wrapper so V8 can access the UTF16 string wrapped by
// ProxyResolverScriptData.
class V8ExternalStringFromScriptData
    : public v8::String::ExternalStringResource {
 public:
  explicit V8ExternalStringFromScriptData(
      const scoped_refptr<ProxyResolverScriptData>& script_data)
      : script_data_(script_data) {}

  virtual const uint16_t* data() const OVERRIDE {
    return reinterpret_cast<const uint16*>(script_data_->utf16().data());
  }

  virtual size_t length() const OVERRIDE {
    return script_data_->utf16().size();
  }

 private:
  const scoped_refptr<ProxyResolverScriptData> script_data_;
  DISALLOW_COPY_AND_ASSIGN(V8ExternalStringFromScriptData);
};

// External string wrapper so V8 can access a string literal.
class V8ExternalASCIILiteral : public v8::String::ExternalAsciiStringResource {
 public:
  // |ascii| must be a NULL-terminated C string, and must remain valid
  // throughout this object's lifetime.
  V8ExternalASCIILiteral(const char* ascii, size_t length)
      : ascii_(ascii), length_(length) {
    DCHECK(base::IsStringASCII(ascii));
  }

  virtual const char* data() const OVERRIDE {
    return ascii_;
  }

  virtual size_t length() const OVERRIDE {
    return length_;
  }

 private:
  const char* ascii_;
  size_t length_;
  DISALLOW_COPY_AND_ASSIGN(V8ExternalASCIILiteral);
};

// When creating a v8::String from a C++ string we have two choices: create
// a copy, or create a wrapper that shares the same underlying storage.
// For small strings it is better to just make a copy, whereas for large
// strings there are savings by sharing the storage. This number identifies
// the cutoff length for when to start wrapping rather than creating copies.
const size_t kMaxStringBytesForCopy = 256;

// Converts a V8 String to a UTF8 std::string.
std::string V8StringToUTF8(v8::Handle<v8::String> s) {
  int len = s->Length();
  std::string result;
  if (len > 0)
    s->WriteUtf8(WriteInto(&result, len + 1));
  return result;
}

// Converts a V8 String to a UTF16 base::string16.
base::string16 V8StringToUTF16(v8::Handle<v8::String> s) {
  int len = s->Length();
  base::string16 result;
  // Note that the reinterpret cast is because on Windows string16 is an alias
  // to wstring, and hence has character type wchar_t not uint16_t.
  if (len > 0)
    s->Write(reinterpret_cast<uint16_t*>(WriteInto(&result, len + 1)), 0, len);
  return result;
}

// Converts an ASCII std::string to a V8 string.
v8::Local<v8::String> ASCIIStringToV8String(v8::Isolate* isolate,
                                            const std::string& s) {
  DCHECK(base::IsStringASCII(s));
  return v8::String::NewFromUtf8(isolate, s.data(), v8::String::kNormalString,
                                 s.size());
}

// Converts a UTF16 base::string16 (warpped by a ProxyResolverScriptData) to a
// V8 string.
v8::Local<v8::String> ScriptDataToV8String(
    v8::Isolate* isolate, const scoped_refptr<ProxyResolverScriptData>& s) {
  if (s->utf16().size() * 2 <= kMaxStringBytesForCopy) {
    return v8::String::NewFromTwoByte(
        isolate,
        reinterpret_cast<const uint16_t*>(s->utf16().data()),
        v8::String::kNormalString,
        s->utf16().size());
  }
  return v8::String::NewExternal(isolate,
                                 new V8ExternalStringFromScriptData(s));
}

// Converts an ASCII string literal to a V8 string.
v8::Local<v8::String> ASCIILiteralToV8String(v8::Isolate* isolate,
                                             const char* ascii) {
  DCHECK(base::IsStringASCII(ascii));
  size_t length = strlen(ascii);
  if (length <= kMaxStringBytesForCopy)
    return v8::String::NewFromUtf8(isolate, ascii, v8::String::kNormalString,
                                   length);
  return v8::String::NewExternal(isolate,
                                 new V8ExternalASCIILiteral(ascii, length));
}

// Stringizes a V8 object by calling its toString() method. Returns true
// on success. This may fail if the toString() throws an exception.
bool V8ObjectToUTF16String(v8::Handle<v8::Value> object,
                           base::string16* utf16_result,
                           v8::Isolate* isolate) {
  if (object.IsEmpty())
    return false;

  v8::HandleScope scope(isolate);
  v8::Local<v8::String> str_object = object->ToString();
  if (str_object.IsEmpty())
    return false;
  *utf16_result = V8StringToUTF16(str_object);
  return true;
}

// Extracts an hostname argument from |args|. On success returns true
// and fills |*hostname| with the result.
bool GetHostnameArgument(const v8::FunctionCallbackInfo<v8::Value>& args,
                         std::string* hostname) {
  // The first argument should be a string.
  if (args.Length() == 0 || args[0].IsEmpty() || !args[0]->IsString())
    return false;

  const base::string16 hostname_utf16 = V8StringToUTF16(args[0]->ToString());

  // If the hostname is already in ASCII, simply return it as is.
  if (base::IsStringASCII(hostname_utf16)) {
    *hostname = base::UTF16ToASCII(hostname_utf16);
    return true;
  }

  // Otherwise try to convert it from IDN to punycode.
  const int kInitialBufferSize = 256;
  url::RawCanonOutputT<base::char16, kInitialBufferSize> punycode_output;
  if (!url::IDNToASCII(hostname_utf16.data(), hostname_utf16.length(),
                       &punycode_output)) {
    return false;
  }

  // |punycode_output| should now be ASCII; convert it to a std::string.
  // (We could use UTF16ToASCII() instead, but that requires an extra string
  // copy. Since ASCII is a subset of UTF8 the following is equivalent).
  bool success = base::UTF16ToUTF8(punycode_output.data(),
                             punycode_output.length(),
                             hostname);
  DCHECK(success);
  DCHECK(base::IsStringASCII(*hostname));
  return success;
}

// Wrapper for passing around IP address strings and IPAddressNumber objects.
struct IPAddress {
  IPAddress(const std::string& ip_string, const IPAddressNumber& ip_number)
      : string_value(ip_string),
        ip_address_number(ip_number) {
  }

  // Used for sorting IP addresses in ascending order in SortIpAddressList().
  // IP6 addresses are placed ahead of IPv4 addresses.
  bool operator<(const IPAddress& rhs) const {
    const IPAddressNumber& ip1 = this->ip_address_number;
    const IPAddressNumber& ip2 = rhs.ip_address_number;
    if (ip1.size() != ip2.size())
      return ip1.size() > ip2.size();  // IPv6 before IPv4.
    DCHECK(ip1.size() == ip2.size());
    return memcmp(&ip1[0], &ip2[0], ip1.size()) < 0;  // Ascending order.
  }

  std::string string_value;
  IPAddressNumber ip_address_number;
};

// Handler for "sortIpAddressList(IpAddressList)". |ip_address_list| is a
// semi-colon delimited string containing IP addresses.
// |sorted_ip_address_list| is the resulting list of sorted semi-colon delimited
// IP addresses or an empty string if unable to sort the IP address list.
// Returns 'true' if the sorting was successful, and 'false' if the input was an
// empty string, a string of separators (";" in this case), or if any of the IP
// addresses in the input list failed to parse.
bool SortIpAddressList(const std::string& ip_address_list,
                       std::string* sorted_ip_address_list) {
  sorted_ip_address_list->clear();

  // Strip all whitespace (mimics IE behavior).
  std::string cleaned_ip_address_list;
  base::RemoveChars(ip_address_list, " \t", &cleaned_ip_address_list);
  if (cleaned_ip_address_list.empty())
    return false;

  // Split-up IP addresses and store them in a vector.
  std::vector<IPAddress> ip_vector;
  IPAddressNumber ip_num;
  base::StringTokenizer str_tok(cleaned_ip_address_list, ";");
  while (str_tok.GetNext()) {
    if (!ParseIPLiteralToNumber(str_tok.token(), &ip_num))
      return false;
    ip_vector.push_back(IPAddress(str_tok.token(), ip_num));
  }

  if (ip_vector.empty())  // Can happen if we have something like
    return false;         // sortIpAddressList(";") or sortIpAddressList("; ;")

  DCHECK(!ip_vector.empty());

  // Sort lists according to ascending numeric value.
  if (ip_vector.size() > 1)
    std::stable_sort(ip_vector.begin(), ip_vector.end());

  // Return a semi-colon delimited list of sorted addresses (IPv6 followed by
  // IPv4).
  for (size_t i = 0; i < ip_vector.size(); ++i) {
    if (i > 0)
      *sorted_ip_address_list += ";";
    *sorted_ip_address_list += ip_vector[i].string_value;
  }
  return true;
}

// Handler for "isInNetEx(ip_address, ip_prefix)". |ip_address| is a string
// containing an IPv4/IPv6 address, and |ip_prefix| is a string containg a
// slash-delimited IP prefix with the top 'n' bits specified in the bit
// field. This returns 'true' if the address is in the same subnet, and
// 'false' otherwise. Also returns 'false' if the prefix is in an incorrect
// format, or if an address and prefix of different types are used (e.g. IPv6
// address and IPv4 prefix).
bool IsInNetEx(const std::string& ip_address, const std::string& ip_prefix) {
  IPAddressNumber address;
  if (!ParseIPLiteralToNumber(ip_address, &address))
    return false;

  IPAddressNumber prefix;
  size_t prefix_length_in_bits;
  if (!ParseCIDRBlock(ip_prefix, &prefix, &prefix_length_in_bits))
    return false;

  // Both |address| and |prefix| must be of the same type (IPv4 or IPv6).
  if (address.size() != prefix.size())
    return false;

  DCHECK((address.size() == 4 && prefix.size() == 4) ||
         (address.size() == 16 && prefix.size() == 16));

  return IPNumberMatchesPrefix(address, prefix, prefix_length_in_bits);
}

}  // namespace

// ProxyResolverV8::Context ---------------------------------------------------

class ProxyResolverV8::Context {
 public:
  Context(ProxyResolverV8* parent, v8::Isolate* isolate)
      : parent_(parent),
        isolate_(isolate) {
    DCHECK(isolate);
  }

  ~Context() {
    v8::Locker locked(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);

    v8_this_.Reset();
    v8_context_.Reset();
  }

  JSBindings* js_bindings() {
    return parent_->js_bindings_;
  }

  int ResolveProxy(const GURL& query_url, ProxyInfo* results) {
    v8::Locker locked(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope scope(isolate_);

    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, v8_context_);
    v8::Context::Scope function_scope(context);

    v8::Local<v8::Value> function;
    if (!GetFindProxyForURL(&function)) {
      js_bindings()->OnError(
          -1, base::ASCIIToUTF16("FindProxyForURL() is undefined."));
      return ERR_PAC_SCRIPT_FAILED;
    }

    v8::Handle<v8::Value> argv[] = {
      ASCIIStringToV8String(isolate_, query_url.spec()),
      ASCIIStringToV8String(isolate_, query_url.HostNoBrackets()),
    };

    v8::TryCatch try_catch;
    v8::Local<v8::Value> ret = v8::Function::Cast(*function)->Call(
        context->Global(), arraysize(argv), argv);

    if (try_catch.HasCaught()) {
      HandleError(try_catch.Message());
      return ERR_PAC_SCRIPT_FAILED;
    }

    if (!ret->IsString()) {
      js_bindings()->OnError(
          -1, base::ASCIIToUTF16("FindProxyForURL() did not return a string."));
      return ERR_PAC_SCRIPT_FAILED;
    }

    base::string16 ret_str = V8StringToUTF16(ret->ToString());

    if (!base::IsStringASCII(ret_str)) {
      // TODO(eroman): Rather than failing when a wide string is returned, we
      //               could extend the parsing to handle IDNA hostnames by
      //               converting them to ASCII punycode.
      //               crbug.com/47234
      base::string16 error_message =
          base::ASCIIToUTF16("FindProxyForURL() returned a non-ASCII string "
                             "(crbug.com/47234): ") + ret_str;
      js_bindings()->OnError(-1, error_message);
      return ERR_PAC_SCRIPT_FAILED;
    }

    results->UsePacString(base::UTF16ToASCII(ret_str));
    return OK;
  }

  int InitV8(const scoped_refptr<ProxyResolverScriptData>& pac_script) {
    v8::Locker locked(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope scope(isolate_);

    v8_this_.Reset(isolate_, v8::External::New(isolate_, this));
    v8::Local<v8::External> v8_this =
        v8::Local<v8::External>::New(isolate_, v8_this_);
    v8::Local<v8::ObjectTemplate> global_template =
        v8::ObjectTemplate::New(isolate_);

    // Attach the javascript bindings.
    v8::Local<v8::FunctionTemplate> alert_template =
        v8::FunctionTemplate::New(isolate_, &AlertCallback, v8_this);
    global_template->Set(ASCIILiteralToV8String(isolate_, "alert"),
                         alert_template);

    v8::Local<v8::FunctionTemplate> my_ip_address_template =
        v8::FunctionTemplate::New(isolate_, &MyIpAddressCallback, v8_this);
    global_template->Set(ASCIILiteralToV8String(isolate_, "myIpAddress"),
                         my_ip_address_template);

    v8::Local<v8::FunctionTemplate> dns_resolve_template =
        v8::FunctionTemplate::New(isolate_, &DnsResolveCallback, v8_this);
    global_template->Set(ASCIILiteralToV8String(isolate_, "dnsResolve"),
                         dns_resolve_template);

    // Microsoft's PAC extensions:

    v8::Local<v8::FunctionTemplate> dns_resolve_ex_template =
        v8::FunctionTemplate::New(isolate_, &DnsResolveExCallback, v8_this);
    global_template->Set(ASCIILiteralToV8String(isolate_, "dnsResolveEx"),
                         dns_resolve_ex_template);

    v8::Local<v8::FunctionTemplate> my_ip_address_ex_template =
        v8::FunctionTemplate::New(isolate_, &MyIpAddressExCallback, v8_this);
    global_template->Set(ASCIILiteralToV8String(isolate_, "myIpAddressEx"),
                         my_ip_address_ex_template);

    v8::Local<v8::FunctionTemplate> sort_ip_address_list_template =
        v8::FunctionTemplate::New(isolate_,
                                  &SortIpAddressListCallback,
                                  v8_this);
    global_template->Set(ASCIILiteralToV8String(isolate_, "sortIpAddressList"),
                         sort_ip_address_list_template);

    v8::Local<v8::FunctionTemplate> is_in_net_ex_template =
        v8::FunctionTemplate::New(isolate_, &IsInNetExCallback, v8_this);
    global_template->Set(ASCIILiteralToV8String(isolate_, "isInNetEx"),
                         is_in_net_ex_template);

    v8_context_.Reset(
        isolate_, v8::Context::New(isolate_, NULL, global_template));

    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, v8_context_);
    v8::Context::Scope ctx(context);

    // Add the PAC utility functions to the environment.
    // (This script should never fail, as it is a string literal!)
    // Note that the two string literals are concatenated.
    int rv = RunScript(
        ASCIILiteralToV8String(
            isolate_,
            PROXY_RESOLVER_SCRIPT
            PROXY_RESOLVER_SCRIPT_EX),
        kPacUtilityResourceName);
    if (rv != OK) {
      NOTREACHED();
      return rv;
    }

    // Add the user's PAC code to the environment.
    rv =
        RunScript(ScriptDataToV8String(isolate_, pac_script), kPacResourceName);
    if (rv != OK)
      return rv;

    // At a minimum, the FindProxyForURL() function must be defined for this
    // to be a legitimiate PAC script.
    v8::Local<v8::Value> function;
    if (!GetFindProxyForURL(&function)) {
      js_bindings()->OnError(
          -1, base::ASCIIToUTF16("FindProxyForURL() is undefined."));
      return ERR_PAC_SCRIPT_FAILED;
    }

    return OK;
  }

 private:
  bool GetFindProxyForURL(v8::Local<v8::Value>* function) {
    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, v8_context_);
    *function =
        context->Global()->Get(
            ASCIILiteralToV8String(isolate_, "FindProxyForURL"));
    return (*function)->IsFunction();
  }

  // Handle an exception thrown by V8.
  void HandleError(v8::Handle<v8::Message> message) {
    base::string16 error_message;
    int line_number = -1;

    if (!message.IsEmpty()) {
      line_number = message->GetLineNumber();
      V8ObjectToUTF16String(message->Get(), &error_message, isolate_);
    }

    js_bindings()->OnError(line_number, error_message);
  }

  // Compiles and runs |script| in the current V8 context.
  // Returns OK on success, otherwise an error code.
  int RunScript(v8::Handle<v8::String> script, const char* script_name) {
    v8::TryCatch try_catch;

    // Compile the script.
    v8::ScriptOrigin origin =
        v8::ScriptOrigin(ASCIILiteralToV8String(isolate_, script_name));
    v8::Local<v8::Script> code = v8::Script::Compile(script, &origin);

    // Execute.
    if (!code.IsEmpty())
      code->Run();

    // Check for errors.
    if (try_catch.HasCaught()) {
      HandleError(try_catch.Message());
      return ERR_PAC_SCRIPT_FAILED;
    }

    return OK;
  }

  // V8 callback for when "alert()" is invoked by the PAC script.
  static void AlertCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    // Like firefox we assume "undefined" if no argument was specified, and
    // disregard any arguments beyond the first.
    base::string16 message;
    if (args.Length() == 0) {
      message = base::ASCIIToUTF16("undefined");
    } else {
      if (!V8ObjectToUTF16String(args[0], &message, args.GetIsolate()))
        return;  // toString() threw an exception.
    }

    context->js_bindings()->Alert(message);
  }

  // V8 callback for when "myIpAddress()" is invoked by the PAC script.
  static void MyIpAddressCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    DnsResolveCallbackHelper(args, JSBindings::MY_IP_ADDRESS);
  }

  // V8 callback for when "myIpAddressEx()" is invoked by the PAC script.
  static void MyIpAddressExCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    DnsResolveCallbackHelper(args, JSBindings::MY_IP_ADDRESS_EX);
  }

  // V8 callback for when "dnsResolve()" is invoked by the PAC script.
  static void DnsResolveCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    DnsResolveCallbackHelper(args, JSBindings::DNS_RESOLVE);
  }

  // V8 callback for when "dnsResolveEx()" is invoked by the PAC script.
  static void DnsResolveExCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    DnsResolveCallbackHelper(args, JSBindings::DNS_RESOLVE_EX);
  }

  // Shared code for implementing:
  //   - myIpAddress(), myIpAddressEx(), dnsResolve(), dnsResolveEx().
  static void DnsResolveCallbackHelper(
      const v8::FunctionCallbackInfo<v8::Value>& args,
      JSBindings::ResolveDnsOperation op) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    std::string hostname;

    // dnsResolve() and dnsResolveEx() need at least 1 argument.
    if (op == JSBindings::DNS_RESOLVE || op == JSBindings::DNS_RESOLVE_EX) {
      if (!GetHostnameArgument(args, &hostname)) {
        if (op == JSBindings::DNS_RESOLVE)
          args.GetReturnValue().SetNull();
        return;
      }
    }

    std::string result;
    bool success;
    bool terminate = false;

    {
      v8::Unlocker unlocker(args.GetIsolate());
      success = context->js_bindings()->ResolveDns(
          hostname, op, &result, &terminate);
    }

    if (terminate)
      v8::V8::TerminateExecution(args.GetIsolate());

    if (success) {
      args.GetReturnValue().Set(
          ASCIIStringToV8String(args.GetIsolate(), result));
      return;
    }

    // Each function handles resolution errors differently.
    switch (op) {
      case JSBindings::DNS_RESOLVE:
        args.GetReturnValue().SetNull();
        return;
      case JSBindings::DNS_RESOLVE_EX:
        args.GetReturnValue().SetEmptyString();
        return;
      case JSBindings::MY_IP_ADDRESS:
        args.GetReturnValue().Set(
            ASCIILiteralToV8String(args.GetIsolate(), "127.0.0.1"));
        return;
      case JSBindings::MY_IP_ADDRESS_EX:
        args.GetReturnValue().SetEmptyString();
        return;
    }

    NOTREACHED();
  }

  // V8 callback for when "sortIpAddressList()" is invoked by the PAC script.
  static void SortIpAddressListCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    // We need at least one string argument.
    if (args.Length() == 0 || args[0].IsEmpty() || !args[0]->IsString()) {
      args.GetReturnValue().SetNull();
      return;
    }

    std::string ip_address_list = V8StringToUTF8(args[0]->ToString());
    if (!base::IsStringASCII(ip_address_list)) {
      args.GetReturnValue().SetNull();
      return;
    }
    std::string sorted_ip_address_list;
    bool success = SortIpAddressList(ip_address_list, &sorted_ip_address_list);
    if (!success) {
      args.GetReturnValue().Set(false);
      return;
    }
    args.GetReturnValue().Set(
        ASCIIStringToV8String(args.GetIsolate(), sorted_ip_address_list));
  }

  // V8 callback for when "isInNetEx()" is invoked by the PAC script.
  static void IsInNetExCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    // We need at least 2 string arguments.
    if (args.Length() < 2 || args[0].IsEmpty() || !args[0]->IsString() ||
        args[1].IsEmpty() || !args[1]->IsString()) {
      args.GetReturnValue().SetNull();
      return;
    }

    std::string ip_address = V8StringToUTF8(args[0]->ToString());
    if (!base::IsStringASCII(ip_address)) {
      args.GetReturnValue().Set(false);
      return;
    }
    std::string ip_prefix = V8StringToUTF8(args[1]->ToString());
    if (!base::IsStringASCII(ip_prefix)) {
      args.GetReturnValue().Set(false);
      return;
    }
    args.GetReturnValue().Set(IsInNetEx(ip_address, ip_prefix));
  }

  mutable base::Lock lock_;
  ProxyResolverV8* parent_;
  v8::Isolate* isolate_;
  v8::Persistent<v8::External> v8_this_;
  v8::Persistent<v8::Context> v8_context_;
};

// ProxyResolverV8 ------------------------------------------------------------

ProxyResolverV8::ProxyResolverV8()
    : ProxyResolver(true /*expects_pac_bytes*/),
      js_bindings_(NULL) {
}

ProxyResolverV8::~ProxyResolverV8() {}

int ProxyResolverV8::GetProxyForURL(
    const GURL& query_url, ProxyInfo* results,
    const CompletionCallback& /*callback*/,
    RequestHandle* /*request*/,
    const BoundNetLog& net_log) {
  DCHECK(js_bindings_);

  // If the V8 instance has not been initialized (either because
  // SetPacScript() wasn't called yet, or because it failed.
  if (!context_)
    return ERR_FAILED;

  // Otherwise call into V8.
  int rv = context_->ResolveProxy(query_url, results);

  return rv;
}

void ProxyResolverV8::CancelRequest(RequestHandle request) {
  // This is a synchronous ProxyResolver; no possibility for async requests.
  NOTREACHED();
}

LoadState ProxyResolverV8::GetLoadState(RequestHandle request) const {
  NOTREACHED();
  return LOAD_STATE_IDLE;
}

void ProxyResolverV8::CancelSetPacScript() {
  NOTREACHED();
}

int ProxyResolverV8::SetPacScript(
    const scoped_refptr<ProxyResolverScriptData>& script_data,
    const CompletionCallback& /*callback*/) {
  DCHECK(script_data.get());
  DCHECK(js_bindings_);

  context_.reset();
  if (script_data->utf16().empty())
    return ERR_PAC_SCRIPT_FAILED;

  // Try parsing the PAC script.
  scoped_ptr<Context> context(new Context(this, GetDefaultIsolate()));
  int rv = context->InitV8(script_data);
  if (rv == OK)
    context_.reset(context.release());
  return rv;
}

// static
void ProxyResolverV8::EnsureIsolateCreated() {
  if (g_proxy_resolver_isolate_)
    return;
  g_proxy_resolver_isolate_ =
      new gin::IsolateHolder(gin::IsolateHolder::kNonStrictMode);
  ANNOTATE_LEAKING_OBJECT_PTR(g_proxy_resolver_isolate_);
}

// static
v8::Isolate* ProxyResolverV8::GetDefaultIsolate() {
  DCHECK(g_proxy_resolver_isolate_)
      << "Must call ProxyResolverV8::EnsureIsolateCreated() first";
  return g_proxy_resolver_isolate_->isolate();
}

gin::IsolateHolder* ProxyResolverV8::g_proxy_resolver_isolate_ = NULL;

// static
size_t ProxyResolverV8::GetTotalHeapSize() {
  if (!g_proxy_resolver_isolate_)
    return 0;

  v8::Locker locked(g_proxy_resolver_isolate_->isolate());
  v8::Isolate::Scope isolate_scope(g_proxy_resolver_isolate_->isolate());
  v8::HeapStatistics heap_statistics;
  g_proxy_resolver_isolate_->isolate()->GetHeapStatistics(&heap_statistics);
  return heap_statistics.total_heap_size();
}

// static
size_t ProxyResolverV8::GetUsedHeapSize() {
  if (!g_proxy_resolver_isolate_)
    return 0;

  v8::Locker locked(g_proxy_resolver_isolate_->isolate());
  v8::Isolate::Scope isolate_scope(g_proxy_resolver_isolate_->isolate());
  v8::HeapStatistics heap_statistics;
  g_proxy_resolver_isolate_->isolate()->GetHeapStatistics(&heap_statistics);
  return heap_statistics.used_heap_size();
}

}  // namespace net
