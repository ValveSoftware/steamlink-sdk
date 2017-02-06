/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qv4compilationunitmapper_p.h"

#include "qv4compileddata_p.h"
#include <QFileInfo>
#include <QDateTime>
#include <QCoreApplication>

QT_BEGIN_NAMESPACE

using namespace QV4;

CompilationUnitMapper::CompilationUnitMapper()
    : dataPtr(nullptr)
{

}

CompilationUnitMapper::~CompilationUnitMapper()
{
    close();
}

bool CompilationUnitMapper::verifyHeader(const CompiledData::Unit *header, const QString &sourcePath, QString *errorString)
{
    if (strncmp(header->magic, CompiledData::magic_str, sizeof(header->magic))) {
        *errorString = QStringLiteral("Magic bytes in the header do not match");
        return false;
    }

    if (header->version != quint32(QV4_DATA_STRUCTURE_VERSION)) {
        *errorString = QString::fromUtf8("V4 data structure version mismatch. Found %1 expected %2").arg(header->version, 0, 16).arg(QV4_DATA_STRUCTURE_VERSION, 0, 16);
        return false;
    }

    if (header->qtVersion != quint32(QT_VERSION)) {
        *errorString = QString::fromUtf8("Qt version mismatch. Found %1 expected %2").arg(header->qtVersion, 0, 16).arg(QT_VERSION, 0, 16);
        return false;
    }

    {
        QFileInfo sourceCode(sourcePath);
        QDateTime sourceTimeStamp;
        if (sourceCode.exists())
            sourceTimeStamp = sourceCode.lastModified();

        // Files from the resource system do not have any time stamps, so fall back to the application
        // executable.
        if (!sourceTimeStamp.isValid())
            sourceTimeStamp = QFileInfo(QCoreApplication::applicationFilePath()).lastModified();

        if (sourceTimeStamp.isValid() && sourceTimeStamp.toMSecsSinceEpoch() != header->sourceTimeStamp) {
            *errorString = QStringLiteral("QML source file has a different time stamp than cached file.");
            return false;
        }
    }

    return true;
}

QT_END_NAMESPACE
