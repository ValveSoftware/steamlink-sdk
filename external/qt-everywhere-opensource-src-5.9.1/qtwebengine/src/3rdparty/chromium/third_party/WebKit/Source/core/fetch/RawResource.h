/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef RawResource_h
#define RawResource_h

#include "core/CoreExport.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceClient.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/WeakPtr.h"
#include <memory>

namespace blink {
class FetchRequest;
class RawResourceClient;
class ResourceFetcher;
class SubstituteData;

class CORE_EXPORT RawResource final : public Resource {
 public:
  using ClientType = RawResourceClient;

  static Resource* fetchSynchronously(FetchRequest&, ResourceFetcher*);
  static RawResource* fetch(FetchRequest&, ResourceFetcher*);
  static RawResource* fetchMainResource(FetchRequest&,
                                        ResourceFetcher*,
                                        const SubstituteData&);
  static RawResource* fetchImport(FetchRequest&, ResourceFetcher*);
  static RawResource* fetchMedia(FetchRequest&, ResourceFetcher*);
  static RawResource* fetchTextTrack(FetchRequest&, ResourceFetcher*);
  static RawResource* fetchManifest(FetchRequest&, ResourceFetcher*);

  // Exposed for testing
  static RawResource* create(const ResourceRequest& request, Type type) {
    return new RawResource(request, type, ResourceLoaderOptions());
  }

  // FIXME: AssociatedURLLoader shouldn't be a DocumentThreadableLoader and
  // therefore shouldn't use RawResource. However, it is, and it needs to be
  // able to defer loading. This can be fixed by splitting CORS preflighting out
  // of DocumentThreadableLoader.
  void setDefersLoading(bool);

  bool canReuse(const ResourceRequest&) const override;

 private:
  class RawResourceFactory : public ResourceFactory {
   public:
    explicit RawResourceFactory(Resource::Type type) : ResourceFactory(type) {}

    Resource* create(const ResourceRequest& request,
                     const ResourceLoaderOptions& options,
                     const String& charset) const override {
      return new RawResource(request, m_type, options);
    }
  };

  RawResource(const ResourceRequest&, Type, const ResourceLoaderOptions&);

  void didAddClient(ResourceClient*) override;
  void appendData(const char*, size_t) override;

  bool shouldIgnoreHTTPStatusCodeErrors() const override {
    return !isLinkPreload();
  }

  bool willFollowRedirect(const ResourceRequest&,
                          const ResourceResponse&) override;
  void willNotFollowRedirect() override;
  void responseReceived(const ResourceResponse&,
                        std::unique_ptr<WebDataConsumerHandle>) override;
  void setSerializedCachedMetadata(const char*, size_t) override;
  void didSendData(unsigned long long bytesSent,
                   unsigned long long totalBytesToBeSent) override;
  void didDownloadData(int) override;
  void reportResourceTimingToClients(const ResourceTimingInfo&) override;
};

#if ENABLE(SECURITY_ASSERT)
inline bool isRawResource(const Resource& resource) {
  Resource::Type type = resource.getType();
  return type == Resource::MainResource || type == Resource::Raw ||
         type == Resource::TextTrack || type == Resource::Media ||
         type == Resource::Manifest || type == Resource::ImportResource;
}
#endif
inline RawResource* toRawResource(Resource* resource) {
  SECURITY_DCHECK(!resource || isRawResource(*resource));
  return static_cast<RawResource*>(resource);
}

class CORE_EXPORT RawResourceClient : public ResourceClient {
 public:
  static bool isExpectedType(ResourceClient* client) {
    return client->getResourceClientType() == RawResourceType;
  }
  ResourceClientType getResourceClientType() const final {
    return RawResourceType;
  }

  // The order of the callbacks is as follows:
  // [Case 1] A successful load:
  // 0+  redirectReceived() and/or dataSent()
  // 1   responseReceived()
  // 0-1 setSerializedCachedMetadata()
  // 0+  dataReceived() or dataDownloaded(), but never both
  // 1   notifyFinished() with errorOccurred() = false
  // [Case 2] When redirect is blocked:
  // 0+  redirectReceived() and/or dataSent()
  // 1   redirectBlocked()
  // 1   notifyFinished() with errorOccurred() = true
  // [Case 3] Other failures:
  //     notifyFinished() with errorOccurred() = true is called at any time
  //     (unless notifyFinished() is already called).
  // In all cases:
  //     No callbacks are made after notifyFinished() or
  //     removeClient() is called.
  virtual void dataSent(Resource*,
                        unsigned long long /* bytesSent */,
                        unsigned long long /* totalBytesToBeSent */) {}
  virtual void responseReceived(Resource*,
                                const ResourceResponse&,
                                std::unique_ptr<WebDataConsumerHandle>) {}
  virtual void setSerializedCachedMetadata(Resource*, const char*, size_t) {}
  virtual void dataReceived(Resource*,
                            const char* /* data */,
                            size_t /* length */) {}
  virtual bool redirectReceived(Resource*,
                                const ResourceRequest&,
                                const ResourceResponse&) {
    return true;
  }
  virtual void redirectBlocked() {}
  virtual void dataDownloaded(Resource*, int) {}
  virtual void didReceiveResourceTiming(Resource*, const ResourceTimingInfo&) {}
};

// Checks the sequence of callbacks of RawResourceClient. This can be used only
// when a RawResourceClient is added as a client to at most one RawResource.
class CORE_EXPORT RawResourceClientStateChecker final {
 public:
  RawResourceClientStateChecker();
  ~RawResourceClientStateChecker();

  // Call before addClient()/removeClient() is called.
  void willAddClient();
  void willRemoveClient();

  // Call RawResourceClientStateChecker::f() at the beginning of
  // RawResourceClient::f().
  void redirectReceived();
  void redirectBlocked();
  void dataSent();
  void responseReceived();
  void setSerializedCachedMetadata();
  void dataReceived();
  void dataDownloaded();
  void notifyFinished(Resource*);

 private:
  enum State {
    NotAddedAsClient,
    Started,
    RedirectBlocked,
    ResponseReceived,
    SetSerializedCachedMetadata,
    DataReceived,
    DataDownloaded,
    NotifyFinished
  };
  State m_state;
};

}  // namespace blink

#endif  // RawResource_h
