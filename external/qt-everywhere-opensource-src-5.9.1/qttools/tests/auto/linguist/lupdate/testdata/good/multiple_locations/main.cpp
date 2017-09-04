/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/




//: This is a comment, too.
QT_TRANSLATE_NOOP("context", "just a message");



//: commented
qtTrId("lollipop");

//% "this is the source text"
//~ meta so-meta
//: even more commented
qtTrId("lollipop");

//% "this is contradicting source text"
qtTrId("lollipop");

//~ meta too-much-meta
qtTrId("lollipop");



//~ meta so-meta
QObject::tr("another message", "here with a lot of noise in the comment so it is long enough");

//~ meta too-much-meta
QObject::tr("another message", "here with a lot of noise in the comment so it is long enough");



//: commented
qtTrId("lollipop");
