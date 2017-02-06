// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/SubresourceIntegrity.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/fetch/Resource.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/Crypto.h"
#include "platform/ParsingUtilities.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ASCIICType.h"
#include "wtf/Vector.h"
#include "wtf/dtoa/utils.h"
#include "wtf/text/Base64.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/WTFString.h"

namespace blink {

// FIXME: This should probably use common functions with ContentSecurityPolicy.
static bool isIntegrityCharacter(UChar c)
{
    // Check if it's a base64 encoded value. We're pretty loose here, as there's
    // not much risk in it, and it'll make it simpler for developers.
    return isASCIIAlphanumeric(c) || c == '_' || c == '-' || c == '+' || c == '/' || c == '=';
}

static bool isValueCharacter(UChar c)
{
    // VCHAR per https://tools.ietf.org/html/rfc5234#appendix-B.1
    return c >= 0x21 && c <= 0x7e;
}

static void logErrorToConsole(const String& message, Document& document)
{
    document.addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, message));
}

static bool DigestsEqual(const DigestValue& digest1, const DigestValue& digest2)
{
    if (digest1.size() != digest2.size())
        return false;

    for (size_t i = 0; i < digest1.size(); i++) {
        if (digest1[i] != digest2[i])
            return false;
    }

    return true;
}

static String digestToString(const DigestValue& digest)
{
    return base64Encode(reinterpret_cast<const char*>(digest.data()), digest.size(), Base64DoNotInsertLFs);
}


HashAlgorithm SubresourceIntegrity::getPrioritizedHashFunction(HashAlgorithm algorithm1, HashAlgorithm algorithm2)
{
    const HashAlgorithm weakerThanSha384[] = { HashAlgorithmSha256 };
    const HashAlgorithm weakerThanSha512[] = { HashAlgorithmSha256, HashAlgorithmSha384 };

    ASSERT(algorithm1 != HashAlgorithmSha1);
    ASSERT(algorithm2 != HashAlgorithmSha1);

    if (algorithm1 == algorithm2)
        return algorithm1;

    const HashAlgorithm* weakerAlgorithms = 0;
    size_t length = 0;
    switch (algorithm1) {
    case HashAlgorithmSha256:
        break;
    case HashAlgorithmSha384:
        weakerAlgorithms = weakerThanSha384;
        length = WTF_ARRAY_LENGTH(weakerThanSha384);
        break;
    case HashAlgorithmSha512:
        weakerAlgorithms = weakerThanSha512;
        length = WTF_ARRAY_LENGTH(weakerThanSha512);
        break;
    default:
        ASSERT_NOT_REACHED();
    };

    for (size_t i = 0; i < length; i++) {
        if (weakerAlgorithms[i] == algorithm2)
            return algorithm1;
    }

    return algorithm2;
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(const Element& element, const char* content, size_t size, const KURL& resourceUrl, const Resource& resource)
{
    Document& document = element.document();
    String attribute = element.fastGetAttribute(HTMLNames::integrityAttr);
    if (attribute.isEmpty())
        return true;

    IntegrityMetadataSet metadataSet;
    IntegrityParseResult integrityParseResult = parseIntegrityAttribute(attribute, metadataSet, &document);
    // On failed parsing, there's no need to log an error here, as
    // parseIntegrityAttribute() will output an appropriate console message.
    if (integrityParseResult != IntegrityParseValidResult)
        return true;

    return CheckSubresourceIntegrity(metadataSet, element, content, size, resourceUrl, resource);
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(const IntegrityMetadataSet& metadataSet, const Element& element, const char* content, size_t size, const KURL& resourceUrl, const Resource& resource)
{
    Document& document = element.document();

    if (!resource.isEligibleForIntegrityCheck(document.getSecurityOrigin())) {
        UseCounter::count(document, UseCounter::SRIElementIntegrityAttributeButIneligible);
        logErrorToConsole("Subresource Integrity: The resource '" + resourceUrl.elidedString() + "' has an integrity attribute, but the resource requires the request to be CORS enabled to check the integrity, and it is not. The resource has been blocked because the integrity cannot be enforced.", document);
        return false;
    }

    String errorMessage;
    bool result = CheckSubresourceIntegrity(metadataSet, content, size, resourceUrl, document, errorMessage);
    if (!result)
        logErrorToConsole(errorMessage, document);
    return result;
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(const String& integrityMetadata, const char* content, size_t size, const KURL& resourceUrl, Document& document, String& errorMessage)
{
    IntegrityMetadataSet metadataSet;
    IntegrityParseResult integrityParseResult = parseIntegrityAttribute(integrityMetadata, metadataSet, &document);
    // On failed parsing, there's no need to log an error here, as
    // parseIntegrityAttribute() will output an appropriate console message.
    if (integrityParseResult != IntegrityParseValidResult)
        return true;

    return CheckSubresourceIntegrity(metadataSet, content, size, resourceUrl, document, errorMessage);
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(const IntegrityMetadataSet& metadataSet, const char* content, size_t size, const KURL& resourceUrl, Document& document, String& errorMessage)
{
    if (!metadataSet.size())
        return true;

    HashAlgorithm strongestAlgorithm = HashAlgorithmSha256;
    for (const IntegrityMetadata& metadata : metadataSet)
        strongestAlgorithm = getPrioritizedHashFunction(metadata.algorithm(), strongestAlgorithm);

    DigestValue digest;
    for (const IntegrityMetadata& metadata : metadataSet) {
        if (metadata.algorithm() != strongestAlgorithm)
            continue;

        digest.clear();
        bool digestSuccess = computeDigest(metadata.algorithm(), content, size, digest);

        if (digestSuccess) {
            Vector<char> hashVector;
            base64Decode(metadata.digest(), hashVector);
            DigestValue convertedHashVector;
            convertedHashVector.append(reinterpret_cast<uint8_t*>(hashVector.data()), hashVector.size());

            if (DigestsEqual(digest, convertedHashVector)) {
                UseCounter::count(document, UseCounter::SRIElementWithMatchingIntegrityAttribute);
                return true;
            }
        }
    }

    digest.clear();
    if (computeDigest(HashAlgorithmSha256, content, size, digest)) {
        // This message exposes the digest of the resource to the console.
        // Because this is only to the console, that's okay for now, but we
        // need to be very careful not to expose this in exceptions or
        // JavaScript, otherwise it risks exposing information about the
        // resource cross-origin.
        errorMessage = "Failed to find a valid digest in the 'integrity' attribute for resource '" + resourceUrl.elidedString() + "' with computed SHA-256 integrity '" + digestToString(digest) + "'. The resource has been blocked.";
    } else {
        errorMessage = "There was an error computing an integrity value for resource '" + resourceUrl.elidedString() + "'. The resource has been blocked.";
    }
    UseCounter::count(document, UseCounter::SRIElementWithNonMatchingIntegrityAttribute);
    return false;
}


// Before:
//
// [algorithm]-[hash]
// ^                 ^
// position          end
//
// After (if successful: if the method does not return AlgorithmValid, we make
// no promises and the caller should exit early):
//
// [algorithm]-[hash]
//            ^      ^
//     position    end
SubresourceIntegrity::AlgorithmParseResult SubresourceIntegrity::parseAlgorithm(const UChar*& position, const UChar* end, HashAlgorithm& algorithm)
{
    // Any additions or subtractions from this struct should also modify the
    // respective entries in the kAlgorithmMap array in checkDigest() as well
    // as the array in algorithmToString().
    static const struct {
        const char* prefix;
        HashAlgorithm algorithm;
    } kSupportedPrefixes[] = {
        { "sha256", HashAlgorithmSha256 },
        { "sha-256", HashAlgorithmSha256 },
        { "sha384", HashAlgorithmSha384 },
        { "sha-384", HashAlgorithmSha384 },
        { "sha512", HashAlgorithmSha512 },
        { "sha-512", HashAlgorithmSha512 }
    };

    const UChar* begin = position;

    for (auto& prefix : kSupportedPrefixes) {
        if (skipToken<UChar>(position, end, prefix.prefix)) {
            if (!skipExactly<UChar>(position, end, '-')) {
                position = begin;
                continue;
            }
            algorithm = prefix.algorithm;
            return AlgorithmValid;
        }
    }

    skipUntil<UChar>(position, end, '-');
    if (position < end && *position == '-') {
        position = begin;
        return AlgorithmUnknown;
    }

    position = begin;
    return AlgorithmUnparsable;
}

// Before:
//
// [algorithm]-[hash]      OR     [algorithm]-[hash]?[options]
//             ^     ^                        ^               ^
//      position   end                 position             end
//
// After (if successful: if the method returns false, we make no promises and the caller should exit early):
//
// [algorithm]-[hash]      OR     [algorithm]-[hash]?[options]
//                   ^                              ^         ^
//        position/end                       position       end
bool SubresourceIntegrity::parseDigest(const UChar*& position, const UChar* end, String& digest)
{
    const UChar* begin = position;
    skipWhile<UChar, isIntegrityCharacter>(position, end);

    if (position == begin || (position != end && *position != '?')) {
        digest = emptyString();
        return false;
    }

    // We accept base64url encoding, but normalize to "normal" base64 internally:
    digest = normalizeToBase64(String(begin, position - begin));
    return true;
}

SubresourceIntegrity::IntegrityParseResult SubresourceIntegrity::parseIntegrityAttribute(const WTF::String& attribute, IntegrityMetadataSet& metadataSet)
{
    return parseIntegrityAttribute(attribute, metadataSet, nullptr);
}

SubresourceIntegrity::IntegrityParseResult SubresourceIntegrity::parseIntegrityAttribute(const WTF::String& attribute, IntegrityMetadataSet& metadataSet, Document* document)
{
    Vector<UChar> characters;
    attribute.stripWhiteSpace().appendTo(characters);
    const UChar* position = characters.data();
    const UChar* end = characters.end();
    const UChar* currentIntegrityEnd;

    metadataSet.clear();
    bool error = false;

    // The integrity attribute takes the form:
    //    *WSP hash-with-options *( 1*WSP hash-with-options ) *WSP / *WSP
    // To parse this, break on whitespace, parsing each algorithm/digest/option
    // in order.
    while (position < end) {
        WTF::String digest;
        HashAlgorithm algorithm;

        skipWhile<UChar, isASCIISpace>(position, end);
        currentIntegrityEnd = position;
        skipUntil<UChar, isASCIISpace>(currentIntegrityEnd, end);

        // Algorithm parsing errors are non-fatal (the subresource should
        // still be loaded) because strong hash algorithms should be used
        // without fear of breaking older user agents that don't support
        // them.
        AlgorithmParseResult parseResult = parseAlgorithm(position, currentIntegrityEnd, algorithm);
        if (parseResult == AlgorithmUnknown) {
            // Unknown hash algorithms are treated as if they're not present,
            // and thus are not marked as an error, they're just skipped.
            skipUntil<UChar, isASCIISpace>(position, end);
            if (document) {
                logErrorToConsole("Error parsing 'integrity' attribute ('" + attribute + "'). The specified hash algorithm must be one of 'sha256', 'sha384', or 'sha512'.", *document);
                UseCounter::count(*document, UseCounter::SRIElementWithUnparsableIntegrityAttribute);
            }
            continue;
        }

        if (parseResult == AlgorithmUnparsable) {
            error = true;
            skipUntil<UChar, isASCIISpace>(position, end);
            if (document) {
                logErrorToConsole("Error parsing 'integrity' attribute ('" + attribute + "'). The hash algorithm must be one of 'sha256', 'sha384', or 'sha512', followed by a '-' character.", *document);
                UseCounter::count(*document, UseCounter::SRIElementWithUnparsableIntegrityAttribute);
            }
            continue;
        }

        ASSERT(parseResult == AlgorithmValid);

        if (!parseDigest(position, currentIntegrityEnd, digest)) {
            error = true;
            skipUntil<UChar, isASCIISpace>(position, end);
            if (document) {
                logErrorToConsole("Error parsing 'integrity' attribute ('" + attribute + "'). The digest must be a valid, base64-encoded value.", *document);
                UseCounter::count(*document, UseCounter::SRIElementWithUnparsableIntegrityAttribute);
            }
            continue;
        }

        // The spec defines a space in the syntax for options, separated by a
        // '?' character followed by unbounded VCHARs, but no actual options
        // have been defined yet. Thus, for forward compatibility, ignore any
        // options specified.
        if (skipExactly<UChar>(position, end, '?')) {
            const UChar* begin = position;
            skipWhile<UChar, isValueCharacter>(position, end);
            if (begin != position && document)
                logErrorToConsole("Ignoring unrecogized 'integrity' attribute option '" + String(begin, position - begin) + "'.", *document);
        }

        IntegrityMetadata integrityMetadata(digest, algorithm);
        metadataSet.add(integrityMetadata.toPair());
    }

    if (metadataSet.size() == 0 && error)
        return IntegrityParseNoValidResult;

    return IntegrityParseValidResult;
}

} // namespace blink
