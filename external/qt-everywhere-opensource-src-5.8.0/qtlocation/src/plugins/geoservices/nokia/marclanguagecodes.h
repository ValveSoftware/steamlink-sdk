/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MARCLANGUAGECODES_H_
#define MARCLANGUAGECODES_H_

// MARC language codes for GeoCoding service language/locale support
// http://www.loc.gov/marc/languages/language_code.html
// Order matches QLocale::Language

QT_BEGIN_NAMESPACE

static const unsigned char marc_language_code_list[] =
    "\0\0\0" // Unused
    "\0\0\0" // C
    "abk" // Abkhazian
    "\0\0\0" // Oromo
    "aar" // Afar
    "afr" // Afrikaans
    "alb" // Albanian
    "amh" // Amharic
    "ara" // Arabic
    "arm" // Armenian
    "asm" // Assamese
    "aym" // Aymara
    "aze" // Azerbaijani
    "bak" // Bashkir
    "baq" // Basque
    "ben" // Bengali
    "\0\0\0" // Dzongkha
    "bih" // Bihari
    "bis" // Bislama
    "bre" // Breton
    "bul" // Bulgarian
    "bur" // Burmese
    "bel" // Belarusian
    "khm" // Khmer
    "cat" // Catalan
    "chi" // Chinese
    "cos" // Corsican
    "hrv" // Croatian
    "cze" // Czech
    "dan" // Danish
    "dut" // Dutch
    "eng" // English
    "epo" // Esperanto
    "est" // Estonian
    "fao" // Faroese
    "fij" // Fijian
    "fin" // Finnish
    "fre" // French
    "fry" // WesternFrisian
    "gla" // Gaelic
    "glg" // Galician
    "geo" // Georgian
    "ger" // German
    "gre" // Greek
    "\0\0\0" // Greenlandic
    "grn" // Guarani
    "guj" // Gujarati
    "hau" // Hausa
    "heb" // Hebrew
    "hin" // Hindi
    "hun" // Hungarian
    "ice" // Icelandic
    "ind" // Indonesian
    "ina" // Interlingua
    "ile" // Interlingue
    "iku" // Inuktitut
    "ipk" // Inupiak
    "gle" // Irish
    "ita" // Italian
    "jpn" // Japanese
    "jav" // Javanese
    "kan" // Kannada
    "kas" // Kashmiri
    "kaz" // Kazakh
    "kin" // Kinyarwanda
    "kir" // Kirghiz
    "kor" // Korean
    "kur" // Kurdish
    "\0\0\0" // Rundi
    "lao" // Lao
    "lat" // Latin
    "lav" // Latvian
    "lin" // Lingala
    "lit" // Lithuanian
    "mac" // Macedonian
    "mlg" // Malagasy
    "may" // Malay
    "mal" // Malayalam
    "mlt" // Maltese
    "mao" // Maori
    "mar" // Marathi
    "mah" // Marshallese
    "mon" // Mongolian
    "nau" // NauruLanguage
    "nep" // Nepali
    "nor" // NorwegianBokmal
    "oci" // Occitan
    "ori" // Oriya
    "\0\0\0" // Pashto
    "per" // Persian
    "pol" // Polish
    "por" // Portuguese
    "pan" // Punjabi
    "que" // Quechua
    "roh" // Romansh
    "rum" // Romanian
    "rus" // Russian
    "smo" // Samoan
    "sag" // Sango
    "san" // Sanskrit
    "srp" // Serbian
    "oss" // Ossetic
    "\0\0\0" // SouthernSotho
    "\0\0\0" // Tswana
    "sna" // Shona
    "snd" // Sindhi
    "\0\0\0" // Sinhala
    "\0\0\0" // Swati
    "slo" // Slovak
    "slv" // Slovenian
    "som" // Somali
    "spa" // Spanish
    "sun" // Sundanese
    "swa" // Swahili
    "swe" // Swedish
    "srd" // Sardinian
    "tgk" // Tajik
    "tam" // Tamil
    "tat" // Tatar
    "tel" // Telugu
    "tha" // Thai
    "tib" // Tibetan
    "tir" // Tigrinya
    "tog" // Tongan
    "tso" // Tsonga
    "tur" // Turkish
    "tuk" // Turkmen
    "tah" // Tahitian
    "uig" // Uigur
    "ukr" // Ukrainian
    "urd" // Urdu
    "uzb" // Uzbek
    "vie" // Vietnamese
    "vol" // Volapuk
    "wel" // Welsh
    "wol" // Wolof
    "xho" // Xhosa
    "yid" // Yiddish
    "yor" // Yoruba
    "zha" // Zhuang
    "zul" // Zulu
    "nno" // NorwegianNynorsk
    "bos" // Bosnian
    "div" // Divehi
    "glv" // Manx
    "cor" // Cornish
    "aka" // Akan
    "kok" // Konkani
    "gaa" // Ga
    "ibo" // Igbo
    "kam" // Kamba
    "syc" // Syriac
    "\0\0\0" // Blin
    "\0\0\0" // Geez
    "\0\0\0" // Koro
    "sid" // Sidamo
    "\0\0\0" // Atsam
    "tig" // Tigre
    "\0\0\0" // Jju
    "fur" // Friulian
    "ven" // Venda
    "ewe" // Ewe
    "\0\0\0" // Walamo
    "haw" // Hawaiian
    "\0\0\0" // Tyap
    "\0\0\0" // Nyanja
    "fil" // Filipino
    "gsw" // SwissGerman
    "iii" // SichuanYi
    "kpe" // Kpelle
    "nds" // LowGerman
    "nbl" // SouthNdebele
    "nso" // NorthernSotho
    "sme" // NorthernSami
    "\0\0\0" // Taroko
    "\0\0\0" // Gusii
    "\0\0\0" // Taita
    "ful" // Fulah
    "kik" // Kikuyu
    "\0\0\0" // Samburu
    "\0\0\0" // Sena
    "nde" // NorthNdebele
    "\0\0\0" // Rombo
    "\0\0\0" // Tachelhit
    "kab" // Kabyle
    "nyn" // Nyankole
    "\0\0\0" // Bena
    "\0\0\0" // Vunjo
    "bam" // Bambara
    "\0\0\0" // Embu
    "chr" // Cherokee
    "\0\0\0" // Morisyen
    "\0\0\0" // Makonde
    "\0\0\0" // Langi
    "lug" // Ganda
    "bem" // Bemba
    "\0\0\0" // Kabuverdianu
    "\0\0\0" // Meru
    "\0\0\0" // Kalenjin
    "\0\0\0" // Nama
    "\0\0\0" // Machame
    "\0\0\0" // Colognian
    "mas" // Masai
    "\0\0\0" // Soga
    "\0\0\0" // Luyia
    "\0\0\0" // Asu
    "\0\0\0" // Teso
    "\0\0\0" // Saho
    "\0\0\0" // KoyraChiini
    "\0\0\0" // Rwa
    "luo" // Luo
    "\0\0\0" // Chiga
    "\0\0\0" // CentralMoroccoTamazight
    "\0\0\0" // KoyraboroSenni
    "\0\0\0" // Shambala
    "\0\0\0" // Bodo
    "ava" // Avaric
    "cha" // Chamorro
    "che" // Chechen
    "chu" // Church
    "chv" // Chuvash
    "cre" // Cree
    "hat" // Haitian
    "her" // Herero
    "hmo" // HiriMotu
    "kau" // Kanuri
    "kom" // Komi
    "kon" // Kongo
    "\0\0\0" // Kwanyama
    "lim" // Limburgish
    "lub" // LubaKatanga
    "ltz" // Luxembourgish
    "\0\0\0" // Navaho
    "ndo" // Ndonga
    "oji" // Ojibwa
    "pli" // Pali
    "wln" // Walloon
    "\0\0\0" // Aghem
    "bas" // Basaa
    "\0\0\0" // Zarma
    "dua" // Duala
    "\0\0\0" // JolaFonyi
    "ewo" // Ewondo
    "\0\0\0" // Bafia
    "\0\0\0" // MakhuwaMeetto
    "\0\0\0" // Mundang
    "\0\0\0" // Kwasio
    "\0\0\0" // Nuer
    "\0\0\0" // Sakha
    "\0\0\0" // Sangu
    "\0\0\0" // CongoSwahili
    "\0\0\0" // Tasawaq
    "vai" // Vai
    "\0\0\0" // Walser
    "\0\0\0" // Yangben
    "ave" // Avestan
    "\0\0\0" // Asturian
    "\0\0\0" // Ngomba
    "\0\0\0" // Kako
    "\0\0\0" // Meta
    "\0\0\0" // Ngiemboon
    ;

QT_END_NAMESPACE

#endif /* MARCLANGUAGECODES_H_ */
