/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtCanvas3D API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef CANVASGLSTATEDUMP_P_H
#define CANVASGLSTATEDUMP_P_H

#include "canvas3dcommon_p.h"

#include <QtCore/QObject>
#include <QtGui/qopengl.h>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

#define DUMP_ENUM_AS_PROPERTY(A,B,C) Q_PROPERTY(A::B C READ C ## _read); inline A::B C ## _read() { return A::C; }

class EnumToStringMap;
class CanvasContext;

class CanvasGLStateDump : public QObject
{
    Q_OBJECT

    Q_ENUMS(stateDumpEnums)

public:
    enum stateDumpEnums {
        DUMP_BASIC_ONLY                        = 0x00,
        DUMP_VERTEX_ATTRIB_ARRAYS_BIT          = 0x01,
        DUMP_VERTEX_ATTRIB_ARRAYS_BUFFERS_BIT  = 0x02,
        DUMP_FULL                              = 0x03
    };

    DUMP_ENUM_AS_PROPERTY(QtCanvas3D::CanvasGLStateDump,stateDumpEnums,DUMP_BASIC_ONLY)
    DUMP_ENUM_AS_PROPERTY(QtCanvas3D::CanvasGLStateDump,stateDumpEnums,DUMP_VERTEX_ATTRIB_ARRAYS_BIT)
    DUMP_ENUM_AS_PROPERTY(QtCanvas3D::CanvasGLStateDump,stateDumpEnums,DUMP_VERTEX_ATTRIB_ARRAYS_BUFFERS_BIT)
    DUMP_ENUM_AS_PROPERTY(QtCanvas3D::CanvasGLStateDump,stateDumpEnums,DUMP_FULL)

    CanvasGLStateDump(CanvasContext *canvasContext, bool isEs, QObject *parent = 0);
    ~CanvasGLStateDump();

    Q_INVOKABLE QString getGLStateDump(stateDumpEnums options = DUMP_BASIC_ONLY);

    void getGLArrayObjectDump(int target, int arrayObject, int type);

    void doGLStateDump();

private:
    GLint m_maxVertexAttribs;
    EnumToStringMap *m_map;
    bool m_isOpenGLES2;
    CanvasContext *m_canvasContext;
    QString m_stateDumpStr;
    stateDumpEnums m_options;
};

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE

#endif // CANVASGLSTATEDUMP_P_H
