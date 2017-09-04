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

#ifndef QDECLARATIVEMEDIAMETADATA_P_H
#define QDECLARATIVEMEDIAMETADATA_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtQml/qqml.h>
#include <QtMultimedia/qmediametadata.h>
#include <QtMultimedia/qmediaservice.h>
#include <QtMultimedia/qmetadatawritercontrol.h>
#include "qmediaobject.h"

QT_BEGIN_NAMESPACE

class QDeclarativeMediaMetaData : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant title READ title WRITE setTitle NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant subTitle READ subTitle WRITE setSubTitle NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant author READ author WRITE setAuthor NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant comment READ comment WRITE setComment NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant description READ description WRITE setDescription NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant category READ category WRITE setCategory NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant genre READ genre WRITE setGenre NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant year READ year WRITE setYear NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant date READ date WRITE setDate NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant userRating READ userRating WRITE setUserRating NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant keywords READ keywords WRITE setKeywords NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant language READ language WRITE setLanguage NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant publisher READ publisher WRITE setPublisher NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant copyright READ copyright WRITE setCopyright NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant parentalRating READ parentalRating WRITE setParentalRating NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant ratingOrganization READ ratingOrganization WRITE setRatingOrganization NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant size READ size WRITE setSize NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant mediaType READ mediaType WRITE setMediaType NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant duration READ duration WRITE setDuration NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant audioBitRate READ audioBitRate WRITE setAudioBitRate NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant audioCodec READ audioCodec WRITE setAudioCodec NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant averageLevel READ averageLevel WRITE setAverageLevel NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant channelCount READ channelCount WRITE setChannelCount NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant peakValue READ peakValue WRITE setPeakValue NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant sampleRate READ sampleRate WRITE setSampleRate NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant albumTitle READ albumTitle WRITE setAlbumTitle NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant albumArtist READ albumArtist WRITE setAlbumArtist NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant contributingArtist READ contributingArtist WRITE setContributingArtist NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant composer READ composer WRITE setComposer NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant conductor READ conductor WRITE setConductor NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant lyrics READ lyrics WRITE setLyrics NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant mood READ mood WRITE setMood NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant trackNumber READ trackNumber WRITE setTrackNumber NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant trackCount READ trackCount WRITE setTrackCount NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant coverArtUrlSmall READ coverArtUrlSmall WRITE setCoverArtUrlSmall NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant coverArtUrlLarge READ coverArtUrlLarge WRITE setCoverArtUrlLarge NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant resolution READ resolution WRITE setResolution NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant pixelAspectRatio READ pixelAspectRatio WRITE setPixelAspectRatio NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant videoFrameRate READ videoFrameRate WRITE setVideoFrameRate NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant videoBitRate READ videoBitRate WRITE setVideoBitRate NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant videoCodec READ videoCodec WRITE setVideoCodec NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant posterUrl READ posterUrl WRITE setPosterUrl NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant chapterNumber READ chapterNumber WRITE setChapterNumber NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant director READ director WRITE setDirector NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant leadPerformer READ leadPerformer WRITE setLeadPerformer NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant writer READ writer WRITE setWriter NOTIFY metaDataChanged)

    Q_PROPERTY(QVariant cameraManufacturer READ cameraManufacturer WRITE setCameraManufacturer NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant cameraModel READ cameraModel WRITE setCameraModel NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant event READ event WRITE setEvent NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant subject READ subject WRITE setSubject NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant orientation READ orientation WRITE setOrientation NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant exposureTime READ exposureTime WRITE setExposureTime NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant fNumber READ fNumber WRITE setFNumber NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant exposureProgram READ exposureProgram WRITE setExposureProgram NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant isoSpeedRatings READ isoSpeedRatings WRITE setISOSpeedRatings NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant exposureBiasValue READ exposureBiasValue WRITE setExposureBiasValue NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant dateTimeOriginal READ dateTimeOriginal WRITE setDateTimeOriginal NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant dateTimeDigitized READ dateTimeDigitized WRITE setDateTimeDigitized NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant subjectDistance READ subjectDistance WRITE setSubjectDistance NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant meteringMode READ meteringMode WRITE setMeteringMode NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant lightSource READ lightSource WRITE setLightSource NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant flash READ flash WRITE setFlash NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant focalLength READ focalLength WRITE setFocalLength NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant exposureMode READ exposureMode WRITE setExposureMode NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant whiteBalance READ whiteBalance WRITE setWhiteBalance NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant digitalZoomRatio READ digitalZoomRatio WRITE setDigitalZoomRatio NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant focalLengthIn35mmFilm READ focalLengthIn35mmFilm WRITE setFocalLengthIn35mmFilm NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant sceneCaptureType READ sceneCaptureType WRITE setSceneCaptureType NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gainControl READ gainControl WRITE setGainControl NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant contrast READ contrast WRITE setContrast NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant saturation READ saturation WRITE setSaturation NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant sharpness READ sharpness WRITE setSharpness NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant deviceSettingDescription READ deviceSettingDescription WRITE setDeviceSettingDescription NOTIFY metaDataChanged)

    Q_PROPERTY(QVariant gpsLatitude READ gpsLatitude WRITE setGPSLatitude NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsLongitude READ gpsLongitude WRITE setGPSLongitude NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsAltitude READ gpsAltitude WRITE setGPSAltitude NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsTimeStamp READ gpsTimeStamp WRITE setGPSTimeStamp NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsSatellites READ gpsSatellites WRITE setGPSSatellites NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsStatus READ gpsStatus WRITE setGPSStatus NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsDOP READ gpsDOP WRITE setGPSDOP NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsSpeed READ gpsSpeed WRITE setGPSSpeed NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsTrack READ gpsTrack WRITE setGPSTrack NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsTrackRef READ gpsTrackRef WRITE setGPSTrackRef NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsImgDirection READ gpsImgDirection WRITE setGPSImgDirection NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsImgDirectionRef READ gpsImgDirectionRef WRITE setGPSImgDirectionRef NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsMapDatum READ gpsMapDatum WRITE setGPSMapDatum NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsProcessingMethod READ gpsProcessingMethod WRITE setGPSProcessingMethod NOTIFY metaDataChanged)
    Q_PROPERTY(QVariant gpsAreaInformation READ gpsAreaInformation WRITE setGPSAreaInformation NOTIFY metaDataChanged)

public:
    QDeclarativeMediaMetaData(QMediaObject *player, QObject *parent = 0)
        : QObject(parent)
        , m_mediaObject(player)
        , m_writerControl(0)
        , m_requestedWriterControl(false)
    {
    }

    ~QDeclarativeMediaMetaData()
    {
        if (m_writerControl) {
            if (QMediaService *service = m_mediaObject->service())
                service->releaseControl(m_writerControl);
        }
    }

    QVariant title() const { return m_mediaObject->metaData(QMediaMetaData::Title); }
    void setTitle(const QVariant &title) { setMetaData(QMediaMetaData::Title, title); }
    QVariant subTitle() const { return m_mediaObject->metaData(QMediaMetaData::SubTitle); }
    void setSubTitle(const QVariant &title) {
        setMetaData(QMediaMetaData::SubTitle, title); }
    QVariant author() const { return m_mediaObject->metaData(QMediaMetaData::Author); }
    void setAuthor(const QVariant &author) { setMetaData(QMediaMetaData::Author, author); }
    QVariant comment() const { return m_mediaObject->metaData(QMediaMetaData::Comment); }
    void setComment(const QVariant &comment) { setMetaData(QMediaMetaData::Comment, comment); }
    QVariant description() const { return m_mediaObject->metaData(QMediaMetaData::Description); }
    void setDescription(const QVariant &description) {
        setMetaData(QMediaMetaData::Description, description); }
    QVariant category() const { return m_mediaObject->metaData(QMediaMetaData::Category); }
    void setCategory(const QVariant &category) { setMetaData(QMediaMetaData::Category, category); }
    QVariant genre() const { return m_mediaObject->metaData(QMediaMetaData::Genre); }
    void setGenre(const QVariant &genre) { setMetaData(QMediaMetaData::Genre, genre); }
    QVariant year() const { return m_mediaObject->metaData(QMediaMetaData::Year); }
    void setYear(const QVariant &year) { setMetaData(QMediaMetaData::Year, year); }
    QVariant date() const { return m_mediaObject->metaData(QMediaMetaData::Date); }
    void setDate(const QVariant &date) { setMetaData(QMediaMetaData::Date, date); }
    QVariant userRating() const { return m_mediaObject->metaData(QMediaMetaData::UserRating); }
    void setUserRating(const QVariant &rating) { setMetaData(QMediaMetaData::UserRating, rating); }
    QVariant keywords() const { return m_mediaObject->metaData(QMediaMetaData::Keywords); }
    void setKeywords(const QVariant &keywords) { setMetaData(QMediaMetaData::Keywords, keywords); }
    QVariant language() const { return m_mediaObject->metaData(QMediaMetaData::Language); }
    void setLanguage(const QVariant &language) { setMetaData(QMediaMetaData::Language, language); }
    QVariant publisher() const { return m_mediaObject->metaData(QMediaMetaData::Publisher); }
    void setPublisher(const QVariant &publisher) {
        setMetaData(QMediaMetaData::Publisher, publisher); }
    QVariant copyright() const { return m_mediaObject->metaData(QMediaMetaData::Copyright); }
    void setCopyright(const QVariant &copyright) {
        setMetaData(QMediaMetaData::Copyright, copyright); }
    QVariant parentalRating() const { return m_mediaObject->metaData(QMediaMetaData::ParentalRating); }
    void setParentalRating(const QVariant &rating) {
        setMetaData(QMediaMetaData::ParentalRating, rating); }
    QVariant ratingOrganization() const {
        return m_mediaObject->metaData(QMediaMetaData::RatingOrganization); }
    void setRatingOrganization(const QVariant &organization) {
        setMetaData(QMediaMetaData::RatingOrganization, organization); }
    QVariant size() const { return m_mediaObject->metaData(QMediaMetaData::Size); }
    void setSize(const QVariant &size) { setMetaData(QMediaMetaData::Size, size); }
    QVariant mediaType() const { return m_mediaObject->metaData(QMediaMetaData::MediaType); }
    void setMediaType(const QVariant &type) { setMetaData(QMediaMetaData::MediaType, type); }
    QVariant duration() const { return m_mediaObject->metaData(QMediaMetaData::Duration); }
    void setDuration(const QVariant &duration) { setMetaData(QMediaMetaData::Duration, duration); }
    QVariant audioBitRate() const { return m_mediaObject->metaData(QMediaMetaData::AudioBitRate); }
    void setAudioBitRate(const QVariant &rate) { setMetaData(QMediaMetaData::AudioBitRate, rate); }
    QVariant audioCodec() const { return m_mediaObject->metaData(QMediaMetaData::AudioCodec); }
    void setAudioCodec(const QVariant &codec) { setMetaData(QMediaMetaData::AudioCodec, codec); }
    QVariant averageLevel() const { return m_mediaObject->metaData(QMediaMetaData::AverageLevel); }
    void setAverageLevel(const QVariant &level) {
        setMetaData(QMediaMetaData::AverageLevel, level); }
    QVariant channelCount() const { return m_mediaObject->metaData(QMediaMetaData::ChannelCount); }
    void setChannelCount(const QVariant &count) {
        setMetaData(QMediaMetaData::ChannelCount, count); }
    QVariant peakValue() const { return m_mediaObject->metaData(QMediaMetaData::PeakValue); }
    void setPeakValue(const QVariant &value) { setMetaData(QMediaMetaData::PeakValue, value); }
    QVariant sampleRate() const { return m_mediaObject->metaData(QMediaMetaData::SampleRate); }
    void setSampleRate(const QVariant &rate) { setMetaData(QMediaMetaData::SampleRate, rate); }
    QVariant albumTitle() const { return m_mediaObject->metaData(QMediaMetaData::AlbumTitle); }
    void setAlbumTitle(const QVariant &title) { setMetaData(QMediaMetaData::AlbumTitle, title); }
    QVariant albumArtist() const { return m_mediaObject->metaData(QMediaMetaData::AlbumArtist); }
    void setAlbumArtist(const QVariant &artist) {
        setMetaData(QMediaMetaData::AlbumArtist, artist); }
    QVariant contributingArtist() const {
        return m_mediaObject->metaData(QMediaMetaData::ContributingArtist); }
    void setContributingArtist(const QVariant &artist) {
        setMetaData(QMediaMetaData::ContributingArtist, artist); }
    QVariant composer() const { return m_mediaObject->metaData(QMediaMetaData::Composer); }
    void setComposer(const QVariant &composer) { setMetaData(QMediaMetaData::Composer, composer); }
    QVariant conductor() const { return m_mediaObject->metaData(QMediaMetaData::Conductor); }
    void setConductor(const QVariant &conductor) {
        setMetaData(QMediaMetaData::Conductor, conductor); }
    QVariant lyrics() const { return m_mediaObject->metaData(QMediaMetaData::Lyrics); }
    void setLyrics(const QVariant &lyrics) { setMetaData(QMediaMetaData::Lyrics, lyrics); }
    QVariant mood() const { return m_mediaObject->metaData(QMediaMetaData::Mood); }
    void setMood(const QVariant &mood) { setMetaData(QMediaMetaData::Mood, mood); }
    QVariant trackNumber() const { return m_mediaObject->metaData(QMediaMetaData::TrackNumber); }
    void setTrackNumber(const QVariant &track) { setMetaData(QMediaMetaData::TrackNumber, track); }
    QVariant trackCount() const { return m_mediaObject->metaData(QMediaMetaData::TrackCount); }
    void setTrackCount(const QVariant &count) { setMetaData(QMediaMetaData::TrackCount, count); }
    QVariant coverArtUrlSmall() const {
        return m_mediaObject->metaData(QMediaMetaData::CoverArtUrlSmall); }
    void setCoverArtUrlSmall(const QVariant &url) {
        setMetaData(QMediaMetaData::CoverArtUrlSmall, url); }
    QVariant coverArtUrlLarge() const {
        return m_mediaObject->metaData(QMediaMetaData::CoverArtUrlLarge); }
    void setCoverArtUrlLarge(const QVariant &url) {
        setMetaData(QMediaMetaData::CoverArtUrlLarge, url); }
    QVariant resolution() const { return m_mediaObject->metaData(QMediaMetaData::Resolution); }
    void setResolution(const QVariant &resolution) {
        setMetaData(QMediaMetaData::Resolution, resolution); }
    QVariant pixelAspectRatio() const {
        return m_mediaObject->metaData(QMediaMetaData::PixelAspectRatio); }
    void setPixelAspectRatio(const QVariant &ratio) {
        setMetaData(QMediaMetaData::PixelAspectRatio, ratio); }
    QVariant videoFrameRate() const { return m_mediaObject->metaData(QMediaMetaData::VideoFrameRate); }
    void setVideoFrameRate(const QVariant &rate) {
        setMetaData(QMediaMetaData::VideoFrameRate, rate); }
    QVariant videoBitRate() const { return m_mediaObject->metaData(QMediaMetaData::VideoBitRate); }
    void setVideoBitRate(const QVariant &rate) {
        setMetaData(QMediaMetaData::VideoBitRate, rate); }
    QVariant videoCodec() const { return m_mediaObject->metaData(QMediaMetaData::VideoCodec); }
    void setVideoCodec(const QVariant &codec) {
        setMetaData(QMediaMetaData::VideoCodec, codec); }
    QVariant posterUrl() const { return m_mediaObject->metaData(QMediaMetaData::PosterUrl); }
    void setPosterUrl(const QVariant &url) {
        setMetaData(QMediaMetaData::PosterUrl, url); }
    QVariant chapterNumber() const { return m_mediaObject->metaData(QMediaMetaData::ChapterNumber); }
    void setChapterNumber(const QVariant &chapter) {
        setMetaData(QMediaMetaData::ChapterNumber, chapter); }
    QVariant director() const { return m_mediaObject->metaData(QMediaMetaData::Director); }
    void setDirector(const QVariant &director) { setMetaData(QMediaMetaData::Director, director); }
    QVariant leadPerformer() const { return m_mediaObject->metaData(QMediaMetaData::LeadPerformer); }
    void setLeadPerformer(const QVariant &performer) {
        setMetaData(QMediaMetaData::LeadPerformer, performer); }
    QVariant writer() const { return m_mediaObject->metaData(QMediaMetaData::Writer); }
    void setWriter(const QVariant &writer) { setMetaData(QMediaMetaData::Writer, writer); }

    QVariant cameraManufacturer() const {
        return m_mediaObject->metaData(QMediaMetaData::CameraManufacturer); }
    void setCameraManufacturer(const QVariant &manufacturer) {
        setMetaData(QMediaMetaData::CameraManufacturer, manufacturer); }
    QVariant cameraModel() const { return m_mediaObject->metaData(QMediaMetaData::CameraModel); }
    void setCameraModel(const QVariant &model) { setMetaData(QMediaMetaData::CameraModel, model); }

QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Woverloaded-virtual")
QT_WARNING_DISABLE_CLANG("-Woverloaded-virtual")
    QVariant event() const { return m_mediaObject->metaData(QMediaMetaData::Event); }
QT_WARNING_POP

    void setEvent(const QVariant &event) { setMetaData(QMediaMetaData::Event, event); }
    QVariant subject() const { return m_mediaObject->metaData(QMediaMetaData::Subject); }
    void setSubject(const QVariant &subject) { setMetaData(QMediaMetaData::Subject, subject); }
    QVariant orientation() const { return m_mediaObject->metaData(QMediaMetaData::Orientation); }
    void setOrientation(const QVariant &orientation) {
        setMetaData(QMediaMetaData::Orientation, orientation); }
    QVariant exposureTime() const { return m_mediaObject->metaData(QMediaMetaData::ExposureTime); }
    void setExposureTime(const QVariant &time) { setMetaData(QMediaMetaData::ExposureTime, time); }
    QVariant fNumber() const { return m_mediaObject->metaData(QMediaMetaData::FNumber); }
    void setFNumber(const QVariant &number) { setMetaData(QMediaMetaData::FNumber, number); }
    QVariant exposureProgram() const {
        return m_mediaObject->metaData(QMediaMetaData::ExposureProgram); }
    void setExposureProgram(const QVariant &program) {
        setMetaData(QMediaMetaData::ExposureProgram, program); }
    QVariant isoSpeedRatings() const {
        return m_mediaObject->metaData(QMediaMetaData::ISOSpeedRatings); }
    void setISOSpeedRatings(const QVariant &ratings) {
        setMetaData(QMediaMetaData::ISOSpeedRatings, ratings); }
    QVariant exposureBiasValue() const {
        return m_mediaObject->metaData(QMediaMetaData::ExposureBiasValue); }
    void setExposureBiasValue(const QVariant &bias) {
        setMetaData(QMediaMetaData::ExposureBiasValue, bias); }
    QVariant dateTimeOriginal() const {
        return m_mediaObject->metaData(QMediaMetaData::DateTimeOriginal); }
    void setDateTimeOriginal(const QVariant &dateTime) {
        setMetaData(QMediaMetaData::DateTimeOriginal, dateTime); }
    QVariant dateTimeDigitized() const {
        return m_mediaObject->metaData(QMediaMetaData::DateTimeDigitized); }
    void setDateTimeDigitized(const QVariant &dateTime) {
        setMetaData(QMediaMetaData::DateTimeDigitized, dateTime); }
    QVariant subjectDistance() const {
        return m_mediaObject->metaData(QMediaMetaData::SubjectDistance); }
    void setSubjectDistance(const QVariant &distance) {
        setMetaData(QMediaMetaData::SubjectDistance, distance); }
    QVariant meteringMode() const { return m_mediaObject->metaData(QMediaMetaData::MeteringMode); }
    void setMeteringMode(const QVariant &mode) { setMetaData(QMediaMetaData::MeteringMode, mode); }
    QVariant lightSource() const { return m_mediaObject->metaData(QMediaMetaData::LightSource); }
    void setLightSource(const QVariant &source) {
        setMetaData(QMediaMetaData::LightSource, source); }
    QVariant flash() const { return m_mediaObject->metaData(QMediaMetaData::Flash); }
    void setFlash(const QVariant &flash) { setMetaData(QMediaMetaData::Flash, flash); }
    QVariant focalLength() const { return m_mediaObject->metaData(QMediaMetaData::FocalLength); }
    void setFocalLength(const QVariant &length) {
        setMetaData(QMediaMetaData::FocalLength, length); }
    QVariant exposureMode() const { return m_mediaObject->metaData(QMediaMetaData::ExposureMode); }
    void setExposureMode(const QVariant &mode) {
        setMetaData(QMediaMetaData::ExposureMode, mode); }
    QVariant whiteBalance() const { return m_mediaObject->metaData(QMediaMetaData::WhiteBalance); }
    void setWhiteBalance(const QVariant &balance) {
        setMetaData(QMediaMetaData::WhiteBalance, balance); }
    QVariant digitalZoomRatio() const {
        return m_mediaObject->metaData(QMediaMetaData::DigitalZoomRatio); }
    void setDigitalZoomRatio(const QVariant &ratio) {
        setMetaData(QMediaMetaData::DigitalZoomRatio, ratio); }
    QVariant focalLengthIn35mmFilm() const {
        return m_mediaObject->metaData(QMediaMetaData::FocalLengthIn35mmFilm); }
    void setFocalLengthIn35mmFilm(const QVariant &length) {
        setMetaData(QMediaMetaData::FocalLengthIn35mmFilm, length); }
    QVariant sceneCaptureType() const {
        return m_mediaObject->metaData(QMediaMetaData::SceneCaptureType); }
    void setSceneCaptureType(const QVariant &type) {
        setMetaData(QMediaMetaData::SceneCaptureType, type); }
    QVariant gainControl() const { return m_mediaObject->metaData(QMediaMetaData::GainControl); }
    void setGainControl(const QVariant &gain) { setMetaData(QMediaMetaData::GainControl, gain); }
    QVariant contrast() const { return m_mediaObject->metaData(QMediaMetaData::Contrast); }
    void setContrast(const QVariant &contrast) { setMetaData(QMediaMetaData::Contrast, contrast); }
    QVariant saturation() const { return m_mediaObject->metaData(QMediaMetaData::Saturation); }
    void setSaturation(const QVariant &saturation) {
        setMetaData(QMediaMetaData::Saturation, saturation); }
    QVariant sharpness() const { return m_mediaObject->metaData(QMediaMetaData::Sharpness); }
    void setSharpness(const QVariant &sharpness) {
        setMetaData(QMediaMetaData::Sharpness, sharpness); }
    QVariant deviceSettingDescription() const {
        return m_mediaObject->metaData(QMediaMetaData::DeviceSettingDescription); }
    void setDeviceSettingDescription(const QVariant &description) {
        setMetaData(QMediaMetaData::DeviceSettingDescription, description); }

    QVariant gpsLatitude() const { return m_mediaObject->metaData(QMediaMetaData::GPSLatitude); }
    void setGPSLatitude(const QVariant &latitude) {
        setMetaData(QMediaMetaData::GPSLatitude, latitude); }
    QVariant gpsLongitude() const { return m_mediaObject->metaData(QMediaMetaData::GPSLongitude); }
    void setGPSLongitude(const QVariant &longitude) {
        setMetaData(QMediaMetaData::GPSLongitude, longitude); }
    QVariant gpsAltitude() const { return m_mediaObject->metaData(QMediaMetaData::GPSAltitude); }
    void setGPSAltitude(const QVariant &altitude) {
        setMetaData(QMediaMetaData::GPSAltitude, altitude); }
    QVariant gpsTimeStamp() const { return m_mediaObject->metaData(QMediaMetaData::GPSTimeStamp); }
    void setGPSTimeStamp(const QVariant &timestamp) {
        setMetaData(QMediaMetaData::GPSTimeStamp, timestamp); }
    QVariant gpsSatellites() const {
        return m_mediaObject->metaData(QMediaMetaData::GPSSatellites); }
    void setGPSSatellites(const QVariant &satellites) {
        setMetaData(QMediaMetaData::GPSSatellites, satellites); }
    QVariant gpsStatus() const { return m_mediaObject->metaData(QMediaMetaData::GPSStatus); }
    void setGPSStatus(const QVariant &status) { setMetaData(QMediaMetaData::GPSStatus, status); }
    QVariant gpsDOP() const { return m_mediaObject->metaData(QMediaMetaData::GPSDOP); }
    void setGPSDOP(const QVariant &dop) { setMetaData(QMediaMetaData::GPSDOP, dop); }
    QVariant gpsSpeed() const { return m_mediaObject->metaData(QMediaMetaData::GPSSpeed); }
    void setGPSSpeed(const QVariant &speed) { setMetaData(QMediaMetaData::GPSSpeed, speed); }
    QVariant gpsTrack() const { return m_mediaObject->metaData(QMediaMetaData::GPSTrack); }
    void setGPSTrack(const QVariant &track) { setMetaData(QMediaMetaData::GPSTrack, track); }
    QVariant gpsTrackRef() const { return m_mediaObject->metaData(QMediaMetaData::GPSTrackRef); }
    void setGPSTrackRef(const QVariant &ref) { setMetaData(QMediaMetaData::GPSTrackRef, ref); }
    QVariant gpsImgDirection() const {
        return m_mediaObject->metaData(QMediaMetaData::GPSImgDirection); }
    void setGPSImgDirection(const QVariant &direction) {
        setMetaData(QMediaMetaData::GPSImgDirection, direction); }
    QVariant gpsImgDirectionRef() const {
        return m_mediaObject->metaData(QMediaMetaData::GPSImgDirectionRef); }
    void setGPSImgDirectionRef(const QVariant &ref) {
        setMetaData(QMediaMetaData::GPSImgDirectionRef, ref); }
    QVariant gpsMapDatum() const { return m_mediaObject->metaData(QMediaMetaData::GPSMapDatum); }
    void setGPSMapDatum(const QVariant &datum) {
        setMetaData(QMediaMetaData::GPSMapDatum, datum); }
    QVariant gpsProcessingMethod() const {
        return m_mediaObject->metaData(QMediaMetaData::GPSProcessingMethod); }
    void setGPSProcessingMethod(const QVariant &method) {
        setMetaData(QMediaMetaData::GPSProcessingMethod, method); }
    QVariant gpsAreaInformation() const {
        return m_mediaObject->metaData(QMediaMetaData::GPSAreaInformation); }
    void setGPSAreaInformation(const QVariant &information) {
        setMetaData(QMediaMetaData::GPSAreaInformation, information); }

Q_SIGNALS:
    void metaDataChanged();

private:
    void setMetaData(const QString &key, const QVariant &value)
    {
        if (!m_requestedWriterControl) {
            m_requestedWriterControl = true;
            if (QMediaService *service = m_mediaObject->service())
                m_writerControl = service->requestControl<QMetaDataWriterControl *>();
        }
        if (m_writerControl)
            m_writerControl->setMetaData(key, value);
    }

    QMediaObject *m_mediaObject;
    QMetaDataWriterControl *m_writerControl;
    bool m_requestedWriterControl;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QT_PREPEND_NAMESPACE(QDeclarativeMediaMetaData))

#endif
