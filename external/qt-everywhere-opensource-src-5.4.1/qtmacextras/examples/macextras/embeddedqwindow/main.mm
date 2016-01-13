/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <Cocoa/Cocoa.h>

#include "window.h"

#include <QtGui>
#include <qpa/qplatformnativeinterface.h>


NSView *getEmbeddableView(QWindow *qtWindow)
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
    NSView *qtView = getEmbeddableView(qtWindow);
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



