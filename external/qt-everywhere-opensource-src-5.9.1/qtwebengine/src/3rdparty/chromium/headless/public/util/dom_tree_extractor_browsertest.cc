// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/dom_tree_extractor.h"

#include <memory>
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/emulation.h"
#include "headless/public/devtools/domains/network.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace headless {

namespace {

std::string NormaliseJSON(const std::string& json) {
  std::unique_ptr<base::Value> parsed_json = base::JSONReader::Read(json);
  DCHECK(parsed_json);
  std::string normalized_json;
  base::JSONWriter::WriteWithOptions(
      *parsed_json, base::JSONWriter::OPTIONS_PRETTY_PRINT, &normalized_json);
  return normalized_json;
}

}  // namespace

class DomTreeExtractorBrowserTest : public HeadlessAsyncDevTooledBrowserTest,
                                    public page::Observer {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable();
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/dom_tree_test.html").spec());
  }

  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    devtools_client_->GetPage()->RemoveObserver(this);

    extractor_.reset(new DomTreeExtractor(devtools_client_.get()));

    std::vector<std::string> css_whitelist = {
        "color",        "display",    "font-style",   "margin-left",
        "margin-right", "margin-top", "margin-bottom"};
    extractor_->ExtractDomTree(
        css_whitelist,
        base::Bind(&DomTreeExtractorBrowserTest::OnDomTreeExtracted,
                   base::Unretained(this)));
  }

  void OnDomTreeExtracted(DomTreeExtractor::DomTree dom_tree) {
    GURL::Replacements replace_port;
    replace_port.SetPortStr("");

    std::vector<std::unique_ptr<base::DictionaryValue>> dom_nodes(
        dom_tree.dom_nodes_.size());

    // For convenience flatten the dom tree into an array.
    for (size_t i = 0; i < dom_tree.dom_nodes_.size(); i++) {
      dom::Node* node = const_cast<dom::Node*>(dom_tree.dom_nodes_[i]);

      dom_nodes[i].reset(
          static_cast<base::DictionaryValue*>(node->Serialize().release()));

      // Convert child & content document pointers into indexes.
      if (node->HasChildren()) {
        std::unique_ptr<base::ListValue> children(new base::ListValue());
        for (const std::unique_ptr<dom::Node>& child : *node->GetChildren()) {
          children->AppendInteger(
              dom_tree.node_id_to_index_[child->GetNodeId()]);
        }
        dom_nodes[i]->Set("childIndices", std::move(children));
        dom_nodes[i]->Remove("children", nullptr);
      }

      if (node->HasContentDocument()) {
        dom_nodes[i]->SetInteger(
            "contentDocumentIndex",
            dom_tree
                .node_id_to_index_[node->GetContentDocument()->GetNodeId()]);
        dom_nodes[i]->Remove("contentDocument", nullptr);
      }

      dom_nodes[i]->Remove("childNodeCount", nullptr);

      // Frame IDs are random.
      if (dom_nodes[i]->HasKey("frameId"))
        dom_nodes[i]->SetString("frameId", "?");

      // Ports are random.
      std::string url;
      if (dom_nodes[i]->GetString("baseURL", &url)) {
        dom_nodes[i]->SetString(
            "baseURL", GURL(url).ReplaceComponents(replace_port).spec());
      }

      if (dom_nodes[i]->GetString("documentURL", &url)) {
        dom_nodes[i]->SetString(
            "documentURL", GURL(url).ReplaceComponents(replace_port).spec());
      }
    }

    // Merge LayoutTreeNode data into the dictionaries.
    for (const css::LayoutTreeNode* layout_node : dom_tree.layout_tree_nodes_) {
      auto it = dom_tree.node_id_to_index_.find(layout_node->GetNodeId());
      ASSERT_TRUE(it != dom_tree.node_id_to_index_.end());

      base::DictionaryValue* node_dict = dom_nodes[it->second].get();
      node_dict->Set("boundingBox", layout_node->GetBoundingBox()->Serialize());

      if (layout_node->HasLayoutText())
        node_dict->SetString("layoutText", layout_node->GetLayoutText());

      if (layout_node->HasStyleIndex())
        node_dict->SetInteger("styleIndex", layout_node->GetStyleIndex());

      if (layout_node->HasInlineTextNodes()) {
        std::unique_ptr<base::ListValue> inline_text_nodes(
            new base::ListValue());
        for (const std::unique_ptr<css::InlineTextBox>& inline_text_box :
             *layout_node->GetInlineTextNodes()) {
          size_t index = inline_text_nodes->GetSize();
          inline_text_nodes->Set(index, inline_text_box->Serialize());
        }
        node_dict->Set("inlineTextNodes", std::move(inline_text_nodes));
      }
    }

    std::vector<std::unique_ptr<base::DictionaryValue>> computed_styles(
        dom_tree.computed_styles_.size());

    for (size_t i = 0; i < dom_tree.computed_styles_.size(); i++) {
      std::unique_ptr<base::DictionaryValue> style(new base::DictionaryValue());
      for (const auto& style_property :
           *dom_tree.computed_styles_[i]->GetProperties()) {
        style->SetString(style_property->GetName(), style_property->GetValue());
      }
      computed_styles[i] = std::move(style);
    }

    const std::vector<std::string> expected_dom_nodes = {
        R"raw_string({
           "backendNodeId": 3,
           "baseURL": "http://127.0.0.1/dom_tree_test.html",
           "boundingBox": {
              "height": 600.0,
              "width": 800.0,
              "x": 0.0,
              "y": 0.0
           },
           "childIndices": [ 1 ],
           "documentURL": "http://127.0.0.1/dom_tree_test.html",
           "localName": "",
           "nodeId": 1,
           "nodeName": "#document",
           "nodeType": 9,
           "nodeValue": "",
           "xmlVersion": ""
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 4,
           "boundingBox": {
              "height": 600.0,
              "width": 800.0,
              "x": 0.0,
              "y": 0.0
           },
           "childIndices": [ 2, 5 ],
           "frameId": "?",
           "localName": "html",
           "nodeId": 2,
           "nodeName": "HTML",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 0
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 5,
           "childIndices": [ 3 ],
           "localName": "head",
           "nodeId": 3,
           "nodeName": "HEAD",
           "nodeType": 1,
           "nodeValue": ""
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 6,
           "childIndices": [ 4 ],
           "localName": "title",
           "nodeId": 4,
           "nodeName": "TITLE",
           "nodeType": 1,
           "nodeValue": ""
        })raw_string",

        R"raw_string({
           "backendNodeId": 7,
           "localName": "",
           "nodeId": 5,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Hello world!"
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 8,
           "boundingBox": {
              "height": 584.0,
              "width": 784.0,
              "x": 8.0,
              "y": 8.0
           },
           "childIndices": [ 6 ],
           "localName": "body",
           "nodeId": 6,
           "nodeName": "BODY",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 1
        })raw_string",

        R"raw_string({
           "attributes": [ "id", "id1" ],
           "backendNodeId": 9,
           "boundingBox": {
              "height": 367.0,
              "width": 784.0,
              "x": 8.0,
              "y": 8.0
           },
           "childIndices": [ 7, 9, 16 ],
           "localName": "div",
           "nodeId": 7,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 0
        })raw_string",

        R"raw_string({
           "attributes": [ "style", "color: red" ],
           "backendNodeId": 10,
           "boundingBox": {
              "height": 37.0,
              "width": 784.0,
              "x": 8.0,
              "y": 8.0
           },
           "childIndices": [ 8 ],
           "localName": "h1",
           "nodeId": 8,
           "nodeName": "H1",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 2
        })raw_string",

        R"raw_string({
           "backendNodeId": 11,
           "boundingBox": {
              "height": 36.0,
              "width": 143.0,
              "x": 8.0,
              "y": 8.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 36.0,
                 "width": 142.171875,
                 "x": 8.0,
                 "y": 8.0
              },
              "numCharacters": 10,
              "startCharacterIndex": 0
           } ],
           "layoutText": "Some text.",
           "localName": "",
           "nodeId": 9,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Some text.",
           "styleIndex": 2
        })raw_string",

        R"raw_string({
           "attributes": [
             "src", "/iframe.html", "width", "400", "height", "200" ],
           "backendNodeId": 12,
           "boundingBox": {
              "height": 205.0,
              "width": 404.0,
              "x": 8.0,
              "y": 66.0
           },
           "childIndices": [  ],
           "contentDocumentIndex": 10,
           "frameId": "?",
           "localName": "iframe",
           "nodeId": 10,
           "nodeName": "IFRAME",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 4
        })raw_string",

        R"raw_string({
           "backendNodeId": 13,
           "baseURL": "http://127.0.0.1/iframe.html",
           "childIndices": [ 11 ],
           "documentURL": "http://127.0.0.1/iframe.html",
           "localName": "",
           "nodeId": 11,
           "nodeName": "#document",
           "nodeType": 9,
           "nodeValue": "",
           "xmlVersion": ""
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 14,
           "boundingBox": {
              "height": 200.0,
              "width": 400.0,
              "x": 10.0,
              "y": 68.0
           },
           "childIndices": [ 12, 13 ],
           "frameId": "?",
           "localName": "html",
           "nodeId": 12,
           "nodeName": "HTML",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 0
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 15,
           "childIndices": [  ],
           "localName": "head",
           "nodeId": 13,
           "nodeName": "HEAD",
           "nodeType": 1,
           "nodeValue": ""
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 16,
           "boundingBox": {
              "height": 171.0,
              "width": 384.0,
              "x": 18.0,
              "y": 76.0
           },
           "childIndices": [ 14 ],
           "localName": "body",
           "nodeId": 14,
           "nodeName": "BODY",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 1
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 17,
           "boundingBox": {
              "height": 37.0,
              "width": 384.0,
              "x": 18.0,
              "y": 76.0
           },
           "childIndices": [ 15 ],
           "localName": "h1",
           "nodeId": 15,
           "nodeName": "H1",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 3
        })raw_string",

        R"raw_string({
           "backendNodeId": 18,
           "boundingBox": {
              "height": 36.0,
              "width": 308.0,
              "x": 8.0,
              "y": 8.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 36.0,
                 "width": 307.734375,
                 "x": 8.0,
                 "y": 8.0
              },
              "numCharacters": 22,
              "startCharacterIndex": 0
           } ],
           "layoutText": "Hello from the iframe!",
           "localName": "",
           "nodeId": 16,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Hello from the iframe!",
           "styleIndex": 3
        })raw_string",

        R"raw_string({
           "attributes": [ "id", "id2" ],
           "backendNodeId": 19,
           "boundingBox": {
              "height": 105.0,
              "width": 784.0,
              "x": 8.0,
              "y": 270.0
           },
           "childIndices": [ 17 ],
           "localName": "div",
           "nodeId": 17,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 0
        })raw_string",

        R"raw_string({
           "attributes": [ "id", "id3" ],
           "backendNodeId": 20,
           "boundingBox": {
              "height": 105.0,
              "width": 784.0,
              "x": 8.0,
              "y": 270.0
           },
           "childIndices": [ 18 ],
           "localName": "div",
           "nodeId": 18,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 0
        })raw_string",

        R"raw_string({
           "attributes": [ "id", "id4" ],
           "backendNodeId": 21,
           "boundingBox": {
              "height": 105.0,
              "width": 784.0,
              "x": 8.0,
              "y": 270.0
           },
           "childIndices": [ 19, 21, 23, 24 ],
           "localName": "div",
           "nodeId": 19,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 0
        })raw_string",

        R"raw_string({
           "attributes": [ "href", "https://www.google.com" ],
           "backendNodeId": 22,
           "boundingBox": {
              "height": 18.0,
              "width": 53.0,
              "x": 8.0,
              "y": 270.0
           },
           "childIndices": [ 20 ],
           "localName": "a",
           "nodeId": 20,
           "nodeName": "A",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 5
        })raw_string",

        R"raw_string({
           "backendNodeId": 23,
           "boundingBox": {
              "height": 18.0,
              "width": 53.0,
              "x": 8.0,
              "y": 270.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 17.0,
                 "width": 52.421875,
                 "x": 8.0,
                 "y": 270.4375
              },
              "numCharacters": 7,
              "startCharacterIndex": 0
           } ],
           "layoutText": "Google!",
           "localName": "",
           "nodeId": 21,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Google!",
           "styleIndex": 5
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 24,
           "boundingBox": {
              "height": 19.0,
              "width": 784.0,
              "x": 8.0,
              "y": 304.0
           },
           "childIndices": [ 22 ],
           "localName": "p",
           "nodeId": 22,
           "nodeName": "P",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 6
        })raw_string",

        R"raw_string({
           "backendNodeId": 25,
           "boundingBox": {
              "height": 18.0,
              "width": 85.0,
              "x": 8.0,
              "y": 304.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 17.0,
                 "width": 84.84375,
                 "x": 8.0,
                 "y": 304.4375
              },
              "numCharacters": 12,
              "startCharacterIndex": 0
           } ],
           "layoutText": "A paragraph!",
           "localName": "",
           "nodeId": 23,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "A paragraph!",
           "styleIndex": 6
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 26,
           "boundingBox": {
              "height": 0.0,
              "width": 0.0,
              "x": 0.0,
              "y": 0.0
           },
           "childIndices": [  ],
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 17.0,
                 "width": 0.0,
                 "x": 8.0,
                 "y": 338.4375
              },
              "numCharacters": 1,
              "startCharacterIndex": 0
           } ],
           "layoutText": "\n",
           "localName": "br",
           "nodeId": 24,
           "nodeName": "BR",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 4
        })raw_string",

        R"raw_string({
           "attributes": [ "style", "color: green" ],
           "backendNodeId": 27,
           "boundingBox": {
              "height": 19.0,
              "width": 784.0,
              "x": 8.0,
              "y": 356.0
           },
           "childIndices": [ 25, 26, 28 ],
           "localName": "div",
           "nodeId": 25,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 7
        })raw_string",

        R"raw_string({
           "backendNodeId": 28,
           "boundingBox": {
              "height": 18.0,
              "width": 41.0,
              "x": 8.0,
              "y": 356.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 17.0,
                 "width": 40.4375,
                 "x": 8.0,
                 "y": 356.4375
              },
              "numCharacters": 5,
              "startCharacterIndex": 0
           } ],
           "layoutText": "Some ",
           "localName": "",
           "nodeId": 26,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Some ",
           "styleIndex": 7
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 29,
           "boundingBox": {
              "height": 18.0,
              "width": 37.0,
              "x": 48.0,
              "y": 356.0
           },
           "childIndices": [ 27 ],
           "localName": "em",
           "nodeId": 27,
           "nodeName": "EM",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 8
        })raw_string",

        R"raw_string({
           "backendNodeId": 30,
           "boundingBox": {
              "height": 18.0,
              "width": 37.0,
              "x": 48.0,
              "y": 356.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 17.0,
                 "width": 35.828125,
                 "x": 48.4375,
                 "y": 356.4375
              },
              "numCharacters": 5,
              "startCharacterIndex": 0
           } ],
           "layoutText": "green",
           "localName": "",
           "nodeId": 28,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "green",
           "styleIndex": 8
        })raw_string",

        R"raw_string({
           "backendNodeId": 31,
           "boundingBox": {
              "height": 18.0,
              "width": 41.0,
              "x": 84.0,
              "y": 356.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 17.0,
                 "width": 39.984375,
                 "x": 84.265625,
                 "y": 356.4375
              },
              "numCharacters": 8,
              "startCharacterIndex": 0
           } ],
           "layoutText": " text...",
           "localName": "",
           "nodeId": 29,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": " text...",
           "styleIndex": 7
        })raw_string"};

    EXPECT_EQ(expected_dom_nodes.size(), dom_nodes.size());

    for (size_t i = 0; i < dom_nodes.size(); i++) {
      std::string result_json;
      base::JSONWriter::WriteWithOptions(
          *dom_nodes[i], base::JSONWriter::OPTIONS_PRETTY_PRINT, &result_json);

      ASSERT_LT(i, expected_dom_nodes.size());
      EXPECT_EQ(NormaliseJSON(expected_dom_nodes[i]), result_json) << " Node # "
                                                                   << i;
    }

    const std::vector<std::string> expected_styles = {
        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-style": "normal",
           "margin-bottom": "8px",
           "margin-left": "8px",
           "margin-right": "8px",
           "margin-top": "8px"
        })raw_string",

        R"raw_string({
           "color": "rgb(255, 0, 0)",
           "display": "block",
           "font-style": "normal",
           "margin-bottom": "21.44px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "21.44px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-style": "normal",
           "margin-bottom": "21.44px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "21.44px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "inline",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 238)",
           "display": "inline",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-style": "normal",
           "margin-bottom": "16px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "16px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 128, 0)",
           "display": "block",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 128, 0)",
           "display": "inline",
           "font-style": "italic",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string"};

    for (size_t i = 0; i < computed_styles.size(); i++) {
      std::string result_json;
      base::JSONWriter::WriteWithOptions(*computed_styles[i],
                                         base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                         &result_json);

      ASSERT_LT(i, expected_styles.size());
      EXPECT_EQ(NormaliseJSON(expected_styles[i]), result_json) << " Style # "
                                                                << i;
    }

    FinishAsynchronousTest();
  }

  std::unique_ptr<DomTreeExtractor> extractor_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(DomTreeExtractorBrowserTest);

}  // namespace headless
