// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/savable_resources.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebElementCollection.h"
#include "third_party/WebKit/public/web/WebFrameSerializer.h"
#include "third_party/WebKit/public/web/WebFrameSerializerClient.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebMetaElement.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebCString;
using blink::WebData;
using blink::WebDocument;
using blink::WebElement;
using blink::WebMetaElement;
using blink::WebElementCollection;
using blink::WebFrame;
using blink::WebFrameSerializer;
using blink::WebFrameSerializerClient;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebString;
using blink::WebURL;
using blink::WebView;
using blink::WebVector;

namespace content {

bool HasDocType(const WebDocument& doc) {
  return doc.firstChild().isDocumentTypeNode();
}

class LoadObserver : public RenderViewObserver {
 public:
  LoadObserver(RenderView* render_view, const base::Closure& quit_closure)
      : RenderViewObserver(render_view),
        quit_closure_(quit_closure) {}

  void DidFinishLoad(blink::WebLocalFrame* frame) override {
    if (frame == render_view()->GetWebView()->mainFrame())
      quit_closure_.Run();
  }

 private:
  // RenderViewObserver implementation.
  void OnDestruct() override { delete this; }

  base::Closure quit_closure_;
};

class DomSerializerTests : public ContentBrowserTest,
                           public WebFrameSerializerClient {
 public:
  DomSerializerTests() : serialization_reported_end_of_data_(false) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
#if defined(OS_WIN)
    // Don't want to try to create a GPU process.
    command_line->AppendSwitch(switches::kDisableGpu);
#endif
  }

  void SetUpOnMainThread() override {
    render_view_routing_id_ =
        shell()->web_contents()->GetRenderViewHost()->GetRoutingID();
  }

  // DomSerializerDelegate.
  void didSerializeDataForFrame(const WebCString& data,
                                FrameSerializationStatus status) override {
    // Check finish status of current frame.
    ASSERT_FALSE(serialization_reported_end_of_data_);

    // Add data to corresponding frame's content.
    serialized_contents_ += data;

    // Current frame is completed saving, change the finish status.
    if (status == WebFrameSerializerClient::CurrentFrameIsFinished)
      serialization_reported_end_of_data_ = true;
  }

  RenderView* GetRenderView() {
    return RenderView::FromRoutingID(render_view_routing_id_);
  }

  WebView* GetWebView() {
    return GetRenderView()->GetWebView();
  }

  WebFrame* GetMainFrame() {
    return GetWebView()->mainFrame();
  }

  WebFrame* FindSubFrameByURL(const GURL& url) {
    for (WebFrame* frame = GetWebView()->mainFrame(); frame;
        frame = frame->traverseNext(false)) {
      if (GURL(frame->document().url()) == url)
        return frame;
    }
    return nullptr;
  }

  // Load web page according to input content and relative URLs within
  // the document.
  void LoadContents(const std::string& contents,
                    const GURL& base_url,
                    const WebString encoding_info) {
    scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
    LoadObserver observer(GetRenderView(), runner->QuitClosure());

    // If input encoding is empty, use UTF-8 as default encoding.
    if (encoding_info.isEmpty()) {
      GetMainFrame()->loadHTMLString(contents, base_url);
    } else {
      WebData data(contents.data(), contents.length());

      // Do not use WebFrame.LoadHTMLString because it assumes that input
      // html contents use UTF-8 encoding.
      // TODO(darin): This should use WebFrame::loadData.
      WebFrame* web_frame = GetMainFrame();

      ASSERT_TRUE(web_frame != NULL);

      web_frame->toWebLocalFrame()->loadData(data, "text/html", encoding_info,
                                             base_url);
    }

    runner->Run();
  }

  class SingleLinkRewritingDelegate
      : public WebFrameSerializer::LinkRewritingDelegate {
   public:
    SingleLinkRewritingDelegate(const WebURL& url, const WebString& localPath)
        : url_(url), local_path_(localPath) {}

    bool rewriteFrameSource(WebFrame* frame,
                            WebString* rewritten_link) override {
      return false;
    }

    bool rewriteLink(const WebURL& url, WebString* rewritten_link) override {
      if (url != url_)
        return false;

      *rewritten_link = local_path_;
      return true;
    }

   private:
    const WebURL url_;
    const WebString local_path_;
  };

  // Serialize DOM belonging to a frame with the specified |frame_url|.
  void SerializeDomForURL(const GURL& frame_url) {
    // Find corresponding WebFrame according to frame_url.
    WebFrame* web_frame = FindSubFrameByURL(frame_url);
    ASSERT_TRUE(web_frame != NULL);
    WebString file_path =
        base::FilePath(FILE_PATH_LITERAL("c:\\dummy.htm")).AsUTF16Unsafe();
    SingleLinkRewritingDelegate delegate(frame_url, file_path);
    // Start serializing DOM.
    bool result = WebFrameSerializer::serialize(web_frame->toWebLocalFrame(),
                                                this, &delegate);
    ASSERT_TRUE(result);
  }

  void SerializeHTMLDOMWithDocTypeOnRenderer(const GURL& file_url) {
    // Make sure original contents have document type.
    WebFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(HasDocType(doc));
    // Do serialization.
    SerializeDomForURL(file_url);
    // Load the serialized contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    LoadContents(serialized_contents_, file_url,
                 web_frame->document().encoding());
    // Make sure serialized contents still have document type.
    web_frame = GetMainFrame();
    doc = web_frame->document();
    ASSERT_TRUE(HasDocType(doc));
  }

  void SerializeHTMLDOMWithoutDocTypeOnRenderer(const GURL& file_url) {
    // Make sure original contents do not have document type.
    WebFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(!HasDocType(doc));
    // Do serialization.
    SerializeDomForURL(file_url);
    // Load the serialized contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    LoadContents(serialized_contents_, file_url,
                 web_frame->document().encoding());
    // Make sure serialized contents do not have document type.
    web_frame = GetMainFrame();
    doc = web_frame->document();
    ASSERT_TRUE(!HasDocType(doc));
  }

  void SerializeXMLDocWithBuiltInEntitiesOnRenderer(
      const GURL& xml_file_url, const std::string& original_contents) {
    // Do serialization.
    SerializeDomForURL(xml_file_url);
    // Compare the serialized contents with original contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    ASSERT_EQ(original_contents, serialized_contents_);
  }

  void SerializeHTMLDOMWithAddingMOTWOnRenderer(
      const GURL& file_url, const std::string& original_contents) {
    // Make sure original contents does not have MOTW;
    std::string motw_declaration =
        WebFrameSerializer::generateMarkOfTheWebDeclaration(file_url).utf8();
    ASSERT_FALSE(motw_declaration.empty());
    // The encoding of original contents is ISO-8859-1, so we convert the MOTW
    // declaration to ASCII and search whether original contents has it or not.
    ASSERT_TRUE(std::string::npos == original_contents.find(motw_declaration));

    // Do serialization.
    SerializeDomForURL(file_url);
    // Make sure the serialized contents have MOTW ;
    ASSERT_TRUE(serialization_reported_end_of_data_);
    ASSERT_FALSE(std::string::npos ==
                 serialized_contents_.find(motw_declaration));
  }

  void SerializeHTMLDOMWithNoMetaCharsetInOriginalDocOnRenderer(
      const GURL& file_url) {
    // Make sure there is no META charset declaration in original document.
    WebFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    WebElement head_element = doc.head();
    ASSERT_TRUE(!head_element.isNull());
    // Go through all children of HEAD element.
    WebElementCollection meta_elements = head_element.
        getElementsByHTMLTagName("meta");
    for (WebElement element = meta_elements.firstItem(); !element.isNull();
       element = meta_elements.nextItem()) {
      ASSERT_TRUE(element.to<WebMetaElement>().computeEncoding().isEmpty());
    }
    // Do serialization.
    SerializeDomForURL(file_url);

    // Load the serialized contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    LoadContents(serialized_contents_, file_url,
                 web_frame->document().encoding());
    // Make sure the first child of HEAD element is META which has charset
    // declaration in serialized contents.
    web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != NULL);
    doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    head_element = doc.head();
    ASSERT_TRUE(!head_element.isNull());
    ASSERT_TRUE(!head_element.firstChild().isNull());
    ASSERT_TRUE(head_element.firstChild().isElementNode());
    WebMetaElement meta_element = head_element.firstChild().
        to<WebMetaElement>();
    ASSERT_EQ(meta_element.computeEncoding(), web_frame->document().encoding());

    // Make sure no more additional META tags which have charset declaration.
    meta_elements = head_element.getElementsByHTMLTagName("meta");
    for (WebElement element = meta_elements.firstItem(); !element.isNull();
       element = meta_elements.nextItem()) {
      if (element == meta_element)
        continue;
      ASSERT_TRUE(element.to<WebMetaElement>().computeEncoding().isEmpty());
    }
  }

  void SerializeHTMLDOMWithMultipleMetaCharsetInOriginalDocOnRenderer(
      const GURL& file_url) {
    // Make sure there are multiple META charset declarations in original
    // document.
    WebFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    WebElement head_element = doc.head();
    ASSERT_TRUE(!head_element.isNull());
    // Go through all children of HEAD element.
    int charset_declaration_count = 0;
    WebElementCollection meta_elements = head_element.
        getElementsByHTMLTagName("meta");
    for (WebElement element = meta_elements.firstItem(); !element.isNull();
       element = meta_elements.nextItem()) {
      if (!element.to<WebMetaElement>().computeEncoding().isEmpty())
        ++charset_declaration_count;
    }
    // The original doc has more than META tags which have charset declaration.
    ASSERT_TRUE(charset_declaration_count > 1);

    // Do serialization.
    SerializeDomForURL(file_url);

    // Load the serialized contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    LoadContents(serialized_contents_, file_url,
                 web_frame->document().encoding());
    // Make sure only first child of HEAD element is META which has charset
    // declaration in serialized contents.
    web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != NULL);
    doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    head_element = doc.head();
    ASSERT_TRUE(!head_element.isNull());
    ASSERT_TRUE(!head_element.firstChild().isNull());
    ASSERT_TRUE(head_element.firstChild().isElementNode());
    WebMetaElement meta_element = head_element.firstChild().
        to<WebMetaElement>();
    ASSERT_EQ(meta_element.computeEncoding(), web_frame->document().encoding());

    // Make sure no more additional META tags which have charset declaration.
    meta_elements = head_element.getElementsByHTMLTagName("meta");
    for (WebElement element = meta_elements.firstItem(); !element.isNull();
       element = meta_elements.nextItem()) {
      if (element == meta_element)
        continue;
      ASSERT_TRUE(element.to<WebMetaElement>().computeEncoding().isEmpty());
    }
  }

  void SerializeHTMLDOMWithEntitiesInTextOnRenderer() {
    base::FilePath page_file_path = GetTestFilePath(
        "dom_serializer", "dom_serializer/htmlentities_in_text.htm");
    // Get file URL. The URL is dummy URL to identify the following loading
    // actions. The test content is in constant:original_contents.
    GURL file_url = net::FilePathToFileURL(page_file_path);
    ASSERT_TRUE(file_url.SchemeIsFile());
    // Test contents.
    static const char* const original_contents =
        "<html><body>&amp;&lt;&gt;\"\'</body></html>";
    // Load the test contents.
    LoadContents(original_contents, file_url, WebString());

    // Get BODY's text content in DOM.
    WebFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    WebElement body_ele = doc.body();
    ASSERT_TRUE(!body_ele.isNull());
    WebNode text_node = body_ele.firstChild();
    ASSERT_TRUE(text_node.isTextNode());
    ASSERT_TRUE(std::string(text_node.nodeValue().utf8()) == "&<>\"\'");
    // Do serialization.
    SerializeDomForURL(file_url);
    // Compare the serialized contents with original contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    // Compare the serialized contents with original contents to make sure
    // they are same.
    // Because we add MOTW when serializing DOM, so before comparison, we also
    // need to add MOTW to original_contents.
    std::string original_str =
        WebFrameSerializer::generateMarkOfTheWebDeclaration(file_url).utf8();
    original_str += original_contents;
    // Since WebCore now inserts a new HEAD element if there is no HEAD element
    // when creating BODY element. (Please see
    // HTMLParser::bodyCreateErrorCheck.) We need to append the HEAD content and
    // corresponding META content if we find WebCore-generated HEAD element.
    if (!doc.head().isNull()) {
      WebString encoding = web_frame->document().encoding();
      std::string htmlTag("<html>");
      std::string::size_type pos = original_str.find(htmlTag);
      ASSERT_NE(std::string::npos, pos);
      pos += htmlTag.length();
      std::string head_part("<head>");
      head_part +=
          WebFrameSerializer::generateMetaCharsetDeclaration(encoding).utf8();
      head_part += "</head>";
      original_str.insert(pos, head_part);
    }
    ASSERT_EQ(original_str, serialized_contents_);
  }

  void SerializeHTMLDOMWithEntitiesInAttributeValueOnRenderer() {
    base::FilePath page_file_path = GetTestFilePath(
        "dom_serializer", "dom_serializer/htmlentities_in_attribute_value.htm");
    // Get file URL. The URL is dummy URL to identify the following loading
    // actions. The test content is in constant:original_contents.
    GURL file_url = net::FilePathToFileURL(page_file_path);
    ASSERT_TRUE(file_url.SchemeIsFile());
    // Test contents.
    static const char* const original_contents =
        "<html><body title=\"&amp;&lt;&gt;&quot;&#39;\"></body></html>";
    // Load the test contents.
    LoadContents(original_contents, file_url, WebString());
    // Get value of BODY's title attribute in DOM.
    WebFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    WebElement body_ele = doc.body();
    ASSERT_TRUE(!body_ele.isNull());
    WebString value = body_ele.getAttribute("title");
    ASSERT_TRUE(std::string(value.utf8()) == "&<>\"\'");
    // Do serialization.
    SerializeDomForURL(file_url);
    // Compare the serialized contents with original contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    // Compare the serialized contents with original contents to make sure
    // they are same.
    std::string original_str =
        WebFrameSerializer::generateMarkOfTheWebDeclaration(file_url).utf8();
    original_str += original_contents;
    if (!doc.isNull()) {
      WebString encoding = web_frame->document().encoding();
      std::string htmlTag("<html>");
      std::string::size_type pos = original_str.find(htmlTag);
      ASSERT_NE(std::string::npos, pos);
      pos += htmlTag.length();
      std::string head_part("<head>");
      head_part +=
          WebFrameSerializer::generateMetaCharsetDeclaration(encoding).utf8();
      head_part += "</head>";
      original_str.insert(pos, head_part);
    }
    ASSERT_EQ(original_str, serialized_contents_);
  }

  void SerializeHTMLDOMWithNonStandardEntitiesOnRenderer(const GURL& file_url) {
    // Get value of BODY's title attribute in DOM.
    WebFrame* web_frame = FindSubFrameByURL(file_url);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    WebElement body_element = doc.body();
    // Unescaped string for "&percnt;&nsup;&sup1;&apos;".
    static const wchar_t parsed_value[] = {
      '%', 0x2285, 0x00b9, '\'', 0
    };
    WebString value = body_element.getAttribute("title");
    WebString content = doc.contentAsTextForTesting();
    ASSERT_TRUE(base::UTF16ToWide(value) == parsed_value);
    ASSERT_TRUE(base::UTF16ToWide(content) == parsed_value);

    // Do serialization.
    SerializeDomForURL(file_url);
    // Check the serialized string.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    // Confirm that the serialized string has no non-standard HTML entities.
    ASSERT_EQ(std::string::npos, serialized_contents_.find("&percnt;"));
    ASSERT_EQ(std::string::npos, serialized_contents_.find("&nsup;"));
    ASSERT_EQ(std::string::npos, serialized_contents_.find("&sup1;"));
    ASSERT_EQ(std::string::npos, serialized_contents_.find("&apos;"));
  }

  void SerializeHTMLDOMWithBaseTagOnRenderer(const GURL& file_url,
                                             const GURL& path_dir_url) {
    // There are total 2 available base tags in this test file.
    const int kTotalBaseTagCountInTestFile = 2;

    // Since for this test, we assume there is no savable sub-resource links for
    // this test file, also all links are relative URLs in this test file, so we
    // need to check those relative URLs and make sure document has BASE tag.
    WebFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    // Go through all descent nodes.
    WebElementCollection all = doc.all();
    int original_base_tag_count = 0;
    for (WebElement element = all.firstItem(); !element.isNull();
         element = all.nextItem()) {
      if (element.hasHTMLTagName("base")) {
        original_base_tag_count++;
      } else {
        // Get link.
        WebString value = GetSubResourceLinkFromElement(element);
        if (value.isNull() && element.hasHTMLTagName("a")) {
          value = element.getAttribute("href");
          if (value.isEmpty())
            value = WebString();
        }
        // Each link is relative link.
        if (!value.isNull()) {
          GURL link(value.utf8());
          ASSERT_TRUE(link.scheme().empty());
        }
      }
    }
    ASSERT_EQ(original_base_tag_count, kTotalBaseTagCountInTestFile);
    // Make sure in original document, the base URL is not equal with the
    // |path_dir_url|.
    GURL original_base_url(doc.baseURL());
    ASSERT_NE(original_base_url, path_dir_url);

    // Do serialization.
    SerializeDomForURL(file_url);

    // Load the serialized contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    LoadContents(serialized_contents_, file_url,
                 web_frame->document().encoding());

    // Make sure all links are absolute URLs and doc there are some number of
    // BASE tags in serialized HTML data. Each of those BASE tags have same base
    // URL which is as same as URL of current test file.
    web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != NULL);
    doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    // Go through all descent nodes.
    all = doc.all();
    int new_base_tag_count = 0;
    for (WebNode node = all.firstItem(); !node.isNull();
         node = all.nextItem()) {
      if (!node.isElementNode())
        continue;
      WebElement element = node.to<WebElement>();
      if (element.hasHTMLTagName("base")) {
        new_base_tag_count++;
      } else {
        // Get link.
        WebString value = GetSubResourceLinkFromElement(element);
        if (value.isNull() && element.hasHTMLTagName("a")) {
          value = element.getAttribute("href");
          if (value.isEmpty())
            value = WebString();
        }
        // Each link is absolute link.
        if (!value.isNull()) {
          GURL link(std::string(value.utf8()));
          ASSERT_FALSE(link.scheme().empty());
        }
      }
    }
    // We have one more added BASE tag which is generated by JavaScript.
    ASSERT_EQ(new_base_tag_count, original_base_tag_count + 1);
    // Make sure in new document, the base URL is equal with the |path_dir_url|.
    GURL new_base_url(doc.baseURL());
    ASSERT_EQ(new_base_url, path_dir_url);
  }

  void SerializeHTMLDOMWithEmptyHeadOnRenderer() {
    base::FilePath page_file_path = GetTestFilePath(
        "dom_serializer", "empty_head.htm");
    GURL file_url = net::FilePathToFileURL(page_file_path);
    ASSERT_TRUE(file_url.SchemeIsFile());

    // Load the test html content.
    static const char* const empty_head_contents =
      "<html><head></head><body>hello world</body></html>";
    LoadContents(empty_head_contents, file_url, WebString());

    // Make sure the head tag is empty.
    WebFrame* web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    WebElement head_element = doc.head();
    ASSERT_TRUE(!head_element.isNull());
    ASSERT_TRUE(!head_element.hasChildNodes());

    // Do serialization.
    SerializeDomForURL(file_url);
    ASSERT_TRUE(serialization_reported_end_of_data_);

    // Reload serialized contents and make sure there is only one META tag.
    LoadContents(serialized_contents_, file_url,
                 web_frame->document().encoding());
    web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != NULL);
    doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    head_element = doc.head();
    ASSERT_TRUE(!head_element.isNull());
    ASSERT_TRUE(!head_element.firstChild().isNull());
    ASSERT_TRUE(head_element.firstChild().isElementNode());
    ASSERT_TRUE(head_element.firstChild().nextSibling().isNull());
    WebMetaElement meta_element = head_element.firstChild().
        to<WebMetaElement>();
    ASSERT_EQ(meta_element.computeEncoding(), web_frame->document().encoding());

    // Check the body's first node is text node and its contents are
    // "hello world"
    WebElement body_element = doc.body();
    ASSERT_TRUE(!body_element.isNull());
    WebNode text_node = body_element.firstChild();
    ASSERT_TRUE(text_node.isTextNode());
    ASSERT_EQ("hello world", text_node.nodeValue());
  }

  void SubResourceForElementsInNonHTMLNamespaceOnRenderer(
      const GURL& file_url) {
    WebFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    WebNode lastNodeInBody = doc.body().lastChild();
    ASSERT_TRUE(lastNodeInBody.isElementNode());
    WebString uri = GetSubResourceLinkFromElement(
        lastNodeInBody.to<WebElement>());
    EXPECT_TRUE(uri.isNull());
  }

 private:
  int32_t render_view_routing_id_;
  std::string serialized_contents_;
  bool serialization_reported_end_of_data_;
};

// If original contents have document type, the serialized contents also have
// document type.
// Disabled on OSX by ellyjones@ on 2015-05-18, see https://crbug.com/488495,
// on all platforms by tsergeant@ on 2016-03-10, see https://crbug.com/593575

IN_PROC_BROWSER_TEST_F(DomSerializerTests,
                       DISABLED_SerializeHTMLDOMWithDocType) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_1.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  NavigateToURL(shell(), file_url);

  PostTaskToInProcessRendererAndWait(
        base::Bind(&DomSerializerTests::SerializeHTMLDOMWithDocTypeOnRenderer,
                   base::Unretained(this), file_url));
}

// If original contents do not have document type, the serialized contents
// also do not have document type.
IN_PROC_BROWSER_TEST_F(DomSerializerTests, SerializeHTMLDOMWithoutDocType) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_2.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  NavigateToURL(shell(), file_url);

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &DomSerializerTests::SerializeHTMLDOMWithoutDocTypeOnRenderer,
            base::Unretained(this), file_url));
}

// Serialize XML document which has all 5 built-in entities. After
// finishing serialization, the serialized contents should be same
// with original XML document.
IN_PROC_BROWSER_TEST_F(DomSerializerTests,
                       SerializeXMLDocWithBuiltInEntities) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "note.html");
  base::FilePath xml_file_path = GetTestFilePath("dom_serializer", "note.xml");
  // Read original contents for later comparison.
  std::string original_contents;
  ASSERT_TRUE(base::ReadFileToString(xml_file_path, &original_contents));
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  GURL xml_file_url = net::FilePathToFileURL(xml_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  NavigateToURL(shell(), file_url);

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &DomSerializerTests::SerializeXMLDocWithBuiltInEntitiesOnRenderer,
            base::Unretained(this), xml_file_url, original_contents));
}

// When serializing DOM, we add MOTW declaration before html tag.
IN_PROC_BROWSER_TEST_F(DomSerializerTests, SerializeHTMLDOMWithAddingMOTW) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_2.htm");
  // Read original contents for later comparison .
  std::string original_contents;
  ASSERT_TRUE(base::ReadFileToString(page_file_path, &original_contents));
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());

  // Load the test file.
  NavigateToURL(shell(), file_url);

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &DomSerializerTests::SerializeHTMLDOMWithAddingMOTWOnRenderer,
            base::Unretained(this), file_url, original_contents));
}

// When serializing DOM, we will add the META which have correct charset
// declaration as first child of HEAD element for resolving WebKit bug:
// http://bugs.webkit.org/show_bug.cgi?id=16621 even the original document
// does not have META charset declaration.
// Disabled on OSX by battre@ on 2015-05-21, see https://crbug.com/488495,
// on all platforms by tsergeant@ on 2016-03-10, see https://crbug.com/593575
IN_PROC_BROWSER_TEST_F(
    DomSerializerTests,
    DISABLED_SerializeHTMLDOMWithNoMetaCharsetInOriginalDoc) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_1.htm");
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  NavigateToURL(shell(), file_url);

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &DomSerializerTests::
                SerializeHTMLDOMWithNoMetaCharsetInOriginalDocOnRenderer,
            base::Unretained(this), file_url));
}

// When serializing DOM, if the original document has multiple META charset
// declaration, we will add the META which have correct charset declaration
// as first child of HEAD element and remove all original META charset
// declarations.
IN_PROC_BROWSER_TEST_F(DomSerializerTests,
                       SerializeHTMLDOMWithMultipleMetaCharsetInOriginalDoc) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_2.htm");
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  NavigateToURL(shell(), file_url);

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &DomSerializerTests::
                SerializeHTMLDOMWithMultipleMetaCharsetInOriginalDocOnRenderer,
            base::Unretained(this), file_url));
}

// Test situation of html entities in text when serializing HTML DOM.
IN_PROC_BROWSER_TEST_F(DomSerializerTests, SerializeHTMLDOMWithEntitiesInText) {
  // Need to spin up the renderer and also navigate to a file url so that the
  // renderer code doesn't attempt a fork when it sees a load to file scheme
  // from non-file scheme.
  NavigateToURL(shell(), GetTestUrl(".", "simple_page.html"));

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &DomSerializerTests::SerializeHTMLDOMWithEntitiesInTextOnRenderer,
            base::Unretained(this)));
}

// Test situation of html entities in attribute value when serializing
// HTML DOM.
// This test started to fail at WebKit r65388. See http://crbug.com/52279.
IN_PROC_BROWSER_TEST_F(DomSerializerTests,
                       SerializeHTMLDOMWithEntitiesInAttributeValue) {
  // Need to spin up the renderer and also navigate to a file url so that the
  // renderer code doesn't attempt a fork when it sees a load to file scheme
  // from non-file scheme.
  NavigateToURL(shell(), GetTestUrl(".", "simple_page.html"));

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &DomSerializerTests::
                SerializeHTMLDOMWithEntitiesInAttributeValueOnRenderer,
            base::Unretained(this)));
}

// Test situation of non-standard HTML entities when serializing HTML DOM.
// This test started to fail at WebKit r65351. See http://crbug.com/52279.
IN_PROC_BROWSER_TEST_F(DomSerializerTests,
                       SerializeHTMLDOMWithNonStandardEntities) {
  // Make a test file URL and load it.
  base::FilePath page_file_path = GetTestFilePath(
      "dom_serializer", "nonstandard_htmlentities.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  NavigateToURL(shell(), file_url);

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &DomSerializerTests::
                SerializeHTMLDOMWithNonStandardEntitiesOnRenderer,
            base::Unretained(this), file_url));
}

// Test situation of BASE tag in original document when serializing HTML DOM.
// When serializing, we should comment the BASE tag, append a new BASE tag.
// rewrite all the savable URLs to relative local path, and change other URLs
// to absolute URLs.
IN_PROC_BROWSER_TEST_F(DomSerializerTests,
                       SerializeHTMLDOMWithBaseTag) {
  base::FilePath page_file_path = GetTestFilePath(
      "dom_serializer", "html_doc_has_base_tag.htm");

  // Get page dir URL which is base URL of this file.
  base::FilePath dir_name = page_file_path.DirName();
  dir_name = dir_name.Append(
      base::FilePath::StringType(base::FilePath::kSeparators[0], 1));
  GURL path_dir_url = net::FilePathToFileURL(dir_name);

  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  NavigateToURL(shell(), file_url);

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &DomSerializerTests::SerializeHTMLDOMWithBaseTagOnRenderer,
            base::Unretained(this), file_url, path_dir_url));
}

// Serializing page which has an empty HEAD tag.
IN_PROC_BROWSER_TEST_F(DomSerializerTests, SerializeHTMLDOMWithEmptyHead) {
  // Need to spin up the renderer and also navigate to a file url so that the
  // renderer code doesn't attempt a fork when it sees a load to file scheme
  // from non-file scheme.
  NavigateToURL(shell(), GetTestUrl(".", "simple_page.html"));

  PostTaskToInProcessRendererAndWait(
        base::Bind(&DomSerializerTests::SerializeHTMLDOMWithEmptyHeadOnRenderer,
                   base::Unretained(this)));
}

IN_PROC_BROWSER_TEST_F(DomSerializerTests,
                       SubResourceForElementsInNonHTMLNamespace) {
  base::FilePath page_file_path = GetTestFilePath(
      "dom_serializer", "non_html_namespace.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  NavigateToURL(shell(), file_url);

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &DomSerializerTests::
                SubResourceForElementsInNonHTMLNamespaceOnRenderer,
            base::Unretained(this), file_url));
}

}  // namespace content
