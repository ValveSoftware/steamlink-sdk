/*
 * Qt implementation of OpenWnn library
 * This file is part of the Qt Virtual Keyboard module.
 * Contact: http://www.qt.io/licensing/
 *
 * Copyright (C) 2015  The Qt Company
 * Copyright (C) 2008-2012  OMRON SOFTWARE Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nj_lib.h"
#include "nj_err.h"
#include "nj_ext.h"
#include "nj_dic.h"


#include <stdlib.h>
#include <string.h>

#include "openwnndictionary.h"

#include <QtCore/private/qobject_p.h>

/**
 * Error codes
 */
#define NJ_FUNC_SET_DICTIONARY_PARAMETERS               0x00FA
#define NJ_FUNC_SET_APPROX_PATTERN                      0x00F6
#define NJ_FUNC_GET_LEFT_PART_OF_SPEECH                 0x00F5
#define NJ_FUNC_GET_RIGHT_PART_OF_SPEECH                0x00F4
#define NJ_FUNC_SET_LEFT_PART_OF_SPEECH                 0x00F3
#define NJ_FUNC_SET_RIGHT_PART_OF_SPEECH                0x00F2
#define NJ_FUNC_SET_STROKE                              0x00F1
#define NJ_FUNC_SET_CANDIDATE                           0x00F0
#define NJ_FUNC_GET_LEFT_PART_OF_SPEECH_SPECIFIED_TYPE  0x00EE
#define NJ_FUNC_GET_RIGHT_PART_OF_SPEECH_SPECIFIED_TYPE 0x00ED

#define NJ_ERR_ALLOC_FAILED                             0x7D00
#define NJ_ERR_NOT_ALLOCATED                            0x7C00
#define NJ_ERR_INVALID_PARAM                            0x7B00
#define NJ_ERR_APPROX_PATTERN_IS_FULL                   0x7A00

/**
 * Structure of internal work area
 */
#define NJ_MAX_CHARSET_FROM_LEN                     1
#define NJ_MAX_CHARSET_TO_LEN                       3
#define NJ_APPROXSTORE_SIZE                         (NJ_MAX_CHARSET_FROM_LEN + NJ_TERM_LEN + NJ_MAX_CHARSET_TO_LEN + NJ_TERM_LEN)


#define NJ_JNI_FLAG_NONE                            (0x00)
#define NJ_JNI_FLAG_ENABLE_CURSOR                   (0x01)
#define NJ_JNI_FLAG_ENABLE_RESULT                   (0x02)

/**
 * Predefined approx patterns
 */
typedef struct {
    int         size;
    NJ_UINT8   *from;
    NJ_UINT8   *to;
} PREDEF_APPROX_PATTERN;

#include "predef_table.h"

typedef struct {
    NJ_DIC_HANDLE       dicHandle[NJ_MAX_DIC];
    NJ_UINT32           dicSize[NJ_MAX_DIC];
    NJ_UINT8            dicType[NJ_MAX_DIC];
    NJ_CHAR             keyString[NJ_MAX_LEN + NJ_TERM_LEN];
    NJ_RESULT           result;
    NJ_CURSOR           cursor;
    NJ_SEARCH_CACHE     srhCache[NJ_MAX_DIC];
    NJ_DIC_SET          dicSet;
    NJ_CLASS            wnnClass;
    NJ_CHARSET          approxSet;
    NJ_CHAR             approxStr[NJ_MAX_CHARSET * NJ_APPROXSTORE_SIZE];
    NJ_CHAR             previousStroke[NJ_MAX_LEN + NJ_TERM_LEN];
    NJ_CHAR             previousCandidate[NJ_MAX_RESULT_LEN + NJ_TERM_LEN];
    NJ_UINT8            flag;
} NJ_WORK;

extern "C" {
    extern NJ_UINT32 dic_size[];
    extern NJ_UINT8 dic_type[];
    extern NJ_UINT8 *dic_data[];
    extern NJ_UINT8 *con_data[];
}

class OpenWnnDictionaryPrivate : public QObjectPrivate
{
public:
    OpenWnnDictionaryPrivate()
    {
        init();
    }

    ~OpenWnnDictionaryPrivate()
    {
    }

    void init()
    {
        /* Initialize the work area */
        memset(&work, 0, sizeof(NJ_WORK));

        for (int i = 0; i < NJ_MAX_DIC; i++) {
            work.dicHandle[i] = dic_data[i];
            work.dicSize[i] = dic_size[i];
            work.dicType[i] = dic_type[i];
        }

        if (con_data != NULL) {
            work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN] = con_data[0];
        }

        /* Execute the initialize method to initialize the internal work area */
        njx_init(&(work.wnnClass));
    }

    static void clearDictionaryStructure(NJ_DIC_INFO *dicInfo)
    {
        dicInfo->type       = 0;
        dicInfo->handle     = NULL;
    /*  dicInfo->srhCache   = NULL; */

        dicInfo->dic_freq[NJ_MODE_TYPE_HENKAN].base = 0;
        dicInfo->dic_freq[NJ_MODE_TYPE_HENKAN].high = 0;
    }

    static NJ_CHAR convertUTFCharToNjChar(const NJ_UINT8 *src)
    {
        NJ_CHAR     ret;
        NJ_UINT8*   dst;

        /* convert UTF-16BE character to NJ_CHAR format */
        dst = (NJ_UINT8*)&ret;
        dst[0] = src[0];
        dst[1] = src[1];

        return ret;
    }

    static void convertStringToNjChar(NJ_CHAR *dst, const QString &srcString, int maxChars)
    {
        const QByteArray utf8Data(srcString.toUtf8());
        const unsigned char *src = (const unsigned char *)utf8Data.constData();
        int i, o;

        /* convert UTF-8 to UTF-16BE */
        for (i = o = 0; src[i] != 0x00 && o < maxChars;) {
            NJ_UINT8 *dst_tmp;
            dst_tmp = (NJ_UINT8 *)&(dst[o]);

            if ((src[i] & 0x80) == 0x00) {
                /* U+0000 ... U+007f */
                /* 8[0xxxxxxx] -> 16BE[00000000 0xxxxxxx] */
                dst_tmp[0] = 0x00;
                dst_tmp[1] = src[i + 0] & 0x7f;
                i++;
                o++;
            } else if ((src[i] & 0xe0) == 0xc0) {
                /* U+0080 ... U+07ff */
                /* 8[110xxxxx 10yyyyyy] -> 16BE[00000xxx xxyyyyyy] */
                if (src[i + 1] == 0x00) {
                    break;
                }
                dst_tmp[0] = ((src[i + 0] & 0x1f) >> 2);
                dst_tmp[1] = ((src[i + 0] & 0x1f) << 6) | (src[i + 1] & 0x3f);
                i += 2;
                o++;
            } else if ((src[i] & 0xf0) == 0xe0) {
                /* U+0800 ... U+ffff */
                /* 8[1110xxxx 10yyyyyy 10zzzzzz] -> 16BE[xxxxyyyy yyzzzzzz] */
                if (src[i + 1] == 0x00 || src[i + 2] == 0x00) {
                    break;
                }
                dst_tmp[0] = ((src[i + 0] & 0x0f) << 4) | ((src[i + 1] & 0x3f) >> 2);
                dst_tmp[1] = ((src[i + 1] & 0x3f) << 6) | (src[i + 2] & 0x3f);
                i += 3;
                o++;
            } else if ((src[i] & 0xf8) == 0xf0) {
                NJ_UINT8    dst1, dst2, dst3;
                /* U+10000 ... U+10ffff */
                /* 8[11110www 10xxxxxx 10yyyyyy 10zzzzzz] -> 32BE[00000000 000wwwxx xxxxyyyy yyzzzzzz] */
                /*                                        -> 16BE[110110WW XXxxxxyy 110111yy yyzzzzzz] */
                /*                                                      -- --======       == --------  */
                /*                                                      dst1   dst2          dst3      */
                /*                                        "wwwxx"(00001-10000) - 1 = "WWXX"(0000-1111) */
                if (!(o < maxChars - 1)) {
                    /* output buffer is full */
                    break;
                }
                if (src[i + 1] == 0x00 || src[i + 2] == 0x00 || src[i + 3] == 0x00) {
                    break;
                }
                dst1 = (((src[i + 0] & 0x07) << 2) | ((src[i + 1] & 0x3f) >> 4)) - 1;
                dst2 = ((src[i + 1] & 0x3f) << 4) | ((src[i + 2] & 0x3f) >> 2);
                dst3 = ((src[i + 2] & 0x3f) << 6) | (src[i + 3] & 0x3f);

                dst_tmp[0] = 0xd8 | ((dst1 & 0x0c) >> 2);
                dst_tmp[1] = ((dst1 & 0x03) << 6) | ((dst2 & 0xfc) >> 2);
                dst_tmp[2] = 0xdc | ((dst2 & 0x03));
                dst_tmp[3] = dst3;
                i += 4;
                o += 2;
            } else {    /* Broken code */
                break;
            }
        }

        dst[o] = NJ_CHAR_NUL;
    }

    static QString convertNjCharToString(const NJ_CHAR *src, int maxChars)
    {
        QByteArray dst((NJ_MAX_LEN + NJ_MAX_RESULT_LEN + NJ_TERM_LEN) * 3 + 1, Qt::Uninitialized);

        int i, o;

        /* convert UTF-16BE to a UTF-8 */
        for (i = o = 0; src[i] != 0x0000 && i < maxChars;) {
            NJ_UINT8* src_tmp;
            src_tmp = (NJ_UINT8*)&(src[i]);

            if (src_tmp[0] == 0x00 && src_tmp[1] <= 0x7f) {
                /* U+0000 ... U+007f */
                /* 16BE[00000000 0xxxxxxx] -> 8[0xxxxxxx] */
                dst[o + 0] = src_tmp[1] & 0x007f;
                i++;
                o++;
            } else if (src_tmp[0] <= 0x07) {
                /* U+0080 ... U+07ff */
                /* 16BE[00000xxx xxyyyyyy] -> 8[110xxxxx 10yyyyyy] */
                dst[o + 0] = 0xc0 | ((src_tmp[0] & 0x07) << 2) | ((src_tmp[1] & 0xc0) >> 6);
                dst[o + 1] = 0x80 | ((src_tmp[1] & 0x3f));
                i++;
                o += 2;
            } else if (src_tmp[0] >= 0xd8 && src_tmp[0] <= 0xdb) {
                NJ_UINT8    src1, src2, src3;
                /* U+10000 ... U+10ffff (surrogate pair) */
                /* 32BE[00000000 000wwwxx xxxxyyyy yyzzzzzz] -> 8[11110www 10xxxxxx 10yyyyyy 10zzzzzz] */
                /* 16BE[110110WW XXxxxxyy 110111yy yyzzzzzz]                                           */
                /*            -- --======       == --------                                            */
                /*            src1 src2            src3                                                */
                /* "WWXX"(0000-1111) + 1 = "wwwxx"(0001-10000)                                         */
                if (!(i < maxChars - 1) || src_tmp[2] < 0xdc || src_tmp[2] > 0xdf) {
                    /* That is broken code */
                    break;
                }
                src1 = (((src_tmp[0] & 0x03) << 2) | ((src_tmp[1] & 0xc0) >> 6)) + 1;
                src2 = ((src_tmp[1] & 0x3f) << 2) | ((src_tmp[2] & 0x03));
                src3 = src_tmp[3];

                dst[o + 0] = 0xf0 | ((src1 & 0x1c) >> 2);
                dst[o + 1] = 0x80 | ((src1 & 0x03) << 4) | ((src2 & 0xf0) >> 4);
                dst[o + 2] = 0x80 |                            ((src2 & 0x0f) << 2) | ((src3 & 0xc0) >> 6);
                dst[o + 3] = 0x80 |                                                         (src3 & 0x3f);
                i += 2;
                o += 4;
            } else {
                /* U+0800 ... U+ffff (except range of surrogate pair) */
                /* 16BE[xxxxyyyy yyzzzzzz] -> 8[1110xxxx 10yyyyyy 10zzzzzz] */
                dst[o + 0] = 0xe0 | ((src_tmp[0] & 0xf0) >> 4);
                dst[o + 1] = 0x80 | ((src_tmp[0] & 0x0f) << 2) | ((src_tmp[1] & 0xc0) >> 6);
                dst[o + 2] = 0x80 | ((src_tmp[1] & 0x3f));
                i++;
                o += 3;
            }
        }
        dst.resize(o);

        return QString::fromUtf8(dst.constData(), dst.size());
    }

    void clearDictionaryParameters()
    {
        int index;

        /* Clear all dictionary set information structure and reset search state */
        for (index = 0; index < NJ_MAX_DIC; index++) {
            clearDictionaryStructure(&(work.dicSet.dic[index]));
        }
        work.flag = NJ_JNI_FLAG_NONE;

        /* Clear the cache information */
        memset(work.dicSet.keyword, 0, sizeof(work.dicSet.keyword));
    }

    int setDictionaryParameter(int index, int base, int high)
    {
        if ((index < 0  || index > NJ_MAX_DIC-1) ||
            (base <  -1 || base > 1000) ||
            (high <  -1 || high > 1000)) {
            /* If a invalid parameter was specified, return an error code */
            return NJ_SET_ERR_VAL(NJ_FUNC_SET_DICTIONARY_PARAMETERS, NJ_ERR_INVALID_PARAM);
        }

        /* Create the dictionary set information structure */
        if (base < 0 || high < 0 || base > high) {
            /* If -1 was specified to base or high, clear that dictionary information structure */
            /* If base is larger than high, clear that dictionary information structure */
            clearDictionaryStructure(&(work.dicSet.dic[index]));
        } else {
            /* Set the dictionary information structure */
            work.dicSet.dic[index].type      = work.dicType[index];
            work.dicSet.dic[index].handle    = work.dicHandle[index];
            work.dicSet.dic[index].srhCache  = &(work.srhCache[index]);

            work.dicSet.dic[index].dic_freq[NJ_MODE_TYPE_HENKAN].base = base;
            work.dicSet.dic[index].dic_freq[NJ_MODE_TYPE_HENKAN].high = high;
        }

        /* Reset search state because the dicionary information was changed */
        work.flag = NJ_JNI_FLAG_NONE;

        return 0;
    }

    int searchWord(OpenWnnDictionary::SearchOperation operation, OpenWnnDictionary::SearchOrder order, const QString &keyString)
    {
        if (!(operation == OpenWnnDictionary::SEARCH_EXACT ||
              operation == OpenWnnDictionary::SEARCH_PREFIX ||
              operation == OpenWnnDictionary::SEARCH_LINK) ||
            !(order == OpenWnnDictionary::ORDER_BY_FREQUENCY ||
              order == OpenWnnDictionary::ORDER_BY_KEY) ||
            keyString.isEmpty()) {
            /* If a invalid parameter was specified, return an error code */
            return NJ_SET_ERR_VAL(NJ_FUNC_SEARCH_WORD, NJ_ERR_INVALID_PARAM);
        }

        if (keyString.length() > NJ_MAX_LEN) {
            /* If too long key string was specified, return "No result is found" */
            work.flag &= ~NJ_JNI_FLAG_ENABLE_CURSOR;
            work.flag &= ~NJ_JNI_FLAG_ENABLE_RESULT;
            return 0;
        }

        convertStringToNjChar(work.keyString, keyString, NJ_MAX_LEN);

        /* Set the structure for search */
        memset(&(work.cursor), 0, sizeof(NJ_CURSOR));
        work.cursor.cond.operation = operation;
        work.cursor.cond.mode      = order;
        work.cursor.cond.ds        = &(work.dicSet);
        work.cursor.cond.yomi      = work.keyString;
        work.cursor.cond.charset   = &(work.approxSet);

        /* If the link search feature is specified, set the predict search information to structure */
        if (operation == OpenWnnDictionary::SEARCH_LINK) {
            work.cursor.cond.yomi  = work.previousStroke;
            work.cursor.cond.kanji = work.previousCandidate;
        }

        /* Search a specified word */
        memcpy(&(work.wnnClass.dic_set), &(work.dicSet), sizeof(NJ_DIC_SET));
        int result = njx_search_word(&(work.wnnClass), &(work.cursor));

        /* If a result is found, enable getNextWord method */
        if (result == 1) {
            work.flag |= NJ_JNI_FLAG_ENABLE_CURSOR;
        } else {
            work.flag &= ~NJ_JNI_FLAG_ENABLE_CURSOR;
        }
        work.flag &= ~NJ_JNI_FLAG_ENABLE_RESULT;

        return result;
    }

    int getNextWord(int length)
    {
        if (work.flag & NJ_JNI_FLAG_ENABLE_CURSOR) {
            int result;

            /* Get a specified word and search a next word */
            if (length <= 0) {
                result = njx_get_word(&(work.wnnClass), &(work.cursor), &(work.result));
            } else {
                do {
                    result = njx_get_word(&(work.wnnClass), &(work.cursor), &(work.result));
                    if (length == (NJ_GET_YLEN_FROM_STEM(&(work.result.word)) + NJ_GET_YLEN_FROM_FZK(&(work.result.word)))) {
                        break;
                    }
                } while(result > 0);
            }

            /* If a result is found, enable getStroke, getCandidate, getFrequency methods */
            if (result > 0) {
                work.flag |= NJ_JNI_FLAG_ENABLE_RESULT;
            } else {
                work.flag &= ~NJ_JNI_FLAG_ENABLE_RESULT;
            }
            return result;
        } else {
            /* When njx_search_word() was not yet called, return "No result is found" */
            return 0;
        }
    }

    QString getStroke()
    {
        if (work.flag & NJ_JNI_FLAG_ENABLE_RESULT) {
            NJ_CHAR stroke[NJ_MAX_LEN + NJ_TERM_LEN];

            if (njx_get_stroke(&(work.wnnClass), &(work.result), stroke, sizeof(NJ_CHAR) * (NJ_MAX_LEN + NJ_TERM_LEN)) >= 0) {
                return convertNjCharToString(stroke, NJ_MAX_LEN);
            }
        }
        return QString();
    }

    QString getCandidate()
    {
        if (work.flag & NJ_JNI_FLAG_ENABLE_RESULT) {
            NJ_CHAR candidate[NJ_MAX_LEN + NJ_TERM_LEN];

            if (njx_get_candidate(&(work.wnnClass), &(work.result), candidate, sizeof(NJ_CHAR) * (NJ_MAX_RESULT_LEN + NJ_TERM_LEN)) >= 0) {
                return convertNjCharToString(candidate, NJ_MAX_RESULT_LEN);
            }
        }
        return QString();
    }

    int getFrequency()
    {
        if (work.flag & NJ_JNI_FLAG_ENABLE_RESULT) {
            return work.result.word.stem.hindo;
        }
        return 0;
    }

    int getLeftPartOfSpeech()
    {
        return NJ_GET_FPOS_FROM_STEM(&(work.result.word));
    }

    int getRightPartOfSpeech()
    {
        return NJ_GET_BPOS_FROM_STEM(&(work.result.word));
    }

    void clearApproxPatterns()
    {
        /* Clear state */
        work.flag = NJ_JNI_FLAG_NONE;

        /* Clear approximate patterns */
        work.approxSet.charset_count = 0;
        for (int i = 0; i < NJ_MAX_CHARSET; i++) {
            work.approxSet.from[i] = NULL;
            work.approxSet.to[i]   = NULL;
        }

        /* Clear the cache information */
        memset(work.dicSet.keyword, 0, sizeof(work.dicSet.keyword));
    }

    int setApproxPattern(const QString &src, const QString &dst)
    {
        if (src.isEmpty() || src.length() > 1 ||
            dst.isEmpty() || dst.length() > 3) {
            /* If a invalid parameter was specified, return an error code */
            return NJ_SET_ERR_VAL(NJ_FUNC_SET_APPROX_PATTERN, NJ_ERR_INVALID_PARAM);
        }

        if (work.approxSet.charset_count < NJ_MAX_CHARSET) {
            NJ_CHAR*        from;
            NJ_CHAR*        to;

            /* Set pointers of string to store approximate informations */
            from = work.approxStr + NJ_APPROXSTORE_SIZE * work.approxSet.charset_count;
            to   = work.approxStr + NJ_APPROXSTORE_SIZE * work.approxSet.charset_count + NJ_MAX_CHARSET_FROM_LEN + NJ_TERM_LEN;
            work.approxSet.from[work.approxSet.charset_count] = from;
            work.approxSet.to[work.approxSet.charset_count]   = to;

            /* Convert approximate informations to internal format */
            convertStringToNjChar(from, src, NJ_MAX_CHARSET_FROM_LEN);
            convertStringToNjChar(to, dst, NJ_MAX_CHARSET_TO_LEN);
            work.approxSet.charset_count++;

            /* Reset search state because the seach condition was changed */
            work.flag = NJ_JNI_FLAG_NONE;

            return 0;
        }

        /* If the approx pattern registration area was full, return an error code */
        return NJ_SET_ERR_VAL(NJ_FUNC_SET_APPROX_PATTERN, NJ_ERR_APPROX_PATTERN_IS_FULL);
    }

    int setApproxPattern(OpenWnnDictionary::ApproxPattern approxPattern)
    {
        if (!(approxPattern == OpenWnnDictionary::APPROX_PATTERN_EN_TOUPPER ||
              approxPattern == OpenWnnDictionary::APPROX_PATTERN_EN_TOLOWER ||
              approxPattern == OpenWnnDictionary::APPROX_PATTERN_EN_QWERTY_NEAR ||
              approxPattern == OpenWnnDictionary::APPROX_PATTERN_EN_QWERTY_NEAR_UPPER ||
              approxPattern == OpenWnnDictionary::APPROX_PATTERN_JAJP_12KEY_NORMAL)) {
            /* If a invalid parameter was specified, return an error code */
            return NJ_SET_ERR_VAL(NJ_FUNC_SET_APPROX_PATTERN, NJ_ERR_INVALID_PARAM);
        }

        const PREDEF_APPROX_PATTERN *pattern = predefinedApproxPatterns[approxPattern];
        if (work.approxSet.charset_count + pattern->size <= NJ_MAX_CHARSET) {
            for (int i = 0; i < pattern->size; i++) {
                NJ_CHAR *from;
                NJ_CHAR *to;

                /* Set pointers of string to store approximate informations */
                from = work.approxStr + NJ_APPROXSTORE_SIZE * (work.approxSet.charset_count + i);
                to   = work.approxStr + NJ_APPROXSTORE_SIZE * (work.approxSet.charset_count + i) + NJ_MAX_CHARSET_FROM_LEN + NJ_TERM_LEN;
                work.approxSet.from[work.approxSet.charset_count + i] = from;
                work.approxSet.to[work.approxSet.charset_count + i]   = to;

                /* Set approximate pattern */
                from[0] = convertUTFCharToNjChar(pattern->from + i * 2);    /* "2" means the size of UTF-16BE */
                from[1] = 0x0000;

                to[0] = convertUTFCharToNjChar(pattern->to + i * 2);        /* "2" means the size of UTF-16BE */
                to[1] = 0x0000;
            }
            work.approxSet.charset_count += pattern->size;

            /* Reset search state because the seach condition was changed */
            work.flag = NJ_JNI_FLAG_NONE;

            return 0;
        }

        /* If the approx pattern registration area was full, return an error code */
        return NJ_SET_ERR_VAL(NJ_FUNC_SET_APPROX_PATTERN, NJ_ERR_APPROX_PATTERN_IS_FULL);
    }

    QStringList getApproxPattern(const QString &src)
    {
        if (src.isEmpty() || src.length() > 1)
            return QStringList();

        NJ_CHAR from[NJ_MAX_CHARSET_FROM_LEN + NJ_TERM_LEN];
        convertStringToNjChar(from, src, NJ_MAX_CHARSET_FROM_LEN);

        QStringList result;
        for (int i = 0; i < work.approxSet.charset_count; i++) {
            if (nj_strcmp(from, work.approxSet.from[i]) == 0) {
                result.append(convertNjCharToString(work.approxSet.to[i], NJ_MAX_CHARSET_TO_LEN));
            }
        }

        return result;
    }

    void clearResult()
    {
        /* Clear the current word information */
        memset(&(work.result), 0, sizeof(NJ_RESULT));
        memset(&(work.previousStroke), 0, sizeof(work.previousStroke));
        memset(&(work.previousCandidate), 0, sizeof(work.previousCandidate));
    }

    int setLeftPartOfSpeech(int leftPartOfSpeech)
    {
        NJ_UINT16 lcount = 0, rcount = 0;

        if (work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN] == NULL) {
            /* No rule dictionary was set */
            return NJ_SET_ERR_VAL(NJ_FUNC_SET_LEFT_PART_OF_SPEECH, NJ_ERR_NO_RULEDIC);
        }

        njd_r_get_count(work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN], &lcount, &rcount);

        if (leftPartOfSpeech < 1 || leftPartOfSpeech > lcount) {
            /* If a invalid parameter was specified, return an error code */
            return NJ_SET_ERR_VAL(NJ_FUNC_SET_LEFT_PART_OF_SPEECH, NJ_ERR_INVALID_PARAM);
        }

        NJ_SET_FPOS_TO_STEM(&(work.result.word), leftPartOfSpeech);
        return 0;
    }

    int setRightPartOfSpeech(int rightPartOfSpeech)
    {
        NJ_UINT16 lcount = 0, rcount = 0;

        if (work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN] == NULL) {
            /* No rule dictionary was set */
            return NJ_SET_ERR_VAL(NJ_FUNC_SET_RIGHT_PART_OF_SPEECH, NJ_ERR_NO_RULEDIC);
        }

        njd_r_get_count(work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN], &lcount, &rcount);

        if (rightPartOfSpeech < 1 || rightPartOfSpeech > rcount) {
            /* If a invalid parameter was specified, return an error code */
            return NJ_SET_ERR_VAL(NJ_FUNC_SET_RIGHT_PART_OF_SPEECH, NJ_ERR_INVALID_PARAM);
        }

        NJ_SET_BPOS_TO_STEM(&(work.result.word), rightPartOfSpeech);
        return 0;
    }

    int setStroke(const QString &stroke)
    {
        if (stroke.isEmpty()) {
            /* If a invalid parameter was specified, return an error code */
            return NJ_SET_ERR_VAL(NJ_FUNC_SET_STROKE, NJ_ERR_INVALID_PARAM);
        }

        if (stroke.length() > NJ_MAX_LEN) {
            /* If a invalid parameter was specified, return an error code */
            return NJ_SET_ERR_VAL(NJ_FUNC_SET_STROKE, NJ_ERR_YOMI_TOO_LONG);
        }

        /* Store stroke string */
        convertStringToNjChar(work.previousStroke, stroke, NJ_MAX_LEN);

        return 0;
    }

    int setCandidate(const QString &candidate)
    {
        if (candidate.isEmpty()) {
            /* If a invalid parameter was specified, return an error code */
            return NJ_SET_ERR_VAL(NJ_FUNC_SET_CANDIDATE, NJ_ERR_INVALID_PARAM);
        }

        if (candidate.length() > NJ_MAX_RESULT_LEN) {
            /* If a invalid parameter was specified, return an error code */
            return NJ_SET_ERR_VAL(NJ_FUNC_SET_CANDIDATE, NJ_ERR_CANDIDATE_TOO_LONG);
        }

        /* Store candidate string */
        convertStringToNjChar(work.previousCandidate, candidate, NJ_MAX_RESULT_LEN);

        return 0;
    }

    int selectWord()
    {
        /* Put the previous word information to engine */
        memcpy(&(work.wnnClass.dic_set), &(work.dicSet), sizeof(NJ_DIC_SET));
        return njx_select(&(work.wnnClass), &(work.result));
    }

    QBitArray getConnectArray(int leftPartOfSpeech)
    {
        NJ_UINT16 lcount = 0, rcount = 0;

        if (work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN] == NULL) {
            /* No rule dictionary was set */
            return QBitArray();
        }

        njd_r_get_count(work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN], &lcount, &rcount);

        if (leftPartOfSpeech < 0 || leftPartOfSpeech > lcount) {
            /* Invalid POS is specified */
            return QBitArray();
        }

        /* 1-origin */
        QBitArray result(rcount + 1);
        int         i;
        NJ_UINT8*   connect;

        if (leftPartOfSpeech > 0) {
            /* Get the packed connect array */
            njd_r_get_connect(work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN], leftPartOfSpeech, NJ_RULE_TYPE_FTOB, &connect);

            /* Extract connect array from bit field */
            for (i = 0; i < rcount; i++) {
                if (connect[i / 8] & (0x80 >> (i % 8))) {
                    result.setBit(i + 1);
                }
            }
        }
        return result;
    }

    int getNumberOfLeftPOS()
    {
        if (work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN] == NULL)
            /* No rule dictionary was set */
            return 0;

        NJ_UINT16 lcount = 0, rcount = 0;

        njd_r_get_count(work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN], &lcount, &rcount);
        return lcount;
    }

    int getNumberOfRightPOS()
    {
        if (work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN] == NULL)
            /* No rule dictionary was set */
            return 0;

        NJ_UINT16 lcount = 0, rcount = 0;

        njd_r_get_count(work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN], &lcount, &rcount);
        return rcount;
    }

    int getLeftPartOfSpeechSpecifiedType(OpenWnnDictionary::PartOfSpeechType posType)
    {
        NJ_UINT8 type;
        switch(posType) {
        case OpenWnnDictionary::POS_TYPE_V1:
            type = NJ_HINSI_V1_F;
            break;
        case OpenWnnDictionary::POS_TYPE_V2:
            type = NJ_HINSI_V2_F;
            break;
        case OpenWnnDictionary::POS_TYPE_V3:
            type = NJ_HINSI_V3_F;
            break;
        case OpenWnnDictionary::POS_TYPE_BUNTOU:
            /* No part of speech is defined at this type */
            return 0;
        case OpenWnnDictionary::POS_TYPE_TANKANJI:
            type = NJ_HINSI_TANKANJI_F;
            break;
        case OpenWnnDictionary::POS_TYPE_SUUJI:
            /* No part of speech is defined at this type */
            return 0;
        case OpenWnnDictionary::POS_TYPE_MEISI:
            type = NJ_HINSI_MEISI_F;
            break;
        case OpenWnnDictionary::POS_TYPE_JINMEI:
            type = NJ_HINSI_JINMEI_F;
            break;
        case OpenWnnDictionary::POS_TYPE_CHIMEI:
            type = NJ_HINSI_CHIMEI_F;
            break;
        case OpenWnnDictionary::POS_TYPE_KIGOU:
            type = NJ_HINSI_KIGOU_F;
            break;
        default:
            /* If a invalid parameter was specified, return an error code */
            return NJ_SET_ERR_VAL(NJ_FUNC_GET_LEFT_PART_OF_SPEECH_SPECIFIED_TYPE, NJ_ERR_INVALID_PARAM);
        }
        return njd_r_get_hinsi(work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN], type);
    }

    int getRightPartOfSpeechSpecifiedType(OpenWnnDictionary::PartOfSpeechType posType)
    {
        NJ_UINT8 type;
        switch(posType) {
        case OpenWnnDictionary::POS_TYPE_V1:
            /* No part of speech is defined at this type */
            return 0;
        case OpenWnnDictionary::POS_TYPE_V2:
            /* No part of speech is defined at this type */
            return 0;
        case OpenWnnDictionary::POS_TYPE_V3:
            /* No part of speech is defined at this type */
            return 0;
        case OpenWnnDictionary::POS_TYPE_BUNTOU:
            type = NJ_HINSI_BUNTOU_B;
            break;
        case OpenWnnDictionary::POS_TYPE_TANKANJI:
            type = NJ_HINSI_TANKANJI_B;
            break;
        case OpenWnnDictionary::POS_TYPE_SUUJI:
            type = NJ_HINSI_SUUJI_B;
            break;
        case OpenWnnDictionary::POS_TYPE_MEISI:
            type = NJ_HINSI_MEISI_B;
            break;
        case OpenWnnDictionary::POS_TYPE_JINMEI:
            type = NJ_HINSI_JINMEI_B;
            break;
        case OpenWnnDictionary::POS_TYPE_CHIMEI:
            type = NJ_HINSI_CHIMEI_B;
            break;
        case OpenWnnDictionary::POS_TYPE_KIGOU:
            type = NJ_HINSI_KIGOU_B;
            break;
        default:
            /* If a invalid parameter was specified, return an error code */
            return NJ_SET_ERR_VAL(NJ_FUNC_GET_RIGHT_PART_OF_SPEECH_SPECIFIED_TYPE, NJ_ERR_INVALID_PARAM);
        }
        return njd_r_get_hinsi(work.dicSet.rHandle[NJ_MODE_TYPE_HENKAN], type);
    }

    NJ_WORK work;
};

OpenWnnDictionary::OpenWnnDictionary(QObject *parent) :
    QObject(*new OpenWnnDictionaryPrivate(), parent)
{
}

OpenWnnDictionary::~OpenWnnDictionary()
{
}

void OpenWnnDictionary::setInUseState(bool flag)
{
    Q_UNUSED(flag)
    // Not implemented
}

void OpenWnnDictionary::clearDictionary()
{
    Q_D(OpenWnnDictionary);
    d->clearDictionaryParameters();
}

int OpenWnnDictionary::setDictionary(int index, int base, int high)
{
    Q_D(OpenWnnDictionary);
    switch (index) {
    case OpenWnnDictionary::INDEX_USER_DICTIONARY:
        // Not implemented
        return 0;
    case OpenWnnDictionary::INDEX_LEARN_DICTIONARY:
        // Not implemented
        return 0;
    default:
        return d->setDictionaryParameter(index, base, high);
    }
}

void OpenWnnDictionary::clearApproxPattern()
{
    Q_D(OpenWnnDictionary);
    return d->clearApproxPatterns();
}

int OpenWnnDictionary::setApproxPattern(const QString &src, const QString &dst)
{
    Q_D(OpenWnnDictionary);
    return d->setApproxPattern(src, dst);
}

int OpenWnnDictionary::setApproxPattern(ApproxPattern approxPattern)
{
    Q_D(OpenWnnDictionary);
    return d->setApproxPattern(approxPattern);
}

int OpenWnnDictionary::searchWord(SearchOperation operation, SearchOrder order, const QString &keyString)
{
    Q_D(OpenWnnDictionary);
    /* Unset the previous word information */
    d->clearResult();
    /* Search to fixed dictionary */
    return d->searchWord(operation, order, keyString);
}

int OpenWnnDictionary::searchWord(SearchOperation operation, SearchOrder order, const QString &keyString, const WnnWord &wnnWord)
{
    Q_D(OpenWnnDictionary);

    /* Search to fixed dictionary with link information */
    d->clearResult();
    d->setStroke(wnnWord.stroke);
    d->setCandidate(wnnWord.candidate);
    d->setLeftPartOfSpeech(wnnWord.partOfSpeech.left);
    d->setRightPartOfSpeech(wnnWord.partOfSpeech.right);
    d->selectWord();

    return d->searchWord(operation, order, keyString);
}

QSharedPointer<WnnWord> OpenWnnDictionary::getNextWord(int length)
{
    Q_D(OpenWnnDictionary);

    /* Get the result from fixed dictionary */
    int res = d->getNextWord(length);
    if (res > 0)
        return QSharedPointer<WnnWord>(new WnnWord(d->getCandidate(), d->getStroke(), WnnPOS(d->getLeftPartOfSpeech(), d->getRightPartOfSpeech()), d->getFrequency()));
    return QSharedPointer<WnnWord>();
}

QList<QBitArray> OpenWnnDictionary::getConnectMatrix()
{
    Q_D(OpenWnnDictionary);
    QList<QBitArray> result;

    /* 1-origin */
    int lcount = d->getNumberOfLeftPOS();
    result.reserve(lcount + 1);

    for (int i = 0; i < lcount + 1; i++) {
        result.append(d->getConnectArray(i));
    }
    return result;
}

WnnPOS OpenWnnDictionary::getPOS(PartOfSpeechType type)
{
    Q_D(OpenWnnDictionary);
    return WnnPOS(d->getLeftPartOfSpeechSpecifiedType(type), d->getRightPartOfSpeechSpecifiedType(type));
}

int OpenWnnDictionary::learnWord(const WnnWord &word, const WnnWord *previousWord)
{
    Q_UNUSED(word)
    Q_UNUSED(previousWord)
    // Not implemented
    return -1;
}
