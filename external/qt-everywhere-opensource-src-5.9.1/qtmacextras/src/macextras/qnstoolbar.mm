/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
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
