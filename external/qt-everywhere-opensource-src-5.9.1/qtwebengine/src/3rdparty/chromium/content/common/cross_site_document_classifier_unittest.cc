// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_piece.h"
#include "content/common/cross_site_document_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;

namespace content {

TEST(CrossSiteDocumentClassifierTest, IsBlockableScheme) {
  GURL data_url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAA==");
  GURL ftp_url("ftp://google.com");
  GURL mailto_url("mailto:google@google.com");
  GURL about_url("about:chrome");
  GURL http_url("http://google.com");
  GURL https_url("https://google.com");

  EXPECT_FALSE(CrossSiteDocumentClassifier::IsBlockableScheme(data_url));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsBlockableScheme(ftp_url));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsBlockableScheme(mailto_url));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsBlockableScheme(about_url));
  EXPECT_TRUE(CrossSiteDocumentClassifier::IsBlockableScheme(http_url));
  EXPECT_TRUE(CrossSiteDocumentClassifier::IsBlockableScheme(https_url));
}

TEST(CrossSiteDocumentClassifierTest, IsSameSite) {
  GURL a_com_url0("https://mock1.a.com:8080/page1.html");
  GURL a_com_url1("https://mock2.a.com:9090/page2.html");
  GURL a_com_url2("https://a.com/page3.html");
  EXPECT_TRUE(CrossSiteDocumentClassifier::IsSameSite(a_com_url0, a_com_url1));
  EXPECT_TRUE(CrossSiteDocumentClassifier::IsSameSite(a_com_url1, a_com_url2));
  EXPECT_TRUE(CrossSiteDocumentClassifier::IsSameSite(a_com_url2, a_com_url0));

  GURL b_com_url0("https://mock1.b.com/index.html");
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsSameSite(a_com_url0, b_com_url0));

  GURL about_blank_url("about:blank");
  EXPECT_FALSE(
      CrossSiteDocumentClassifier::IsSameSite(a_com_url0, about_blank_url));

  GURL chrome_url("chrome://extension");
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsSameSite(a_com_url0, chrome_url));

  GURL empty_url("");
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsSameSite(a_com_url0, empty_url));
}

TEST(CrossSiteDocumentClassifierTest, IsValidCorsHeaderSet) {
  GURL frame_origin("http://www.google.com");
  GURL site_origin("http://www.yahoo.com");

  EXPECT_TRUE(CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
      frame_origin, site_origin, "*"));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
      frame_origin, site_origin, "\"*\""));
  EXPECT_TRUE(CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
      frame_origin, site_origin, "http://mail.google.com"));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
      frame_origin, site_origin, "https://mail.google.com"));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
      frame_origin, site_origin, "http://yahoo.com"));
  EXPECT_FALSE(CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
      frame_origin, site_origin, "www.google.com"));
}

TEST(CrossSiteDocumentClassifierTest, SniffForHTML) {
  StringPiece html_data("  \t\r\n    <HtMladfokadfkado");
  StringPiece comment_html_data(" <!-- this is comment --> <html><body>");
  StringPiece two_comments_html_data(
      "<!-- this is comment -->\n<!-- this is comment --><html><body>");
  StringPiece mixed_comments_html_data(
      "<!-- this is comment <!-- --> <script></script>");
  StringPiece non_html_data("        var name=window.location;\nadfadf");
  StringPiece comment_js_data(" <!-- this is comment -> document.write(1); ");
  StringPiece empty_data("");

  EXPECT_TRUE(CrossSiteDocumentClassifier::SniffForHTML(html_data));
  EXPECT_TRUE(CrossSiteDocumentClassifier::SniffForHTML(comment_html_data));
  EXPECT_TRUE(
      CrossSiteDocumentClassifier::SniffForHTML(two_comments_html_data));
  EXPECT_TRUE(
      CrossSiteDocumentClassifier::SniffForHTML(mixed_comments_html_data));
  EXPECT_FALSE(CrossSiteDocumentClassifier::SniffForHTML(non_html_data));
  EXPECT_FALSE(CrossSiteDocumentClassifier::SniffForHTML(comment_js_data));

  // Basic bounds check.
  EXPECT_FALSE(CrossSiteDocumentClassifier::SniffForHTML(empty_data));
}

TEST(CrossSiteDocumentClassifierTest, SniffForXML) {
  StringPiece xml_data("   \t \r \n     <?xml version=\"1.0\"?>\n <catalog");
  StringPiece non_xml_data("        var name=window.location;\nadfadf");
  StringPiece empty_data("");

  EXPECT_TRUE(CrossSiteDocumentClassifier::SniffForXML(xml_data));
  EXPECT_FALSE(CrossSiteDocumentClassifier::SniffForXML(non_xml_data));

  // Basic bounds check.
  EXPECT_FALSE(CrossSiteDocumentClassifier::SniffForXML(empty_data));
}

TEST(CrossSiteDocumentClassifierTest, SniffForJSON) {
  StringPiece json_data("\t\t\r\n   { \"name\" : \"chrome\", ");
  StringPiece non_json_data0("\t\t\r\n   { name : \"chrome\", ");
  StringPiece non_json_data1("\t\t\r\n   foo({ \"name\" : \"chrome\", ");
  StringPiece empty_data("");

  EXPECT_TRUE(CrossSiteDocumentClassifier::SniffForJSON(json_data));
  EXPECT_FALSE(CrossSiteDocumentClassifier::SniffForJSON(non_json_data0));
  EXPECT_FALSE(CrossSiteDocumentClassifier::SniffForJSON(non_json_data1));

  // Basic bounds check.
  EXPECT_FALSE(CrossSiteDocumentClassifier::SniffForJSON(empty_data));
}

}  // namespace content
