/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
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

#import "qnstoolbar_p.h"

NSString *QtNSToolbarDisplayModeChangedNotification = @"QtNSToolbarDisplayModeChangedNotification";
NSString *QtNSToolbarShowsBaselineSeparatorChangedNotification = @"QtNSToolbarShowsBaselineSeparatorChangedNotification";
NSString *QtNSToolbarAllowsUserCustomizationChangedNotification = @"QtNSToolbarAllowsUserCustomizationChangedNotification";
NSString *QtNSToolbarSizeModeChangedNotification = @"QtNSToolbarSizeModeChangedNotification";
NSString *QtNSToolbarVisibilityChangedNotification = @"QtNSToolbarVisibilityChangedNotification";

QT_USE_NAMESPACE

@implementation QtNSToolbar

- (void)setDisplayMode:(NSToolbarDisplayMode)displayMode
{
    if ([self displayMode] != displayMode)
    {
        [super setDisplayMode:displayMode];
        [[NSNotificationCenter defaultCenter] postNotificationName:QtNSToolbarDisplayModeChangedNotification object:self];
    }
}

- (void)setShowsBaselineSeparator:(BOOL)flag
{
    if ([self showsBaselineSeparator] != flag)
    {
        [super setShowsBaselineSeparator:flag];
        [[NSNotificationCenter defaultCenter] postNotificationName:QtNSToolbarShowsBaselineSeparatorChangedNotification object:self];
    }
}

- (void)setAllowsUserCustomization:(BOOL)allowsCustomization
{
    if ([self allowsUserCustomization] != allowsCustomization)
    {
        [super setAllowsUserCustomization:allowsCustomization];
        [[NSNotificationCenter defaultCenter] postNotificationName:QtNSToolbarAllowsUserCustomizationChangedNotification object:self];
    }
}

- (void)setSizeMode:(NSToolbarSizeMode)sizeMode
{
    if ([self sizeMode] != sizeMode)
    {
        [super setSizeMode:sizeMode];
        [[NSNotificationCenter defaultCenter] postNotificationName:QtNSToolbarSizeModeChangedNotification object:self];
    }
}

- (void)setVisible:(BOOL)shown
{
    BOOL isVisible = [self isVisible];
    [super setVisible:shown];

    // The super call might not always have effect - i.e. trying to hide the toolbar when the customization palette is running
    if ([self isVisible] != isVisible)
        [[NSNotificationCenter defaultCenter] postNotificationName:QtNSToolbarVisibilityChangedNotification object:self];
}

@end
