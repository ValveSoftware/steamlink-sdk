/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorPageAgent_h
#define InspectorPageAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Page.h"
#include "core/page/ChromeClient.h"
#include "wtf/HashMap.h"
#include "wtf/text/WTFString.h"

#include <v8-inspector.h>

namespace blink {

class Resource;
class Document;
class DocumentLoader;
class InspectedFrames;
class InspectorResourceContentLoader;
class KURL;
class LocalFrame;
class SharedBuffer;

using blink::protocol::Maybe;

class CORE_EXPORT InspectorPageAgent final
    : public InspectorBaseAgent<protocol::Page::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorPageAgent);

 public:
  class Client {
   public:
    virtual ~Client() {}
    virtual void pageLayoutInvalidated(bool resized) {}
    virtual void configureOverlay(bool suspended, const String& message) {}
    virtual void waitForCreateWindow(LocalFrame*) {}
  };

  enum ResourceType {
    DocumentResource,
    StylesheetResource,
    ImageResource,
    FontResource,
    MediaResource,
    ScriptResource,
    TextTrackResource,
    XHRResource,
    FetchResource,
    EventSourceResource,
    WebSocketResource,
    ManifestResource,
    OtherResource
  };

  static InspectorPageAgent* create(InspectedFrames*,
                                    Client*,
                                    InspectorResourceContentLoader*,
                                    v8_inspector::V8InspectorSession*);

  static HeapVector<Member<Document>> importsForFrame(LocalFrame*);
  static bool cachedResourceContent(Resource*,
                                    String* result,
                                    bool* base64Encoded);
  static bool sharedBufferContent(PassRefPtr<const SharedBuffer>,
                                  const String& mimeType,
                                  const String& textEncodingName,
                                  String* result,
                                  bool* base64Encoded);

  static Resource* cachedResource(LocalFrame*, const KURL&);
  static String resourceTypeJson(ResourceType);
  static ResourceType cachedResourceType(const Resource&);
  static String cachedResourceTypeJson(const Resource&);

  // Page API for frontend
  Response enable() override;
  Response disable() override;
  Response addScriptToEvaluateOnLoad(const String& scriptSource,
                                     String* identifier) override;
  Response removeScriptToEvaluateOnLoad(const String& identifier) override;
  Response setAutoAttachToCreatedPages(bool) override;
  Response reload(Maybe<bool> bypassCache,
                  Maybe<String> scriptToEvaluateOnLoad) override;
  Response navigate(const String& url, String* frameId) override;
  Response getResourceTree(
      std::unique_ptr<protocol::Page::FrameResourceTree>* frameTree) override;
  void getResourceContent(const String& frameId,
                          const String& url,
                          std::unique_ptr<GetResourceContentCallback>) override;
  void searchInResource(const String& frameId,
                        const String& url,
                        const String& query,
                        Maybe<bool> caseSensitive,
                        Maybe<bool> isRegex,
                        std::unique_ptr<SearchInResourceCallback>) override;
  Response setDocumentContent(const String& frameId,
                              const String& html) override;
  Response startScreencast(Maybe<String> format,
                           Maybe<int> quality,
                           Maybe<int> maxWidth,
                           Maybe<int> maxHeight,
                           Maybe<int> everyNthFrame) override;
  Response stopScreencast() override;
  Response configureOverlay(Maybe<bool> suspended,
                            Maybe<String> message) override;
  Response getLayoutMetrics(
      std::unique_ptr<protocol::Page::LayoutViewport>*,
      std::unique_ptr<protocol::Page::VisualViewport>*) override;

  // InspectorInstrumentation API
  void didClearDocumentOfWindowObject(LocalFrame*);
  void domContentLoadedEventFired(LocalFrame*);
  void loadEventFired(LocalFrame*);
  void didCommitLoad(LocalFrame*, DocumentLoader*);
  void frameAttachedToParent(LocalFrame*);
  void frameDetachedFromParent(LocalFrame*);
  void frameStartedLoading(LocalFrame*);
  void frameStoppedLoading(LocalFrame*);
  void frameScheduledNavigation(LocalFrame*, double delay);
  void frameClearedScheduledNavigation(LocalFrame*);
  void willRunJavaScriptDialog(const String& message, ChromeClient::DialogType);
  void didRunJavaScriptDialog(bool result);
  void didUpdateLayout();
  void didResizeMainFrame();
  void didRecalculateStyle();
  void windowCreated(LocalFrame*);

  // Inspector Controller API
  void restore() override;
  bool screencastEnabled();

  DECLARE_VIRTUAL_TRACE();

 private:
  InspectorPageAgent(InspectedFrames*,
                     Client*,
                     InspectorResourceContentLoader*,
                     v8_inspector::V8InspectorSession*);

  void finishReload();
  void getResourceContentAfterResourcesContentLoaded(
      const String& frameId,
      const String& url,
      std::unique_ptr<GetResourceContentCallback>);
  void searchContentAfterResourcesContentLoaded(
      const String& frameId,
      const String& url,
      const String& query,
      bool caseSensitive,
      bool isRegex,
      std::unique_ptr<SearchInResourceCallback>);

  static bool dataContent(const char* data,
                          unsigned size,
                          const String& textEncodingName,
                          bool withBase64Encode,
                          String* result);

  std::unique_ptr<protocol::Page::Frame> buildObjectForFrame(LocalFrame*);
  std::unique_ptr<protocol::Page::FrameResourceTree> buildObjectForFrameTree(
      LocalFrame*);
  Member<InspectedFrames> m_inspectedFrames;
  v8_inspector::V8InspectorSession* m_v8Session;
  Client* m_client;
  long m_lastScriptIdentifier;
  String m_pendingScriptToEvaluateOnLoadOnce;
  String m_scriptToEvaluateOnLoadOnce;
  bool m_enabled;
  bool m_reloading;
  Member<InspectorResourceContentLoader> m_inspectorResourceContentLoader;
  int m_resourceContentLoaderClientId;
};

}  // namespace blink

#endif  // !defined(InspectorPagerAgent_h)
