/****************************************************************************
 **
 ** Copyright (C) 2016 The Qt Company Ltd.
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

#import "InterfaceController.h"

@interface InterfaceController() <WCSessionDelegate>

@end

@implementation InterfaceController

- (id)init {
    if ((self = [super init])) {
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSString *host = [defaults stringForKey:@"host"];
        int port = [defaults integerForKey:@"port"];

        if (host == nil) {
            self.host = @"127.0.0.1";
        } else {
            self.host = host;
        }

        if (port == 0) {
            self.port = [NSNumber numberWithInt:8080];
        } else {
            self.port = [NSNumber numberWithInt:port];
        }

        self.planets = @[@"Sun",
                         @"Mercury",
                         @"Venus",
                         @"Earth",
                         @"Mars",
                         @"Jupiter",
                         @"Saturn",
                         @"Uranus",
                         @"Neptune",
                         @"Solar System"];

        self.minimumValue = 1.0f;
        self.maximumValue = 10.0f;
    }

    return self;
}

- (void)awakeWithContext:(id)context {
    [super awakeWithContext:context];

    int planetsCount = [self.planets count];

    NSMutableArray *pickerItems = [[NSMutableArray alloc] init];

    for (int i = 0; i < planetsCount; i++) {
        WKPickerItem *item = [WKPickerItem alloc];
        item.title = self.planets[i];
        [pickerItems addObject:item];
    }

    [self.planetPicker setItems:pickerItems];

    [self.hostLabel setText:[NSString stringWithFormat:@"%@:%@", self.host, [NSString stringWithFormat:@"%d", [self.port intValue]]]];
}

- (void)willActivate {
    [super willActivate];

    if ([WCSession isSupported]) {
        WCSession *session = [WCSession defaultSession];
        session.delegate = self;
        [session activateSession];
    }
}

- (IBAction)selectedPlanetChanged:(NSInteger)value {
    NSString *command = [@[@"selectPlanet", self.planets[value]] componentsJoinedByString:@"/"];

    [self sendCommand:command];
}

- (IBAction)rotationSpeedChanged:(float)value {
    NSString* formattedValue = [NSString stringWithFormat:@"%.02f", (value - self.minimumValue) /
                                (self.maximumValue - self.minimumValue)];
    NSString *command = [@[@"setRotationSpeed", formattedValue] componentsJoinedByString:@"/"];

    [self sendCommand:command];
}

- (IBAction)viewingDistanceChanged:(float)value {
    NSString* formattedValue = [NSString stringWithFormat:@"%.02f", (value - self.minimumValue) /
                                (self.maximumValue - self.minimumValue)];
    NSString *command = [@[@"setViewingDistance", formattedValue] componentsJoinedByString:@"/"];

    [self sendCommand:command];
}

- (IBAction)planetSizeChanged:(float)value {
    NSString* formattedValue = [NSString stringWithFormat:@"%.02f", (value - self.minimumValue) /
                                (self.maximumValue - self.minimumValue)];
    NSString *command = [@[@"setPlanetSize", formattedValue] componentsJoinedByString:@"/"];

    [self sendCommand:command];
}


- (void)sendCommand:(NSString *)command {
    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] init];

    NSURLComponents *urlComponents = [[NSURLComponents alloc] init];
    urlComponents.scheme = @"http";
    urlComponents.host = self.host;
    urlComponents.port = self.port;
    urlComponents.path = [NSString stringWithFormat:@"/%@", command];

    [request setURL:urlComponents.URL];
    [request setHTTPMethod:@"GET"];

    NSURLSession *session = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration]];
    [[session dataTaskWithRequest:request completionHandler:^(NSData *data, NSURLResponse *response,
                                                              NSError *error) {}] resume];
}

- (void)session:(nonnull WCSession *)session didReceiveMessage:(nonnull NSDictionary *)message replyHandler:(nonnull void (^)(NSDictionary<NSString *,id> *))replyHandler {
    NSString *host = [message objectForKey:@"host"];
    NSString *port = [message objectForKey:@"port"];

    NSNumberFormatter *numberFormatter = [[NSNumberFormatter alloc] init];
    numberFormatter.numberStyle = NSNumberFormatterNoStyle;

    self.host = host;
    self.port = [numberFormatter numberFromString:port];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:host forKey:@"host"];
    [defaults setInteger:[self.port integerValue] forKey:@"port"];
    [defaults synchronize];

    [self.hostLabel setText:[NSString stringWithFormat:@"%@:%@", host, port]];
}

- (void)session:(WCSession *)session activationDidCompleteWithState:(WCSessionActivationState)activationState error:(NSError *)error {

}

@end
