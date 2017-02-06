/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <Cocoa/Cocoa.h>

#include "window.h"

#include <QtGui>
#include <qpa/qplatformnativeinterface.h>


NSView *myGetEmbeddableView(QWindow *qtWindow)
{
    // Make sure the platform window is created
    qtWindow->create();

    QPlatformNativeInterface *platformNativeInterface = QGuiApplication::platformNativeInterface();

    // Inform the window that it's a "guest" of a non-QWindow
    typedef void (*SetEmbeddedInForeignViewFunction)(QPlatformWindow *window, bool embedded);
    reinterpret_cast<SetEmbeddedInForeignViewFunction>(platformNativeInterface->
        nativeResourceFunctionForIntegration("setEmbeddedInForeignView"))(qtWindow->handle(), true);

    // Get the Qt content NSView for the QWindow from the Qt platform plugin
    NSView *qtView = (NSView *)platformNativeInterface->nativeResourceForWindow("nsview", qtWindow);
    return qtView; // qtView is ready for use.
}

@interface WindowCreator : NSObject {}
- (void)createWindow;
@end

@implementation WindowCreator
- (void)createWindow {	

    // Create the NSWindow
    NSRect frame = NSMakeRect(500, 500, 500, 500);
    NSWindow* window  = [[NSWindow alloc] initWithContentRect:frame
                        styleMask:NSTitledWindowMask |  NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
                        backing:NSBackingStoreBuffered
                        defer:NO];

    [window setTitle:@"NSWindow"];
    [window setBackgroundColor:[NSColor blueColor]]; // if you see blue something is wrong

    // Create the QWindow and embed its view.
    Window *qtWindow = new Window(); // ### who owns this window?
    NSView *qtView = myGetEmbeddableView(qtWindow);
    [window setContentView:qtView];

    // Show the NSWindow
    [window makeKeyAndOrderFront:NSApp];
}
@end

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Start Cocoa. Create NSApplicaiton.
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];

    // Schedule call to create the UI using a timer.
    WindowCreator *windowCreator= [WindowCreator alloc];
    [NSTimer scheduledTimerWithTimeInterval:0 target:windowCreator selector:@selector(createWindow) userInfo:nil repeats:NO];

    // Starte the Cocoa event loop.
    [(NSApplication *)NSApp run];
    [NSApp release];
    [pool release];
    exit(0);
    return 0;
}



