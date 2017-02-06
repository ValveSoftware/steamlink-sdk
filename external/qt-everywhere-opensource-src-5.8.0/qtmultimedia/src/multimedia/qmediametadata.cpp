/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:FDL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Free Documentation License Usage
** Alternatively, this file may be used under the terms of the GNU Free
** Documentation License version 1.3 as published by the Free Software
** Foundation and appearing in the file included in the packaging of
** this file. Please review the following information to ensure
** the GNU Free Documentation License version 1.3 requirements
** will be met: http://www.gnu.org/copyleft/fdl.html.
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmediametadata.h"

QT_BEGIN_NAMESPACE

/*
    When these conditions are satisfied, QStringLiteral is implemented by
    gcc's statement-expression extension.  However, in this file it will
    not work, because "statement-expressions are not allowed outside functions
    nor in template-argument lists".
    MSVC 2012 produces an internal compiler error on encountering
    QStringLiteral in this context.

    Fall back to the less-performant QLatin1String in this case.
*/
#if defined(Q_CC_GNU) && defined(Q_COMPILER_LAMBDA)
#    define Q_DEFINE_METADATA(key) const QString QMediaMetaData::key(QStringLiteral(#key))
#else
#    define Q_DEFINE_METADATA(key) const QString QMediaMetaData::key(QLatin1String(#key))
#endif

// Common
Q_DEFINE_METADATA(Title);
Q_DEFINE_METADATA(SubTitle);
Q_DEFINE_METADATA(Author);
Q_DEFINE_METADATA(Comment);
Q_DEFINE_METADATA(Description);
Q_DEFINE_METADATA(Category);
Q_DEFINE_METADATA(Genre);
Q_DEFINE_METADATA(Year);
Q_DEFINE_METADATA(Date);
Q_DEFINE_METADATA(UserRating);
Q_DEFINE_METADATA(Keywords);
Q_DEFINE_METADATA(Language);
Q_DEFINE_METADATA(Publisher);
Q_DEFINE_METADATA(Copyright);
Q_DEFINE_METADATA(ParentalRating);
Q_DEFINE_METADATA(RatingOrganization);

// Media
Q_DEFINE_METADATA(Size);
Q_DEFINE_METADATA(MediaType);
Q_DEFINE_METADATA(Duration);

// Audio
Q_DEFINE_METADATA(AudioBitRate);
Q_DEFINE_METADATA(AudioCodec);
Q_DEFINE_METADATA(AverageLevel);
Q_DEFINE_METADATA(ChannelCount);
Q_DEFINE_METADATA(PeakValue);
Q_DEFINE_METADATA(SampleRate);

// Music
Q_DEFINE_METADATA(AlbumTitle);
Q_DEFINE_METADATA(AlbumArtist);
Q_DEFINE_METADATA(ContributingArtist);
Q_DEFINE_METADATA(Composer);
Q_DEFINE_METADATA(Conductor);
Q_DEFINE_METADATA(Lyrics);
Q_DEFINE_METADATA(Mood);
Q_DEFINE_METADATA(TrackNumber);
Q_DEFINE_METADATA(TrackCount);

Q_DEFINE_METADATA(CoverArtUrlSmall);
Q_DEFINE_METADATA(CoverArtUrlLarge);

// Image/Video
Q_DEFINE_METADATA(Resolution);
Q_DEFINE_METADATA(PixelAspectRatio);

// Video
Q_DEFINE_METADATA(VideoFrameRate);
Q_DEFINE_METADATA(VideoBitRate);
Q_DEFINE_METADATA(VideoCodec);

Q_DEFINE_METADATA(PosterUrl);

// Movie
Q_DEFINE_METADATA(ChapterNumber);
Q_DEFINE_METADATA(Director);
Q_DEFINE_METADATA(LeadPerformer);
Q_DEFINE_METADATA(Writer);

// Photos
Q_DEFINE_METADATA(CameraManufacturer);
Q_DEFINE_METADATA(CameraModel);
Q_DEFINE_METADATA(Event);
Q_DEFINE_METADATA(Subject);
Q_DEFINE_METADATA(Orientation);
Q_DEFINE_METADATA(ExposureTime);
Q_DEFINE_METADATA(FNumber);
Q_DEFINE_METADATA(ExposureProgram);
Q_DEFINE_METADATA(ISOSpeedRatings);
Q_DEFINE_METADATA(ExposureBiasValue);
Q_DEFINE_METADATA(DateTimeOriginal);
Q_DEFINE_METADATA(DateTimeDigitized);
Q_DEFINE_METADATA(SubjectDistance);
Q_DEFINE_METADATA(MeteringMode);
Q_DEFINE_METADATA(LightSource);
Q_DEFINE_METADATA(Flash);
Q_DEFINE_METADATA(FocalLength);
Q_DEFINE_METADATA(ExposureMode);
Q_DEFINE_METADATA(WhiteBalance);
Q_DEFINE_METADATA(DigitalZoomRatio);
Q_DEFINE_METADATA(FocalLengthIn35mmFilm);
Q_DEFINE_METADATA(SceneCaptureType);
Q_DEFINE_METADATA(GainControl);
Q_DEFINE_METADATA(Contrast);
Q_DEFINE_METADATA(Saturation);
Q_DEFINE_METADATA(Sharpness);
Q_DEFINE_METADATA(DeviceSettingDescription);

// Location
Q_DEFINE_METADATA(GPSLatitude);
Q_DEFINE_METADATA(GPSLongitude);
Q_DEFINE_METADATA(GPSAltitude);
Q_DEFINE_METADATA(GPSTimeStamp);
Q_DEFINE_METADATA(GPSSatellites);
Q_DEFINE_METADATA(GPSStatus);
Q_DEFINE_METADATA(GPSDOP);
Q_DEFINE_METADATA(GPSSpeed);
Q_DEFINE_METADATA(GPSTrack);
Q_DEFINE_METADATA(GPSTrackRef);
Q_DEFINE_METADATA(GPSImgDirection);
Q_DEFINE_METADATA(GPSImgDirectionRef);
Q_DEFINE_METADATA(GPSMapDatum);
Q_DEFINE_METADATA(GPSProcessingMethod);
Q_DEFINE_METADATA(GPSAreaInformation);

Q_DEFINE_METADATA(PosterImage);
Q_DEFINE_METADATA(CoverArtImage);
Q_DEFINE_METADATA(ThumbnailImage);


/*!
    \namespace QMediaMetaData
    \ingroup multimedia-namespaces
    \ingroup multimedia
    \inmodule QtMultimedia

    \brief Provides identifiers for meta-data attributes.

    \note Not all identifiers are supported on all platforms. Please consult vendor documentation for specific support
    on different platforms.

    \table 60%
    \header \li {3,1}
    Common attributes
    \header \li Value \li Description \li Type
    \row \li Title \li The title of the media.  \li QString
    \row \li SubTitle \li The sub-title of the media. \li QString
    \row \li Author \li The authors of the media. \li QStringList
    \row \li Comment \li A user comment about the media. \li QString
    \row \li Description \li A description of the media.  \li QString
    \row \li Category \li The category of the media.  \li QStringList
    \row \li Genre \li The genre of the media.  \li QStringList
    \row \li Year \li The year of release of the media.  \li int
    \row \li Date \li The date of the media. \li QDate.
    \row \li UserRating \li A user rating of the media. \li int [0..100]
    \row \li Keywords \li A list of keywords describing the media.  \li QStringList
    \row \li Language \li The language of media, as an ISO 639-2 code. \li QString

    \row \li Publisher \li The publisher of the media.  \li QString
    \row \li Copyright \li The media's copyright notice.  \li QString
    \row \li ParentalRating \li The parental rating of the media.  \li QString
    \row \li RatingOrganization \li The organization responsible for the parental rating of the media.
    \li QString

    \header \li {3,1}
    Media attributes
    \row \li Size \li The size in bytes of the media. \li qint64
    \row \li MediaType \li The type of the media (audio, video, etc).  \li QString
    \row \li Duration \li The duration in millseconds of the media.  \li qint64

    \header \li {3,1}
    Audio attributes
    \row \li AudioBitRate \li The bit rate of the media's audio stream in bits per second.  \li int
    \row \li AudioCodec \li The codec of the media's audio stream.  \li QString
    \row \li AverageLevel \li The average volume level of the media.  \li int
    \row \li ChannelCount \li The number of channels in the media's audio stream. \li int
    \row \li PeakValue \li The peak volume of the media's audio stream. \li int
    \row \li SampleRate \li The sample rate of the media's audio stream in hertz. \li int

    \header \li {3,1}
    Music attributes
    \row \li AlbumTitle \li The title of the album the media belongs to.  \li QString
    \row \li AlbumArtist \li The principal artist of the album the media belongs to.  \li QString
    \row \li ContributingArtist \li The artists contributing to the media.  \li QStringList
    \row \li Composer \li The composer of the media.  \li QStringList
    \row \li Conductor \li The conductor of the media. \li QString
    \row \li Lyrics \li The lyrics to the media. \li QString
    \row \li Mood \li The mood of the media.  \li QString
    \row \li TrackNumber \li The track number of the media.  \li int
    \row \li TrackCount \li The number of tracks on the album containing the media.  \li int

    \row \li CoverArtUrlSmall \li The URL of a small cover art image. \li  QUrl
    \row \li CoverArtUrlLarge \li The URL of a large cover art image. \li  QUrl
    \row \li CoverArtImage \li An embedded cover art image. \li  QImage

    \header \li {3,1}
    Image and video attributes
    \row \li Resolution \li The dimensions of an image or video. \li QSize
    \row \li PixelAspectRatio \li The pixel aspect ratio of an image or video. \li QSize

    \header \li {3,1}
    Video attributes
    \row \li VideoFrameRate \li The frame rate of the media's video stream. \li qreal
    \row \li VideoBitRate \li The bit rate of the media's video stream in bits per second.  \li int
    \row \li VideoCodec \li The codec of the media's video stream.  \li QString

    \row \li PosterUrl \li The URL of a poster image. \li QUrl
    \row \li PosterImage \li An embedded poster image. \li QImage

    \header \li {3,1}
    Movie attributes
    \row \li ChapterNumber \li The chapter number of the media.  \li int
    \row \li Director \li The director of the media.  \li QString
    \row \li LeadPerformer \li The lead performer in the media.  \li QStringList
    \row \li Writer \li The writer of the media.  \li QStringList

    \header \li {3,1}
    Photo attributes.
    \row \li CameraManufacturer \li The manufacturer of the camera used to capture the media.  \li QString
    \row \li CameraModel \li The model of the camera used to capture the media.  \li QString
    \row \li Event \li The event during which the media was captured.  \li QString
    \row \li Subject \li The subject of the media.  \li QString
    \row \li Orientation \li Orientation of image.  \li int (degrees)
    \row \li ExposureTime \li Exposure time, given in seconds.  \li qreal
    \row \li FNumber \li The F Number.  \li int
    \row \li ExposureProgram
        \li The class of the program used by the camera to set exposure when the picture is taken.  \li QString
    \row \li ISOSpeedRatings
        \li Indicates the ISO Speed and ISO Latitude of the camera or input device as specified in ISO 12232. \li qreal
    \row \li ExposureBiasValue
        \li The exposure bias.
        The unit is the APEX (Additive System of Photographic Exposure) setting.  \li qreal
    \row \li DateTimeOriginal \li The date and time when the original image data was generated. \li QDateTime
    \row \li DateTimeDigitized \li The date and time when the image was stored as digital data.  \li QDateTime
    \row \li SubjectDistance \li The distance to the subject, given in meters. \li qreal
    \row \li MeteringMode \li The metering mode.  \li QCameraExposure::MeteringMode
    \row \li LightSource
        \li The kind of light source. \li QString
    \row \li Flash
        \li Status of flash when the image was shot. \li QCameraExposure::FlashMode
    \row \li FocalLength
        \li The actual focal length of the lens, in mm. \li qreal
    \row \li ExposureMode
        \li Indicates the exposure mode set when the image was shot. \li QCameraExposure::ExposureMode
    \row \li WhiteBalance
        \li Indicates the white balance mode set when the image was shot. \li QCameraImageProcessing::WhiteBalanceMode
    \row \li DigitalZoomRatio
        \li Indicates the digital zoom ratio when the image was shot. \li qreal
    \row \li FocalLengthIn35mmFilm
        \li Indicates the equivalent focal length assuming a 35mm film camera, in mm. \li qreal
    \row \li SceneCaptureType
        \li Indicates the type of scene that was shot.
        It can also be used to record the mode in which the image was shot. \li QString
    \row \li GainControl
        \li Indicates the degree of overall image gain adjustment. \li qreal
    \row \li Contrast
        \li Indicates the direction of contrast processing applied by the camera when the image was shot. \li qreal
    \row \li Saturation
        \li Indicates the direction of saturation processing applied by the camera when the image was shot. \li qreal
    \row \li Sharpness
        \li Indicates the direction of sharpness processing applied by the camera when the image was shot. \li qreal
    \row \li DeviceSettingDescription
        \li Exif tag, indicates information on the picture-taking conditions of a particular camera model. \li QString

    \row \li GPSLatitude
        \li Latitude value of the geographical position (decimal degrees).
        A positive latitude indicates the Northern Hemisphere,
        and a negative latitude indicates the Southern Hemisphere. \li double
    \row \li GPSLongitude
        \li Longitude value of the geographical position (decimal degrees).
        A positive longitude indicates the Eastern Hemisphere,
        and a negative longitude indicates the Western Hemisphere. \li double
    \row \li GPSAltitude
        \li The value of altitude in meters above sea level. \li double
    \row \li GPSTimeStamp
        \li Time stamp of GPS data. \li QDateTime
    \row \li GPSSatellites
        \li GPS satellites used for measurements. \li QString
    \row \li GPSStatus
        \li Status of GPS receiver at image creation time. \li QString
    \row \li GPSDOP
        \li Degree of precision for GPS data. \li qreal
    \row \li GPSSpeed
        \li Speed of GPS receiver movement in kilometers per hour. \li qreal
    \row \li GPSTrack
        \li Direction of GPS receiver movement.
        The range of values is [0.0, 360),
        with 0 direction pointing on either true or magnetic north,
        depending on GPSTrackRef. \li qreal
    \row \li GPSTrackRef
        \li Reference for movement direction. \li QChar.
        'T' means true direction and 'M' is magnetic direction.
    \row \li GPSImgDirection
        \li Direction of image when captured. \li qreal
        The range of values is [0.0, 360).
    \row \li GPSImgDirectionRef
        \li Reference for image direction. \li QChar.
        'T' means true direction and 'M' is magnetic direction.
    \row \li GPSMapDatum
        \li Geodetic survey data used by the GPS receiver. \li QString
    \row \li GPSProcessingMethod
        \li The name of the method used for location finding. \li QString
    \row \li GPSAreaInformation
        \li The name of the GPS area. \li QString

    \row \li ThumbnailImage \li An embedded thumbnail image.  \li QImage
    \endtable
*/

QT_END_NAMESPACE
