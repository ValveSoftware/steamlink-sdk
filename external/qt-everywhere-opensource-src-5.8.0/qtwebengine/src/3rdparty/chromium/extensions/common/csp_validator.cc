// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/csp_validator.h"

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace extensions {

namespace csp_validator {

namespace {

const char kDefaultSrc[] = "default-src";
const char kScriptSrc[] = "script-src";
const char kObjectSrc[] = "object-src";
const char kPluginTypes[] = "plugin-types";

const char kObjectSrcDefaultDirective[] = "object-src 'self';";
const char kScriptSrcDefaultDirective[] =
    "script-src 'self' chrome-extension-resource:;";

const char kSandboxDirectiveName[] = "sandbox";
const char kAllowSameOriginToken[] = "allow-same-origin";
const char kAllowTopNavigation[] = "allow-top-navigation";

// This is the list of plugin types which are fully sandboxed and are safe to
// load up in an extension, regardless of the URL they are navigated to.
const char* const kSandboxedPluginTypes[] = {
  "application/pdf",
  "application/x-google-chrome-pdf",
  "application/x-pnacl"
};

// List of CSP hash-source prefixes that are accepted. Blink is a bit more
// lenient, but we only accept standard hashes to be forward-compatible.
// http://www.w3.org/TR/2015/CR-CSP2-20150721/#hash_algo
const char* const kHashSourcePrefixes[] = {
  "'sha256-",
  "'sha384-",
  "'sha512-"
};

struct DirectiveStatus {
  explicit DirectiveStatus(const char* name)
      : directive_name(name), seen_in_policy(false) {}

  const char* directive_name;
  bool seen_in_policy;
};

// Returns whether |url| starts with |scheme_and_separator| and does not have a
// too permissive wildcard host name. If |should_check_rcd| is true, then the
// Public suffix list is used to exclude wildcard TLDs such as "https://*.org".
bool isNonWildcardTLD(const std::string& url,
                      const std::string& scheme_and_separator,
                      bool should_check_rcd) {
  if (!base::StartsWith(url, scheme_and_separator,
                        base::CompareCase::SENSITIVE))
    return false;

  size_t start_of_host = scheme_and_separator.length();

  size_t end_of_host = url.find("/", start_of_host);
  if (end_of_host == std::string::npos)
    end_of_host = url.size();

  // Note: It is sufficient to only compare the first character against '*'
  // because the CSP only allows wildcards at the start of a directive, see
  // host-source and host-part at http://www.w3.org/TR/CSP2/#source-list-syntax
  bool is_wildcard_subdomain = end_of_host > start_of_host + 2 &&
      url[start_of_host] == '*' && url[start_of_host + 1] == '.';
  if (is_wildcard_subdomain)
    start_of_host += 2;

  size_t start_of_port = url.rfind(":", end_of_host);
  // The ":" check at the end of the following condition is used to avoid
  // treating the last part of an IPv6 address as a port.
  if (start_of_port > start_of_host && url[start_of_port - 1] != ':') {
    bool is_valid_port = false;
    // Do a quick sanity check. The following check could mistakenly flag
    // ":123456" or ":****" as valid, but that does not matter because the
    // relaxing CSP directive will just be ignored by Blink.
    for (size_t i = start_of_port + 1; i < end_of_host; ++i) {
      is_valid_port = base::IsAsciiDigit(url[i]) || url[i] == '*';
      if (!is_valid_port)
        break;
    }
    if (is_valid_port)
      end_of_host = start_of_port;
  }

  std::string host(url, start_of_host, end_of_host - start_of_host);
  // Global wildcards are not allowed.
  if (host.empty() || host.find("*") != std::string::npos)
    return false;

  if (!is_wildcard_subdomain || !should_check_rcd)
    return true;

  // Allow *.googleapis.com to be whitelisted for backwards-compatibility.
  // (crbug.com/409952)
  if (host == "googleapis.com")
    return true;

  // Wildcards on subdomains of a TLD are not allowed.
  size_t registry_length = net::registry_controlled_domains::GetRegistryLength(
      host,
      net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return registry_length != 0;
}

// Checks whether the source is a syntactically valid hash.
bool IsHashSource(const std::string& source) {
  size_t hash_end = source.length() - 1;
  if (source.empty() || source[hash_end] != '\'') {
    return false;
  }

  for (const char* prefix : kHashSourcePrefixes) {
    if (base::StartsWith(source, prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      for (size_t i = strlen(prefix); i < hash_end; ++i) {
        const char c = source[i];
        // The hash must be base64-encoded. Do not allow any other characters.
        if (!base::IsAsciiAlpha(c) && !base::IsAsciiDigit(c) && c != '+' &&
            c != '/' && c != '=') {
          return false;
        }
      }
      return true;
    }
  }
  return false;
}

InstallWarning CSPInstallWarning(const std::string& csp_warning) {
  return InstallWarning(csp_warning, manifest_keys::kContentSecurityPolicy);
}

void GetSecureDirectiveValues(const std::string& directive_name,
                              base::StringTokenizer* tokenizer,
                              int options,
                              std::vector<std::string>* sane_csp_parts,
                              std::vector<InstallWarning>* warnings) {
  sane_csp_parts->push_back(directive_name);
  while (tokenizer->GetNext()) {
    std::string source_literal = tokenizer->token();
    std::string source_lower = base::ToLowerASCII(source_literal);
    bool is_secure_csp_token = false;

    // We might need to relax this whitelist over time.
    if (source_lower == "'self'" || source_lower == "'none'" ||
        source_lower == "http://127.0.0.1" || source_lower == "blob:" ||
        source_lower == "filesystem:" || source_lower == "http://localhost" ||
        base::StartsWith(source_lower, "http://127.0.0.1:",
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(source_lower, "http://localhost:",
                         base::CompareCase::SENSITIVE) ||
        isNonWildcardTLD(source_lower, "https://", true) ||
        isNonWildcardTLD(source_lower, "chrome://", false) ||
        isNonWildcardTLD(source_lower,
                         std::string(extensions::kExtensionScheme) +
                             url::kStandardSchemeSeparator,
                         false) ||
        IsHashSource(source_literal) ||
        base::StartsWith(source_lower, "chrome-extension-resource:",
                         base::CompareCase::SENSITIVE)) {
      is_secure_csp_token = true;
    } else if ((options & OPTIONS_ALLOW_UNSAFE_EVAL) &&
               source_lower == "'unsafe-eval'") {
      is_secure_csp_token = true;
    }

    if (is_secure_csp_token) {
      sane_csp_parts->push_back(source_literal);
    } else if (warnings) {
      warnings->push_back(CSPInstallWarning(ErrorUtils::FormatErrorMessage(
          manifest_errors::kInvalidCSPInsecureValue, source_literal,
          directive_name)));
    }
  }
  // End of CSP directive that was started at the beginning of this method. If
  // none of the values are secure, the policy will be empty and default to
  // 'none', which is secure.
  sane_csp_parts->back().push_back(';');
}

// Returns true if |directive_name| matches |status.directive_name|.
bool UpdateStatus(const std::string& directive_name,
                  base::StringTokenizer* tokenizer,
                  DirectiveStatus* status,
                  int options,
                  std::vector<std::string>* sane_csp_parts,
                  std::vector<InstallWarning>* warnings) {
  if (directive_name != status->directive_name)
    return false;

  if (!status->seen_in_policy) {
    status->seen_in_policy = true;
    GetSecureDirectiveValues(directive_name, tokenizer, options, sane_csp_parts,
                             warnings);
  } else {
    // Don't show any errors for duplicate CSP directives, because it will be
    // ignored by the CSP parser (http://www.w3.org/TR/CSP2/#policy-parsing).
    GetSecureDirectiveValues(directive_name, tokenizer, options, sane_csp_parts,
                             NULL);
  }
  return true;
}

// Returns true if the |plugin_type| is one of the fully sandboxed plugin types.
bool PluginTypeAllowed(const std::string& plugin_type) {
  for (size_t i = 0; i < arraysize(kSandboxedPluginTypes); ++i) {
    if (plugin_type == kSandboxedPluginTypes[i])
      return true;
  }
  return false;
}

// Returns true if the policy is allowed to contain an insecure object-src
// directive. This requires OPTIONS_ALLOW_INSECURE_OBJECT_SRC to be specified
// as an option and the plugin-types that can be loaded must be restricted to
// the set specified in kSandboxedPluginTypes.
bool AllowedToHaveInsecureObjectSrc(
    int options,
    const std::vector<std::string>& directives) {
  if (!(options & OPTIONS_ALLOW_INSECURE_OBJECT_SRC))
    return false;

  for (size_t i = 0; i < directives.size(); ++i) {
    const std::string& input = directives[i];
    base::StringTokenizer tokenizer(input, " \t\r\n");
    if (!tokenizer.GetNext())
      continue;
    if (!base::LowerCaseEqualsASCII(tokenizer.token(), kPluginTypes))
      continue;
    while (tokenizer.GetNext()) {
      if (!PluginTypeAllowed(tokenizer.token()))
        return false;
    }
    // All listed plugin types are whitelisted.
    return true;
  }
  // plugin-types not specified.
  return false;
}

}  //  namespace

bool ContentSecurityPolicyIsLegal(const std::string& policy) {
  // We block these characters to prevent HTTP header injection when
  // representing the content security policy as an HTTP header.
  const char kBadChars[] = {',', '\r', '\n', '\0'};

  return policy.find_first_of(kBadChars, 0, arraysize(kBadChars)) ==
      std::string::npos;
}

std::string SanitizeContentSecurityPolicy(
    const std::string& policy,
    int options,
    std::vector<InstallWarning>* warnings) {
  // See http://www.w3.org/TR/CSP/#parse-a-csp-policy for parsing algorithm.
  std::vector<std::string> directives = base::SplitString(
      policy, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  DirectiveStatus default_src_status(kDefaultSrc);
  DirectiveStatus script_src_status(kScriptSrc);
  DirectiveStatus object_src_status(kObjectSrc);

  bool allow_insecure_object_src =
      AllowedToHaveInsecureObjectSrc(options, directives);

  std::vector<std::string> sane_csp_parts;
  std::vector<InstallWarning> default_src_csp_warnings;
  for (size_t i = 0; i < directives.size(); ++i) {
    std::string& input = directives[i];
    base::StringTokenizer tokenizer(input, " \t\r\n");
    if (!tokenizer.GetNext())
      continue;

    std::string directive_name = base::ToLowerASCII(tokenizer.token_piece());
    if (UpdateStatus(directive_name, &tokenizer, &default_src_status, options,
                     &sane_csp_parts, &default_src_csp_warnings))
      continue;
    if (UpdateStatus(directive_name, &tokenizer, &script_src_status, options,
                     &sane_csp_parts, warnings))
      continue;
    if (!allow_insecure_object_src &&
        UpdateStatus(directive_name, &tokenizer, &object_src_status, options,
                     &sane_csp_parts, warnings))
      continue;

    // Pass the other CSP directives as-is without further validation.
    sane_csp_parts.push_back(input + ";");
  }

  if (default_src_status.seen_in_policy) {
    if (!script_src_status.seen_in_policy ||
        !object_src_status.seen_in_policy) {
      // Insecure values in default-src are only relevant if either script-src
      // or object-src is omitted.
      if (warnings)
        warnings->insert(warnings->end(),
                         default_src_csp_warnings.begin(),
                         default_src_csp_warnings.end());
    }
  } else {
    if (!script_src_status.seen_in_policy) {
      sane_csp_parts.push_back(kScriptSrcDefaultDirective);
      if (warnings)
        warnings->push_back(CSPInstallWarning(ErrorUtils::FormatErrorMessage(
            manifest_errors::kInvalidCSPMissingSecureSrc, kScriptSrc)));
    }
    if (!object_src_status.seen_in_policy && !allow_insecure_object_src) {
      sane_csp_parts.push_back(kObjectSrcDefaultDirective);
      if (warnings)
        warnings->push_back(CSPInstallWarning(ErrorUtils::FormatErrorMessage(
            manifest_errors::kInvalidCSPMissingSecureSrc, kObjectSrc)));
    }
  }

  return base::JoinString(sane_csp_parts, " ");
}

bool ContentSecurityPolicyIsSandboxed(
    const std::string& policy, Manifest::Type type) {
  // See http://www.w3.org/TR/CSP/#parse-a-csp-policy for parsing algorithm.
  bool seen_sandbox = false;
  for (const std::string& input : base::SplitString(
           policy, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    base::StringTokenizer tokenizer(input, " \t\r\n");
    if (!tokenizer.GetNext())
      continue;

    std::string directive_name = base::ToLowerASCII(tokenizer.token_piece());
    if (directive_name != kSandboxDirectiveName)
      continue;

    seen_sandbox = true;

    while (tokenizer.GetNext()) {
      std::string token = base::ToLowerASCII(tokenizer.token_piece());

      // The same origin token negates the sandboxing.
      if (token == kAllowSameOriginToken)
        return false;

      // Platform apps don't allow navigation.
      if (type == Manifest::TYPE_PLATFORM_APP) {
        if (token == kAllowTopNavigation)
          return false;
      }
    }
  }

  return seen_sandbox;
}

}  // namespace csp_validator

}  // namespace extensions
