// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <string.h>

#include <vector>

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using base::WideToUTF16;

namespace net {

namespace {

static const size_t kNpos = base::string16::npos;

const char* kLanguages[] = {
  "",      "en",    "zh-CN",    "ja",    "ko",
  "he",    "ar",    "ru",       "el",    "fr",
  "de",    "pt",    "sv",       "th",    "hi",
  "de,en", "el,en", "zh-TW,en", "ko,ja", "he,ru,en",
  "zh,ru,en"
};

struct IDNTestCase {
  const char* input;
  const wchar_t* unicode_output;
  const bool unicode_allowed[arraysize(kLanguages)];
};

// TODO(jungshik) This is just a random sample of languages and is far
// from exhaustive.  We may have to generate all the combinations
// of languages (powerset of a set of all the languages).
const IDNTestCase idn_cases[] = {
  // No IDN
  {"www.google.com", L"www.google.com",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  {"www.google.com.", L"www.google.com.",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  {".", L".",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  {"", L"",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  // IDN
  // Hanzi (Traditional Chinese)
  {"xn--1lq90ic7f1rc.cn", L"\x5317\x4eac\x5927\x5b78.cn",
   {true,  false, true,  true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, true,  true,  false,
    true}},
  // Hanzi ('video' in Simplified Chinese : will pass only in zh-CN,zh)
  {"xn--cy2a840a.com", L"\x89c6\x9891.com",
   {true,  false, true,  false,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false,  false,
    true}},
  // Hanzi + '123'
  {"www.xn--123-p18d.com", L"www.\x4e00" L"123.com",
   {true,  false, true,  true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, true,  true,  false,
    true}},
  // Hanzi + Latin : U+56FD is simplified and is regarded
  // as not supported in zh-TW.
  {"www.xn--hello-9n1hm04c.com", L"www.hello\x4e2d\x56fd.com",
   {false, false, true,  true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    true}},
  // Kanji + Kana (Japanese)
  {"xn--l8jvb1ey91xtjb.jp", L"\x671d\x65e5\x3042\x3055\x3072.jp",
   {true,  false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    false}},
  // Katakana including U+30FC
  {"xn--tckm4i2e.jp", L"\x30b3\x30de\x30fc\x30b9.jp",
   {true, false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true, false,
    }},
  {"xn--3ck7a7g.jp", L"\u30ce\u30f3\u30bd.jp",
   {true, false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true, false,
    }},
  // Katakana + Latin (Japanese)
  // TODO(jungshik): Change 'false' in the first element to 'true'
  // after upgrading to ICU 4.2.1 to use new uspoof_* APIs instead
  // of our IsIDNComponentInSingleScript().
  {"xn--e-efusa1mzf.jp", L"e\x30b3\x30de\x30fc\x30b9.jp",
   {false, false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true, false,
    }},
  {"xn--3bkxe.jp", L"\x30c8\x309a.jp",
   {false, false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true, false,
    }},
  // Hangul (Korean)
  {"www.xn--or3b17p6jjc.kr", L"www.\xc804\xc790\xc815\xbd80.kr",
   {true,  false, false, false, true,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    false}},
  // b<u-umlaut>cher (German)
  {"xn--bcher-kva.de", L"b\x00fc" L"cher.de",
   {true,  false, false, false, false,
    false, false, false, false, true,
    true,  false,  false, false, false,
    true,  false, false, false, false,
    false}},
  // a with diaeresis
  {"www.xn--frgbolaget-q5a.se", L"www.f\x00e4rgbolaget.se",
   {true,  false, false, false, false,
    false, false, false, false, false,
    true,  false, true, false, false,
    true,  false, false, false, false,
    false}},
  // c-cedilla (French)
  {"www.xn--alliancefranaise-npb.fr", L"www.alliancefran\x00e7" L"aise.fr",
   {true,  false, false, false, false,
    false, false, false, false, true,
    false, true,  false, false, false,
    false, false, false, false, false,
    false}},
  // caf'e with acute accent' (French)
  {"xn--caf-dma.fr", L"caf\x00e9.fr",
   {true,  false, false, false, false,
    false, false, false, false, true,
    false, true,  true,  false, false,
    false, false, false, false, false,
    false}},
  // c-cedillla and a with tilde (Portuguese)
  {"xn--poema-9qae5a.com.br", L"p\x00e3oema\x00e7\x00e3.com.br",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, true,  false, false, false,
    false, false, false, false, false,
    false}},
  // s with caron
  {"xn--achy-f6a.com", L"\x0161" L"achy.com",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // TODO(jungshik) : Add examples with Cyrillic letters
  // only used in some languages written in Cyrillic.
  // Eutopia (Greek)
  {"xn--kxae4bafwg.gr", L"\x03bf\x03c5\x03c4\x03bf\x03c0\x03af\x03b1.gr",
   {true,  false, false, false, false,
    false, false, false, true,  false,
    false, false, false, false, false,
    false, true,  false, false, false,
    false}},
  // Eutopia + 123 (Greek)
  {"xn---123-pldm0haj2bk.gr",
   L"\x03bf\x03c5\x03c4\x03bf\x03c0\x03af\x03b1-123.gr",
   {true,  false, false, false, false,
    false, false, false, true,  false,
    false, false, false, false, false,
    false, true,  false, false, false,
    false}},
  // Cyrillic (Russian)
  {"xn--n1aeec9b.ru", L"\x0442\x043e\x0440\x0442\x044b.ru",
   {true,  false, false, false, false,
    false, false, true,  false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    true}},
  // Cyrillic + 123 (Russian)
  {"xn---123-45dmmc5f.ru", L"\x0442\x043e\x0440\x0442\x044b-123.ru",
   {true,  false, false, false, false,
    false, false, true,  false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    true}},
  // Arabic
  {"xn--mgba1fmg.ar", L"\x0627\x0641\x0644\x0627\x0645.ar",
   {true,  false, false, false, false,
    false, true,  false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // Hebrew
  {"xn--4dbib.he", L"\x05d5\x05d0\x05d4.he",
   {true,  false, false, false, false,
    true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    false}},
  // Thai
  {"xn--12c2cc4ag3b4ccu.th",
   L"\x0e2a\x0e32\x0e22\x0e01\x0e32\x0e23\x0e1a\x0e34\x0e19.th",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    false, false, false, false, false,
    false}},
  // Devangari (Hindi)
  {"www.xn--l1b6a9e1b7c.in", L"www.\x0905\x0915\x094b\x0932\x093e.in",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    false, false, false, false, false,
    false}},
  // Invalid IDN
  {"xn--hello?world.com", NULL,
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // Unsafe IDNs
  // "payp<alpha>l.com"
  {"www.xn--paypl-g9d.com", L"payp\x03b1l.com",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // google.gr with Greek omicron and epsilon
  {"xn--ggl-6xc1ca.gr", L"g\x03bf\x03bfgl\x03b5.gr",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // google.ru with Cyrillic o
  {"xn--ggl-tdd6ba.ru", L"g\x043e\x043egl\x0435.ru",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // h<e with acute>llo<China in Han>.cn
  {"xn--hllo-bpa7979ih5m.cn", L"h\x00e9llo\x4e2d\x56fd.cn",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // <Greek rho><Cyrillic a><Cyrillic u>.ru
  {"xn--2xa6t2b.ru", L"\x03c1\x0430\x0443.ru",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // One that's really long that will force a buffer realloc
  {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
       "aaaaaaa",
   L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
       L"aaaaaaaa",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  // Test cases for characters we blacklisted although allowed in IDN.
  // Embedded spaces will be turned to %20 in the display.
  // TODO(jungshik): We need to have more cases. This is a typical
  // data-driven trap. The following test cases need to be separated
  // and tested only for a couple of languages.
  {"xn--osd3820f24c.kr", L"\xac00\xb098\x115f.kr",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false}},
  {"www.xn--google-ho0coa.com", L"www.\x2039google\x203a.com",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
  {"google.xn--comabc-k8d", L"google.com\x0338" L"abc",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
  {"google.xn--com-oh4ba.evil.jp", L"google.com\x309a\x309a.evil.jp",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
  {"google.xn--comevil-v04f.jp", L"google.com\x30ce" L"evil.jp",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
#if 0
  // These two cases are special. We need a separate test.
  // U+3000 and U+3002 are normalized to ASCII space and dot.
  {"xn-- -kq6ay5z.cn", L"\x4e2d\x56fd\x3000.cn",
    {false, false, true,  false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, true,  false, false,
     true}},
  {"xn--fiqs8s.cn", L"\x4e2d\x56fd\x3002" L"cn",
    {false, false, true,  false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, true,  false, false,
     true}},
#endif
};

struct AdjustOffsetCase {
  size_t input_offset;
  size_t output_offset;
};

struct UrlTestData {
  const char* description;
  const char* input;
  const char* languages;
  FormatUrlTypes format_types;
  UnescapeRule::Type escape_rules;
  const wchar_t* output;  // Use |wchar_t| to handle Unicode constants easily.
  size_t prefix_len;
};

// A helper for IDN*{Fast,Slow}.
// Append "::<language list>" to |expected| and |actual| to make it
// easy to tell which sub-case fails without debugging.
void AppendLanguagesToOutputs(const char* languages,
                              base::string16* expected,
                              base::string16* actual) {
  base::string16 to_append = ASCIIToUTF16("::") + ASCIIToUTF16(languages);
  expected->append(to_append);
  actual->append(to_append);
}

// A pair of helpers for the FormatUrlWithOffsets() test.
void VerboseExpect(size_t expected,
                   size_t actual,
                   const std::string& original_url,
                   size_t position,
                   const base::string16& formatted_url) {
  EXPECT_EQ(expected, actual) << "Original URL: " << original_url
      << " (at char " << position << ")\nFormatted URL: " << formatted_url;
}

void CheckAdjustedOffsets(const std::string& url_string,
                          const std::string& languages,
                          FormatUrlTypes format_types,
                          UnescapeRule::Type unescape_rules,
                          const size_t* output_offsets) {
  GURL url(url_string);
  size_t url_length = url_string.length();
  std::vector<size_t> offsets;
  for (size_t i = 0; i <= url_length + 1; ++i)
    offsets.push_back(i);
  offsets.push_back(500000);  // Something larger than any input length.
  offsets.push_back(std::string::npos);
  base::string16 formatted_url = FormatUrlWithOffsets(url, languages,
      format_types, unescape_rules, NULL, NULL, &offsets);
  for (size_t i = 0; i < url_length; ++i)
    VerboseExpect(output_offsets[i], offsets[i], url_string, i, formatted_url);
  VerboseExpect(formatted_url.length(), offsets[url_length], url_string,
                url_length, formatted_url);
  VerboseExpect(base::string16::npos, offsets[url_length + 1], url_string,
                500000, formatted_url);
  VerboseExpect(base::string16::npos, offsets[url_length + 2], url_string,
                std::string::npos, formatted_url);
}

}  // anonymous namespace

TEST(NetUtilTest, IDNToUnicodeFast) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(idn_cases); i++) {
    for (size_t j = 0; j < arraysize(kLanguages); j++) {
      // ja || zh-TW,en || ko,ja -> IDNToUnicodeSlow
      if (j == 3 || j == 17 || j == 18)
        continue;
      base::string16 output(IDNToUnicode(idn_cases[i].input, kLanguages[j]));
      base::string16 expected(idn_cases[i].unicode_allowed[j] ?
          WideToUTF16(idn_cases[i].unicode_output) :
          ASCIIToUTF16(idn_cases[i].input));
      AppendLanguagesToOutputs(kLanguages[j], &expected, &output);
      EXPECT_EQ(expected, output);
    }
  }
}

TEST(NetUtilTest, IDNToUnicodeSlow) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(idn_cases); i++) {
    for (size_t j = 0; j < arraysize(kLanguages); j++) {
      // !(ja || zh-TW,en || ko,ja) -> IDNToUnicodeFast
      if (!(j == 3 || j == 17 || j == 18))
        continue;
      base::string16 output(IDNToUnicode(idn_cases[i].input, kLanguages[j]));
      base::string16 expected(idn_cases[i].unicode_allowed[j] ?
          WideToUTF16(idn_cases[i].unicode_output) :
          ASCIIToUTF16(idn_cases[i].input));
      AppendLanguagesToOutputs(kLanguages[j], &expected, &output);
      EXPECT_EQ(expected, output);
    }
  }
}

TEST(NetUtilTest, StripWWW) {
  EXPECT_EQ(base::string16(), StripWWW(base::string16()));
  EXPECT_EQ(base::string16(), StripWWW(ASCIIToUTF16("www.")));
  EXPECT_EQ(ASCIIToUTF16("blah"), StripWWW(ASCIIToUTF16("www.blah")));
  EXPECT_EQ(ASCIIToUTF16("blah"), StripWWW(ASCIIToUTF16("blah")));
}

// This is currently a windows specific function.
#if defined(OS_WIN)
namespace {

struct GetDirectoryListingEntryCase {
  const wchar_t* name;
  const char* raw_bytes;
  bool is_dir;
  int64 filesize;
  base::Time time;
  const char* expected;
};

}  // namespace

TEST(NetUtilTest, GetDirectoryListingEntry) {
  const GetDirectoryListingEntryCase test_cases[] = {
    {L"Foo",
     "",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"Foo\",\"Foo\",0,\"9.8 kB\",\"\");</script>\n"},
    {L"quo\"tes",
     "",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,\"9.8 kB\",\"\");</script>"
         "\n"},
    {L"quo\"tes",
     "quo\"tes",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,\"9.8 kB\",\"\");</script>"
         "\n"},
    // U+D55C0 U+AE00. raw_bytes is empty (either a local file with
    // UTF-8/UTF-16 encoding or a remote file on an ftp server using UTF-8
    {L"\xD55C\xAE00.txt",
     "",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"\xED\x95\x9C\xEA\xB8\x80.txt\","
         "\"%ED%95%9C%EA%B8%80.txt\",0,\"9.8 kB\",\"\");</script>\n"},
    // U+D55C0 U+AE00. raw_bytes is the corresponding EUC-KR sequence:
    // a local or remote file in EUC-KR.
    {L"\xD55C\xAE00.txt",
     "\xC7\xD1\xB1\xDB.txt",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"\xED\x95\x9C\xEA\xB8\x80.txt\",\"%C7%D1%B1%DB.txt\""
         ",0,\"9.8 kB\",\"\");</script>\n"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const std::string results = GetDirectoryListingEntry(
        WideToUTF16(test_cases[i].name),
        test_cases[i].raw_bytes,
        test_cases[i].is_dir,
        test_cases[i].filesize,
        test_cases[i].time);
    EXPECT_EQ(test_cases[i].expected, results);
  }
}

#endif

TEST(NetUtilTest, FormatUrl) {
  FormatUrlTypes default_format_type = kFormatUrlOmitUsernamePassword;
  const UrlTestData tests[] = {
    {"Empty URL", "", "", default_format_type, UnescapeRule::NORMAL, L"", 0},

    {"Simple URL",
     "http://www.google.com/", "", default_format_type, UnescapeRule::NORMAL,
     L"http://www.google.com/", 7},

    {"With a port number and a reference",
     "http://www.google.com:8080/#\xE3\x82\xB0", "", default_format_type,
     UnescapeRule::NORMAL,
     L"http://www.google.com:8080/#\x30B0", 7},

    // -------- IDN tests --------
    {"Japanese IDN with ja",
     "http://xn--l8jvb1ey91xtjb.jp", "ja", default_format_type,
     UnescapeRule::NORMAL, L"http://\x671d\x65e5\x3042\x3055\x3072.jp/", 7},

    {"Japanese IDN with en",
     "http://xn--l8jvb1ey91xtjb.jp", "en", default_format_type,
     UnescapeRule::NORMAL, L"http://xn--l8jvb1ey91xtjb.jp/", 7},

    {"Japanese IDN without any languages",
     "http://xn--l8jvb1ey91xtjb.jp", "", default_format_type,
     UnescapeRule::NORMAL,
     // Single script is safe for empty languages.
     L"http://\x671d\x65e5\x3042\x3055\x3072.jp/", 7},

    {"mailto: with Japanese IDN",
     "mailto:foo@xn--l8jvb1ey91xtjb.jp", "ja", default_format_type,
     UnescapeRule::NORMAL,
     // GURL doesn't assume an email address's domain part as a host name.
     L"mailto:foo@xn--l8jvb1ey91xtjb.jp", 7},

    {"file: with Japanese IDN",
     "file://xn--l8jvb1ey91xtjb.jp/config.sys", "ja", default_format_type,
     UnescapeRule::NORMAL,
     L"file://\x671d\x65e5\x3042\x3055\x3072.jp/config.sys", 7},

    {"ftp: with Japanese IDN",
     "ftp://xn--l8jvb1ey91xtjb.jp/config.sys", "ja", default_format_type,
     UnescapeRule::NORMAL,
     L"ftp://\x671d\x65e5\x3042\x3055\x3072.jp/config.sys", 6},

    // -------- omit_username_password flag tests --------
    {"With username and password, omit_username_password=false",
     "http://user:passwd@example.com/foo", "",
     kFormatUrlOmitNothing, UnescapeRule::NORMAL,
     L"http://user:passwd@example.com/foo", 19},

    {"With username and password, omit_username_password=true",
     "http://user:passwd@example.com/foo", "", default_format_type,
     UnescapeRule::NORMAL, L"http://example.com/foo", 7},

    {"With username and no password",
     "http://user@example.com/foo", "", default_format_type,
     UnescapeRule::NORMAL, L"http://example.com/foo", 7},

    {"Just '@' without username and password",
     "http://@example.com/foo", "", default_format_type, UnescapeRule::NORMAL,
     L"http://example.com/foo", 7},

    // GURL doesn't think local-part of an email address is username for URL.
    {"mailto:, omit_username_password=true",
     "mailto:foo@example.com", "", default_format_type, UnescapeRule::NORMAL,
     L"mailto:foo@example.com", 7},

    // -------- unescape flag tests --------
    {"Do not unescape",
     "http://%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB.jp/"
     "%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
     "?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB", "en", default_format_type,
     UnescapeRule::NONE,
     // GURL parses %-encoded hostnames into Punycode.
     L"http://xn--qcka1pmc.jp/%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
     L"?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB", 7},

    {"Unescape normally",
     "http://%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB.jp/"
     "%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
     "?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB", "en", default_format_type,
     UnescapeRule::NORMAL,
     L"http://xn--qcka1pmc.jp/\x30B0\x30FC\x30B0\x30EB"
     L"?q=\x30B0\x30FC\x30B0\x30EB", 7},

    {"Unescape normally with BiDi control character",
     "http://example.com/%E2%80%AEabc?q=%E2%80%8Fxy", "en", default_format_type,
     UnescapeRule::NORMAL, L"http://example.com/%E2%80%AEabc?q=%E2%80%8Fxy", 7},

    {"Unescape normally including unescape spaces",
     "http://www.google.com/search?q=Hello%20World", "en", default_format_type,
     UnescapeRule::SPACES, L"http://www.google.com/search?q=Hello World", 7},

    /*
    {"unescape=true with some special characters",
    "http://user%3A:%40passwd@example.com/foo%3Fbar?q=b%26z", "",
    kFormatUrlOmitNothing, UnescapeRule::NORMAL,
    L"http://user%3A:%40passwd@example.com/foo%3Fbar?q=b%26z", 25},
    */
    // Disabled: the resultant URL becomes "...user%253A:%2540passwd...".

    // -------- omit http: --------
    {"omit http with user name",
     "http://user@example.com/foo", "", kFormatUrlOmitAll,
     UnescapeRule::NORMAL, L"example.com/foo", 0},

    {"omit http",
     "http://www.google.com/", "en", kFormatUrlOmitHTTP,
     UnescapeRule::NORMAL, L"www.google.com/",
     0},

    {"omit http with https",
     "https://www.google.com/", "en", kFormatUrlOmitHTTP,
     UnescapeRule::NORMAL, L"https://www.google.com/",
     8},

    {"omit http starts with ftp.",
     "http://ftp.google.com/", "en", kFormatUrlOmitHTTP,
     UnescapeRule::NORMAL, L"http://ftp.google.com/",
     7},

    // -------- omit trailing slash on bare hostname --------
    {"omit slash when it's the entire path",
     "http://www.google.com/", "en",
     kFormatUrlOmitTrailingSlashOnBareHostname, UnescapeRule::NORMAL,
     L"http://www.google.com", 7},
    {"omit slash when there's a ref",
     "http://www.google.com/#ref", "en",
     kFormatUrlOmitTrailingSlashOnBareHostname, UnescapeRule::NORMAL,
     L"http://www.google.com/#ref", 7},
    {"omit slash when there's a query",
     "http://www.google.com/?", "en",
     kFormatUrlOmitTrailingSlashOnBareHostname, UnescapeRule::NORMAL,
     L"http://www.google.com/?", 7},
    {"omit slash when it's not the entire path",
     "http://www.google.com/foo", "en",
     kFormatUrlOmitTrailingSlashOnBareHostname, UnescapeRule::NORMAL,
     L"http://www.google.com/foo", 7},
    {"omit slash for nonstandard URLs",
     "data:/", "en", kFormatUrlOmitTrailingSlashOnBareHostname,
     UnescapeRule::NORMAL, L"data:/", 5},
    {"omit slash for file URLs",
     "file:///", "en", kFormatUrlOmitTrailingSlashOnBareHostname,
     UnescapeRule::NORMAL, L"file:///", 7},

    // -------- view-source: --------
    {"view-source",
     "view-source:http://xn--qcka1pmc.jp/", "ja", default_format_type,
     UnescapeRule::NORMAL, L"view-source:http://\x30B0\x30FC\x30B0\x30EB.jp/",
     19},

    {"view-source of view-source",
     "view-source:view-source:http://xn--qcka1pmc.jp/", "ja",
     default_format_type, UnescapeRule::NORMAL,
     L"view-source:view-source:http://xn--qcka1pmc.jp/", 12},

    // view-source should omit http and trailing slash where non-view-source
    // would.
    {"view-source omit http",
     "view-source:http://a.b/c", "en", kFormatUrlOmitAll,
     UnescapeRule::NORMAL, L"view-source:a.b/c",
     12},
    {"view-source omit http starts with ftp.",
     "view-source:http://ftp.b/c", "en", kFormatUrlOmitAll,
     UnescapeRule::NORMAL, L"view-source:http://ftp.b/c",
     19},
    {"view-source omit slash when it's the entire path",
     "view-source:http://a.b/", "en", kFormatUrlOmitAll,
     UnescapeRule::NORMAL, L"view-source:a.b",
     12},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    size_t prefix_len;
    base::string16 formatted = FormatUrl(
        GURL(tests[i].input), tests[i].languages, tests[i].format_types,
        tests[i].escape_rules, NULL, &prefix_len, NULL);
    EXPECT_EQ(WideToUTF16(tests[i].output), formatted) << tests[i].description;
    EXPECT_EQ(tests[i].prefix_len, prefix_len) << tests[i].description;
  }
}

TEST(NetUtilTest, FormatUrlParsed) {
  // No unescape case.
  url::Parsed parsed;
  base::string16 formatted = FormatUrl(
      GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
           "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
      "ja", kFormatUrlOmitNothing, UnescapeRule::NONE, &parsed, NULL,
      NULL);
  EXPECT_EQ(WideToUTF16(
      L"http://%E3%82%B0:%E3%83%BC@\x30B0\x30FC\x30B0\x30EB.jp:8080"
      L"/%E3%82%B0/?q=%E3%82%B0#\x30B0"), formatted);
  EXPECT_EQ(WideToUTF16(L"%E3%82%B0"),
      formatted.substr(parsed.username.begin, parsed.username.len));
  EXPECT_EQ(WideToUTF16(L"%E3%83%BC"),
      formatted.substr(parsed.password.begin, parsed.password.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0\x30FC\x30B0\x30EB.jp"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8080"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/%E3%82%B0/"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"q=%E3%82%B0"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // Unescape case.
  formatted = FormatUrl(
      GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
           "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
      "ja", kFormatUrlOmitNothing, UnescapeRule::NORMAL, &parsed, NULL,
      NULL);
  EXPECT_EQ(WideToUTF16(L"http://\x30B0:\x30FC@\x30B0\x30FC\x30B0\x30EB.jp:8080"
      L"/\x30B0/?q=\x30B0#\x30B0"), formatted);
  EXPECT_EQ(WideToUTF16(L"\x30B0"),
      formatted.substr(parsed.username.begin, parsed.username.len));
  EXPECT_EQ(WideToUTF16(L"\x30FC"),
      formatted.substr(parsed.password.begin, parsed.password.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0\x30FC\x30B0\x30EB.jp"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8080"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/\x30B0/"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"q=\x30B0"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // Omit_username_password + unescape case.
  formatted = FormatUrl(
      GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
           "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
      "ja", kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL, &parsed,
      NULL, NULL);
  EXPECT_EQ(WideToUTF16(L"http://\x30B0\x30FC\x30B0\x30EB.jp:8080"
      L"/\x30B0/?q=\x30B0#\x30B0"), formatted);
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(WideToUTF16(L"\x30B0\x30FC\x30B0\x30EB.jp"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8080"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/\x30B0/"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"q=\x30B0"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // View-source case.
  formatted =
      FormatUrl(GURL("view-source:http://user:passwd@host:81/path?query#ref"),
                std::string(),
                kFormatUrlOmitUsernamePassword,
                UnescapeRule::NORMAL,
                &parsed,
                NULL,
                NULL);
  EXPECT_EQ(WideToUTF16(L"view-source:http://host:81/path?query#ref"),
      formatted);
  EXPECT_EQ(WideToUTF16(L"view-source:http"),
      formatted.substr(parsed.scheme.begin, parsed.scheme.len));
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(WideToUTF16(L"host"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"81"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/path"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"query"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"ref"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // omit http case.
  formatted = FormatUrl(GURL("http://host:8000/a?b=c#d"),
                        std::string(),
                        kFormatUrlOmitHTTP,
                        UnescapeRule::NORMAL,
                        &parsed,
                        NULL,
                        NULL);
  EXPECT_EQ(WideToUTF16(L"host:8000/a?b=c#d"), formatted);
  EXPECT_FALSE(parsed.scheme.is_valid());
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(WideToUTF16(L"host"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8000"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/a"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"b=c"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"d"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // omit http starts with ftp case.
  formatted = FormatUrl(GURL("http://ftp.host:8000/a?b=c#d"),
                        std::string(),
                        kFormatUrlOmitHTTP,
                        UnescapeRule::NORMAL,
                        &parsed,
                        NULL,
                        NULL);
  EXPECT_EQ(WideToUTF16(L"http://ftp.host:8000/a?b=c#d"), formatted);
  EXPECT_TRUE(parsed.scheme.is_valid());
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(WideToUTF16(L"http"),
      formatted.substr(parsed.scheme.begin, parsed.scheme.len));
  EXPECT_EQ(WideToUTF16(L"ftp.host"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8000"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/a"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"b=c"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"d"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // omit http starts with 'f' case.
  formatted = FormatUrl(GURL("http://f/"),
                        std::string(),
                        kFormatUrlOmitHTTP,
                        UnescapeRule::NORMAL,
                        &parsed,
                        NULL,
                        NULL);
  EXPECT_EQ(WideToUTF16(L"f/"), formatted);
  EXPECT_FALSE(parsed.scheme.is_valid());
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_FALSE(parsed.port.is_valid());
  EXPECT_TRUE(parsed.path.is_valid());
  EXPECT_FALSE(parsed.query.is_valid());
  EXPECT_FALSE(parsed.ref.is_valid());
  EXPECT_EQ(WideToUTF16(L"f"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"/"),
      formatted.substr(parsed.path.begin, parsed.path.len));
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// results in the original GURL, for each ASCII character in the path.
TEST(NetUtilTest, FormatUrlRoundTripPathASCII) {
  for (unsigned char test_char = 32; test_char < 128; ++test_char) {
    GURL url(std::string("http://www.google.com/") +
             static_cast<char>(test_char));
    size_t prefix_len;
    base::string16 formatted = FormatUrl(url,
                                         std::string(),
                                         kFormatUrlOmitUsernamePassword,
                                         UnescapeRule::NORMAL,
                                         NULL,
                                         &prefix_len,
                                         NULL);
    EXPECT_EQ(url.spec(), GURL(formatted).spec());
  }
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// results in the original GURL, for each escaped ASCII character in the path.
TEST(NetUtilTest, FormatUrlRoundTripPathEscaped) {
  for (unsigned char test_char = 32; test_char < 128; ++test_char) {
    std::string original_url("http://www.google.com/");
    original_url.push_back('%');
    original_url.append(base::HexEncode(&test_char, 1));

    GURL url(original_url);
    size_t prefix_len;
    base::string16 formatted = FormatUrl(url,
                                         std::string(),
                                         kFormatUrlOmitUsernamePassword,
                                         UnescapeRule::NORMAL,
                                         NULL,
                                         &prefix_len,
                                         NULL);
    EXPECT_EQ(url.spec(), GURL(formatted).spec());
  }
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// results in the original GURL, for each ASCII character in the query.
TEST(NetUtilTest, FormatUrlRoundTripQueryASCII) {
  for (unsigned char test_char = 32; test_char < 128; ++test_char) {
    GURL url(std::string("http://www.google.com/?") +
             static_cast<char>(test_char));
    size_t prefix_len;
    base::string16 formatted = FormatUrl(url,
                                         std::string(),
                                         kFormatUrlOmitUsernamePassword,
                                         UnescapeRule::NORMAL,
                                         NULL,
                                         &prefix_len,
                                         NULL);
    EXPECT_EQ(url.spec(), GURL(formatted).spec());
  }
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// only results in a different GURL for certain characters.
TEST(NetUtilTest, FormatUrlRoundTripQueryEscaped) {
  // A full list of characters which FormatURL should unescape and GURL should
  // not escape again, when they appear in a query string.
  const char* kUnescapedCharacters =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_~";
  for (unsigned char test_char = 0; test_char < 128; ++test_char) {
    std::string original_url("http://www.google.com/?");
    original_url.push_back('%');
    original_url.append(base::HexEncode(&test_char, 1));

    GURL url(original_url);
    size_t prefix_len;
    base::string16 formatted = FormatUrl(url,
                                         std::string(),
                                         kFormatUrlOmitUsernamePassword,
                                         UnescapeRule::NORMAL,
                                         NULL,
                                         &prefix_len,
                                         NULL);

    if (test_char &&
        strchr(kUnescapedCharacters, static_cast<char>(test_char))) {
      EXPECT_NE(url.spec(), GURL(formatted).spec());
    } else {
      EXPECT_EQ(url.spec(), GURL(formatted).spec());
    }
  }
}

TEST(NetUtilTest, FormatUrlWithOffsets) {
  CheckAdjustedOffsets(std::string(), "en", kFormatUrlOmitNothing,
                       UnescapeRule::NORMAL, NULL);

  const size_t basic_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25
  };
  CheckAdjustedOffsets("http://www.google.com/foo/", "en",
                       kFormatUrlOmitNothing, UnescapeRule::NORMAL,
                       basic_offsets);

  const size_t omit_auth_offsets_1[] = {
    0, 1, 2, 3, 4, 5, 6, 7, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 7,
    8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
  };
  CheckAdjustedOffsets("http://foo:bar@www.google.com/", "en",
                       kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL,
                       omit_auth_offsets_1);

  const size_t omit_auth_offsets_2[] = {
    0, 1, 2, 3, 4, 5, 6, 7, kNpos, kNpos, kNpos, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21
  };
  CheckAdjustedOffsets("http://foo@www.google.com/", "en",
                       kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL,
                       omit_auth_offsets_2);

  const size_t dont_omit_auth_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, kNpos, 11, 12, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31
  };
  // Unescape to "http://foo\x30B0:\x30B0bar@www.google.com".
  CheckAdjustedOffsets("http://foo%E3%82%B0:%E3%82%B0bar@www.google.com/", "en",
                       kFormatUrlOmitNothing, UnescapeRule::NORMAL,
                       dont_omit_auth_offsets);

  const size_t view_source_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, kNpos,
    kNpos, kNpos, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33
  };
  CheckAdjustedOffsets("view-source:http://foo@www.google.com/", "en",
                       kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL,
                       view_source_offsets);

  const size_t idn_hostname_offsets_1[] = {
    0, 1, 2, 3, 4, 5, 6, 7, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 12,
    13, 14, 15, 16, 17, 18, 19
  };
  // Convert punycode to "http://\x671d\x65e5\x3042\x3055\x3072.jp/foo/".
  CheckAdjustedOffsets("http://xn--l8jvb1ey91xtjb.jp/foo/", "ja",
                       kFormatUrlOmitNothing, UnescapeRule::NORMAL,
                       idn_hostname_offsets_1);

  const size_t idn_hostname_offsets_2[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 14, 15, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, 19, 20, 21, 22, 23, 24
  };
  // Convert punycode to
  // "http://test.\x89c6\x9891.\x5317\x4eac\x5927\x5b78.test/".
  CheckAdjustedOffsets("http://test.xn--cy2a840a.xn--1lq90ic7f1rc.test/",
                       "zh-CN", kFormatUrlOmitNothing, UnescapeRule::NORMAL,
                       idn_hostname_offsets_2);

  const size_t unescape_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, kNpos, kNpos, 26, 27, 28, 29, 30, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, kNpos, kNpos, 31, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, 32, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, 33, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos
  };
  // Unescape to "http://www.google.com/foo bar/\x30B0\x30FC\x30B0\x30EB".
  CheckAdjustedOffsets(
      "http://www.google.com/foo%20bar/%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB",
      "en", kFormatUrlOmitNothing, UnescapeRule::SPACES, unescape_offsets);

  const size_t ref_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, kNpos, kNpos, 32, kNpos, kNpos,
    33
  };
  // Unescape to "http://www.google.com/foo.html#\x30B0\x30B0z".
  CheckAdjustedOffsets(
      "http://www.google.com/foo.html#\xE3\x82\xB0\xE3\x82\xB0z", "en",
      kFormatUrlOmitNothing, UnescapeRule::NORMAL, ref_offsets);

  const size_t omit_http_offsets[] = {
    0, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14
  };
  CheckAdjustedOffsets("http://www.google.com/", "en", kFormatUrlOmitHTTP,
                       UnescapeRule::NORMAL, omit_http_offsets);

  const size_t omit_http_start_with_ftp_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
  };
  CheckAdjustedOffsets("http://ftp.google.com/", "en", kFormatUrlOmitHTTP,
                       UnescapeRule::NORMAL, omit_http_start_with_ftp_offsets);

  const size_t omit_all_offsets[] = {
    0, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 0, kNpos, kNpos, kNpos, kNpos,
    0, 1, 2, 3, 4, 5, 6, 7
  };
  CheckAdjustedOffsets("http://user@foo.com/", "en", kFormatUrlOmitAll,
                       UnescapeRule::NORMAL, omit_all_offsets);
}

}  // namespace net
