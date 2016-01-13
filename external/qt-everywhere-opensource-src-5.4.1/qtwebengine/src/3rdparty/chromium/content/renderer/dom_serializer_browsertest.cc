// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebPageSerializer.h"
#include "third_party/WebKit/public/web/WebPageSerializerClient.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebCString;
using blink::WebData;
using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebNodeList;
using blink::WebPageSerializer;
using blink::WebPageSerializerClient;
using blink::WebString;
using blink::WebURL;
using blink::WebView;
using blink::WebVector;

namespace {

// The first RenderFrame is routing ID 1, and the first RenderView is 2.
const int kRenderViewRoutingId = 2;

}

namespace content {

// Iterate recursively over sub-frames to find one with with a given url.
WebFrame* FindSubFrameByURL(WebView* web_view, const GURL& url) {
  if (!web_view->mainFrame())
    return NULL;

  std::vector<WebFrame*> stack;
  stack.push_back(web_view->mainFrame());

  while (!stack.empty()) {
    WebFrame* current_frame = stack.back();
    stack.pop_back();
    if (GURL(current_frame->document().url()) == url)
      return current_frame;
    WebElementCollection all = current_frame->document().all();
    for (WebElement element = all.firstItem();
         !element.isNull(); element = all.nextItem()) {
      // Check frame tag and iframe tag
      if (!element.hasTagName("frame") && !element.hasTagName("iframe"))
        continue;
      WebFrame* sub_frame = WebLocalFrame::fromFrameOwnerElement(element);
      if (sub_frame)
        stack.push_back(sub_frame);
    }
  }
  return NULL;
}

// Helper function that test whether the first node in the doc is a doc type
// node.
bool HasDocType(const WebDocument& doc) {
  WebNode node = doc.firstChild();
  if (node.isNull())
    return false;
  return node.nodeType() == WebNode::DocumentTypeNode;
}

  // Helper function for checking whether input node is META tag. Return true
// means it is META element, otherwise return false. The parameter charset_info
// return actual charset info if the META tag has charset declaration.
bool IsMetaElement(const WebNode& node, std::string& charset_info) {
  if (!node.isElementNode())
    return false;
  const WebElement meta = node.toConst<WebElement>();
  if (!meta.hasTagName("meta"))
    return false;
  charset_info.erase(0, charset_info.length());
  // Check the META charset declaration.
  WebString httpEquiv = meta.getAttribute("http-equiv");
  if (LowerCaseEqualsASCII(httpEquiv, "content-type")) {
    std::string content = meta.getAttribute("content").utf8();
    int pos = content.find("charset", 0);
    if (pos > -1) {
      // Add a dummy charset declaration to charset_info, which indicates this
      // META tag has charset declaration although we do not get correct value
      // yet.
      charset_info.append("has-charset-declaration");
      int remaining_length = content.length() - pos - 7;
      if (!remaining_length)
        return true;
      int start_pos = pos + 7;
      // Find "=" symbol.
      while (remaining_length--)
        if (content[start_pos++] == L'=')
          break;
      // Skip beginning space.
      while (remaining_length) {
        if (content[start_pos] > 0x0020)
          break;
        ++start_pos;
        --remaining_length;
      }
      if (!remaining_length)
        return true;
      int end_pos = start_pos;
      // Now we find out the start point of charset info. Search the end point.
      while (remaining_length--) {
        if (content[end_pos] <= 0x0020 || content[end_pos] == L';')
          break;
        ++end_pos;
      }
      // Get actual charset info.
      charset_info = content.substr(start_pos, end_pos - start_pos);
      return true;
    }
  }
  return true;
}

class LoadObserver : public RenderViewObserver {
 public:
  LoadObserver(RenderView* render_view, const base::Closure& quit_closure)
      : RenderViewObserver(render_view),
        quit_closure_(quit_closure) {}

  virtual void DidFinishLoad(blink::WebLocalFrame* frame) OVERRIDE {
    if (frame == render_view()->GetWebView()->mainFrame())
      quit_closure_.Run();
  }

 private:
  base::Closure quit_closure_;
};

class DomSerializerTests : public ContentBrowserTest,
                           public WebPageSerializerClient {
 public:
  DomSerializerTests()
    : serialized_(false),
      local_directory_name_(FILE_PATH_LITERAL("./dummy_files/")) {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kSingleProcess);
#if defined(OS_WIN)
    // Don't want to try to create a GPU process.
    command_line->AppendSwitch(switches::kDisableGpu);
#endif
  }

  // DomSerializerDelegate.
  virtual void didSerializeDataForFrame(const WebURL& frame_web_url,
                                        const WebCString& data,
                                        PageSerializationStatus status) {

    GURL frame_url(frame_web_url);
    // If the all frames are finished saving, check all finish status
    if (status == WebPageSerializerClient::AllFramesAreFinished) {
      SerializationFinishStatusMap::iterator it =
          serialization_finish_status_.begin();
      for (; it != serialization_finish_status_.end(); ++it)
        ASSERT_TRUE(it->second);
      serialized_ = true;
      return;
    }

    // Check finish status of current frame.
    SerializationFinishStatusMap::iterator it =
        serialization_finish_status_.find(frame_url.spec());
    // New frame, set initial status as false.
    if (it == serialization_finish_status_.end())
      serialization_finish_status_[frame_url.spec()] = false;

    it = serialization_finish_status_.find(frame_url.spec());
    ASSERT_TRUE(it != serialization_finish_status_.end());
    // In process frame, finish status should be false.
    ASSERT_FALSE(it->second);

    // Add data to corresponding frame's content.
    serialized_frame_map_[frame_url.spec()] += data.data();

    // Current frame is completed saving, change the finish status.
    if (status == WebPageSerializerClient::CurrentFrameIsFinished)
      it->second = true;
  }

  bool HasSerializedFrame(const GURL& frame_url) {
    return serialized_frame_map_.find(frame_url.spec()) !=
           serialized_frame_map_.end();
  }

  const std::string& GetSerializedContentForFrame(
      const GURL& frame_url) {
    return serialized_frame_map_[frame_url.spec()];
  }

  RenderView* GetRenderView() {
    // We could have the test on the UI thread get the WebContent's routing ID,
    // but we know this will be the first RV so skip that and just hardcode it.
    return RenderView::FromRoutingID(kRenderViewRoutingId);
  }

  WebView* GetWebView() {
    return GetRenderView()->GetWebView();
  }

  WebFrame* GetMainFrame() {
    return GetWebView()->mainFrame();
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

      web_frame->loadData(data, "text/html", encoding_info, base_url);
    }

    runner->Run();
  }

  // Serialize page DOM according to specific page URL. The parameter
  // recursive_serialization indicates whether we will serialize all
  // sub-frames.
  void SerializeDomForURL(const GURL& page_url,
                          bool recursive_serialization) {
    // Find corresponding WebFrame according to page_url.
    WebFrame* web_frame = FindSubFrameByURL(GetWebView(), page_url);
    ASSERT_TRUE(web_frame != NULL);
    WebVector<WebURL> links;
    links.assign(&page_url, 1);
    WebString file_path =
        base::FilePath(FILE_PATH_LITERAL("c:\\dummy.htm")).AsUTF16Unsafe();
    WebVector<WebString> local_paths;
    local_paths.assign(&file_path, 1);
    // Start serializing DOM.
    bool result = WebPageSerializer::serialize(web_frame->toWebLocalFrame(),
       recursive_serialization,
       static_cast<WebPageSerializerClient*>(this),
       links,
       local_paths,
       local_directory_name_.AsUTF16Unsafe());
    ASSERT_TRUE(result);
    ASSERT_TRUE(serialized_);
  }

  void SerializeHTMLDOMWithDocTypeOnRenderer(const GURL& file_url) {
    // Make sure original contents have document type.
    WebFrame* web_frame = FindSubFrameByURL(GetWebView(), file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(HasDocType(doc));
    // Do serialization.
    SerializeDomForURL(file_url, false);
    // Load the serialized contents.
    ASSERT_TRUE(HasSerializedFrame(file_url));
    const std::string& serialized_contents =
        GetSerializedContentForFrame(file_url);
    LoadContents(serialized_contents, file_url,
                 web_frame->document().encoding());
    // Make sure serialized contents still have document type.
    web_frame = GetMainFrame();
    doc = web_frame->document();
    ASSERT_TRUE(HasDocType(doc));
  }

  void SerializeHTMLDOMWithoutDocTypeOnRenderer(const GURL& file_url) {
    // Make sure original contents do not have document type.
    WebFrame* web_frame = FindSubFrameByURL(GetWebView(), file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(!HasDocType(doc));
    // Do serialization.
    SerializeDomForURL(file_url, false);
    // Load the serialized contents.
    ASSERT_TRUE(HasSerializedFrame(file_url));
    const std::string& serialized_contents =
        GetSerializedContentForFrame(file_url);
    LoadContents(serialized_contents, file_url,
                 web_frame->document().encoding());
    // Make sure serialized contents do not have document type.
    web_frame = GetMainFrame();
    doc = web_frame->document();
    ASSERT_TRUE(!HasDocType(doc));
  }

  void SerializeXMLDocWithBuiltInEntitiesOnRenderer(
      const GURL& xml_file_url, const std::string& original_contents) {
    // Do serialization.
    SerializeDomForURL(xml_file_url, false);
    // Compare the serialized contents with original contents.
    ASSERT_TRUE(HasSerializedFrame(xml_file_url));
    const std::string& serialized_contents =
        GetSerializedContentForFrame(xml_file_url);
    ASSERT_EQ(original_contents, serialized_contents);
  }

  void SerializeHTMLDOMWithAddingMOTWOnRenderer(
      const GURL& file_url, const std::string& original_contents) {
    // Make sure original contents does not have MOTW;
    std::string motw_declaration =
       WebPageSerializer::generateMarkOfTheWebDeclaration(file_url).utf8();
    ASSERT_FALSE(motw_declaration.empty());
    // The encoding of original contents is ISO-8859-1, so we convert the MOTW
    // declaration to ASCII and search whether original contents has it or not.
    ASSERT_TRUE(std::string::npos == original_contents.find(motw_declaration));

    // Do serialization.
    SerializeDomForURL(file_url, false);
    // Make sure the serialized contents have MOTW ;
    ASSERT_TRUE(HasSerializedFrame(file_url));
    const std::string& serialized_contents =
        GetSerializedContentForFrame(file_url);
    ASSERT_FALSE(std::string::npos ==
        serialized_contents.find(motw_declaration));
  }

  void SerializeHTMLDOMWithNoMetaCharsetInOriginalDocOnRenderer(
      const GURL& file_url) {
    // Make sure there is no META charset declaration in original document.
    WebFrame* web_frame = FindSubFrameByURL(GetWebView(), file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    WebElement head_element = doc.head();
    ASSERT_TRUE(!head_element.isNull());
    // Go through all children of HEAD element.
    for (WebNode child = head_element.firstChild(); !child.isNull();
         child = child.nextSibling()) {
      std::string charset_info;
      if (IsMetaElement(child, charset_info))
        ASSERT_TRUE(charset_info.empty());
    }
    // Do serialization.
    SerializeDomForURL(file_url, false);

    // Load the serialized contents.
    ASSERT_TRUE(HasSerializedFrame(file_url));
    const std::string& serialized_contents =
        GetSerializedContentForFrame(file_url);
    LoadContents(serialized_contents, file_url,
                 web_frame->document().encoding());
    // Make sure the first child of HEAD element is META which has charset
    // declaration in serialized contents.
    web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != NULL);
    doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    head_element = doc.head();
    ASSERT_TRUE(!head_element.isNull());
    WebNode meta_node = head_element.firstChild();
    ASSERT_TRUE(!meta_node.isNull());
    // Get meta charset info.
    std::string charset_info2;
    ASSERT_TRUE(IsMetaElement(meta_node, charset_info2));
    ASSERT_TRUE(!charset_info2.empty());
    ASSERT_EQ(charset_info2,
              std::string(web_frame->document().encoding().utf8()));

    // Make sure no more additional META tags which have charset declaration.
    for (WebNode child = meta_node.nextSibling(); !child.isNull();
         child = child.nextSibling()) {
      std::string charset_info;
      if (IsMetaElement(child, charset_info))
        ASSERT_TRUE(charset_info.empty());
    }
  }

  void SerializeHTMLDOMWithMultipleMetaCharsetInOriginalDocOnRenderer(
      const GURL& file_url) {
    // Make sure there are multiple META charset declarations in original
    // document.
    WebFrame* web_frame = FindSubFrameByURL(GetWebView(), file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    WebElement head_ele = doc.head();
    ASSERT_TRUE(!head_ele.isNull());
    // Go through all children of HEAD element.
    int charset_declaration_count = 0;
    for (WebNode child = head_ele.firstChild(); !child.isNull();
         child = child.nextSibling()) {
      std::string charset_info;
      if (IsMetaElement(child, charset_info) && !charset_info.empty())
        charset_declaration_count++;
    }
    // The original doc has more than META tags which have charset declaration.
    ASSERT_TRUE(charset_declaration_count > 1);

    // Do serialization.
    SerializeDomForURL(file_url, false);

    // Load the serialized contents.
    ASSERT_TRUE(HasSerializedFrame(file_url));
    const std::string& serialized_contents =
        GetSerializedContentForFrame(file_url);
    LoadContents(serialized_contents, file_url,
                 web_frame->document().encoding());
    // Make sure only first child of HEAD element is META which has charset
    // declaration in serialized contents.
    web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != NULL);
    doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    head_ele = doc.head();
    ASSERT_TRUE(!head_ele.isNull());
    WebNode meta_node = head_ele.firstChild();
    ASSERT_TRUE(!meta_node.isNull());
    // Get meta charset info.
    std::string charset_info2;
    ASSERT_TRUE(IsMetaElement(meta_node, charset_info2));
    ASSERT_TRUE(!charset_info2.empty());
    ASSERT_EQ(charset_info2,
              std::string(web_frame->document().encoding().utf8()));

    // Make sure no more additional META tags which have charset declaration.
    for (WebNode child = meta_node.nextSibling(); !child.isNull();
         child = child.nextSibling()) {
      std::string charset_info;
      if (IsMetaElement(child, charset_info))
        ASSERT_TRUE(charset_info.empty());
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
    WebFrame* web_frame = FindSubFrameByURL(GetWebView(), file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    WebElement body_ele = doc.body();
    ASSERT_TRUE(!body_ele.isNull());
    WebNode text_node = body_ele.firstChild();
    ASSERT_TRUE(text_node.isTextNode());
    ASSERT_TRUE(std::string(text_node.createMarkup().utf8()) ==
                "&amp;&lt;&gt;\"\'");
    // Do serialization.
    SerializeDomForURL(file_url, false);
    // Compare the serialized contents with original contents.
    ASSERT_TRUE(HasSerializedFrame(file_url));
    const std::string& serialized_contents =
        GetSerializedContentForFrame(file_url);
    // Compare the serialized contents with original contents to make sure
    // they are same.
    // Because we add MOTW when serializing DOM, so before comparison, we also
    // need to add MOTW to original_contents.
    std::string original_str =
      WebPageSerializer::generateMarkOfTheWebDeclaration(file_url).utf8();
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
          WebPageSerializer::generateMetaCharsetDeclaration(encoding).utf8();
      head_part += "</head>";
      original_str.insert(pos, head_part);
    }
    ASSERT_EQ(original_str, serialized_contents);
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
    WebFrame* web_frame = FindSubFrameByURL(GetWebView(), file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    WebElement body_ele = doc.body();
    ASSERT_TRUE(!body_ele.isNull());
    WebString value = body_ele.getAttribute("title");
    ASSERT_TRUE(std::string(value.utf8()) == "&<>\"\'");
    // Do serialization.
    SerializeDomForURL(file_url, false);
    // Compare the serialized contents with original contents.
    ASSERT_TRUE(HasSerializedFrame(file_url));
    const std::string& serialized_contents =
        GetSerializedContentForFrame(file_url);
    // Compare the serialized contents with original contents to make sure
    // they are same.
    std::string original_str =
        WebPageSerializer::generateMarkOfTheWebDeclaration(file_url).utf8();
    original_str += original_contents;
    if (!doc.isNull()) {
      WebString encoding = web_frame->document().encoding();
      std::string htmlTag("<html>");
      std::string::size_type pos = original_str.find(htmlTag);
      ASSERT_NE(std::string::npos, pos);
      pos += htmlTag.length();
      std::string head_part("<head>");
      head_part +=
          WebPageSerializer::generateMetaCharsetDeclaration(encoding).utf8();
      head_part += "</head>";
      original_str.insert(pos, head_part);
    }
    ASSERT_EQ(original_str, serialized_contents);
  }

  void SerializeHTMLDOMWithNonStandardEntitiesOnRenderer(const GURL& file_url) {
    // Get value of BODY's title attribute in DOM.
    WebFrame* web_frame = FindSubFrameByURL(GetWebView(), file_url);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    WebElement body_element = doc.body();
    // Unescaped string for "&percnt;&nsup;&sup1;&apos;".
    static const wchar_t parsed_value[] = {
      '%', 0x2285, 0x00b9, '\'', 0
    };
    WebString value = body_element.getAttribute("title");
    ASSERT_TRUE(base::UTF16ToWide(value) == parsed_value);
    ASSERT_TRUE(base::UTF16ToWide(body_element.innerText()) == parsed_value);

    // Do serialization.
    SerializeDomForURL(file_url, false);
    // Check the serialized string.
    ASSERT_TRUE(HasSerializedFrame(file_url));
    const std::string& serialized_contents =
        GetSerializedContentForFrame(file_url);
    // Confirm that the serialized string has no non-standard HTML entities.
    ASSERT_EQ(std::string::npos, serialized_contents.find("&percnt;"));
    ASSERT_EQ(std::string::npos, serialized_contents.find("&nsup;"));
    ASSERT_EQ(std::string::npos, serialized_contents.find("&sup1;"));
    ASSERT_EQ(std::string::npos, serialized_contents.find("&apos;"));
  }

  void SerializeHTMLDOMWithBaseTagOnRenderer(const GURL& file_url,
                                             const GURL& path_dir_url) {
    // There are total 2 available base tags in this test file.
    const int kTotalBaseTagCountInTestFile = 2;

    // Since for this test, we assume there is no savable sub-resource links for
    // this test file, also all links are relative URLs in this test file, so we
    // need to check those relative URLs and make sure document has BASE tag.
    WebFrame* web_frame = FindSubFrameByURL(GetWebView(), file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    // Go through all descent nodes.
    WebElementCollection all = doc.all();
    int original_base_tag_count = 0;
    for (WebElement element = all.firstItem(); !element.isNull();
         element = all.nextItem()) {
      if (element.hasTagName("base")) {
        original_base_tag_count++;
      } else {
        // Get link.
        WebString value = GetSubResourceLinkFromElement(element);
        if (value.isNull() && element.hasTagName("a")) {
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
    SerializeDomForURL(file_url, false);

    // Load the serialized contents.
    ASSERT_TRUE(HasSerializedFrame(file_url));
    const std::string& serialized_contents =
        GetSerializedContentForFrame(file_url);
    LoadContents(serialized_contents, file_url,
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
      if (element.hasTagName("base")) {
        new_base_tag_count++;
      } else {
        // Get link.
        WebString value = GetSubResourceLinkFromElement(element);
        if (value.isNull() && element.hasTagName("a")) {
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
    ASSERT_TRUE(head_element.childNodes().length() == 0);

    // Do serialization.
    SerializeDomForURL(file_url, false);
    // Make sure the serialized contents have META ;
    ASSERT_TRUE(HasSerializedFrame(file_url));
    const std::string& serialized_contents =
        GetSerializedContentForFrame(file_url);

    // Reload serialized contents and make sure there is only one META tag.
    LoadContents(serialized_contents, file_url,
                 web_frame->document().encoding());
    web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != NULL);
    doc = web_frame->document();
    ASSERT_TRUE(doc.isHTMLDocument());
    head_element = doc.head();
    ASSERT_TRUE(!head_element.isNull());
    ASSERT_TRUE(head_element.hasChildNodes());
    ASSERT_TRUE(head_element.childNodes().length() == 1);
    WebNode meta_node = head_element.firstChild();
    ASSERT_TRUE(!meta_node.isNull());
    // Get meta charset info.
    std::string charset_info;
    ASSERT_TRUE(IsMetaElement(meta_node, charset_info));
    ASSERT_TRUE(!charset_info.empty());
    ASSERT_EQ(charset_info,
              std::string(web_frame->document().encoding().utf8()));

    // Check the body's first node is text node and its contents are
    // "hello world"
    WebElement body_element = doc.body();
    ASSERT_TRUE(!body_element.isNull());
    WebNode text_node = body_element.firstChild();
    ASSERT_TRUE(text_node.isTextNode());
    WebString text_node_contents = text_node.nodeValue();
    ASSERT_TRUE(std::string(text_node_contents.utf8()) == "hello world");
  }

  void SerializeDocumentWithDownloadedIFrameOnRenderer(const GURL& file_url) {
    // Do a recursive serialization. We pass if we don't crash.
    SerializeDomForURL(file_url, true);
  }

  void SubResourceForElementsInNonHTMLNamespaceOnRenderer(
      const GURL& file_url) {
    WebFrame* web_frame = FindSubFrameByURL(GetWebView(), file_url);
    ASSERT_TRUE(web_frame != NULL);
    WebDocument doc = web_frame->document();
    WebNode lastNodeInBody = doc.body().lastChild();
    ASSERT_EQ(WebNode::ElementNode, lastNodeInBody.nodeType());
    WebString uri = GetSubResourceLinkFromElement(
        lastNodeInBody.to<WebElement>());
    EXPECT_TRUE(uri.isNull());
  }

 private:
  // Map frame_url to corresponding serialized_content.
  typedef base::hash_map<std::string, std::string> SerializedFrameContentMap;
  SerializedFrameContentMap serialized_frame_map_;
  // Map frame_url to corresponding status of serialization finish.
  typedef base::hash_map<std::string, bool> SerializationFinishStatusMap;
  SerializationFinishStatusMap serialization_finish_status_;
  // Flag indicates whether the process of serializing DOM is finished or not.
  bool serialized_;
  // The local_directory_name_ is dummy relative path of directory which
  // contain all saved auxiliary files included all sub frames and resources.
  const base::FilePath local_directory_name_;
};

// If original contents have document type, the serialized contents also have
// document type.
IN_PROC_BROWSER_TEST_F(DomSerializerTests, SerializeHTMLDOMWithDocType) {
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
//
// TODO(tiger@opera.com): Disabled in preparation of page serializer merge --
// XML headers are handled differently in the merged serializer.
// Bug: http://crbug.com/328354
IN_PROC_BROWSER_TEST_F(DomSerializerTests,
                       DISABLED_SerializeXMLDocWithBuiltInEntities) {
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
IN_PROC_BROWSER_TEST_F(DomSerializerTests,
                       SerializeHTMLDOMWithNoMetaCharsetInOriginalDoc) {
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
//
// TODO(tiger@opera.com): Disabled in preparation of page serializer merge --
// Some attributes are handled differently in the merged serializer.
// Bug: http://crbug.com/328354
IN_PROC_BROWSER_TEST_F(DomSerializerTests,
                       DISABLED_SerializeHTMLDOMWithEntitiesInAttributeValue) {
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
//
// TODO(tiger@opera.com): Disabled in preparation of page serializer merge --
// Base tags are handled a bit different in merged version.
// Bug: http://crbug.com/328354
IN_PROC_BROWSER_TEST_F(DomSerializerTests,
                       DISABLED_SerializeHTMLDOMWithBaseTag) {
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

// Test that we don't crash when the page contains an iframe that
// was handled as a download (http://crbug.com/42212).
IN_PROC_BROWSER_TEST_F(DomSerializerTests,
                       SerializeDocumentWithDownloadedIFrame) {
  base::FilePath page_file_path = GetTestFilePath(
      "dom_serializer", "iframe-src-is-exe.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  NavigateToURL(shell(), file_url);

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &DomSerializerTests::
                SerializeDocumentWithDownloadedIFrameOnRenderer,
            base::Unretained(this), file_url));
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
