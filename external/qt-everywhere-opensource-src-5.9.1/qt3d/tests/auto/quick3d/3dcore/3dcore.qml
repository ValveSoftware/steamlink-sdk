/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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


import Qt3D.Core 2.0 as QQ3Core20
import Qt3D.Core 2.9 as QQ3Core29
import QtQuick 2.0

Item {

    // Misc
    //QQ3Core20.Component3D                  // (uncreatable) Qt3DCore::QComponent
    QQ3Core20.Entity {}                      //Qt3DCore::QEntity, Qt3DCore::Quick::Quick3DEntity
    QQ3Core20.EntityLoader {}                //Qt3DCore::Quick::Quick3DEntityLoader
    QQ3Core20.NodeInstantiator {}            //Qt3DCore::Quick::Quick3DNodeInstantiator
    QQ3Core20.Transform {}                   //Qt3DCore::QTransform
    QQ3Core20.QuaternionAnimation {}         //Qt3DCore::Quick::QQuaternionAnimation

    QQ3Core29.Entity {}                      //Qt3DCore::QEntity, Qt3DCore::Quick::Quick3DEntity
}
