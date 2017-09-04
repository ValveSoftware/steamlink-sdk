/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <dshow.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <QtMultimedia/qmediametadata.h>
#include <QtCore/qcoreapplication.h>
#include <QSize>
#include <qdatetime.h>
#include <qimage.h>

#include <initguid.h>
#include <qnetwork.h>

#include "directshowmetadatacontrol.h"
#include "directshowplayerservice.h"

#include <QtMultimedia/private/qtmultimedia-config_p.h>

#if QT_CONFIG(wmsdk)
#include <wmsdk.h>
#endif

#if QT_CONFIG(wshellitem)
#include <ShlObj.h>
#include <propkeydef.h>
#include <private/qsystemlibrary_p.h>

DEFINE_PROPERTYKEY(PKEY_Author, 0xF29F85E0, 0x4FF9, 0x1068, 0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9, 4);
DEFINE_PROPERTYKEY(PKEY_Title, 0xF29F85E0, 0x4FF9, 0x1068, 0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9, 2);
DEFINE_PROPERTYKEY(PKEY_Media_SubTitle, 0x56A3372E, 0xCE9C, 0x11D2, 0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6, 38);
DEFINE_PROPERTYKEY(PKEY_ParentalRating, 0x64440492, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 21);
DEFINE_PROPERTYKEY(PKEY_Comment, 0xF29F85E0, 0x4FF9, 0x1068, 0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9, 6);
DEFINE_PROPERTYKEY(PKEY_Copyright, 0x64440492, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 11);
DEFINE_PROPERTYKEY(PKEY_Media_ProviderStyle, 0x64440492, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 40);
DEFINE_PROPERTYKEY(PKEY_Media_Year, 0x56A3372E, 0xCE9C, 0x11D2, 0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6, 5);
DEFINE_PROPERTYKEY(PKEY_Media_DateEncoded, 0x2E4B640D, 0x5019, 0x46D8, 0x88, 0x81, 0x55, 0x41, 0x4C, 0xC5, 0xCA, 0xA0, 100);
DEFINE_PROPERTYKEY(PKEY_Rating, 0x64440492, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 9);
DEFINE_PROPERTYKEY(PKEY_Keywords, 0xF29F85E0, 0x4FF9, 0x1068, 0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9, 5);
DEFINE_PROPERTYKEY(PKEY_Language, 0xD5CDD502, 0x2E9C, 0x101B, 0x93, 0x97, 0x08, 0x00, 0x2B, 0x2C, 0xF9, 0xAE, 28);
DEFINE_PROPERTYKEY(PKEY_Media_Publisher, 0x64440492, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 30);
DEFINE_PROPERTYKEY(PKEY_Media_Duration, 0x64440490, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 3);
DEFINE_PROPERTYKEY(PKEY_Audio_EncodingBitrate, 0x64440490, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 4);
DEFINE_PROPERTYKEY(PKEY_Media_AverageLevel, 0x09EDD5B6, 0xB301, 0x43C5, 0x99, 0x90, 0xD0, 0x03, 0x02, 0xEF, 0xFD, 0x46, 100);
DEFINE_PROPERTYKEY(PKEY_Audio_ChannelCount, 0x64440490, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 7);
DEFINE_PROPERTYKEY(PKEY_Audio_PeakValue, 0x2579E5D0, 0x1116, 0x4084, 0xBD, 0x9A, 0x9B, 0x4F, 0x7C, 0xB4, 0xDF, 0x5E, 100);
DEFINE_PROPERTYKEY(PKEY_Audio_SampleRate, 0x64440490, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 5);
DEFINE_PROPERTYKEY(PKEY_Audio_Format, 0x64440490, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 2);
DEFINE_PROPERTYKEY(PKEY_Music_AlbumTitle, 0x56A3372E, 0xCE9C, 0x11D2, 0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6, 4);
DEFINE_PROPERTYKEY(PKEY_Music_AlbumArtist, 0x56A3372E, 0xCE9C, 0x11D2, 0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6, 13);
DEFINE_PROPERTYKEY(PKEY_Music_Artist, 0x56A3372E, 0xCE9C, 0x11D2, 0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6, 2);
DEFINE_PROPERTYKEY(PKEY_Music_Composer, 0x64440492, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 19);
DEFINE_PROPERTYKEY(PKEY_Music_Conductor, 0x56A3372E, 0xCE9C, 0x11D2, 0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6, 36);
DEFINE_PROPERTYKEY(PKEY_Music_Lyrics, 0x56A3372E, 0xCE9C, 0x11D2, 0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6, 12);
DEFINE_PROPERTYKEY(PKEY_Music_Mood, 0x56A3372E, 0xCE9C, 0x11D2, 0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6, 39);
DEFINE_PROPERTYKEY(PKEY_Music_TrackNumber, 0x56A3372E, 0xCE9C, 0x11D2, 0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6, 7);
DEFINE_PROPERTYKEY(PKEY_Music_Genre, 0x56A3372E, 0xCE9C, 0x11D2, 0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6, 11);
DEFINE_PROPERTYKEY(PKEY_ThumbnailStream, 0xF29F85E0, 0x4FF9, 0x1068, 0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9, 27);
DEFINE_PROPERTYKEY(PKEY_Video_FrameHeight, 0x64440491, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 4);
DEFINE_PROPERTYKEY(PKEY_Video_FrameWidth, 0x64440491, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 3);
DEFINE_PROPERTYKEY(PKEY_Video_HorizontalAspectRatio, 0x64440491, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 42);
DEFINE_PROPERTYKEY(PKEY_Video_VerticalAspectRatio, 0x64440491, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 45);
DEFINE_PROPERTYKEY(PKEY_Video_FrameRate, 0x64440491, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 6);
DEFINE_PROPERTYKEY(PKEY_Video_EncodingBitrate, 0x64440491, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 8);
DEFINE_PROPERTYKEY(PKEY_Video_Director, 0x64440492, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 20);
DEFINE_PROPERTYKEY(PKEY_Video_Compression, 0x64440491, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 10);
DEFINE_PROPERTYKEY(PKEY_Media_Writer, 0x64440492, 0x4C8B, 0x11D1, 0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03, 23);

static QString nameForGUIDString(const QString &guid)
{
    // Audio formats
    if (guid == "{00001610-0000-0010-8000-00AA00389B71}" || guid == "{000000FF-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("MPEG AAC Audio");
    else if (guid == "{00001600-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("MPEG ADTS AAC Audio");
    else if (guid == "{00000092-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Dolby AC-3 SPDIF");
    else if (guid == "{E06D802C-DB46-11CF-B4D1-00805F6CBBEA}" || guid == "{00002000-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Dolby AC-3");
    else if (guid == "{A7FB87AF-2D02-42FB-A4D4-05CD93843BDD}")
        return QStringLiteral("Dolby Digital Plus");
    else if (guid == "{00000009-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("DRM");
    else if (guid == "{00000008-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Digital Theater Systems Audio (DTS)");
    else if (guid == "{00000003-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("IEEE Float Audio");
    else if (guid == "{00000055-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("MPEG Audio Layer-3 (MP3)");
    else if (guid == "{00000050-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("MPEG-1 Audio");
    else if (guid == "{2E6D7033-767A-494D-B478-F29D25DC9037}")
        return QStringLiteral("MPEG Audio Layer 1/2");
    else if (guid == "{0000000A-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Windows Media Audio Voice");
    else if (guid == "{00000001-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Uncompressed PCM Audio");
    else if (guid == "{00000164-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Windows Media Audio 9 SPDIF");
    else if (guid == "{00000161-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Windows Media Audio 8 (WMA2)");
    else if (guid == "{00000162-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Windows Media Audio 9 (WMA3");
    else if (guid == "{00000163-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Windows Media Audio 9 Lossless");
    else if (guid == "{8D2FD10B-5841-4a6b-8905-588FEC1ADED9}")
        return QStringLiteral("Vorbis");
    else if (guid == "{0000F1AC-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Free Lossless Audio Codec (FLAC)");
    else if (guid == "{00006C61-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Apple Lossless Audio Codec (ALAC)");

    // Video formats
    if (guid == "{35327664-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("DVCPRO 25 (DV25)");
    else if (guid == "{30357664-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("DVCPRO 50 (DV50)");
    else if (guid == "{20637664-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("DVC/DV Video");
    else if (guid == "{31687664-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("DVCPRO 100 (DVH1)");
    else if (guid == "{64687664-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("HD-DVCR (DVHD)");
    else if (guid == "{64737664-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("SDL-DVCR (DVSD)");
    else if (guid == "{6C737664-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("SD-DVCR (DVSL)");
    else if (guid == "{33363248-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("H.263 Video");
    else if (guid == "{34363248-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("H.264 Video");
    else if (guid == "{35363248-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("H.265 Video");
    else if (guid == "{43564548-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("High Efficiency Video Coding (HEVC)");
    else if (guid == "{3253344D-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("MPEG-4 part 2 Video (M4S2)");
    else if (guid == "{47504A4D-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Motion JPEG (MJPG)");
    else if (guid == "{3334504D-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Microsoft MPEG 4 version 3 (MP43)");
    else if (guid == "{5334504D-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("ISO MPEG 4 version 1 (MP4S)");
    else if (guid == "{5634504D-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("MPEG-4 part 2 Video (MP4V)");
    else if (guid == "{E06D8026-DB46-11CF-B4D1-00805F6CBBEA}")
        return QStringLiteral("MPEG-2 Video");
    else if (guid == "{3147504D-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("MPEG-1 Video");
    else if (guid == "{3153534D-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Windows Media Screen 1 (MSS1)");
    else if (guid == "{3253534D-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Windows Media Video 9 Screen (MSS2)");
    else if (guid == "{31564D57-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Windows Media Video 7 (WMV1)");
    else if (guid == "{32564D57-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Windows Media Video 8 (WMV2)");
    else if (guid == "{33564D57-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Windows Media Video 9 (WMV3)");
    else if (guid == "{31435657-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("Windows Media Video VC1 (WVC1)");
    else if (guid == "{30385056-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("VP8 Video");
    else if (guid == "{30395056-0000-0010-8000-00AA00389B71}")
        return QStringLiteral("VP9 Video");

    else
        return QStringLiteral("Unknown codec");
}

typedef HRESULT (WINAPI *q_SHCreateItemFromParsingName)(PCWSTR, IBindCtx *, const GUID&, void **);
static q_SHCreateItemFromParsingName sHCreateItemFromParsingName = 0;
#endif

#if QT_CONFIG(wmsdk)

namespace
{
    struct QWMMetaDataKey
    {
        QString qtName;
        const wchar_t *wmName;

        QWMMetaDataKey(const QString &qtn, const wchar_t *wmn) : qtName(qtn), wmName(wmn) { }
    };
}

typedef QList<QWMMetaDataKey> QWMMetaDataKeys;
Q_GLOBAL_STATIC(QWMMetaDataKeys, metadataKeys)

static const QWMMetaDataKeys *qt_wmMetaDataKeys()
{
    if (metadataKeys->isEmpty()) {
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Title, L"Title"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::SubTitle, L"WM/SubTitle"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Author, L"Author"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Comment, L"Comment"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Description, L"Description"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Category, L"WM/Category"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Genre, L"WM/Genre"));
        //metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Date, 0));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Year, L"WM/Year"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::UserRating, L"Rating"));
        //metadataKeys->append(QWMMetaDataKey(QMediaMetaData::MetaDatawords, 0));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Language, L"WM/Language"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Publisher, L"WM/Publisher"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Copyright, L"Copyright"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::ParentalRating, L"WM/ParentalRating"));
        //metadataKeys->append(QWMMetaDataKey(QMediaMetaData::RatingOrganisation, L"RatingOrganisation"));

        // Media
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Size, L"FileSize"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::MediaType, L"MediaType"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Duration, L"Duration"));

        // Audio
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::AudioBitRate, L"AudioBitRate"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::AudioCodec, L"AudioCodec"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::ChannelCount, L"ChannelCount"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::SampleRate, L"Frequency"));

        // Music
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::AlbumTitle, L"WM/AlbumTitle"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::AlbumArtist, L"WM/AlbumArtist"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::ContributingArtist, L"Author"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Composer, L"WM/Composer"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Conductor, L"WM/Conductor"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Lyrics, L"WM/Lyrics"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Mood, L"WM/Mood"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::TrackNumber, L"WM/TrackNumber"));
        //metadataKeys->append(QWMMetaDataKey(QMediaMetaData::TrackCount, 0));
        //metadataKeys->append(QWMMetaDataKey(QMediaMetaData::CoverArtUriSmall, 0));
        //metadataKeys->append(QWMMetaDataKey(QMediaMetaData::CoverArtUriLarge, 0));

        // Image/Video
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Resolution, L"WM/VideoHeight"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::PixelAspectRatio, L"AspectRatioX"));

        // Video
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::VideoFrameRate, L"WM/VideoFrameRate"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::VideoBitRate, L"VideoBitRate"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::VideoCodec, L"VideoCodec"));

        //metadataKeys->append(QWMMetaDataKey(QMediaMetaData::PosterUri, 0));

        // Movie
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::ChapterNumber, L"ChapterNumber"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Director, L"WM/Director"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::LeadPerformer, L"LeadPerformer"));
        metadataKeys->append(QWMMetaDataKey(QMediaMetaData::Writer, L"WM/Writer"));
    }

    return metadataKeys;
}

static QVariant getValue(IWMHeaderInfo *header, const wchar_t *key)
{
    WORD streamNumber = 0;
    WMT_ATTR_DATATYPE type = WMT_TYPE_DWORD;
    WORD size = 0;

    if (header->GetAttributeByName(&streamNumber, key, &type, 0, &size) == S_OK) {
        switch (type) {
        case WMT_TYPE_DWORD:
            if (size == sizeof(DWORD)) {
                DWORD word;
                if (header->GetAttributeByName(
                        &streamNumber,
                        key,
                        &type,
                        reinterpret_cast<BYTE *>(&word),
                        &size) == S_OK) {
                    return int(word);
                }
            }
            break;
        case WMT_TYPE_STRING:
            {
                QString string;
                string.resize(size / 2); // size is in bytes, string is in UTF16

                if (header->GetAttributeByName(
                        &streamNumber,
                        key,
                        &type,
                        reinterpret_cast<BYTE *>(const_cast<ushort *>(string.utf16())),
                        &size) == S_OK) {
                    return string;
                }
            }
            break;
        case WMT_TYPE_BINARY:
            {
                QByteArray bytes;
                bytes.resize(size);
                if (header->GetAttributeByName(
                        &streamNumber,
                        key,
                        &type,
                        reinterpret_cast<BYTE *>(bytes.data()),
                        &size) == S_OK) {
                    return bytes;
                }
            }
            break;
        case WMT_TYPE_BOOL:
            if (size == sizeof(DWORD)) {
                DWORD word;
                if (header->GetAttributeByName(
                        &streamNumber,
                        key,
                        &type,
                        reinterpret_cast<BYTE *>(&word),
                        &size) == S_OK) {
                    return bool(word);
                }
            }
            break;
        case WMT_TYPE_QWORD:
            if (size == sizeof(QWORD)) {
                QWORD word;
                if (header->GetAttributeByName(
                        &streamNumber,
                        key,
                        &type,
                        reinterpret_cast<BYTE *>(&word),
                        &size) == S_OK) {
                    return qint64(word);
                }
            }
            break;
        case WMT_TYPE_WORD:
            if (size == sizeof(WORD)){
                WORD word;
                if (header->GetAttributeByName(
                        &streamNumber,
                        key,
                        &type,
                        reinterpret_cast<BYTE *>(&word),
                        &size) == S_OK) {
                    return short(word);
                }
            }
            break;
        case WMT_TYPE_GUID:
            if (size == 16) {
            }
            break;
        default:
            break;
        }
    }
    return QVariant();
}
#endif

#if QT_CONFIG(wshellitem)
static QVariant convertValue(const PROPVARIANT& var)
{
    QVariant value;
    switch (var.vt) {
    case VT_LPWSTR:
        value = QString::fromUtf16(reinterpret_cast<const ushort*>(var.pwszVal));
        break;
    case VT_UI4:
        value = uint(var.ulVal);
        break;
    case VT_UI8:
        value = qulonglong(var.uhVal.QuadPart);
        break;
    case VT_BOOL:
        value = bool(var.boolVal);
        break;
    case VT_FILETIME:
        SYSTEMTIME sysDate;
        if (!FileTimeToSystemTime(&var.filetime, &sysDate))
            break;
        value = QDate(sysDate.wYear, sysDate.wMonth, sysDate.wDay);
        break;
    case VT_STREAM:
    {
        STATSTG stat;
        if (FAILED(var.pStream->Stat(&stat, STATFLAG_NONAME)))
            break;
        void *data = malloc(stat.cbSize.QuadPart);
        ULONG read = 0;
        if (FAILED(var.pStream->Read(data, stat.cbSize.QuadPart, &read))) {
            free(data);
            break;
        }
        value = QImage::fromData(reinterpret_cast<const uchar*>(data), read);
        free(data);
    }
        break;
    case VT_VECTOR | VT_LPWSTR:
        QStringList vList;
        for (ULONG i = 0; i < var.calpwstr.cElems; ++i)
            vList.append(QString::fromUtf16(reinterpret_cast<const ushort*>(var.calpwstr.pElems[i])));
        value = vList;
        break;
    }
    return value;
}
#endif

DirectShowMetaDataControl::DirectShowMetaDataControl(QObject *parent)
    : QMetaDataReaderControl(parent)
    , m_available(false)
{
}

DirectShowMetaDataControl::~DirectShowMetaDataControl()
{
}

bool DirectShowMetaDataControl::isMetaDataAvailable() const
{
    return m_available;
}

QVariant DirectShowMetaDataControl::metaData(const QString &key) const
{
    return m_metadata.value(key);
}

QStringList DirectShowMetaDataControl::availableMetaData() const
{
    return m_metadata.keys();
}

static QString convertBSTR(BSTR *string)
{
    QString value = QString::fromUtf16(reinterpret_cast<ushort *>(*string),
                                       ::SysStringLen(*string));

    ::SysFreeString(*string);
    string = 0;

    return value;
}

void DirectShowMetaDataControl::reset()
{
    bool hadMetadata = !m_metadata.isEmpty();
    m_metadata.clear();

    setMetadataAvailable(false);

    if (hadMetadata)
        emit metaDataChanged();
}

void DirectShowMetaDataControl::updateMetadata(IFilterGraph2 *graph, IBaseFilter *source, const QString &fileSrc)
{
    m_metadata.clear();

#if QT_CONFIG(wshellitem)
    if (!sHCreateItemFromParsingName) {
        QSystemLibrary lib(QStringLiteral("shell32"));
        sHCreateItemFromParsingName = (q_SHCreateItemFromParsingName)(lib.resolve("SHCreateItemFromParsingName"));
    }

    if (!fileSrc.isEmpty() && sHCreateItemFromParsingName) {
        IShellItem2* shellItem = 0;
        if (sHCreateItemFromParsingName(reinterpret_cast<const WCHAR*>(fileSrc.utf16()),
                                        0, IID_PPV_ARGS(&shellItem)) == S_OK) {

            IPropertyStore *pStore = 0;
            if (shellItem->GetPropertyStore(GPS_DEFAULT, IID_PPV_ARGS(&pStore)) == S_OK) {
                DWORD cProps;
                if (SUCCEEDED(pStore->GetCount(&cProps))) {
                    for (DWORD i = 0; i < cProps; ++i)
                    {
                        PROPERTYKEY key;
                        PROPVARIANT var;
                        PropVariantInit(&var);
                        if (FAILED(pStore->GetAt(i, &key)))
                            continue;
                        if (FAILED(pStore->GetValue(key, &var)))
                            continue;

                        if (IsEqualPropertyKey(key, PKEY_Author)) {
                            m_metadata.insert(QMediaMetaData::Author, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Title)) {
                            m_metadata.insert(QMediaMetaData::Title, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Media_SubTitle)) {
                            m_metadata.insert(QMediaMetaData::SubTitle, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_ParentalRating)) {
                            m_metadata.insert(QMediaMetaData::ParentalRating, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Comment)) {
                            m_metadata.insert(QMediaMetaData::Description, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Copyright)) {
                            m_metadata.insert(QMediaMetaData::Copyright, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Media_ProviderStyle)) {
                            m_metadata.insert(QMediaMetaData::Genre, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Media_Year)) {
                            m_metadata.insert(QMediaMetaData::Year, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Media_DateEncoded)) {
                            m_metadata.insert(QMediaMetaData::Date, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Rating)) {
                            m_metadata.insert(QMediaMetaData::UserRating,
                                              int((convertValue(var).toUInt() - 1) / qreal(98) * 100));
                        } else if (IsEqualPropertyKey(key, PKEY_Keywords)) {
                            m_metadata.insert(QMediaMetaData::Keywords, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Language)) {
                            m_metadata.insert(QMediaMetaData::Language, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Media_Publisher)) {
                            m_metadata.insert(QMediaMetaData::Publisher, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Media_Duration)) {
                            m_metadata.insert(QMediaMetaData::Duration,
                                              (convertValue(var).toLongLong() + 10000) / 10000);
                        } else if (IsEqualPropertyKey(key, PKEY_Audio_EncodingBitrate)) {
                            m_metadata.insert(QMediaMetaData::AudioBitRate, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Media_AverageLevel)) {
                            m_metadata.insert(QMediaMetaData::AverageLevel, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Audio_ChannelCount)) {
                            m_metadata.insert(QMediaMetaData::ChannelCount, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Audio_PeakValue)) {
                            m_metadata.insert(QMediaMetaData::PeakValue, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Audio_SampleRate)) {
                            m_metadata.insert(QMediaMetaData::SampleRate, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Music_AlbumTitle)) {
                            m_metadata.insert(QMediaMetaData::AlbumTitle, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Music_AlbumArtist)) {
                            m_metadata.insert(QMediaMetaData::AlbumArtist, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Music_Artist)) {
                            m_metadata.insert(QMediaMetaData::ContributingArtist, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Music_Composer)) {
                            m_metadata.insert(QMediaMetaData::Composer, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Music_Conductor)) {
                            m_metadata.insert(QMediaMetaData::Conductor, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Music_Lyrics)) {
                            m_metadata.insert(QMediaMetaData::Lyrics, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Music_Mood)) {
                            m_metadata.insert(QMediaMetaData::Mood, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Music_TrackNumber)) {
                            m_metadata.insert(QMediaMetaData::TrackNumber, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Music_Genre)) {
                            m_metadata.insert(QMediaMetaData::Genre, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_ThumbnailStream)) {
                            m_metadata.insert(QMediaMetaData::ThumbnailImage, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Video_FrameHeight)) {
                            QSize res;
                            res.setHeight(convertValue(var).toUInt());
                            if (SUCCEEDED(pStore->GetValue(PKEY_Video_FrameWidth, &var)))
                                res.setWidth(convertValue(var).toUInt());
                            m_metadata.insert(QMediaMetaData::Resolution, res);
                        } else if (IsEqualPropertyKey(key, PKEY_Video_HorizontalAspectRatio)) {
                            QSize aspectRatio;
                            aspectRatio.setWidth(convertValue(var).toUInt());
                            if (SUCCEEDED(pStore->GetValue(PKEY_Video_VerticalAspectRatio, &var)))
                                aspectRatio.setHeight(convertValue(var).toUInt());
                            m_metadata.insert(QMediaMetaData::PixelAspectRatio, aspectRatio);
                        } else if (IsEqualPropertyKey(key, PKEY_Video_FrameRate)) {
                            m_metadata.insert(QMediaMetaData::VideoFrameRate,
                                              convertValue(var).toReal() / 1000);
                        } else if (IsEqualPropertyKey(key, PKEY_Video_EncodingBitrate)) {
                            m_metadata.insert(QMediaMetaData::VideoBitRate, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Video_Director)) {
                            m_metadata.insert(QMediaMetaData::Director, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Media_Writer)) {
                            m_metadata.insert(QMediaMetaData::Writer, convertValue(var));
                        } else if (IsEqualPropertyKey(key, PKEY_Video_Compression)) {
                            m_metadata.insert(QMediaMetaData::VideoCodec, nameForGUIDString(convertValue(var).toString()));
                        } else if (IsEqualPropertyKey(key, PKEY_Audio_Format)) {
                            m_metadata.insert(QMediaMetaData::AudioCodec, nameForGUIDString(convertValue(var).toString()));
                        }

                        PropVariantClear(&var);
                    }
                }

                pStore->Release();
            }

            shellItem->Release();
        }
    }

    if (!m_metadata.isEmpty())
        goto send_event;
#endif

#if QT_CONFIG(wmsdk)
    IWMHeaderInfo *info = com_cast<IWMHeaderInfo>(source, IID_IWMHeaderInfo);

    if (info) {
        const auto keys = *qt_wmMetaDataKeys();
        for (const QWMMetaDataKey &key : keys) {
            QVariant var = getValue(info, key.wmName);
            if (var.isValid()) {
                if (key.qtName == QMediaMetaData::Duration) {
                    // duration is provided in 100-nanosecond units, convert to milliseconds
                    var = (var.toLongLong() + 10000) / 10000;
                } else if (key.qtName == QMediaMetaData::Resolution) {
                    QSize res;
                    res.setHeight(var.toUInt());
                    res.setWidth(getValue(info, L"WM/VideoWidth").toUInt());
                    var = res;
                } else if (key.qtName == QMediaMetaData::VideoFrameRate) {
                    var = var.toReal() / 1000.f;
                } else if (key.qtName == QMediaMetaData::PixelAspectRatio) {
                    QSize aspectRatio;
                    aspectRatio.setWidth(var.toUInt());
                    aspectRatio.setHeight(getValue(info, L"AspectRatioY").toUInt());
                    var = aspectRatio;
                } else if (key.qtName == QMediaMetaData::UserRating) {
                    var = (var.toUInt() - 1) / qreal(98) * 100;
                }

                m_metadata.insert(key.qtName, var);
            }
        }

        info->Release();
    }

    if (!m_metadata.isEmpty())
        goto send_event;
#endif
    {
        IAMMediaContent *content = 0;

        if ((!graph || graph->QueryInterface(
                 IID_IAMMediaContent, reinterpret_cast<void **>(&content)) != S_OK)
                && (!source || source->QueryInterface(
                        IID_IAMMediaContent, reinterpret_cast<void **>(&content)) != S_OK)) {
            content = 0;
        }

        if (content) {
            BSTR string = 0;

            if (content->get_AuthorName(&string) == S_OK)
                m_metadata.insert(QMediaMetaData::Author, convertBSTR(&string));

            if (content->get_Title(&string) == S_OK)
                m_metadata.insert(QMediaMetaData::Title, convertBSTR(&string));

            if (content->get_Description(&string) == S_OK)
                m_metadata.insert(QMediaMetaData::Description, convertBSTR(&string));

            if (content->get_Rating(&string) == S_OK)
                m_metadata.insert(QMediaMetaData::UserRating, convertBSTR(&string));

            if (content->get_Copyright(&string) == S_OK)
                m_metadata.insert(QMediaMetaData::Copyright, convertBSTR(&string));

            content->Release();
        }
    }

send_event:
    // DirectShowMediaPlayerService holds a lock at this point so defer emitting signals to a later
    // time.
    QCoreApplication::postEvent(this, new QEvent(QEvent::Type(MetaDataChanged)));
}

void DirectShowMetaDataControl::customEvent(QEvent *event)
{
    if (event->type() == QEvent::Type(MetaDataChanged)) {
        event->accept();

        setMetadataAvailable(!m_metadata.isEmpty());

        emit metaDataChanged();
    } else {
        QMetaDataReaderControl::customEvent(event);
    }
}

void DirectShowMetaDataControl::setMetadataAvailable(bool available)
{
    if (m_available == available)
        return;

    m_available = available;
    emit metaDataAvailableChanged(m_available);
}
