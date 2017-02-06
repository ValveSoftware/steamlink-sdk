// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElementPermissions.h"

#include "public/platform/modules/permissions/WebPermissionType.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(HTMLIFrameElementPermissionsTest, ParseDelegatedPermissionsValid)
{
    HTMLIFrameElementPermissions* permissions = HTMLIFrameElementPermissions::create(nullptr);
    Vector<WebPermissionType> result;
    String errorMessage;

    permissions->setValue("midi");
    result = permissions->parseDelegatedPermissions(errorMessage);
    EXPECT_EQ(1u, result.size());
    EXPECT_EQ(WebPermissionTypeMidiSysEx, result[0]);

    permissions->setValue("");
    result = permissions->parseDelegatedPermissions(errorMessage);
    EXPECT_EQ(0u, result.size());

    permissions->setValue("geolocation midi");
    result = permissions->parseDelegatedPermissions(errorMessage);
    EXPECT_EQ(2u, result.size());
    EXPECT_EQ(WebPermissionTypeGeolocation, result[0]);
    EXPECT_EQ(WebPermissionTypeMidiSysEx, result[1]);
}

TEST(HTMLIFrameElementPermissionsTest, ParseDelegatedPermissionsInvalid)
{
    HTMLIFrameElementPermissions* permissions = HTMLIFrameElementPermissions::create(nullptr);
    Vector<WebPermissionType> result;
    String errorMessage;

    permissions->setValue("midis");
    result = permissions->parseDelegatedPermissions(errorMessage);
    EXPECT_EQ(0u, result.size());
    EXPECT_EQ("'midis' is an invalid permissions flag.", errorMessage);

    permissions->setValue("geolocations midis");
    result = permissions->parseDelegatedPermissions(errorMessage);
    EXPECT_EQ(0u, result.size());
    EXPECT_EQ("'geolocations', 'midis' are invalid permissions flags.", errorMessage);

    permissions->setValue("geolocation midis");
    result = permissions->parseDelegatedPermissions(errorMessage);
    EXPECT_EQ(1u, result.size());
    EXPECT_EQ(WebPermissionTypeGeolocation, result[0]);
    EXPECT_EQ("'midis' is an invalid permissions flag.", errorMessage);
}

} // namespace blink
