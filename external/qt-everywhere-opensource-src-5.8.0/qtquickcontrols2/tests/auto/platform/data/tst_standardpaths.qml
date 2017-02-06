/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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

import QtQuick 2.2
import QtTest 1.0
import Qt.labs.platform 1.0

TestCase {
    id: testCase
    name: "StandardPaths"

    function test_standardLocation() {
        // Q_ENUMS(QStandardPaths::StandardLocation)
        compare(StandardPaths.DesktopLocation, 0)
        compare(StandardPaths.DocumentsLocation, 1)
        compare(StandardPaths.FontsLocation, 2)
        compare(StandardPaths.ApplicationsLocation, 3)
        compare(StandardPaths.MusicLocation, 4)
        compare(StandardPaths.MoviesLocation, 5)
        compare(StandardPaths.PicturesLocation, 6)
        compare(StandardPaths.TempLocation, 7)
        compare(StandardPaths.HomeLocation, 8)
        compare(StandardPaths.DataLocation, 9)
        compare(StandardPaths.CacheLocation, 10)
        compare(StandardPaths.GenericDataLocation, 11)
        compare(StandardPaths.RuntimeLocation, 12)
        compare(StandardPaths.ConfigLocation, 13)
        compare(StandardPaths.DownloadLocation, 14)
        compare(StandardPaths.GenericCacheLocation, 15)
        compare(StandardPaths.GenericConfigLocation, 16)
        compare(StandardPaths.AppDataLocation, 17)
        compare(StandardPaths.AppConfigLocation, 18)
        compare(StandardPaths.AppLocalDataLocation, StandardPaths.DataLocation)
    }

    function test_locateOptions() {
        // Q_ENUMS(QStandardPaths::LocateOptions)
        compare(StandardPaths.LocateFile, 0)
        compare(StandardPaths.LocateDirectory, 1)
    }
}
