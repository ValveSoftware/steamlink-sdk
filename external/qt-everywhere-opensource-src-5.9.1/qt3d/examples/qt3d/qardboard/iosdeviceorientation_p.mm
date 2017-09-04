/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "iosdeviceorientation.h"
#import "iosdeviceorientation_p.h"

#define RADIANS_TO_DEGREES(radians) ((radians) * (180.0 / M_PI))
#define DEGREES_TO_RADIANS(angle) ((angle) / 180.0 * M_PI)


@interface QIOSMotionManager : NSObject {
}

+ (CMMotionManager *)sharedManager;
@end

@interface iOSDeviceOrientationP()
{
    BOOL active;
    iOSDeviceOrientation* handler;
}

@property (strong) CMMotionManager *motionManager;
@end



@implementation iOSDeviceOrientationP

#define kUpdateFrequency 20.0


+ (iOSDeviceOrientationP*)instance
{
    static iOSDeviceOrientationP* _myInstance = 0;
    if (0 == _myInstance)
        _myInstance = [[iOSDeviceOrientationP alloc] init];
    return _myInstance;
}

- (id)init
{
    self = [super init];
    if (self) {
        self.motionManager = [QIOSMotionManager sharedManager]; // [[CMMotionManager alloc] init];
        self.motionManager.deviceMotionUpdateInterval = 1. / kUpdateFrequency;

        active = FALSE;
    }
    return self;
}

- (void)setHandler:(iOSDeviceOrientation*)h
{
    handler = h;
}

- (BOOL)isActive
{
    return active;
}

- (void)start
{
    // Motion updates
    [self.motionManager startDeviceMotionUpdatesUsingReferenceFrame:CMAttitudeReferenceFrameXArbitraryCorrectedZVertical
                                                            toQueue:[NSOperationQueue currentQueue]
                                                        withHandler: ^(CMDeviceMotion *motion, NSError *error) {
        //CMAttitude *attitude = motion.attitude;
        //NSLog(@"rotation rate = [%f, %f, %f]", attitude.pitch, attitude.roll, attitude.yaw);
        if (error)
            NSLog(@"%@", [error description]);
        else
            [self performSelectorOnMainThread:@selector(handleDeviceMotion:) withObject:motion waitUntilDone:YES];
    }];
    active = TRUE;
}

- (void)stop
{
    [_motionManager stopDeviceMotionUpdates];
    active = FALSE;
}

- (void)handleDeviceMotion:(CMDeviceMotion*)motion
{
    if (!active)
        return;

    if (motion.magneticField.accuracy == CMMagneticFieldCalibrationAccuracyUncalibrated)
        return;

    // X: A pitch is a rotation around a lateral axis that passes through the device from side to side.
    // Y: A roll is a rotation around a longitudinal axis that passes through the device from its top to bottom.
    // Z: A yaw is a rotation around an axis that runs vertically through the device. It is perpendicular to the
    //    body of the device, with its origin at the center of gravity and directed toward the bottom of the device.

    CMAttitude *attitude = motion.attitude;
    handler->setRoll(90 - RADIANS_TO_DEGREES(attitude.roll));
    handler->setPitch(RADIANS_TO_DEGREES(attitude.pitch));
    handler->setYaw(RADIANS_TO_DEGREES(attitude.yaw));
}

@end
