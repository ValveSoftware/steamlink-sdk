/*
 * Platform specific crypto wrappers
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ryan Sleevi <ryan.sleevi@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/* $Id$ */
#include "certt.h"
#include "cryptohi.h"
#include "keythi.h"
#include "nss.h"
#include "secitem.h"
#include "ssl.h"
#include "sslimpl.h"
#include "prerror.h"
#include "prinit.h"

#ifdef NSS_PLATFORM_CLIENT_AUTH
#ifdef XP_WIN32
#include <NCrypt.h>
#endif
#endif

#ifdef NSS_PLATFORM_CLIENT_AUTH
CERTCertificateList*
hack_NewCertificateListFromCertList(CERTCertList* list)
{
    CERTCertificateList * chain = NULL;
    PLArenaPool * arena = NULL;
    CERTCertListNode * node;
    int len;

    if (CERT_LIST_EMPTY(list))
        goto loser;

    arena = PORT_NewArena(4096);
    if (arena == NULL)
        goto loser;

    for (len = 0, node = CERT_LIST_HEAD(list); !CERT_LIST_END(node, list);
        len++, node = CERT_LIST_NEXT(node)) {
    }

    chain = PORT_ArenaNew(arena, CERTCertificateList);
    if (chain == NULL)
        goto loser;

    chain->certs = PORT_ArenaNewArray(arena, SECItem, len);
    if (!chain->certs)
        goto loser;
    chain->len = len;

    for (len = 0, node = CERT_LIST_HEAD(list); !CERT_LIST_END(node, list);
        len++, node = CERT_LIST_NEXT(node)) {
        // Check to see if the last cert to be sent is a self-signed cert,
        // and if so, omit it from the list of certificates. However, if
        // there is only one cert (len == 0), include the cert, as it means
        // the EE cert is self-signed.
        if (len > 0 && (len == chain->len - 1) && node->cert->isRoot) {
            chain->len = len;
            break;
        }
        SECITEM_CopyItem(arena, &chain->certs[len], &node->cert->derCert);
    }

    chain->arena = arena;
    return chain;

loser:
    if (arena) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    return NULL;
}

#if defined(XP_WIN32)
typedef SECURITY_STATUS (WINAPI *NCryptFreeObjectFunc)(NCRYPT_HANDLE);
typedef SECURITY_STATUS (WINAPI *NCryptSignHashFunc)(
    NCRYPT_KEY_HANDLE /* hKey */,
    VOID* /* pPaddingInfo */,
    PBYTE /* pbHashValue */,
    DWORD /* cbHashValue */,
    PBYTE /* pbSignature */,
    DWORD /* cbSignature */,
    DWORD* /* pcbResult */,
    DWORD /* dwFlags */);

static PRCallOnceType cngFunctionsInitOnce;
static const PRCallOnceType pristineCallOnce;

static PRLibrary *ncrypt_library = NULL;
static NCryptFreeObjectFunc pNCryptFreeObject = NULL;
static NCryptSignHashFunc pNCryptSignHash = NULL;

static SECStatus
ssl_ShutdownCngFunctions(void *appData, void *nssData)
{
    pNCryptSignHash = NULL;
    pNCryptFreeObject = NULL;
    if (ncrypt_library) {
        PR_UnloadLibrary(ncrypt_library);
        ncrypt_library = NULL;
    }

    cngFunctionsInitOnce = pristineCallOnce;

    return SECSuccess;
}

static PRStatus
ssl_InitCngFunctions(void)
{
    SECStatus rv;

    ncrypt_library = PR_LoadLibrary("ncrypt.dll");
    if (ncrypt_library == NULL)
        goto loser;

    pNCryptFreeObject = (NCryptFreeObjectFunc)PR_FindFunctionSymbol(
        ncrypt_library, "NCryptFreeObject");
    if (pNCryptFreeObject == NULL)
        goto loser;

    pNCryptSignHash = (NCryptSignHashFunc)PR_FindFunctionSymbol(
        ncrypt_library, "NCryptSignHash");
    if (pNCryptSignHash == NULL)
        goto loser;

    rv = NSS_RegisterShutdown(ssl_ShutdownCngFunctions, NULL);
    if (rv != SECSuccess)
        goto loser;

    return PR_SUCCESS;

loser:
    pNCryptSignHash = NULL;
    pNCryptFreeObject = NULL;
    if (ncrypt_library) {
        PR_UnloadLibrary(ncrypt_library);
        ncrypt_library = NULL;
    }

    return PR_FAILURE;
}

static SECStatus
ssl_InitCng(void)
{
    if (PR_CallOnce(&cngFunctionsInitOnce, ssl_InitCngFunctions) != PR_SUCCESS)
        return SECFailure;
    return SECSuccess;
}

void
ssl_FreePlatformKey(PlatformKey key)
{
    if (!key)
        return;

    if (key->dwKeySpec == CERT_NCRYPT_KEY_SPEC) {
        if (ssl_InitCng() == SECSuccess) {
            (*pNCryptFreeObject)(key->hNCryptKey);
        }
    } else {
        CryptReleaseContext(key->hCryptProv, 0);
    }
    PORT_Free(key);
}

static SECStatus
ssl3_CngPlatformSignHashes(SSL3Hashes *hash, PlatformKey key, SECItem *buf,
                           PRBool isTLS, KeyType keyType)
{
    SECStatus       rv                = SECFailure;
    SECURITY_STATUS ncrypt_status;
    PRBool          doDerEncode       = PR_FALSE;
    SECItem         hashItem;
    DWORD           signatureLen      = 0;
    DWORD           dwFlags           = 0;
    VOID           *pPaddingInfo      = NULL;

    /* Always encode using PKCS#1 block type. */
    BCRYPT_PKCS1_PADDING_INFO rsaPaddingInfo;

    if (key->dwKeySpec != CERT_NCRYPT_KEY_SPEC) {
        PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
        return SECFailure;
    }
    if (ssl_InitCng() != SECSuccess) {
        PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
        return SECFailure;
    }

    switch (keyType) {
        case rsaKey:
            switch (hash->hashAlg) {
                case SEC_OID_UNKNOWN:
                    /* No OID/encoded DigestInfo. */
                    rsaPaddingInfo.pszAlgId = NULL;
                    break;
                case SEC_OID_SHA1:
                    rsaPaddingInfo.pszAlgId = BCRYPT_SHA1_ALGORITHM;
                    break;
                case SEC_OID_SHA256:
                    rsaPaddingInfo.pszAlgId = BCRYPT_SHA256_ALGORITHM;
                    break;
                case SEC_OID_SHA384:
                    rsaPaddingInfo.pszAlgId = BCRYPT_SHA384_ALGORITHM;
                    break;
                case SEC_OID_SHA512:
                    rsaPaddingInfo.pszAlgId = BCRYPT_SHA512_ALGORITHM;
                    break;
                default:
                    PORT_SetError(SSL_ERROR_UNSUPPORTED_HASH_ALGORITHM);
                    return SECFailure;
            }
            hashItem.data = hash->u.raw;
            hashItem.len  = hash->len;
            dwFlags       = BCRYPT_PAD_PKCS1;
            pPaddingInfo  = &rsaPaddingInfo;
            break;
        case dsaKey:
        case ecKey:
            if (keyType == ecKey) {
                doDerEncode = PR_TRUE;
            } else {
                doDerEncode = isTLS;
            }
            if (hash->hashAlg == SEC_OID_UNKNOWN) {
                hashItem.data = hash->u.s.sha;
                hashItem.len  = sizeof(hash->u.s.sha);
            } else {
                hashItem.data = hash->u.raw;
                hashItem.len  = hash->len;
            }
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_KEY);
            goto done;
    }
    PRINT_BUF(60, (NULL, "hash(es) to be signed", hashItem.data, hashItem.len));

    ncrypt_status = (*pNCryptSignHash)(key->hNCryptKey, pPaddingInfo,
                                       (PBYTE)hashItem.data, hashItem.len,
                                       NULL, 0, &signatureLen, dwFlags);
    if (FAILED(ncrypt_status) || signatureLen == 0) {
        PR_SetError(SSL_ERROR_SIGN_HASHES_FAILURE, ncrypt_status);
        goto done;
    }

    buf->data = (unsigned char *)PORT_Alloc(signatureLen);
    if (!buf->data) {
        goto done;    /* error code was set. */
    }

    ncrypt_status = (*pNCryptSignHash)(key->hNCryptKey, pPaddingInfo,
                                       (PBYTE)hashItem.data, hashItem.len,
                                       (PBYTE)buf->data, signatureLen,
                                       &signatureLen, dwFlags);
    if (FAILED(ncrypt_status) || signatureLen == 0) {
        PR_SetError(SSL_ERROR_SIGN_HASHES_FAILURE, ncrypt_status);
        goto done;
    }

    buf->len = signatureLen;

    if (doDerEncode) {
        SECItem   derSig = {siBuffer, NULL, 0};

        /* This also works for an ECDSA signature */
        rv = DSAU_EncodeDerSigWithLen(&derSig, buf, buf->len);
        if (rv == SECSuccess) {
            PORT_Free(buf->data);     /* discard unencoded signature. */
            *buf = derSig;            /* give caller encoded signature. */
        } else if (derSig.data) {
            PORT_Free(derSig.data);
        }
    } else {
        rv = SECSuccess;
    }

    PRINT_BUF(60, (NULL, "signed hashes", buf->data, buf->len));

done:
    if (rv != SECSuccess && buf->data) {
        PORT_Free(buf->data);
        buf->data = NULL;
        buf->len = 0;
    }

    return rv;
}

static SECStatus
ssl3_CAPIPlatformSignHashes(SSL3Hashes *hash, PlatformKey key, SECItem *buf,
                            PRBool isTLS, KeyType keyType)
{
    SECStatus    rv             = SECFailure;
    PRBool       doDerEncode    = PR_FALSE;
    SECItem      hashItem;
    DWORD        argLen         = 0;
    DWORD        signatureLen   = 0;
    ALG_ID       hashAlg        = 0;
    HCRYPTHASH   hHash          = 0;
    DWORD        hashLen        = 0;
    unsigned int i              = 0;

    buf->data = NULL;

    switch (hash->hashAlg) {
        case SEC_OID_UNKNOWN:
            hashAlg = 0;
            break;
        case SEC_OID_SHA1:
            hashAlg = CALG_SHA1;
            break;
        case SEC_OID_SHA256:
            hashAlg = CALG_SHA_256;
            break;
        case SEC_OID_SHA384:
            hashAlg = CALG_SHA_384;
            break;
        case SEC_OID_SHA512:
            hashAlg = CALG_SHA_512;
            break;
        default:
            PORT_SetError(SSL_ERROR_UNSUPPORTED_HASH_ALGORITHM);
            return SECFailure;
    }

    switch (keyType) {
        case rsaKey:
            if (hashAlg == 0) {
                hashAlg = CALG_SSL3_SHAMD5;
            }
            hashItem.data = hash->u.raw;
            hashItem.len = hash->len;
            break;
        case dsaKey:
        case ecKey:
            if (keyType == ecKey) {
                doDerEncode = PR_TRUE;
            } else {
                doDerEncode = isTLS;
            }
            if (hashAlg == 0) {
                hashAlg = CALG_SHA1;
                hashItem.data = hash->u.s.sha;
                hashItem.len = sizeof(hash->u.s.sha);
            } else {
                hashItem.data = hash->u.raw;
                hashItem.len = hash->len;
            }
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_KEY);
            goto done;
    }
    PRINT_BUF(60, (NULL, "hash(es) to be signed", hashItem.data, hashItem.len));

    if (!CryptCreateHash(key->hCryptProv, hashAlg, 0, 0, &hHash)) {
        PR_SetError(SSL_ERROR_SIGN_HASHES_FAILURE, GetLastError());
        goto done;
    }
    argLen = sizeof(hashLen);
    if (!CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLen, &argLen, 0)) {
        PR_SetError(SSL_ERROR_SIGN_HASHES_FAILURE, GetLastError());
        goto done;
    }
    if (hashLen != hashItem.len) {
        PR_SetError(SSL_ERROR_SIGN_HASHES_FAILURE, 0);
        goto done;
    }
    if (!CryptSetHashParam(hHash, HP_HASHVAL, (BYTE*)hashItem.data, 0)) {
        PR_SetError(SSL_ERROR_SIGN_HASHES_FAILURE, GetLastError());
        goto done;
    }
    if (!CryptSignHash(hHash, key->dwKeySpec, NULL, 0,
                       NULL, &signatureLen) || signatureLen == 0) {
        PR_SetError(SSL_ERROR_SIGN_HASHES_FAILURE, GetLastError());
        goto done;
    }
    buf->data = (unsigned char *)PORT_Alloc(signatureLen);
    if (!buf->data)
        goto done;    /* error code was set. */

    if (!CryptSignHash(hHash, key->dwKeySpec, NULL, 0,
                       (BYTE*)buf->data, &signatureLen)) {
        PR_SetError(SSL_ERROR_SIGN_HASHES_FAILURE, GetLastError());
        goto done;
    }
    buf->len = signatureLen;

    /* CryptoAPI signs in little-endian, so reverse */
    for (i = 0; i < buf->len / 2; ++i) {
        unsigned char tmp = buf->data[i];
        buf->data[i] = buf->data[buf->len - 1 - i];
        buf->data[buf->len - 1 - i] = tmp;
    }
    if (doDerEncode) {
        SECItem   derSig = {siBuffer, NULL, 0};

        /* This also works for an ECDSA signature */
        rv = DSAU_EncodeDerSigWithLen(&derSig, buf, buf->len);
        if (rv == SECSuccess) {
            PORT_Free(buf->data);     /* discard unencoded signature. */
            *buf = derSig;            /* give caller encoded signature. */
        } else if (derSig.data) {
            PORT_Free(derSig.data);
        }
    } else {
        rv = SECSuccess;
    }

    PRINT_BUF(60, (NULL, "signed hashes", buf->data, buf->len));
done:
    if (hHash)
        CryptDestroyHash(hHash);
    if (rv != SECSuccess && buf->data) {
        PORT_Free(buf->data);
        buf->data = NULL;
    }
    return rv;
}

SECStatus
ssl3_PlatformSignHashes(SSL3Hashes *hash, PlatformKey key, SECItem *buf,
                        PRBool isTLS, KeyType keyType)
{
    if (key->dwKeySpec == CERT_NCRYPT_KEY_SPEC) {
        return ssl3_CngPlatformSignHashes(hash, key, buf, isTLS, keyType);
    }
    return ssl3_CAPIPlatformSignHashes(hash, key, buf, isTLS, keyType);
}

#elif defined(XP_MACOSX)
#include <Security/cssm.h>

void
ssl_FreePlatformKey(PlatformKey key)
{
    CFRelease(key);
}

#define SSL_MAX_DIGEST_INFO_PREFIX 20

/* ssl3_GetDigestInfoPrefix sets |out| and |out_len| to point to a buffer that
 * contains ASN.1 data that should be prepended to a hash of the given type in
 * order to create a DigestInfo structure that is valid for use in a PKCS #1
 * v1.5 RSA signature. |out_len| will not be set to a value greater than
 * SSL_MAX_DIGEST_INFO_PREFIX. */
static SECStatus
ssl3_GetDigestInfoPrefix(SECOidTag hashAlg,
                         const SSL3Opaque** out, unsigned int *out_len)
{
    /* These are the DER encoding of ASN.1 DigestInfo structures:
     *   DigestInfo ::= SEQUENCE {
     *     digestAlgorithm AlgorithmIdentifier,
     *     digest OCTET STRING
     *   }
     * See PKCS #1 v2.2 Section 9.2, Note 1.
     */
    static const unsigned char kSHA1[] = {
        0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
        0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14
    };
    static const unsigned char kSHA224[] = {
        0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
        0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04, 0x05,
        0x00, 0x04, 0x1c
    };
    static const unsigned char kSHA256[] = {
        0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
        0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
        0x00, 0x04, 0x20
    };
    static const unsigned char kSHA384[] = {
        0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
        0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05,
        0x00, 0x04, 0x30
    };
    static const unsigned char kSHA512[] = {
        0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
        0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05,
        0x00, 0x04, 0x40
    };

    switch (hashAlg) {
    case SEC_OID_UNKNOWN:
        *out_len = 0;
        break;
    case SEC_OID_SHA1:
        *out = kSHA1;
        *out_len = sizeof(kSHA1);
        break;
    case SEC_OID_SHA224:
        *out = kSHA224;
        *out_len = sizeof(kSHA224);
        break;
    case SEC_OID_SHA256:
        *out = kSHA256;
        *out_len = sizeof(kSHA256);
        break;
    case SEC_OID_SHA384:
        *out = kSHA384;
        *out_len = sizeof(kSHA384);
        break;
    case SEC_OID_SHA512:
        *out = kSHA512;
        *out_len = sizeof(kSHA512);
        break;
    default:
        PORT_SetError(SSL_ERROR_UNSUPPORTED_HASH_ALGORITHM);
        return SECFailure;
    }

    return SECSuccess;
}

/* Given the length of a raw DSA signature (consisting of two integers
 * r and s), returns the maximum length of the DER encoding of the
 * following structure:
 *
 *    Dss-Sig-Value ::= SEQUENCE {
 *        r INTEGER,
 *        s INTEGER
 *    }
 */
static unsigned int
ssl3_DSAMaxDerEncodedLength(unsigned int rawDsaLen)
{
    /* The length of one INTEGER. */
    unsigned int integerDerLen = rawDsaLen/2 +  /* the integer itself */
            1 +  /* additional zero byte if high bit is 1 */
            SEC_ASN1LengthLength(rawDsaLen/2 + 1) +  /* length */
            1;  /* INTEGER tag */

    /* The length of two INTEGERs in a SEQUENCE */
    return 2 * integerDerLen +  /* two INTEGERs */
            SEC_ASN1LengthLength(2 * integerDerLen) +  /* length */
            1;  /* SEQUENCE tag */
}

SECStatus
ssl3_PlatformSignHashes(SSL3Hashes *hash, PlatformKey key, SECItem *buf,
                        PRBool isTLS, KeyType keyType)
{
    SECStatus       rv                  = SECFailure;
    PRBool          doDerDecode         = PR_FALSE;
    unsigned int    rawDsaLen;
    unsigned int    signatureLen;
    OSStatus        status              = noErr;
    CSSM_CSP_HANDLE cspHandle           = 0;
    const CSSM_KEY *cssmKey             = NULL;
    CSSM_ALGORITHMS sigAlg;
    CSSM_ALGORITHMS digestAlg;
    const CSSM_ACCESS_CREDENTIALS * cssmCreds = NULL;
    CSSM_RETURN     cssmRv;
    CSSM_DATA       hashData;
    CSSM_DATA       signatureData;
    CSSM_CC_HANDLE  cssmSignature       = 0;
    const SSL3Opaque* prefix;
    unsigned int    prefixLen;
    SSL3Opaque      prefixAndHash[SSL_MAX_DIGEST_INFO_PREFIX + HASH_LENGTH_MAX];

    buf->data = NULL;

    status = SecKeyGetCSPHandle(key, &cspHandle);
    if (status != noErr) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        goto done;
    }

    status = SecKeyGetCSSMKey(key, &cssmKey);
    if (status != noErr || !cssmKey) {
        PORT_SetError(SEC_ERROR_NO_KEY);
        goto done;
    }

    sigAlg = cssmKey->KeyHeader.AlgorithmId;
    digestAlg = CSSM_ALGID_NONE;

    switch (keyType) {
        case rsaKey:
            PORT_Assert(sigAlg == CSSM_ALGID_RSA);
            signatureLen = (cssmKey->KeyHeader.LogicalKeySizeInBits + 7) / 8;
            if (ssl3_GetDigestInfoPrefix(hash->hashAlg, &prefix, &prefixLen) !=
                SECSuccess) {
                goto done;
            }
            if (prefixLen + hash->len > sizeof(prefixAndHash)) {
                PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
                goto done;
            }
            memcpy(prefixAndHash, prefix, prefixLen);
            memcpy(prefixAndHash + prefixLen, hash->u.raw, hash->len);
            hashData.Data   = prefixAndHash;
            hashData.Length = prefixLen + hash->len;
            break;
        case dsaKey:
        case ecKey:
            /* SSL3 DSA signatures are raw, not DER-encoded. CSSM gives back
             * DER-encoded signatures, so they must be decoded. */
            doDerDecode = (keyType == dsaKey) && !isTLS;

            /* Compute the maximum size of a DER-encoded signature: */
            if (keyType == ecKey) {
                PORT_Assert(sigAlg == CSSM_ALGID_ECDSA);
                /* LogicalKeySizeInBits is the size of an EC public key. But an
                 * ECDSA signature length depends on the size of the base
                 * point's order. For P-256, P-384, and P-521, these two sizes
                 * are the same. */
                rawDsaLen =
                    (cssmKey->KeyHeader.LogicalKeySizeInBits + 7) / 8 * 2;
            } else {
                /* TODO(davidben): Get the size of the subprime out of CSSM. For
                 * now, assume 160; Apple's implementation hardcodes it. */
                PORT_Assert(sigAlg == CSSM_ALGID_DSA);
                rawDsaLen = 2 * (160 / 8);
            }
            signatureLen = ssl3_DSAMaxDerEncodedLength(rawDsaLen);

            /* SEC_OID_UNKNOWN is used to specify the MD5/SHA1 concatenated
             * hash.  In that case, we use just the SHA1 part. */
            if (hash->hashAlg == SEC_OID_UNKNOWN) {
                hashData.Data   = hash->u.s.sha;
                hashData.Length = sizeof(hash->u.s.sha);
            } else {
                hashData.Data   = hash->u.raw;
                hashData.Length = hash->len;
            }
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_KEY);
            goto done;
    }
    PRINT_BUF(60, (NULL, "hash(es) to be signed", hashData.Data, hashData.Length));

    if (signatureLen == 0) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        goto done;
    }

    buf->data = (unsigned char *)PORT_Alloc(signatureLen);
    if (!buf->data)
        goto done;    /* error code was set. */

    /* TODO(rsleevi): Should it be kSecCredentialTypeNoUI? In Win32, at least,
     * you can prevent the UI by setting the provider handle on the
     * certificate to be opened with CRYPT_SILENT, but is there an equivalent?
     */
    status = SecKeyGetCredentials(key, CSSM_ACL_AUTHORIZATION_SIGN,
                                  kSecCredentialTypeDefault, &cssmCreds);
    if (status != noErr) {
        PR_SetError(SSL_ERROR_SIGN_HASHES_FAILURE, status);
        goto done;
    }

    signatureData.Length = signatureLen;
    signatureData.Data   = (uint8*)buf->data;

    cssmRv = CSSM_CSP_CreateSignatureContext(cspHandle, sigAlg, cssmCreds,
                                             cssmKey, &cssmSignature);
    if (cssmRv) {
        PR_SetError(SSL_ERROR_SIGN_HASHES_FAILURE, cssmRv);
        goto done;
    }

    /* See "Apple Cryptographic Service Provider Functional Specification" */
    if (cssmKey->KeyHeader.AlgorithmId == CSSM_ALGID_RSA) {
        /* To set RSA blinding for RSA keys */
        CSSM_CONTEXT_ATTRIBUTE blindingAttr;
        blindingAttr.AttributeType   = CSSM_ATTRIBUTE_RSA_BLINDING;
        blindingAttr.AttributeLength = sizeof(uint32);
        blindingAttr.Attribute.Uint32 = 1;
        cssmRv = CSSM_UpdateContextAttributes(cssmSignature, 1, &blindingAttr);
        if (cssmRv) {
            PR_SetError(SSL_ERROR_SIGN_HASHES_FAILURE, cssmRv);
            goto done;
        }
    }

    cssmRv = CSSM_SignData(cssmSignature, &hashData, 1, digestAlg,
                           &signatureData);
    if (cssmRv) {
        PR_SetError(SSL_ERROR_SIGN_HASHES_FAILURE, cssmRv);
        goto done;
    }
    buf->len = signatureData.Length;

    if (doDerDecode) {
        SECItem* rawSig = DSAU_DecodeDerSigToLen(buf, rawDsaLen);
        if (rawSig != NULL) {
            PORT_Free(buf->data);     /* discard encoded signature. */
            *buf = *rawSig;           /* give caller unencoded signature. */
            PORT_Free(rawSig);
            rv = SECSuccess;
        }
    } else {
        rv = SECSuccess;
    }

    PRINT_BUF(60, (NULL, "signed hashes", buf->data, buf->len));
done:
    /* cspHandle, cssmKey, and cssmCreds are owned by the SecKeyRef and
     * should not be freed. When the PlatformKey is freed, they will be
     * released.
     */
    if (cssmSignature)
        CSSM_DeleteContext(cssmSignature);

    if (rv != SECSuccess && buf->data) {
        PORT_Free(buf->data);
        buf->data = NULL;
    }
    return rv;
}
#else
void
ssl_FreePlatformKey(PlatformKey key)
{
}

SECStatus
ssl3_PlatformSignHashes(SSL3Hashes *hash, PlatformKey key, SECItem *buf,
                        PRBool isTLS, KeyType keyType)
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}
#endif

#endif /* NSS_PLATFORM_CLIENT_AUTH */
