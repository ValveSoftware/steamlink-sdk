/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceResponse_h
#define ResourceResponse_h

#include "platform/PlatformExport.h"
#include "platform/blob/BlobData.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceLoadInfo.h"
#include "platform/network/ResourceLoadTiming.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponseType.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"

namespace blink {

struct CrossThreadResourceResponseData;

class PLATFORM_EXPORT ResourceResponse final {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    enum HTTPVersion { HTTPVersionUnknown,
        HTTPVersion_0_9,
        HTTPVersion_1_0,
        HTTPVersion_1_1,
        HTTPVersion_2_0 };
    enum SecurityStyle {
        SecurityStyleUnknown,
        SecurityStyleUnauthenticated,
        SecurityStyleAuthenticationBroken,
        SecurityStyleWarning,
        SecurityStyleAuthenticated
    };

    class PLATFORM_EXPORT SignedCertificateTimestamp final {
    public:
        SignedCertificateTimestamp(
            String status,
            String origin,
            String logDescription,
            String logId,
            int64_t timestamp,
            String hashAlgorithm,
            String signatureAlgorithm,
            String signatureData)
            : m_status(status)
            , m_origin(origin)
            , m_logDescription(logDescription)
            , m_logId(logId)
            , m_timestamp(timestamp)
            , m_hashAlgorithm(hashAlgorithm)
            , m_signatureAlgorithm(signatureAlgorithm)
            , m_signatureData(signatureData)
        {
        }
        explicit SignedCertificateTimestamp(
            const struct blink::WebURLResponse::SignedCertificateTimestamp&);
        SignedCertificateTimestamp isolatedCopy() const;

        String m_status;
        String m_origin;
        String m_logDescription;
        String m_logId;
        int64_t m_timestamp;
        String m_hashAlgorithm;
        String m_signatureAlgorithm;
        String m_signatureData;
    };

    using SignedCertificateTimestampList = WTF::Vector<SignedCertificateTimestamp>;

    struct SecurityDetails {
        DISALLOW_NEW();
        SecurityDetails()
            : certID(0)
            , numUnknownSCTs(0)
            , numInvalidSCTs(0)
            , numValidSCTs(0)
        {
        }
        // All strings are human-readable values.
        String protocol;
        String keyExchange;
        String cipher;
        // mac is the empty string when the connection cipher suite does not
        // have a separate MAC value (i.e. if the cipher suite is AEAD).
        String mac;
        int certID;
        size_t numUnknownSCTs;
        size_t numInvalidSCTs;
        size_t numValidSCTs;
        SignedCertificateTimestampList sctList;
    };

    class ExtraData : public RefCounted<ExtraData> {
    public:
        virtual ~ExtraData() { }
    };

    explicit ResourceResponse(CrossThreadResourceResponseData*);

    // Gets a copy of the data suitable for passing to another thread.
    std::unique_ptr<CrossThreadResourceResponseData> copyData() const;

    ResourceResponse();
    ResourceResponse(const KURL&, const AtomicString& mimeType, long long expectedLength, const AtomicString& textEncodingName, const String& filename);

    bool isNull() const { return m_isNull; }
    bool isHTTP() const;

    const KURL& url() const;
    void setURL(const KURL&);

    const AtomicString& mimeType() const;
    void setMimeType(const AtomicString&);

    long long expectedContentLength() const;
    void setExpectedContentLength(long long);

    const AtomicString& textEncodingName() const;
    void setTextEncodingName(const AtomicString&);

    // FIXME: Should compute this on the fly.
    // There should not be a setter exposed, as suggested file name is determined based on other headers in a manner that WebCore does not necessarily know about.
    const String& suggestedFilename() const;
    void setSuggestedFilename(const String&);

    int httpStatusCode() const;
    void setHTTPStatusCode(int);

    const AtomicString& httpStatusText() const;
    void setHTTPStatusText(const AtomicString&);

    const AtomicString& httpHeaderField(const AtomicString& name) const;
    void setHTTPHeaderField(const AtomicString& name, const AtomicString& value);
    void addHTTPHeaderField(const AtomicString& name, const AtomicString& value);
    void clearHTTPHeaderField(const AtomicString& name);
    const HTTPHeaderMap& httpHeaderFields() const;

    bool isMultipart() const { return mimeType() == "multipart/x-mixed-replace"; }

    bool isAttachment() const;

    // FIXME: These are used by PluginStream on some platforms. Calculations may differ from just returning plain Last-Modified header.
    // Leaving it for now but this should go away in favor of generic solution.
    void setLastModifiedDate(time_t);
    time_t lastModifiedDate() const;

    // These functions return parsed values of the corresponding response headers.
    // NaN means that the header was not present or had invalid value.
    bool cacheControlContainsNoCache() const;
    bool cacheControlContainsNoStore() const;
    bool cacheControlContainsMustRevalidate() const;
    bool hasCacheValidatorFields() const;
    double cacheControlMaxAge() const;
    double cacheControlStaleWhileRevalidate() const;
    double date() const;
    double age() const;
    double expires() const;
    double lastModified() const;

    unsigned connectionID() const;
    void setConnectionID(unsigned);

    bool connectionReused() const;
    void setConnectionReused(bool);

    bool wasCached() const;
    void setWasCached(bool);

    ResourceLoadTiming* resourceLoadTiming() const;
    void setResourceLoadTiming(PassRefPtr<ResourceLoadTiming>);

    PassRefPtr<ResourceLoadInfo> resourceLoadInfo() const;
    void setResourceLoadInfo(PassRefPtr<ResourceLoadInfo>);

    HTTPVersion httpVersion() const { return m_httpVersion; }
    void setHTTPVersion(HTTPVersion version) { m_httpVersion = version; }

    const CString& getSecurityInfo() const { return m_securityInfo; }
    void setSecurityInfo(const CString& securityInfo) { m_securityInfo = securityInfo; }

    bool hasMajorCertificateErrors() const { return m_hasMajorCertificateErrors; }
    void setHasMajorCertificateErrors(bool hasMajorCertificateErrors) { m_hasMajorCertificateErrors = hasMajorCertificateErrors; }

    SecurityStyle getSecurityStyle() const { return m_securityStyle; }
    void setSecurityStyle(SecurityStyle securityStyle) { m_securityStyle = securityStyle; }

    const SecurityDetails* getSecurityDetails() const { return &m_securityDetails; }
    void setSecurityDetails(const String& protocol, const String& keyExchange, const String& cipher, const String& mac, int certId, size_t numUnknownScts, size_t numInvalidScts, size_t numValidScts, const SignedCertificateTimestampList& sctList);

    long long appCacheID() const { return m_appCacheID; }
    void setAppCacheID(long long id) { m_appCacheID = id; }

    const KURL& appCacheManifestURL() const { return m_appCacheManifestURL; }
    void setAppCacheManifestURL(const KURL& url) { m_appCacheManifestURL = url; }

    bool wasFetchedViaSPDY() const { return m_wasFetchedViaSPDY; }
    void setWasFetchedViaSPDY(bool value) { m_wasFetchedViaSPDY = value; }

    bool wasNpnNegotiated() const { return m_wasNpnNegotiated; }
    void setWasNpnNegotiated(bool value) { m_wasNpnNegotiated = value; }

    bool wasAlternateProtocolAvailable() const
    {
      return m_wasAlternateProtocolAvailable;
    }
    void setWasAlternateProtocolAvailable(bool value)
    {
      m_wasAlternateProtocolAvailable = value;
    }

    bool wasFetchedViaProxy() const { return m_wasFetchedViaProxy; }
    void setWasFetchedViaProxy(bool value) { m_wasFetchedViaProxy = value; }

    bool wasFetchedViaServiceWorker() const { return m_wasFetchedViaServiceWorker; }
    void setWasFetchedViaServiceWorker(bool value) { m_wasFetchedViaServiceWorker = value; }

    bool wasFallbackRequiredByServiceWorker() const { return m_wasFallbackRequiredByServiceWorker; }
    void setWasFallbackRequiredByServiceWorker(bool value) { m_wasFallbackRequiredByServiceWorker = value; }

    WebServiceWorkerResponseType serviceWorkerResponseType() const { return m_serviceWorkerResponseType; }
    void setServiceWorkerResponseType(WebServiceWorkerResponseType value) { m_serviceWorkerResponseType = value; }

    const KURL& originalURLViaServiceWorker() const { return m_originalURLViaServiceWorker; }
    void setOriginalURLViaServiceWorker(const KURL& url) { m_originalURLViaServiceWorker = url; }

    const Vector<char>& multipartBoundary() const { return m_multipartBoundary; }
    void setMultipartBoundary(const char* bytes, size_t size)
    {
        m_multipartBoundary.clear();
        m_multipartBoundary.append(bytes, size);
    }

    const String& cacheStorageCacheName() const { return m_cacheStorageCacheName; }
    void setCacheStorageCacheName(const String& cacheStorageCacheName) { m_cacheStorageCacheName = cacheStorageCacheName; }

    const Vector<String>& corsExposedHeaderNames() const { return m_corsExposedHeaderNames; }
    void setCorsExposedHeaderNames(const Vector<String>& headerNames)
    {
        m_corsExposedHeaderNames = headerNames;
    }

    int64_t responseTime() const { return m_responseTime; }
    void setResponseTime(int64_t responseTime) { m_responseTime = responseTime; }

    const AtomicString& remoteIPAddress() const { return m_remoteIPAddress; }
    void setRemoteIPAddress(const AtomicString& value) { m_remoteIPAddress = value; }

    unsigned short remotePort() const { return m_remotePort; }
    void setRemotePort(unsigned short value) { m_remotePort = value; }

    const String& downloadedFilePath() const { return m_downloadedFilePath; }
    void setDownloadedFilePath(const String&);

    // Extra data associated with this response.
    ExtraData* getExtraData() const { return m_extraData.get(); }
    void setExtraData(PassRefPtr<ExtraData> extraData) { m_extraData = extraData; }

    // The ResourceResponse subclass may "shadow" this method to provide platform-specific memory usage information
    unsigned memoryUsage() const
    {
        // average size, mostly due to URL and Header Map strings
        return 1280;
    }

    // This method doesn't compare the all members.
    static bool compare(const ResourceResponse&, const ResourceResponse&);

private:
    void updateHeaderParsedState(const AtomicString& name);

    KURL m_url;
    AtomicString m_mimeType;
    long long m_expectedContentLength;
    AtomicString m_textEncodingName;
    String m_suggestedFilename;
    int m_httpStatusCode;
    AtomicString m_httpStatusText;
    HTTPHeaderMap m_httpHeaderFields;
    time_t m_lastModifiedDate;
    bool m_wasCached : 1;
    unsigned m_connectionID;
    bool m_connectionReused : 1;
    RefPtr<ResourceLoadTiming> m_resourceLoadTiming;
    RefPtr<ResourceLoadInfo> m_resourceLoadInfo;

    bool m_isNull : 1;

    mutable CacheControlHeader m_cacheControlHeader;

    mutable bool m_haveParsedAgeHeader : 1;
    mutable bool m_haveParsedDateHeader : 1;
    mutable bool m_haveParsedExpiresHeader : 1;
    mutable bool m_haveParsedLastModifiedHeader : 1;

    mutable double m_age;
    mutable double m_date;
    mutable double m_expires;
    mutable double m_lastModified;

    // An opaque value that contains some information regarding the security of
    // the connection for this request, such as SSL connection info (empty
    // string if not over HTTPS).
    CString m_securityInfo;

    // True if the resource was retrieved by the embedder in spite of
    // certificate errors.
    bool m_hasMajorCertificateErrors;

    // The security style of the resource.
    // This only contains a valid value when the DevTools Network domain is
    // enabled. (Otherwise, it contains a default value of Unknown.)
    SecurityStyle m_securityStyle;

    // Security details of this request's connection.
    // If m_securityStyle is Unknown or Unauthenticated, this does not contain
    // valid data.
    SecurityDetails m_securityDetails;

    // HTTP version used in the response, if known.
    HTTPVersion m_httpVersion;

    // The id of the appcache this response was retrieved from, or zero if
    // the response was not retrieved from an appcache.
    long long m_appCacheID;

    // The manifest url of the appcache this response was retrieved from, if any.
    // Note: only valid for main resource responses.
    KURL m_appCacheManifestURL;

    // The multipart boundary of this response.
    Vector<char> m_multipartBoundary;

    // Was the resource fetched over SPDY.  See http://dev.chromium.org/spdy
    bool m_wasFetchedViaSPDY;

    // Was the resource fetched over a channel which used TLS/Next-Protocol-Negotiation (also SPDY related).
    bool m_wasNpnNegotiated;

    // Was the resource fetched over a channel which specified "Alternate-Protocol"
    // (e.g.: Alternate-Protocol: 443:npn-spdy/1).
    bool m_wasAlternateProtocolAvailable;

    // Was the resource fetched over an explicit proxy (HTTP, SOCKS, etc).
    bool m_wasFetchedViaProxy;

    // Was the resource fetched over a ServiceWorker.
    bool m_wasFetchedViaServiceWorker;

    // Was the fallback request with skip service worker flag required.
    bool m_wasFallbackRequiredByServiceWorker;

    // The type of the response which was fetched by the ServiceWorker.
    WebServiceWorkerResponseType m_serviceWorkerResponseType;

    // The original URL of the response which was fetched by the ServiceWorker.
    // This may be empty if the response was created inside the ServiceWorker.
    KURL m_originalURLViaServiceWorker;

    // The cache name of the CacheStorage from where the response is served via
    // the ServiceWorker. Null if the response isn't from the CacheStorage.
    String m_cacheStorageCacheName;

    // The headers that should be exposed according to CORS. Only guaranteed
    // to be set if the response was fetched by a ServiceWorker.
    Vector<String> m_corsExposedHeaderNames;

    // The time at which the response headers were received.  For cached
    // responses, this time could be "far" in the past.
    int64_t m_responseTime;

    // Remote IP address of the socket which fetched this resource.
    AtomicString m_remoteIPAddress;

    // Remote port number of the socket which fetched this resource.
    unsigned short m_remotePort;

    // The downloaded file path if the load streamed to a file.
    String m_downloadedFilePath;

    // The handle to the downloaded file to ensure the underlying file will not
    // be deleted.
    RefPtr<BlobDataHandle> m_downloadedFileHandle;

    // ExtraData associated with the response.
    RefPtr<ExtraData> m_extraData;
};

inline bool operator==(const ResourceResponse& a, const ResourceResponse& b) { return ResourceResponse::compare(a, b); }
inline bool operator!=(const ResourceResponse& a, const ResourceResponse& b) { return !(a == b); }

struct CrossThreadResourceResponseData {
    WTF_MAKE_NONCOPYABLE(CrossThreadResourceResponseData); USING_FAST_MALLOC(CrossThreadResourceResponseData);
public:
    CrossThreadResourceResponseData() { }
    KURL m_url;
    String m_mimeType;
    long long m_expectedContentLength;
    String m_textEncodingName;
    String m_suggestedFilename;
    int m_httpStatusCode;
    String m_httpStatusText;
    std::unique_ptr<CrossThreadHTTPHeaderMapData> m_httpHeaders;
    time_t m_lastModifiedDate;
    RefPtr<ResourceLoadTiming> m_resourceLoadTiming;
    CString m_securityInfo;
    bool m_hasMajorCertificateErrors;
    ResourceResponse::SecurityStyle m_securityStyle;
    ResourceResponse::SecurityDetails m_securityDetails;
    ResourceResponse::HTTPVersion m_httpVersion;
    long long m_appCacheID;
    KURL m_appCacheManifestURL;
    Vector<char> m_multipartBoundary;
    bool m_wasFetchedViaSPDY;
    bool m_wasNpnNegotiated;
    bool m_wasAlternateProtocolAvailable;
    bool m_wasFetchedViaProxy;
    bool m_wasFetchedViaServiceWorker;
    bool m_wasFallbackRequiredByServiceWorker;
    WebServiceWorkerResponseType m_serviceWorkerResponseType;
    KURL m_originalURLViaServiceWorker;
    String m_cacheStorageCacheName;
    int64_t m_responseTime;
    String m_remoteIPAddress;
    unsigned short m_remotePort;
    String m_downloadedFilePath;
    RefPtr<BlobDataHandle> m_downloadedFileHandle;
};

} // namespace blink

#endif // ResourceResponse_h
