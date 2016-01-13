// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "encodings/compact_lang_det/ext_lang_enc.h"

// score_me text [  8] ja not found in tables
// score_me text [  9] ko not found in tables
// score_me text [ 16] zh not found in tables
// score_me text [ 18] el not found in tables
// score_me text [ 26] un not found in tables
// score_me text [ 39] la not found in tables
// score_me text [ 41] ml not found in tables
// score_me text [ 43] ne not found in tables
// score_me text [ 44] te not found in tables
// score_me text [ 46] ta not found in tables
// score_me text [ 48] jw not found in tables
// score_me text [ 49] oc not found in tables
// score_me text [ 51] bh not found in tables
// score_me text [ 52] gu not found in tables
// score_me text [ 53] th not found in tables
// score_me text [ 56] eo not found in tables
// score_me text [ 58] ia not found in tables
// score_me text [ 59] kn not found in tables
// score_me text [ 60] pa not found in tables
// score_me text [ 61] gd not found in tables
// score_me text [ 64] mr not found in tables
// score_me text [ 67] fy not found in tables
// score_me text [ 69] zhT not found in tables
// score_me text [ 70] fo not found in tables
// score_me text [ 71] su not found in tables
// score_me text [ 72] uz not found in tables
// score_me text [ 73] am not found in tables
// score_me text [ 75] ka not found in tables
// score_me text [ 76] ti not found in tables
// score_me text [ 78] bs not found in tables
// score_me text [ 79] si not found in tables
// score_me text [ 80] nn not found in tables
// score_me text [ 83] xh not found in tables
// score_me text [ 84] zu not found in tables
// score_me text [ 85] gn not found in tables
// score_me text [ 86] st not found in tables
// score_me text [ 87] tk not found in tables
// score_me text [ 88] ky not found in tables
// score_me text [ 89] br not found in tables
// score_me text [ 90] tw not found in tables
// score_me text [ 93] so not found in tables
// score_me text [ 94] ug not found in tables
// score_me text [ 95] ku not found in tables
// score_me text [ 96] mn not found in tables
// score_me text [ 97] hy not found in tables
// score_me text [ 98] lo not found in tables
// score_me text [ 99] sd not found in tables
// score_me text [100] rm not found in tables
// score_me text [102] lb not found in tables
// score_me text [103] my not found in tables
// score_me text [104] km not found in tables
// score_me text [105] bo not found in tables
// score_me text [107] chr not found in tables
// score_me text [109] sit-NP not found in tables
// score_me text [110] or not found in tables
// score_me text [111] as not found in tables
// score_me text [112] co not found in tables
// score_me text [113] ie not found in tables
// score_me text [114] kk not found in tables
// score_me text [115] ln not found in tables
// score_me text [116] mo not found in tables
// score_me text [117] ps not found in tables
// score_me text [118] qu not found in tables
// score_me text [119] sn not found in tables
// score_me text [120] tg not found in tables
// score_me text [121] tt not found in tables
// score_me text [122] to not found in tables
// score_me text [123] yo not found in tables
// score_me text [128] mi not found in tables
// score_me text [129] wo not found in tables
// score_me text [130] ab not found in tables
// score_me text [131] aa not found in tables
// score_me text [132] ay not found in tables
// score_me text [133] ba not found in tables
// score_me text [134] bi not found in tables
// score_me text [135] dz not found in tables
// score_me text [136] fj not found in tables
// score_me text [137] kl not found in tables
// score_me text [138] ha not found in tables
// score_me text [140] ik not found in tables
// score_me text [141] iu not found in tables
// score_me text [142] ks not found in tables
// score_me text [143] rw not found in tables
// score_me text [144] mg not found in tables
// score_me text [145] na not found in tables
// score_me text [146] om not found in tables
// score_me text [147] rn not found in tables
// score_me text [148] sm not found in tables
// score_me text [149] sg not found in tables
// score_me text [150] sa not found in tables
// score_me text [151] ss not found in tables
// score_me text [152] ts not found in tables
// score_me text [153] tn not found in tables
// score_me text [154] vo not found in tables
// score_me text [155] za not found in tables
// score_me text [156] kha not found in tables
// score_me text [157] sco not found in tables
// score_me text [158] lg not found in tables
// score_me text [159] gv not found in tables
// score_me text [160] srM not found in tables
// score_me text [161] ak not found in tables
// score_me text [162] ig not found in tables
// score_me text [163] mfe not found in tables
// score_me text [164] haw not found in tables
// score_me text [165] nso not found in tables
// score_me text [166] nr not found in tables
// score_me text [167] ve not found in tables
// score_me text [168] ny not found in tables
// score_me text [169] crs not found in tables
// score_me text [185] zzb not found in tables
// score_me text [186] zzp not found in tables
// score_me text [187] zzh not found in tables
// score_me text [188] tlh not found in tables
// score_me text [189] zze not found in tables

// No score_me text for [ 25] xxx

// Average score per 1024 bytes
// Renamed kAvgQuadScore -> kMeanScore to match older CLD code.
extern const short kMeanScore[244 * 4] = {
// Latn  Cyrl  Arab  Other script
  1234,    0,    0,    0,   // [  0] ENGLISH en
   721,    0,    0,    0,   // [  1] DANISH da
  1037,    0,    0,    0,   // [  2] DUTCH nl
  1337,    0,    0,    0,   // [  3] FINNISH fi
   973,    0,    0,    0,   // [  4] FRENCH fr
  1211,    0,    0,    0,   // [  5] GERMAN de
     0,    0,    0,  727,   // [  6] HEBREW iw
   866,    0,    0,    0,   // [  7] ITALIAN it
     0,    0,    0,    0,   // [  8] Japanese ja  ==Zero==
     0,    0,    0,    0,   // [  9] Korean ko  ==Zero==
   833,    0,    0,    0,   // [ 10] NORWEGIAN no
  1280,    0,    0,    0,   // [ 11] POLISH pl
   718,    0,    0,    0,   // [ 12] PORTUGUESE pt
     0,  767,    0,    0,   // [ 13] RUSSIAN ru
   642,    0,    0,    0,   // [ 14] SPANISH es
   826,    0,    0,    0,   // [ 15] SWEDISH sv
     0,    0,    0,    0,   // [ 16] Chinese zh  ==Zero==
   982,    0,    0,    0,   // [ 17] CZECH cs
     0,    0,    0,    0,   // [ 18] GREEK el  ==Zero==
  1214,    0,    0,    0,   // [ 19] ICELANDIC is
  1194,    0,    0,    0,   // [ 20] LATVIAN lv
  1220,    0,    0,    0,   // [ 21] LITHUANIAN lt
   951,    0,    0,    0,   // [ 22] ROMANIAN ro
  1335,    0,    0,    0,   // [ 23] HUNGARIAN hu
  1145,    0,    0,    0,   // [ 24] ESTONIAN et
     0,    0,    0,    0,   // [ 25] Ignore xxx  ==Zero==
     0,    0,    0,    0,   // [ 26] Unknown un  ==Zero==
     0,  691,    0,    0,   // [ 27] BULGARIAN bg
   774,    0,    0,    0,   // [ 28] CROATIAN hr
  1048,  686,    0,    0,   // [ 29] SERBIAN sr
  1503,    0,    0,    0,   // [ 30] IRISH ga
   464,    0,    0,    0,   // [ 31] GALICIAN gl

  1570,    0,    0,    0,   // [ 32] TAGALOG tl
  1390,    0,    0,    0,   // [ 33] TURKISH tr
     0,  677,    0,    0,   // [ 34] UKRAINIAN uk
     0,    0,    0,  601,   // [ 35] HINDI hi
     0,  695,    0,    0,   // [ 36] MACEDONIAN mk
     0,    0,    0,  563,   // [ 37] BENGALI bn
  1307,    0,    0,    0,   // [ 38] INDONESIAN id
     0,    0,    0,    0,   // [ 39] LATIN la  ==Zero==
  1402,    0,    0,    0,   // [ 40] MALAY ms
     0,    0,    0,    0,   // [ 41] MALAYALAM ml  ==Zero==
  1536,    0,    0,    0,   // [ 42] WELSH cy
     0,    0,    0,    0,   // [ 43] NEPALI ne  ==Zero==
     0,    0,    0,    0,   // [ 44] TELUGU te  ==Zero==
  1146,    0,    0,    0,   // [ 45] ALBANIAN sq
     0,    0,    0,    0,   // [ 46] TAMIL ta  ==Zero==
     0,  834,    0,    0,   // [ 47] BELARUSIAN be
     0,    0,    0,    0,   // [ 48] JAVANESE jw  ==Zero==
     0,    0,    0,    0,   // [ 49] OCCITAN oc  ==Zero==
     0,    0,  964,    0,   // [ 50] URDU ur
     0,    0,    0,    0,   // [ 51] BIHARI bh  ==Zero==
     0,    0,    0,    0,   // [ 52] GUJARATI gu  ==Zero==
     0,    0,    0,    0,   // [ 53] THAI th  ==Zero==
     0,    0,  735,    0,   // [ 54] ARABIC ar
   667,    0,    0,    0,   // [ 55] CATALAN ca
     0,    0,    0,    0,   // [ 56] ESPERANTO eo  ==Zero==
  1442,    0,    0,    0,   // [ 57] BASQUE eu
     0,    0,    0,    0,   // [ 58] INTERLINGUA ia  ==Zero==
     0,    0,    0,    0,   // [ 59] KANNADA kn  ==Zero==
     0,    0,    0,    0,   // [ 60] PUNJABI pa  ==Zero==
     0,    0,    0,    0,   // [ 61] SCOTS_GAELIC gd  ==Zero==
  1448,    0,    0,    0,   // [ 62] SWAHILI sw
   907,    0,    0,    0,   // [ 63] SLOVENIAN sl

     0,    0,    0,    0,   // [ 64] MARATHI mr  ==Zero==
  1027,    0,    0,    0,   // [ 65] MALTESE mt
  1097,    0,    0,    0,   // [ 66] VIETNAMESE vi
     0,    0,    0,    0,   // [ 67] FRISIAN fy  ==Zero==
  1007,    0,    0,    0,   // [ 68] SLOVAK sk
     0,    0,    0,    0,   // [ 69] ChineseT zhT  ==Zero==
     0,    0,    0,    0,   // [ 70] FAROESE fo  ==Zero==
     0,    0,    0,    0,   // [ 71] SUNDANESE su  ==Zero==
     0,    0,    0,    0,   // [ 72] UZBEK uz  ==Zero==
     0,    0,    0,    0,   // [ 73] AMHARIC am  ==Zero==
  1371,    0,    0,    0,   // [ 74] AZERBAIJANI az
     0,    0,    0,    0,   // [ 75] GEORGIAN ka  ==Zero==
     0,    0,    0,    0,   // [ 76] TIGRINYA ti  ==Zero==
     0,    0,  855,    0,   // [ 77] PERSIAN fa
     0,    0,    0,    0,   // [ 78] BOSNIAN bs  ==Zero==
     0,    0,    0,    0,   // [ 79] SINHALESE si  ==Zero==
     0,    0,    0,    0,   // [ 80] NORWEGIAN_N nn  ==Zero==
     0,    0,    0,    0,   // [ 81] PORTUGUESE_P pt-PT  ==Zero==
     0,    0,    0,    0,   // [ 82] PORTUGUESE_B pt-BR  ==Zero==
     0,    0,    0,    0,   // [ 83] XHOSA xh  ==Zero==
     0,    0,    0,    0,   // [ 84] ZULU zu  ==Zero==
     0,    0,    0,    0,   // [ 85] GUARANI gn  ==Zero==
     0,    0,    0,    0,   // [ 86] SESOTHO st  ==Zero==
     0,    0,    0,    0,   // [ 87] TURKMEN tk  ==Zero==
     0,    0,    0,    0,   // [ 88] KYRGYZ ky  ==Zero==
     0,    0,    0,    0,   // [ 89] BRETON br  ==Zero==
     0,    0,    0,    0,   // [ 90] TWI tw  ==Zero==
     0,    0,    0,  807,   // [ 91] YIDDISH yi
     0,    0,    0,    0,   // [ 92] SERBO_CROATIAN sh  ==Zero==
     0,    0,    0,    0,   // [ 93] SOMALI so  ==Zero==
     0,    0,    0,    0,   // [ 94] UIGHUR ug  ==Zero==
     0,    0,    0,    0,   // [ 95] KURDISH ku  ==Zero==

     0,    0,    0,    0,   // [ 96] MONGOLIAN mn  ==Zero==
     0,    0,    0,    0,   // [ 97] ARMENIAN hy  ==Zero==
     0,    0,    0,    0,   // [ 98] LAOTHIAN lo  ==Zero==
     0,    0,    0,    0,   // [ 99] SINDHI sd  ==Zero==
     0,    0,    0,    0,   // [100] RHAETO_ROMANCE rm  ==Zero==
   885,    0,    0,    0,   // [101] AFRIKAANS af
     0,    0,    0,    0,   // [102] LUXEMBOURGISH lb  ==Zero==
     0,    0,    0,    0,   // [103] BURMESE my  ==Zero==
     0,    0,    0,    0,   // [104] KHMER km  ==Zero==
     0,    0,    0,    0,   // [105] TIBETAN bo  ==Zero==
     0,    0,    0,    0,   // [106] DHIVEHI dv  ==Zero==
     0,    0,    0,    0,   // [107] CHEROKEE chr  ==Zero==
     0,    0,    0,    0,   // [108] SYRIAC syr  ==Zero==
     0,    0,    0,    0,   // [109] LIMBU sit-NP  ==Zero==
     0,    0,    0,    0,   // [110] ORIYA or  ==Zero==
     0,    0,    0,    0,   // [111] ASSAMESE as  ==Zero==
     0,    0,    0,    0,   // [112] CORSICAN co  ==Zero==
     0,    0,    0,    0,   // [113] INTERLINGUE ie  ==Zero==
     0,    0,    0,    0,   // [114] KAZAKH kk  ==Zero==
     0,    0,    0,    0,   // [115] LINGALA ln  ==Zero==
     0,    0,    0,    0,   // [116] MOLDAVIAN mo  ==Zero==
     0,    0,    0,    0,   // [117] PASHTO ps  ==Zero==
     0,    0,    0,    0,   // [118] QUECHUA qu  ==Zero==
     0,    0,    0,    0,   // [119] SHONA sn  ==Zero==
     0,    0,    0,    0,   // [120] TAJIK tg  ==Zero==
     0,    0,    0,    0,   // [121] TATAR tt  ==Zero==
     0,    0,    0,    0,   // [122] TONGA to  ==Zero==
     0,    0,    0,    0,   // [123] YORUBA yo  ==Zero==
     0,    0,    0,    0,   // [124] CREOLES_AND_PIDGINS_ENGLISH_BASED cpe  ==Zero==
     0,    0,    0,    0,   // [125] CREOLES_AND_PIDGINS_FRENCH_BASED cpf  ==Zero==
     0,    0,    0,    0,   // [126] CREOLES_AND_PIDGINS_PORTUGUESE_BASED cpp  ==Zero==
     0,    0,    0,    0,   // [127] CREOLES_AND_PIDGINS_OTHER crp  ==Zero==

     0,    0,    0,    0,   // [128] MAORI mi  ==Zero==
     0,    0,    0,    0,   // [129] WOLOF wo  ==Zero==
     0,    0,    0,    0,   // [130] ABKHAZIAN ab  ==Zero==
     0,    0,    0,    0,   // [131] AFAR aa  ==Zero==
     0,    0,    0,    0,   // [132] AYMARA ay  ==Zero==
     0,    0,    0,    0,   // [133] BASHKIR ba  ==Zero==
     0,    0,    0,    0,   // [134] BISLAMA bi  ==Zero==
     0,    0,    0,    0,   // [135] DZONGKHA dz  ==Zero==
     0,    0,    0,    0,   // [136] FIJIAN fj  ==Zero==
     0,    0,    0,    0,   // [137] GREENLANDIC kl  ==Zero==
     0,    0,    0,    0,   // [138] HAUSA ha  ==Zero==
  1151,    0,    0,    0,   // [139] HAITIAN_CREOLE ht
     0,    0,    0,    0,   // [140] INUPIAK ik  ==Zero==
     0,    0,    0,    0,   // [141] INUKTITUT iu  ==Zero==
     0,    0,    0,    0,   // [142] KASHMIRI ks  ==Zero==
     0,    0,    0,    0,   // [143] KINYARWANDA rw  ==Zero==
     0,    0,    0,    0,   // [144] MALAGASY mg  ==Zero==
     0,    0,    0,    0,   // [145] NAURU na  ==Zero==
     0,    0,    0,    0,   // [146] OROMO om  ==Zero==
     0,    0,    0,    0,   // [147] RUNDI rn  ==Zero==
     0,    0,    0,    0,   // [148] SAMOAN sm  ==Zero==
     0,    0,    0,    0,   // [149] SANGO sg  ==Zero==
     0,    0,    0,    0,   // [150] SANSKRIT sa  ==Zero==
     0,    0,    0,    0,   // [151] SISWANT ss  ==Zero==
     0,    0,    0,    0,   // [152] TSONGA ts  ==Zero==
     0,    0,    0,    0,   // [153] TSWANA tn  ==Zero==
     0,    0,    0,    0,   // [154] VOLAPUK vo  ==Zero==
     0,    0,    0,    0,   // [155] ZHUANG za  ==Zero==
     0,    0,    0,    0,   // [156] KHASI kha  ==Zero==
     0,    0,    0,    0,   // [157] SCOTS sco  ==Zero==
     0,    0,    0,    0,   // [158] GANDA lg  ==Zero==
     0,    0,    0,    0,   // [159] MANX gv  ==Zero==

     0,    0,    0,    0,   // [160] MONTENEGRIN srM  ==Zero==
     0,    0,    0,    0,   // [161] AKAN ak  ==Zero==
     0,    0,    0,    0,   // [162] IGBO ig  ==Zero==
     0,    0,    0,    0,   // [163] MAURITIAN_CREOLE mfe  ==Zero==
     0,    0,    0,    0,   // [164] HAWAIIAN haw  ==Zero==
     0,    0,    0,    0,   // [165] PEDI nso  ==Zero==
     0,    0,    0,    0,   // [166] NDEBELE nr  ==Zero==
     0,    0,    0,    0,   // [167] VENDA ve  ==Zero==
     0,    0,    0,    0,   // [168] CHICHEWA ny  ==Zero==
     0,    0,    0,    0,   // [169] SESELWA crs  ==Zero==
     0,    0,    0,    0,   // [170] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [171] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [172] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [173] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [174] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [175] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [176] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [177] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [178] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [179] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [180] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [181] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [182] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [183] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [184] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [185] X_BORK_BORK_BORK zzb  ==Zero==
     0,    0,    0,    0,   // [186] X_PIG_LATIN zzp  ==Zero==
     0,    0,    0,    0,   // [187] X_HACKER zzh  ==Zero==
     0,    0,    0,    0,   // [188] X_KLINGON tlh  ==Zero==
     0,    0,    0,    0,   // [189] X_ELMER_FUDD zze  ==Zero==
     0,    0,    0,    0,   // [190] X_OGHAM xx-Ogam  ==Zero==
     0,    0,    0,    0,   // [191] X_RUNIC xx-Runr  ==Zero==

     0,    0,    0,    0,   // [192] X_YI xx-Yiii  ==Zero==
     0,    0,    0,    0,   // [193] X_OLD_ITALIC xx-Ital  ==Zero==
     0,    0,    0,    0,   // [194] X_GOTHIC xx-Goth  ==Zero==
     0,    0,    0,    0,   // [195] X_DESERET xx-Dsrt  ==Zero==
     0,    0,    0,    0,   // [196] X_HANUNOO xx-Hano  ==Zero==
     0,    0,    0,    0,   // [197] X_BUHID xx-Buhd  ==Zero==
     0,    0,    0,    0,   // [198] X_TAGBANWA xx-Tagb  ==Zero==
     0,    0,    0,    0,   // [199] X_TAI_LE xx-Tale  ==Zero==
     0,    0,    0,    0,   // [200] X_LINEAR_B xx-Linb  ==Zero==
     0,    0,    0,    0,   // [201] X_UGARITIC xx-Ugar  ==Zero==
     0,    0,    0,    0,   // [202] X_SHAVIAN xx-Shaw  ==Zero==
     0,    0,    0,    0,   // [203] X_OSMANYA xx-Osma  ==Zero==
     0,    0,    0,    0,   // [204] X_CYPRIOT xx-Cprt  ==Zero==
     0,    0,    0,    0,   // [205] X_BUGINESE xx-Bugi  ==Zero==
     0,    0,    0,    0,   // [206] X_COPTIC xx-Copt  ==Zero==
     0,    0,    0,    0,   // [207] X_NEW_TAI_LUE xx-Talu  ==Zero==
     0,    0,    0,    0,   // [208] X_GLAGOLITIC xx-Glag  ==Zero==
     0,    0,    0,    0,   // [209] X_TIFINAGH xx-Tfng  ==Zero==
     0,    0,    0,    0,   // [210] X_SYLOTI_NAGRI xx-Sylo  ==Zero==
     0,    0,    0,    0,   // [211] X_OLD_PERSIAN xx-Xpeo  ==Zero==
     0,    0,    0,    0,   // [212] X_KHAROSHTHI xx-Khar  ==Zero==
     0,    0,    0,    0,   // [213] X_BALINESE xx-Bali  ==Zero==
     0,    0,    0,    0,   // [214] X_CUNEIFORM xx-Xsux  ==Zero==
     0,    0,    0,    0,   // [215] X_PHOENICIAN xx-Phnx  ==Zero==
     0,    0,    0,    0,   // [216] X_PHAGS_PA xx-Phag  ==Zero==
     0,    0,    0,    0,   // [217] X_NKO xx-Nkoo  ==Zero==
     0,    0,    0,    0,   // [218] X_SUDANESE xx-Sund  ==Zero==
     0,    0,    0,    0,   // [219] X_LEPCHA xx-Lepc  ==Zero==
     0,    0,    0,    0,   // [220] X_OL_CHIKI xx-Olck  ==Zero==
     0,    0,    0,    0,   // [221] X_VAI xx-Vaii  ==Zero==
     0,    0,    0,    0,   // [222] X_SAURASHTRA xx-Saur  ==Zero==
     0,    0,    0,    0,   // [223] X_KAYAH_LI xx-Kali  ==Zero==

     0,    0,    0,    0,   // [224] X_REJANG xx-Rjng  ==Zero==
     0,    0,    0,    0,   // [225] X_LYCIAN xx-Lyci  ==Zero==
     0,    0,    0,    0,   // [226] X_CARIAN xx-Cari  ==Zero==
     0,    0,    0,    0,   // [227] X_LYDIAN xx-Lydi  ==Zero==
     0,    0,    0,    0,   // [228] X_CHAM xx-Cham  ==Zero==
     0,    0,    0,    0,   // [229] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [230] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [231] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [232] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [233] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [234] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [235] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [236] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [237] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [238] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [239] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [240] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [241] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [242] invalid_language ??  ==Zero==
     0,    0,    0,    0,   // [243] invalid_language ??  ==Zero==
  };

COMPILE_ASSERT(EXT_NUM_LANGUAGES >= 209, k_ext_num_languages_changed);
