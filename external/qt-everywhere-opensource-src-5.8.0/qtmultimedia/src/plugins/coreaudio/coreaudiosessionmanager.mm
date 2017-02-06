/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "coreaudiosessionmanager.h"

#import <AVFoundation/AVAudioSession.h>
#import <Foundation/Foundation.h>

#if QT_IOS_DEPLOYMENT_TARGET_BELOW(__IPHONE_6_0)
#include <AudioToolbox/AudioToolbox.h>
#endif

QT_BEGIN_NAMESPACE

@interface CoreAudioSessionObserver : NSObject
{
    CoreAudioSessionManager *m_sessionManager;
    AVAudioSession *m_audioSession;
}

@property (readonly, getter=sessionManager) CoreAudioSessionManager *m_sessionManager;
@property (readonly, getter=audioSession) AVAudioSession *m_audioSession;

-(CoreAudioSessionObserver *)initWithAudioSessionManager:(CoreAudioSessionManager *)sessionManager;

-(BOOL)activateAudio;
-(BOOL)deactivateAudio;

//Notification handlers
-(void)audioSessionInterruption:(NSNotification *)notification;
-(void)audioSessionRouteChange:(NSNotification *)notification;
-(void)audioSessionMediaServicesWereReset:(NSNotification *)notification;

@end //interface CoreAudioSessionObserver

@implementation CoreAudioSessionObserver

@synthesize m_sessionManager, m_audioSession;

-(CoreAudioSessionObserver *)initWithAudioSessionManager:(CoreAudioSessionManager *)sessionManager
{
        if (!(self = [super init]))
            return nil;

        self->m_sessionManager = sessionManager;
        self->m_audioSession = [AVAudioSession sharedInstance];

#if QT_IOS_DEPLOYMENT_TARGET_BELOW(__IPHONE_6_0)
        if (QSysInfo::MacintoshVersion >= QSysInfo::MV_IOS_6_0)
#endif
        {
            //Set up observers
            [[NSNotificationCenter defaultCenter] addObserver:self
                                                     selector:@selector(audioSessionInterruption:)
                                                         name:AVAudioSessionInterruptionNotification
                                                       object:self->m_audioSession];
            [[NSNotificationCenter defaultCenter] addObserver:self
                                                     selector:@selector(audioSessionMediaServicesWereReset:)
                                                         name:AVAudioSessionMediaServicesWereResetNotification
                                                       object:self->m_audioSession];
            [[NSNotificationCenter defaultCenter] addObserver:self
                                                     selector:@selector(audioSessionRouteChange:)
                                                         name:AVAudioSessionRouteChangeNotification
                                                       object:self->m_audioSession];
        }

        return self;
}

-(void)dealloc
{
#ifdef QT_DEBUG_COREAUDIO
    qDebug() << Q_FUNC_INFO;
#endif

#if QT_IOS_DEPLOYMENT_TARGET_BELOW(__IPHONE_6_0)
    if (QSysInfo::MacintoshVersion >= QSysInfo::MV_IOS_6_0)
#endif
    {
        [[NSNotificationCenter defaultCenter] removeObserver:self
                                                        name:AVAudioSessionInterruptionNotification
                                                      object:self->m_audioSession];
        [[NSNotificationCenter defaultCenter] removeObserver:self
                                                        name:AVAudioSessionMediaServicesWereResetNotification
                                                      object:self->m_audioSession];
        [[NSNotificationCenter defaultCenter] removeObserver:self
                                                        name:AVAudioSessionRouteChangeNotification
                                                      object:self->m_audioSession];
    }

    [super dealloc];
}

-(BOOL)activateAudio
{
    NSError *error = nil;
    BOOL success = [self->m_audioSession setActive:YES error:&error];
    if (![self->m_audioSession setActive:YES error:&error]) {
#ifdef QT_DEBUG_COREAUDIO
        qDebug("audio session activation failed: %s", [[error localizedDescription] UTF8String]);
    } else {
        qDebug("audio session activated");
#endif
    }

    return success;
}

-(BOOL)deactivateAudio
{
    NSError *error = nil;
    BOOL success = [m_audioSession setActive:NO error:&error];
#ifdef QT_DEBUG_COREAUDIO
    if (!success) {
        qDebug("%s", [[error localizedDescription] UTF8String]);
    }
#endif
    return success;
}

-(void)audioSessionInterruption:(NSNotification *)notification
{
    NSNumber *type = [[notification userInfo] valueForKey:AVAudioSessionInterruptionTypeKey];
    if ([type intValue] == AVAudioSessionInterruptionTypeBegan) {
#ifdef QT_DEBUG_COREAUDIO
        qDebug("audioSession Interuption begain");
#endif
    } else if ([type intValue] == AVAudioSessionInterruptionTypeEnded) {
#ifdef QT_DEBUG_COREAUDIO
        qDebug("audioSession Interuption ended");
#endif
        NSNumber *option = [[notification userInfo] valueForKey:AVAudioSessionInterruptionOptionKey];
        if ([option intValue] == AVAudioSessionInterruptionOptionShouldResume) {
#ifdef QT_DEBUG_COREAUDIO
            qDebug("audioSession is active and immediately ready to be used.");
#endif
        } else {
            [self activateAudio];
        }
    }
}

-(void)audioSessionMediaServicesWereReset:(NSNotification *)notification
{
    Q_UNUSED(notification)
#ifdef QT_DEBUG_COREAUDIO
    qDebug("audioSession Media Services were reset");
#endif
    //Reactivate audio when this occurs
    [self activateAudio];
}

-(void)audioSessionRouteChange:(NSNotification *)notification
{
    NSNumber *reason = [[notification userInfo] valueForKey:AVAudioSessionRouteChangeReasonKey];
    NSUInteger reasonEnum = [reason intValue];

    if (reasonEnum == AVAudioSessionRouteChangeReasonUnknown) {
#ifdef QT_DEBUG_COREAUDIO
        qDebug("audioSession route changed. reason: unknown");
#endif
    } else if (reasonEnum == AVAudioSessionRouteChangeReasonNewDeviceAvailable) {
#ifdef QT_DEBUG_COREAUDIO
        qDebug("audioSession route changed. reason: new device available");
#endif
    } else if (reasonEnum == AVAudioSessionRouteChangeReasonOldDeviceUnavailable) {
#ifdef QT_DEBUG_COREAUDIO
        qDebug("audioSession route changed. reason: old device unavailable");
#endif
    } else if (reasonEnum == AVAudioSessionRouteChangeReasonCategoryChange) {
#ifdef QT_DEBUG_COREAUDIO
        qDebug("audioSession route changed. reason: category changed");
#endif
    } else if (reasonEnum == AVAudioSessionRouteChangeReasonOverride) {
#ifdef QT_DEBUG_COREAUDIO
        qDebug("audioSession route changed. reason: override");
#endif
    } else if (reasonEnum == AVAudioSessionRouteChangeReasonWakeFromSleep) {
#ifdef QT_DEBUG_COREAUDIO
        qDebug("audioSession route changed. reason: woken from sleep");
#endif
    } else if (reasonEnum == AVAudioSessionRouteChangeReasonNoSuitableRouteForCategory) {
#ifdef QT_DEBUG_COREAUDIO
        qDebug("audioSession route changed. reason: no suitable route for category");
#endif
    }

}

@end //implementation CoreAudioSessionObserver

CoreAudioSessionManager::CoreAudioSessionManager() :
    QObject(0)
{
    m_sessionObserver = [[CoreAudioSessionObserver alloc] initWithAudioSessionManager:this];
    setActive(true);
    // Set default category to Ambient (implies MixWithOthers). This makes sure audio stops playing
    // if the screen is locked or if the Silent switch is toggled.
    setCategory(CoreAudioSessionManager::Ambient, CoreAudioSessionManager::None);
}

CoreAudioSessionManager::~CoreAudioSessionManager()
{
#ifdef QT_DEBUG_COREAUDIO
    qDebug() << Q_FUNC_INFO;
#endif
    [m_sessionObserver release];
}


CoreAudioSessionManager &CoreAudioSessionManager::instance()
{
    static CoreAudioSessionManager instance;
    return instance;
}

bool CoreAudioSessionManager::setActive(bool active)
{
    if (active) {
        return [m_sessionObserver activateAudio];
    } else {
        return [m_sessionObserver deactivateAudio];
    }
}

bool CoreAudioSessionManager::setCategory(CoreAudioSessionManager::AudioSessionCategorys category, CoreAudioSessionManager::AudioSessionCategoryOptions options)
{
    NSString *targetCategory = nil;

    switch (category) {
    case CoreAudioSessionManager::Ambient:
        targetCategory = AVAudioSessionCategoryAmbient;
        break;
    case CoreAudioSessionManager::SoloAmbient:
        targetCategory = AVAudioSessionCategorySoloAmbient;
        break;
    case CoreAudioSessionManager::Playback:
        targetCategory = AVAudioSessionCategoryPlayback;
        break;
    case CoreAudioSessionManager::Record:
        targetCategory = AVAudioSessionCategoryRecord;
        break;
    case CoreAudioSessionManager::PlayAndRecord:
        targetCategory = AVAudioSessionCategoryPlayAndRecord;
        break;
    case CoreAudioSessionManager::AudioProcessing:
#ifndef Q_OS_TVOS
        targetCategory = AVAudioSessionCategoryAudioProcessing;
#endif
        break;
    case CoreAudioSessionManager::MultiRoute:
#if QT_IOS_DEPLOYMENT_TARGET_BELOW(__IPHONE_6_0)
        if (QSysInfo::MacintoshVersion >= QSysInfo::MV_IOS_6_0)
#endif
        targetCategory = AVAudioSessionCategoryMultiRoute;
        break;
    }

    if (targetCategory == nil)
        return false;

#if QT_IOS_DEPLOYMENT_TARGET_BELOW(__IPHONE_6_0)
    if (QSysInfo::MacintoshVersion < QSysInfo::MV_IOS_6_0) {
        return [[m_sessionObserver audioSession] setCategory:targetCategory error:nil];
    } else
#endif
    {
        return [[m_sessionObserver audioSession] setCategory:targetCategory
                                                 withOptions:(AVAudioSessionCategoryOptions)options
                                                       error:nil];
    }
}

bool CoreAudioSessionManager::setMode(CoreAudioSessionManager::AudioSessionModes mode)
{
    NSString *targetMode = nil;
    switch (mode) {
    case CoreAudioSessionManager::Default:
        targetMode = AVAudioSessionModeDefault;
        break;
    case CoreAudioSessionManager::VoiceChat:
        targetMode = AVAudioSessionModeVoiceChat;
        break;
    case CoreAudioSessionManager::GameChat:
        targetMode = AVAudioSessionModeGameChat;
        break;
    case CoreAudioSessionManager::VideoRecording:
        targetMode = AVAudioSessionModeVideoRecording;
        break;
    case CoreAudioSessionManager::Measurement:
        targetMode = AVAudioSessionModeMeasurement;
        break;
    case CoreAudioSessionManager::MoviePlayback:
#if QT_IOS_DEPLOYMENT_TARGET_BELOW(__IPHONE_6_0)
        if (QSysInfo::MacintoshVersion >= QSysInfo::MV_IOS_6_0)
#endif
        targetMode = AVAudioSessionModeMoviePlayback;
        break;
    }

    if (targetMode == nil)
        return false;

    return [[m_sessionObserver audioSession] setMode:targetMode error:nil];

}

CoreAudioSessionManager::AudioSessionCategorys CoreAudioSessionManager::category()
{
    NSString *category = [[m_sessionObserver audioSession] category];
    AudioSessionCategorys localCategory = Ambient;

    if (category == AVAudioSessionCategoryAmbient) {
        localCategory = Ambient;
    } else if (category == AVAudioSessionCategorySoloAmbient) {
        localCategory = SoloAmbient;
    } else if (category == AVAudioSessionCategoryPlayback) {
        localCategory = Playback;
    } else if (category == AVAudioSessionCategoryRecord) {
        localCategory = Record;
    } else if (category == AVAudioSessionCategoryPlayAndRecord) {
        localCategory = PlayAndRecord;
#ifndef Q_OS_TVOS
    } else if (category == AVAudioSessionCategoryAudioProcessing) {
        localCategory = AudioProcessing;
#endif
    } else if (
#if QT_IOS_DEPLOYMENT_TARGET_BELOW(__IPHONE_6_0)
               QSysInfo::MacintoshVersion >= QSysInfo::MV_IOS_6_0 &&
#endif
               category == AVAudioSessionCategoryMultiRoute) {
        localCategory = MultiRoute;
    }

    return localCategory;
}

CoreAudioSessionManager::AudioSessionModes CoreAudioSessionManager::mode()
{
    NSString *mode = [[m_sessionObserver audioSession] mode];
    AudioSessionModes localMode = Default;

    if (mode == AVAudioSessionModeDefault) {
        localMode = Default;
    } else if (mode == AVAudioSessionModeVoiceChat) {
        localMode = VoiceChat;
    } else if (mode == AVAudioSessionModeGameChat) {
        localMode = GameChat;
    } else if (mode == AVAudioSessionModeVideoRecording) {
        localMode = VideoRecording;
    } else if (mode == AVAudioSessionModeMeasurement) {
        localMode = Measurement;
    } else if (
#if QT_IOS_DEPLOYMENT_TARGET_BELOW(__IPHONE_6_0)
               QSysInfo::MacintoshVersion >= QSysInfo::MV_IOS_6_0 &&
#endif
               mode == AVAudioSessionModeMoviePlayback) {
        localMode = MoviePlayback;
    }

    return localMode;
}

QList<QByteArray> CoreAudioSessionManager::inputDevices()
{
    //TODO: Add support for USB input devices
    //Right now the default behavior on iOS is to have only one input route
    //at a time.
    QList<QByteArray> inputDevices;
    inputDevices << "default";
    return inputDevices;
}

QList<QByteArray> CoreAudioSessionManager::outputDevices()
{
    //TODO: Add support for USB output devices
    //Right now the default behavior on iOS is to have only one output route
    //at a time.
    QList<QByteArray> outputDevices;
    outputDevices << "default";
    return outputDevices;
}

float CoreAudioSessionManager::currentIOBufferDuration()
{
#if QT_IOS_DEPLOYMENT_TARGET_BELOW(__IPHONE_6_0)
    if (QSysInfo::MacintoshVersion < QSysInfo::MV_IOS_6_0) {
        Float32 duration;
        UInt32 size = sizeof(duration);
        AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareIOBufferDuration, &size, &duration);
        return duration;
    } else
#endif
    {
        return [[m_sessionObserver audioSession] IOBufferDuration];
    }
}

float CoreAudioSessionManager::preferredSampleRate()
{
#if QT_IOS_DEPLOYMENT_TARGET_BELOW(__IPHONE_6_0)
    if (QSysInfo::MacintoshVersion < QSysInfo::MV_IOS_6_0) {
        Float64 sampleRate;
        UInt32 size = sizeof(sampleRate);
        AudioSessionGetProperty(kAudioSessionProperty_PreferredHardwareSampleRate, &size, &sampleRate);
        return sampleRate;
    } else
#endif
    {
        return [[m_sessionObserver audioSession] preferredSampleRate];
    }
}

#ifdef QT_DEBUG_COREAUDIO
QDebug operator<<(QDebug dbg, CoreAudioSessionManager::AudioSessionCategorys category)
{
    QDebug output = dbg.nospace();
    switch (category) {
    case CoreAudioSessionManager::Ambient:
        output << "AudioSessionCategoryAmbient";
        break;
    case CoreAudioSessionManager::SoloAmbient:
        output << "AudioSessionCategorySoloAmbient";
        break;
    case CoreAudioSessionManager::Playback:
        output << "AudioSessionCategoryPlayback";
        break;
    case CoreAudioSessionManager::Record:
        output << "AudioSessionCategoryRecord";
        break;
    case CoreAudioSessionManager::PlayAndRecord:
        output << "AudioSessionCategoryPlayAndRecord";
        break;
    case CoreAudioSessionManager::AudioProcessing:
        output << "AudioSessionCategoryAudioProcessing";
        break;
    case CoreAudioSessionManager::MultiRoute:
        output << "AudioSessionCategoryMultiRoute";
        break;
    }
    return output;
}

QDebug operator<<(QDebug dbg, CoreAudioSessionManager::AudioSessionCategoryOptions option)
{
    QDebug output = dbg.nospace();
    switch (option) {
    case CoreAudioSessionManager::None:
        output << "AudioSessionCategoryOptionNone";
        break;
    case CoreAudioSessionManager::MixWithOthers:
        output << "AudioSessionCategoryOptionMixWithOthers";
        break;
    case CoreAudioSessionManager::DuckOthers:
        output << "AudioSessionCategoryOptionDuckOthers";
        break;
    case CoreAudioSessionManager::AllowBluetooth:
        output << "AudioSessionCategoryOptionAllowBluetooth";
        break;
    case CoreAudioSessionManager::DefaultToSpeaker:
        output << "AudioSessionCategoryOptionDefaultToSpeaker";
        break;
    }
    return output;
}

QDebug operator<<(QDebug dbg, CoreAudioSessionManager::AudioSessionModes mode)
{
    QDebug output = dbg.nospace();
    switch (mode) {
    case CoreAudioSessionManager::Default:
        output << "AudioSessionModeDefault";
        break;
    case CoreAudioSessionManager::VoiceChat:
        output << "AudioSessionModeVoiceChat";
        break;
    case CoreAudioSessionManager::GameChat:
        output << "AudioSessionModeGameChat";
        break;
    case CoreAudioSessionManager::VideoRecording:
        output << "AudioSessionModeVideoRecording";
        break;
    case CoreAudioSessionManager::Measurement:
        output << "AudioSessionModeMeasurement";
        break;
    case CoreAudioSessionManager::MoviePlayback:
        output << "AudioSessionModeMoviePlayback";
        break;
    }
    return output;
}
#endif

QT_END_NAMESPACE

#include "moc_coreaudiosessionmanager.cpp"
