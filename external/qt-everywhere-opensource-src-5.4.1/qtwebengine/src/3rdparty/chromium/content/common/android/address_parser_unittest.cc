// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/android/address_parser.h"
#include "content/common/android/address_parser_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace content::address_parser;
using namespace content::address_parser::internal;

class AddressParserTest : public testing::Test {
 public:
  AddressParserTest() {}

  void TokenizeWords(const base::string16& content, WordList* words) const {
    String16Tokenizer tokenizer(content.begin(), content.end(),
                                base::kWhitespaceUTF16);
    while (tokenizer.GetNext()) {
      words->push_back(Word(tokenizer.token_begin(), tokenizer.token_end()));
    }
  }

  std::string GetHouseNumber(const std::string& content) const {
    base::string16 content_16 = base::UTF8ToUTF16(content);
    base::string16 result;

    HouseNumberParser parser;
    Word word;
    if (parser.Parse(content_16.begin(), content_16.end(), &word))
      result = base::string16(word.begin, word.end);
    return base::UTF16ToUTF8(result);
  }

  bool ContainsHouseNumber(const std::string& content) const {
    return !GetHouseNumber(content).empty();
  }

  bool GetState(const std::string& state, size_t* state_index) const {
    base::string16 state_16 = base::UTF8ToUTF16(state);
    String16Tokenizer tokenizer(state_16.begin(), state_16.end(),
                                base::kWhitespaceUTF16);
    if (!tokenizer.GetNext())
      return false;

    size_t state_last_word;
    WordList words;
    words.push_back(Word(tokenizer.token_begin(), tokenizer.token_end()));
    return FindStateStartingInWord(&words, 0, &state_last_word, &tokenizer,
                                   state_index);
  }

  bool IsState(const std::string& state) const {
    size_t state_index;
    return GetState(state, &state_index);
  }

  bool IsZipValid(const std::string& zip, const std::string& state) const {
    size_t state_index;
    EXPECT_TRUE(GetState(state, &state_index));

    base::string16 zip_16 = base::UTF8ToUTF16(zip);
    WordList words;
    TokenizeWords(zip_16, &words);
    EXPECT_TRUE(words.size() == 1);
    return ::IsZipValid(words.front(), state_index);
  }

  bool IsLocationName(const std::string& street) const {
    base::string16 street_16 = base::UTF8ToUTF16(street);
    WordList words;
    TokenizeWords(street_16, &words);
    EXPECT_TRUE(words.size() == 1);
    return IsValidLocationName(words.front());
  }

  std::string FindAddress(const std::string& content) const {
    base::string16 content_16 = base::UTF8ToUTF16(content);
    base::string16 result_16;
    size_t start, end;
    if (::FindAddress(content_16.begin(), content_16.end(), &start, &end))
      result_16 = content_16.substr(start, end - start);
    return base::UTF16ToUTF8(result_16);
  }

  bool ContainsAddress(const std::string& content) const {
    return !FindAddress(content).empty();
  }

  bool IsAddress(const std::string& content) const {
    return FindAddress(content) == content;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressParserTest);
};

TEST_F(AddressParserTest, HouseNumber) {
  // Tests cases with valid home numbers.
  EXPECT_EQ(GetHouseNumber("4 my house"), "4");
  EXPECT_EQ(GetHouseNumber("Something 4 my house"), "4");
  EXPECT_EQ(GetHouseNumber("4"), "4");
  EXPECT_EQ(GetHouseNumber(" 4,5"), "4");
  EXPECT_EQ(GetHouseNumber("one"), "one");
  EXPECT_EQ(GetHouseNumber("Number One somewhere"), "One");
  EXPECT_EQ(GetHouseNumber("Testing \n4\n"), "4");
  EXPECT_EQ(GetHouseNumber("Foo 1ST"), "1ST");
  EXPECT_EQ(GetHouseNumber("Bar 2nd"), "2nd");
  EXPECT_EQ(GetHouseNumber("Blah 3rd"), "3rd");
  EXPECT_EQ(GetHouseNumber("4th"), "4th");
  EXPECT_EQ(GetHouseNumber("Blah 11th"), "11th");
  EXPECT_EQ(GetHouseNumber("Blah 12th meh"), "12th");
  EXPECT_EQ(GetHouseNumber("Blah 13th moo"), "13th");
  EXPECT_EQ(GetHouseNumber("211st"), "211st");
  EXPECT_EQ(GetHouseNumber("1A"), "1A");
  EXPECT_EQ(GetHouseNumber("number:35"), "35");
  EXPECT_EQ(GetHouseNumber("five digits at most: 12345"), "12345");
  EXPECT_EQ(GetHouseNumber("'123'"), "123");
  EXPECT_EQ(GetHouseNumber("\"123\""), "123");
  EXPECT_EQ(GetHouseNumber("\"123, something\""), "123");
  EXPECT_EQ(GetHouseNumber("Testing 12-34"), "12-34");
  EXPECT_EQ(GetHouseNumber("Testing 12-34c,d"), "12-34c");
  EXPECT_EQ(GetHouseNumber("住所は:76 Buckingham Palace Roadです"), "76");

  // Tests cases without valid home numbers.
  EXPECT_FALSE(ContainsHouseNumber("0th"));
  EXPECT_FALSE(ContainsHouseNumber("25st"));
  EXPECT_FALSE(ContainsHouseNumber("111th"));
  EXPECT_FALSE(ContainsHouseNumber("011th"));
  EXPECT_FALSE(ContainsHouseNumber("27AZ"));
  EXPECT_FALSE(ContainsHouseNumber("22ºC"));
  EXPECT_FALSE(ContainsHouseNumber("3.141592"));
  EXPECT_FALSE(ContainsHouseNumber("more than five digits: 123456"));
  EXPECT_FALSE(ContainsHouseNumber("kjhdfkajsdhf98uf93h"));
  EXPECT_FALSE(ContainsHouseNumber("これはテストです。"));
  EXPECT_FALSE(ContainsHouseNumber("Number On"));
  EXPECT_FALSE(ContainsHouseNumber("2: foo"));
  EXPECT_FALSE(ContainsHouseNumber("12-"));
  EXPECT_FALSE(ContainsHouseNumber("\n\"'  \t, "));
  EXPECT_FALSE(ContainsHouseNumber(""));
}

TEST_F(AddressParserTest, FindState) {
  // The complete set of state codes and names is tested together with their
  // returned state indices in the zip code test.
  EXPECT_TRUE(IsState("CALIFORNIA"));
  EXPECT_TRUE(IsState("ca"));

  EXPECT_FALSE(IsState("californi"));
  EXPECT_FALSE(IsState("northern mariana"));
  EXPECT_FALSE(IsState("northern mariana island"));
  EXPECT_FALSE(IsState("zz"));
}

TEST_F(AddressParserTest, ZipCode) {
  EXPECT_TRUE(IsZipValid("90000", "CA"));
  EXPECT_TRUE(IsZipValid("01234", "MA"));
  EXPECT_TRUE(IsZipValid("99999-9999", "Alaska"));

  EXPECT_FALSE(IsZipValid("999999999", "Alaska"));
  EXPECT_FALSE(IsZipValid("9999-99999", "Alaska"));
  EXPECT_FALSE(IsZipValid("999999999-", "Alaska"));
  EXPECT_FALSE(IsZipValid("99999-999a", "Alaska"));
  EXPECT_FALSE(IsZipValid("99999--9999", "Alaska"));
  EXPECT_FALSE(IsZipValid("90000o", "CA"));
  EXPECT_FALSE(IsZipValid("01234", "CA"));
  EXPECT_FALSE(IsZipValid("01234-", "MA"));

  // Test the state index against the zip range table.
  EXPECT_TRUE(IsZipValid("99000", "AK"));
  EXPECT_TRUE(IsZipValid("99000", "Alaska"));
  EXPECT_TRUE(IsZipValid("35000", "AL"));
  EXPECT_TRUE(IsZipValid("36000", "Alabama"));
  EXPECT_TRUE(IsZipValid("71000", "AR"));
  EXPECT_TRUE(IsZipValid("72000", "Arkansas"));
  EXPECT_TRUE(IsZipValid("96000", "AS"));
  EXPECT_TRUE(IsZipValid("96000", "American Samoa"));
  EXPECT_TRUE(IsZipValid("85000", "AZ"));
  EXPECT_TRUE(IsZipValid("86000", "Arizona"));
  EXPECT_TRUE(IsZipValid("90000", "CA"));
  EXPECT_TRUE(IsZipValid("96000", "California"));
  EXPECT_TRUE(IsZipValid("80000", "CO"));
  EXPECT_TRUE(IsZipValid("81000", "Colorado"));
  EXPECT_TRUE(IsZipValid("06000", "CT"));
  EXPECT_TRUE(IsZipValid("06000", "Connecticut"));
  EXPECT_TRUE(IsZipValid("20000", "DC"));
  EXPECT_TRUE(IsZipValid("20000", "District of Columbia"));
  EXPECT_TRUE(IsZipValid("19000", "DE"));
  EXPECT_TRUE(IsZipValid("19000", "Delaware"));
  EXPECT_TRUE(IsZipValid("32000", "FL"));
  EXPECT_TRUE(IsZipValid("34000", "Florida"));
  EXPECT_TRUE(IsZipValid("96000", "FM"));
  EXPECT_TRUE(IsZipValid("96000", "Federated States of Micronesia"));
  EXPECT_TRUE(IsZipValid("30000", "GA"));
  EXPECT_TRUE(IsZipValid("31000", "Georgia"));
  EXPECT_TRUE(IsZipValid("96000", "GU"));
  EXPECT_TRUE(IsZipValid("96000", "Guam"));
  EXPECT_TRUE(IsZipValid("96000", "HI"));
  EXPECT_TRUE(IsZipValid("96000", "Hawaii"));
  EXPECT_TRUE(IsZipValid("50000", "IA"));
  EXPECT_TRUE(IsZipValid("52000", "Iowa"));
  EXPECT_TRUE(IsZipValid("83000", "ID"));
  EXPECT_TRUE(IsZipValid("83000", "Idaho"));
  EXPECT_TRUE(IsZipValid("60000", "IL"));
  EXPECT_TRUE(IsZipValid("62000", "Illinois"));
  EXPECT_TRUE(IsZipValid("46000", "IN"));
  EXPECT_TRUE(IsZipValid("47000", "Indiana"));
  EXPECT_TRUE(IsZipValid("66000", "KS"));
  EXPECT_TRUE(IsZipValid("67000", "Kansas"));
  EXPECT_TRUE(IsZipValid("40000", "KY"));
  EXPECT_TRUE(IsZipValid("42000", "Kentucky"));
  EXPECT_TRUE(IsZipValid("70000", "LA"));
  EXPECT_TRUE(IsZipValid("71000", "Louisiana"));
  EXPECT_TRUE(IsZipValid("01000", "MA"));
  EXPECT_TRUE(IsZipValid("02000", "Massachusetts"));
  EXPECT_TRUE(IsZipValid("20000", "MD"));
  EXPECT_TRUE(IsZipValid("21000", "Maryland"));
  EXPECT_TRUE(IsZipValid("03000", "ME"));
  EXPECT_TRUE(IsZipValid("04000", "Maine"));
  EXPECT_TRUE(IsZipValid("96000", "MH"));
  EXPECT_TRUE(IsZipValid("96000", "Marshall Islands"));
  EXPECT_TRUE(IsZipValid("48000", "MI"));
  EXPECT_TRUE(IsZipValid("49000", "Michigan"));
  EXPECT_TRUE(IsZipValid("55000", "MN"));
  EXPECT_TRUE(IsZipValid("56000", "Minnesota"));
  EXPECT_TRUE(IsZipValid("63000", "MO"));
  EXPECT_TRUE(IsZipValid("65000", "Missouri"));
  EXPECT_TRUE(IsZipValid("96000", "MP"));
  EXPECT_TRUE(IsZipValid("96000", "Northern Mariana Islands"));
  EXPECT_TRUE(IsZipValid("38000", "MS"));
  EXPECT_TRUE(IsZipValid("39000", "Mississippi"));
  EXPECT_TRUE(IsZipValid("55000", "MT"));
  EXPECT_TRUE(IsZipValid("56000", "Montana"));
  EXPECT_TRUE(IsZipValid("27000", "NC"));
  EXPECT_TRUE(IsZipValid("28000", "North Carolina"));
  EXPECT_TRUE(IsZipValid("58000", "ND"));
  EXPECT_TRUE(IsZipValid("58000", "North Dakota"));
  EXPECT_TRUE(IsZipValid("68000", "NE"));
  EXPECT_TRUE(IsZipValid("69000", "Nebraska"));
  EXPECT_TRUE(IsZipValid("03000", "NH"));
  EXPECT_TRUE(IsZipValid("04000", "New Hampshire"));
  EXPECT_TRUE(IsZipValid("07000", "NJ"));
  EXPECT_TRUE(IsZipValid("08000", "New Jersey"));
  EXPECT_TRUE(IsZipValid("87000", "NM"));
  EXPECT_TRUE(IsZipValid("88000", "New Mexico"));
  EXPECT_TRUE(IsZipValid("88000", "NV"));
  EXPECT_TRUE(IsZipValid("89000", "Nevada"));
  EXPECT_TRUE(IsZipValid("10000", "NY"));
  EXPECT_TRUE(IsZipValid("14000", "New York"));
  EXPECT_TRUE(IsZipValid("43000", "OH"));
  EXPECT_TRUE(IsZipValid("45000", "Ohio"));
  EXPECT_TRUE(IsZipValid("73000", "OK"));
  EXPECT_TRUE(IsZipValid("74000", "Oklahoma"));
  EXPECT_TRUE(IsZipValid("97000", "OR"));
  EXPECT_TRUE(IsZipValid("97000", "Oregon"));
  EXPECT_TRUE(IsZipValid("15000", "PA"));
  EXPECT_TRUE(IsZipValid("19000", "Pennsylvania"));
  EXPECT_TRUE(IsZipValid("06000", "PR"));
  EXPECT_TRUE(IsZipValid("06000", "Puerto Rico"));
  EXPECT_TRUE(IsZipValid("96000", "PW"));
  EXPECT_TRUE(IsZipValid("96000", "Palau"));
  EXPECT_TRUE(IsZipValid("02000", "RI"));
  EXPECT_TRUE(IsZipValid("02000", "Rhode Island"));
  EXPECT_TRUE(IsZipValid("29000", "SC"));
  EXPECT_TRUE(IsZipValid("29000", "South Carolina"));
  EXPECT_TRUE(IsZipValid("57000", "SD"));
  EXPECT_TRUE(IsZipValid("57000", "South Dakota"));
  EXPECT_TRUE(IsZipValid("37000", "TN"));
  EXPECT_TRUE(IsZipValid("38000", "Tennessee"));
  EXPECT_TRUE(IsZipValid("75000", "TX"));
  EXPECT_TRUE(IsZipValid("79000", "Texas"));
  EXPECT_TRUE(IsZipValid("84000", "UT"));
  EXPECT_TRUE(IsZipValid("84000", "Utah"));
  EXPECT_TRUE(IsZipValid("22000", "VA"));
  EXPECT_TRUE(IsZipValid("24000", "Virginia"));
  EXPECT_TRUE(IsZipValid("06000", "VI"));
  EXPECT_TRUE(IsZipValid("09000", "Virgin Islands"));
  EXPECT_TRUE(IsZipValid("05000", "VT"));
  EXPECT_TRUE(IsZipValid("05000", "Vermont"));
  EXPECT_TRUE(IsZipValid("98000", "WA"));
  EXPECT_TRUE(IsZipValid("99000", "Washington"));
  EXPECT_TRUE(IsZipValid("53000", "WI"));
  EXPECT_TRUE(IsZipValid("54000", "Wisconsin"));
  EXPECT_TRUE(IsZipValid("24000", "WV"));
  EXPECT_TRUE(IsZipValid("26000", "West Virginia"));
  EXPECT_TRUE(IsZipValid("82000", "WY"));
  EXPECT_TRUE(IsZipValid("83000", "Wyoming"));
}

TEST_F(AddressParserTest, LocationName) {
  EXPECT_FALSE(IsLocationName("str-eet"));
  EXPECT_FALSE(IsLocationName("somewhere"));

  // Test all supported street names and expected plural cases.
  EXPECT_TRUE(IsLocationName("alley"));
  EXPECT_TRUE(IsLocationName("annex"));
  EXPECT_TRUE(IsLocationName("arcade"));
  EXPECT_TRUE(IsLocationName("ave."));
  EXPECT_TRUE(IsLocationName("avenue"));
  EXPECT_TRUE(IsLocationName("alameda"));
  EXPECT_TRUE(IsLocationName("bayou"));
  EXPECT_TRUE(IsLocationName("beach"));
  EXPECT_TRUE(IsLocationName("bend"));
  EXPECT_TRUE(IsLocationName("bluff"));
  EXPECT_TRUE(IsLocationName("bluffs"));
  EXPECT_TRUE(IsLocationName("bottom"));
  EXPECT_TRUE(IsLocationName("boulevard"));
  EXPECT_TRUE(IsLocationName("branch"));
  EXPECT_TRUE(IsLocationName("bridge"));
  EXPECT_TRUE(IsLocationName("brook"));
  EXPECT_TRUE(IsLocationName("brooks"));
  EXPECT_TRUE(IsLocationName("burg"));
  EXPECT_TRUE(IsLocationName("burgs"));
  EXPECT_TRUE(IsLocationName("bypass"));
  EXPECT_TRUE(IsLocationName("broadway"));
  EXPECT_TRUE(IsLocationName("camino"));
  EXPECT_TRUE(IsLocationName("camp"));
  EXPECT_TRUE(IsLocationName("canyon"));
  EXPECT_TRUE(IsLocationName("cape"));
  EXPECT_TRUE(IsLocationName("causeway"));
  EXPECT_TRUE(IsLocationName("center"));
  EXPECT_TRUE(IsLocationName("centers"));
  EXPECT_TRUE(IsLocationName("circle"));
  EXPECT_TRUE(IsLocationName("circles"));
  EXPECT_TRUE(IsLocationName("cliff"));
  EXPECT_TRUE(IsLocationName("cliffs"));
  EXPECT_TRUE(IsLocationName("club"));
  EXPECT_TRUE(IsLocationName("common"));
  EXPECT_TRUE(IsLocationName("corner"));
  EXPECT_TRUE(IsLocationName("corners"));
  EXPECT_TRUE(IsLocationName("course"));
  EXPECT_TRUE(IsLocationName("court"));
  EXPECT_TRUE(IsLocationName("courts"));
  EXPECT_TRUE(IsLocationName("cove"));
  EXPECT_TRUE(IsLocationName("coves"));
  EXPECT_TRUE(IsLocationName("creek"));
  EXPECT_TRUE(IsLocationName("crescent"));
  EXPECT_TRUE(IsLocationName("crest"));
  EXPECT_TRUE(IsLocationName("crossing"));
  EXPECT_TRUE(IsLocationName("crossroad"));
  EXPECT_TRUE(IsLocationName("curve"));
  EXPECT_TRUE(IsLocationName("circulo"));
  EXPECT_TRUE(IsLocationName("dale"));
  EXPECT_TRUE(IsLocationName("dam"));
  EXPECT_TRUE(IsLocationName("divide"));
  EXPECT_TRUE(IsLocationName("drive"));
  EXPECT_TRUE(IsLocationName("drives"));
  EXPECT_TRUE(IsLocationName("estate"));
  EXPECT_TRUE(IsLocationName("estates"));
  EXPECT_TRUE(IsLocationName("expressway"));
  EXPECT_TRUE(IsLocationName("extension"));
  EXPECT_TRUE(IsLocationName("extensions"));
  EXPECT_TRUE(IsLocationName("fall"));
  EXPECT_TRUE(IsLocationName("falls"));
  EXPECT_TRUE(IsLocationName("ferry"));
  EXPECT_TRUE(IsLocationName("field"));
  EXPECT_TRUE(IsLocationName("fields"));
  EXPECT_TRUE(IsLocationName("flat"));
  EXPECT_TRUE(IsLocationName("flats"));
  EXPECT_TRUE(IsLocationName("ford"));
  EXPECT_TRUE(IsLocationName("fords"));
  EXPECT_TRUE(IsLocationName("forest"));
  EXPECT_TRUE(IsLocationName("forge"));
  EXPECT_TRUE(IsLocationName("forges"));
  EXPECT_TRUE(IsLocationName("fork"));
  EXPECT_TRUE(IsLocationName("forks"));
  EXPECT_TRUE(IsLocationName("fort"));
  EXPECT_TRUE(IsLocationName("freeway"));
  EXPECT_TRUE(IsLocationName("garden"));
  EXPECT_TRUE(IsLocationName("gardens"));
  EXPECT_TRUE(IsLocationName("gateway"));
  EXPECT_TRUE(IsLocationName("glen"));
  EXPECT_TRUE(IsLocationName("glens"));
  EXPECT_TRUE(IsLocationName("green"));
  EXPECT_TRUE(IsLocationName("greens"));
  EXPECT_TRUE(IsLocationName("grove"));
  EXPECT_TRUE(IsLocationName("groves"));
  EXPECT_TRUE(IsLocationName("harbor"));
  EXPECT_TRUE(IsLocationName("harbors"));
  EXPECT_TRUE(IsLocationName("haven"));
  EXPECT_TRUE(IsLocationName("heights"));
  EXPECT_TRUE(IsLocationName("highway"));
  EXPECT_TRUE(IsLocationName("hill"));
  EXPECT_TRUE(IsLocationName("hills"));
  EXPECT_TRUE(IsLocationName("hollow"));
  EXPECT_TRUE(IsLocationName("inlet"));
  EXPECT_TRUE(IsLocationName("island"));
  EXPECT_TRUE(IsLocationName("islands"));
  EXPECT_TRUE(IsLocationName("isle"));
  EXPECT_TRUE(IsLocationName("junction"));
  EXPECT_TRUE(IsLocationName("junctions"));
  EXPECT_TRUE(IsLocationName("key"));
  EXPECT_TRUE(IsLocationName("keys"));
  EXPECT_TRUE(IsLocationName("knoll"));
  EXPECT_TRUE(IsLocationName("knolls"));
  EXPECT_TRUE(IsLocationName("lake"));
  EXPECT_TRUE(IsLocationName("lakes"));
  EXPECT_TRUE(IsLocationName("land"));
  EXPECT_TRUE(IsLocationName("landing"));
  EXPECT_TRUE(IsLocationName("lane"));
  EXPECT_TRUE(IsLocationName("light"));
  EXPECT_TRUE(IsLocationName("lights"));
  EXPECT_TRUE(IsLocationName("loaf"));
  EXPECT_TRUE(IsLocationName("lock"));
  EXPECT_TRUE(IsLocationName("locks"));
  EXPECT_TRUE(IsLocationName("lodge"));
  EXPECT_TRUE(IsLocationName("loop"));
  EXPECT_TRUE(IsLocationName("mall"));
  EXPECT_TRUE(IsLocationName("manor"));
  EXPECT_TRUE(IsLocationName("manors"));
  EXPECT_TRUE(IsLocationName("meadow"));
  EXPECT_TRUE(IsLocationName("meadows"));
  EXPECT_TRUE(IsLocationName("mews"));
  EXPECT_TRUE(IsLocationName("mill"));
  EXPECT_TRUE(IsLocationName("mills"));
  EXPECT_TRUE(IsLocationName("mission"));
  EXPECT_TRUE(IsLocationName("motorway"));
  EXPECT_TRUE(IsLocationName("mount"));
  EXPECT_TRUE(IsLocationName("mountain"));
  EXPECT_TRUE(IsLocationName("mountains"));
  EXPECT_TRUE(IsLocationName("neck"));
  EXPECT_TRUE(IsLocationName("orchard"));
  EXPECT_TRUE(IsLocationName("oval"));
  EXPECT_TRUE(IsLocationName("overpass"));
  EXPECT_TRUE(IsLocationName("park"));
  EXPECT_TRUE(IsLocationName("parks"));
  EXPECT_TRUE(IsLocationName("parkway"));
  EXPECT_TRUE(IsLocationName("parkways"));
  EXPECT_TRUE(IsLocationName("pass"));
  EXPECT_TRUE(IsLocationName("passage"));
  EXPECT_TRUE(IsLocationName("path"));
  EXPECT_TRUE(IsLocationName("pike"));
  EXPECT_TRUE(IsLocationName("pine"));
  EXPECT_TRUE(IsLocationName("pines"));
  EXPECT_TRUE(IsLocationName("plain"));
  EXPECT_TRUE(IsLocationName("plains"));
  EXPECT_TRUE(IsLocationName("plaza"));
  EXPECT_TRUE(IsLocationName("point"));
  EXPECT_TRUE(IsLocationName("points"));
  EXPECT_TRUE(IsLocationName("port"));
  EXPECT_TRUE(IsLocationName("ports"));
  EXPECT_TRUE(IsLocationName("prairie"));
  EXPECT_TRUE(IsLocationName("privada"));
  EXPECT_TRUE(IsLocationName("radial"));
  EXPECT_TRUE(IsLocationName("ramp"));
  EXPECT_TRUE(IsLocationName("ranch"));
  EXPECT_TRUE(IsLocationName("rapid"));
  EXPECT_TRUE(IsLocationName("rapids"));
  EXPECT_TRUE(IsLocationName("rest"));
  EXPECT_TRUE(IsLocationName("ridge"));
  EXPECT_TRUE(IsLocationName("ridges"));
  EXPECT_TRUE(IsLocationName("river"));
  EXPECT_TRUE(IsLocationName("road"));
  EXPECT_TRUE(IsLocationName("roads"));
  EXPECT_TRUE(IsLocationName("route"));
  EXPECT_TRUE(IsLocationName("row"));
  EXPECT_TRUE(IsLocationName("rue"));
  EXPECT_TRUE(IsLocationName("run"));
  EXPECT_TRUE(IsLocationName("shoal"));
  EXPECT_TRUE(IsLocationName("shoals"));
  EXPECT_TRUE(IsLocationName("shore"));
  EXPECT_TRUE(IsLocationName("shores"));
  EXPECT_TRUE(IsLocationName("skyway"));
  EXPECT_TRUE(IsLocationName("spring"));
  EXPECT_TRUE(IsLocationName("springs"));
  EXPECT_TRUE(IsLocationName("spur"));
  EXPECT_TRUE(IsLocationName("spurs"));
  EXPECT_TRUE(IsLocationName("square"));
  EXPECT_TRUE(IsLocationName("squares"));
  EXPECT_TRUE(IsLocationName("station"));
  EXPECT_TRUE(IsLocationName("stravenue"));
  EXPECT_TRUE(IsLocationName("stream"));
  EXPECT_TRUE(IsLocationName("st."));
  EXPECT_TRUE(IsLocationName("street"));
  EXPECT_TRUE(IsLocationName("streets"));
  EXPECT_TRUE(IsLocationName("summit"));
  EXPECT_TRUE(IsLocationName("speedway"));
  EXPECT_TRUE(IsLocationName("terrace"));
  EXPECT_TRUE(IsLocationName("throughway"));
  EXPECT_TRUE(IsLocationName("trace"));
  EXPECT_TRUE(IsLocationName("track"));
  EXPECT_TRUE(IsLocationName("trafficway"));
  EXPECT_TRUE(IsLocationName("trail"));
  EXPECT_TRUE(IsLocationName("tunnel"));
  EXPECT_TRUE(IsLocationName("turnpike"));
  EXPECT_TRUE(IsLocationName("underpass"));
  EXPECT_TRUE(IsLocationName("union"));
  EXPECT_TRUE(IsLocationName("unions"));
  EXPECT_TRUE(IsLocationName("valley"));
  EXPECT_TRUE(IsLocationName("valleys"));
  EXPECT_TRUE(IsLocationName("viaduct"));
  EXPECT_TRUE(IsLocationName("view"));
  EXPECT_TRUE(IsLocationName("views"));
  EXPECT_TRUE(IsLocationName("village"));
  EXPECT_TRUE(IsLocationName("villages"));
  EXPECT_TRUE(IsLocationName("ville"));
  EXPECT_TRUE(IsLocationName("vista"));
  EXPECT_TRUE(IsLocationName("walk"));
  EXPECT_TRUE(IsLocationName("walks"));
  EXPECT_TRUE(IsLocationName("wall"));
  EXPECT_TRUE(IsLocationName("way"));
  EXPECT_TRUE(IsLocationName("ways"));
  EXPECT_TRUE(IsLocationName("well"));
  EXPECT_TRUE(IsLocationName("wells"));
  EXPECT_TRUE(IsLocationName("xing"));
  EXPECT_TRUE(IsLocationName("xrd"));
}

TEST_F(AddressParserTest, NumberPrefixCases) {
  EXPECT_EQ(FindAddress("Cafe 21\n750 Fifth Ave. San Diego, California 92101"),
      "750 Fifth Ave. San Diego, California 92101");
  EXPECT_EQ(FindAddress(
      "Century City 15\n 10250 Santa Monica Boulevard Los Angeles, CA 90067"),
      "10250 Santa Monica Boulevard Los Angeles, CA 90067");
  EXPECT_EQ(FindAddress("123 45\n67 My Street, Somewhere, NY 10000"),
      "67 My Street, Somewhere, NY 10000");
  EXPECT_TRUE(IsAddress("123 4th Avenue, Somewhere in NY 10000"));
}

TEST_F(AddressParserTest, FullAddress) {
  // Test US Google corporate addresses. Expects a full string match.
  EXPECT_TRUE(IsAddress("1600 Amphitheatre Parkway Mountain View, CA 94043"));
  EXPECT_TRUE(IsAddress("201 S. Division St. Suite 500 Ann Arbor, MI 48104"));
  EXPECT_TRUE(ContainsAddress(
      "Millennium at Midtown 10 10th Street NE Suite 600 Atlanta, GA 30309"));
  EXPECT_TRUE(IsAddress(
      "9606 North MoPac Expressway Suite 400 Austin, TX 78759"));
  EXPECT_TRUE(IsAddress("2590 Pearl Street Suite 100 Boulder, CO 80302"));
  EXPECT_TRUE(IsAddress("5 Cambridge Center, Floors 3-6 Cambridge, MA 02142"));
  EXPECT_TRUE(IsAddress("410 Market St Suite 415 Chapel Hill, NC 27516"));
  EXPECT_TRUE(IsAddress("20 West Kinzie St. Chicago, IL 60654"));
  EXPECT_TRUE(IsAddress("114 Willits Street Birmingham, MI 48009"));
  EXPECT_TRUE(IsAddress("19540 Jamboree Road 2nd Floor Irvine, CA 92612"));
  EXPECT_TRUE(IsAddress("747 6th Street South, Kirkland, WA 98033"));
  EXPECT_TRUE(IsAddress("301 S. Blount St. Suite 301 Madison, WI 53703"));
  EXPECT_TRUE(IsAddress("76 Ninth Avenue 4th Floor New York, NY 10011"));
  EXPECT_TRUE(ContainsAddress(
      "Chelsea Markset Space, 75 Ninth Avenue 2nd and 4th Floors New York, \
      NY 10011"));
  EXPECT_TRUE(IsAddress("6425 Penn Ave. Suite 700 Pittsburgh, PA 15206"));
  EXPECT_TRUE(IsAddress("1818 Library Street Suite 400 Reston, VA 20190"));
  EXPECT_TRUE(IsAddress("345 Spear Street Floors 2-4 San Francisco, CA 94105"));
  EXPECT_TRUE(IsAddress("604 Arizona Avenue Santa Monica, CA 90401"));
  EXPECT_TRUE(IsAddress("651 N. 34th St. Seattle, WA 98103"));
  EXPECT_TRUE(IsAddress(
      "1101 New York Avenue, N.W. Second Floor Washington, DC 20005"));

  // Other tests.
  EXPECT_TRUE(IsAddress("57th Street and Lake Shore Drive\nChicago, IL 60637"));
  EXPECT_TRUE(IsAddress("308 Congress Street Boston, MA 02210"));
  EXPECT_TRUE(ContainsAddress(
      "Central Park West at 79th Street, New York, NY, 10024-5192"));
  EXPECT_TRUE(ContainsAddress(
      "Lincoln Park | 100 34th Avenue • San Francisco, CA 94121 | 41575036"));

  EXPECT_EQ(FindAddress("Lorem ipsum dolor sit amet, consectetur adipisicing " \
      "elit, sed do 1600 Amphitheatre Parkway Mountain View, CA 94043 " \
      "eiusmod tempor incididunt ut labore et dolore magna aliqua."),
      "1600 Amphitheatre Parkway Mountain View, CA 94043");

  EXPECT_EQ(FindAddress("2590 Pearl Street Suite 100 Boulder, CO 80302 6425 " \
      "Penn Ave. Suite 700 Pittsburgh, PA 15206"),
      "2590 Pearl Street Suite 100 Boulder, CO 80302");

  EXPECT_TRUE(ContainsAddress(
      "住所は 1600 Amphitheatre Parkway Mountain View, CA 94043 です。"));

  EXPECT_FALSE(ContainsAddress("1 st. too-short, CA 90000"));
  EXPECT_TRUE(ContainsAddress("1 st. long enough, CA 90000"));

  EXPECT_TRUE(ContainsAddress("1 st. some city in al 35000"));
  EXPECT_FALSE(ContainsAddress("1 book st Aquinas et al 35000"));

  EXPECT_FALSE(ContainsAddress("1 this comes too late: street, CA 90000"));
  EXPECT_TRUE(ContainsAddress("1 this is ok: street, CA 90000"));

  EXPECT_FALSE(ContainsAddress(
      "1 street I love verbosity, so I'm writing an address with too many " \
      "words CA 90000"));
  EXPECT_TRUE(ContainsAddress("1 street 2 3 4 5 6 7 8 9 10 11 12, CA 90000"));

  EXPECT_TRUE(IsAddress("79th Street 1st Floor New York City, NY 10024-5192"));

  EXPECT_FALSE(ContainsAddress("123 Fake Street, Springfield, Springfield"));
  EXPECT_FALSE(ContainsAddress("999 Street Avenue, City, ZZ 98765"));
  EXPECT_FALSE(ContainsAddress("76 Here be dragons, CA 94043"));
  EXPECT_FALSE(ContainsAddress("1 This, has, too* many, lines, to, be* valid"));
  EXPECT_FALSE(ContainsAddress(
      "1 Supercalifragilisticexpialidocious is too long, CA 90000"));
  EXPECT_FALSE(ContainsAddress(""));
}
