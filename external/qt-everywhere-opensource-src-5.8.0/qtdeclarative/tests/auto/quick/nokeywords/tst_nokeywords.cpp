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

#define QT_NO_KEYWORDS
#undef signals
#undef slots
#undef emit
#define signals FooBar
#define slots Baz
#define emit Yoyodyne

#include <QtQuick/QtQuick>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickflickable_p.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickitemview_p.h>
#include <QtQuick/private/qquicklistview_p.h>
#include <QtQuick/private/qquickpainteditem_p.h>
#include <QtQuick/private/qquickshadereffect_p.h>
#include <QtQuick/private/qquickshadereffectsource_p.h>
#include <QtQuick/private/qquickview_p.h>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>
#include <QtQuick/private/qsgcontext_p.h>
#include <QtQuick/private/qsgcontextplugin_p.h>
#ifndef QT_NO_OPENGL
#include <QtQuick/private/qsgdefaultdistancefieldglyphcache_p.h>
#include <QtQuick/private/qsgdefaultglyphnode_p.h>
#include <QtQuick/private/qsgdefaultinternalimagenode_p.h>
#include <QtQuick/private/qsgdefaultinternalrectanglenode_p.h>
#include <QtQuick/private/qsgdepthstencilbuffer_p.h>
#include <QtQuick/private/qsgdistancefieldglyphnode_p.h>
#include <QtQuick/private/qsgdistancefieldutil_p.h>
#endif
#include <QtQuick/private/qsggeometry_p.h>
#include <QtQuick/private/qsgnode_p.h>
#include <QtQuick/private/qsgnodeupdater_p.h>
#include <QtQuick/private/qsgdefaultpainternode_p.h>
#include <QtQuick/private/qsgrenderer_p.h>
#include <QtQuick/private/qsgrenderloop_p.h>
#include <QtQuick/private/qsgrendernode_p.h>
#include <QtQuick/private/qsgtexturematerial_p.h>
#include <QtQuick/private/qsgtexture_p.h>
#include <QtQuick/private/qsgthreadedrenderloop_p.h>
#include <QtQuick/private/qsgwindowsrenderloop_p.h>

#undef signals
#undef slots
#undef emit

class MyBooooooostishClass : public QObject
{
    Q_OBJECT
public:
    inline MyBooooooostishClass() {}

Q_SIGNALS:
    void mySignal();

public Q_SLOTS:
    inline void mySlot()
    {
        Q_UNUSED(signals);
        Q_UNUSED(slots);

        mySignal();
    }

private:
    int signals;
    double slots;
};

#define signals public
#define slots
#define emit
#undef QT_NO_KEYWORDS

#include <QtTest/QtTest>

class tst_NoKeywords : public QObject
{
    Q_OBJECT
};

QTEST_MAIN(tst_NoKeywords)

#include "tst_nokeywords.moc"
