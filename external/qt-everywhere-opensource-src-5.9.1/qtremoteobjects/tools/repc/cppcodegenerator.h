/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#ifndef CPPCODEGENERATOR_H
#define CPPCODEGENERATOR_H

#include <QVector>

QT_BEGIN_NAMESPACE
struct ClassDef;
class QIODevice;

class CppCodeGenerator
{
public:
    CppCodeGenerator(QIODevice *outputDevice);

    void generate(const QVector<ClassDef> &classList, bool alwaysGenerateClass = false);

private:
    QIODevice *m_outputDevice;
};

QT_END_NAMESPACE

#endif // CPPCODEGENERATOR_H
