/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef PatternistSDK_ExitCode_H
#define PatternistSDK_ExitCode_H

#include "Global.h"

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    /**
     * @short Houses program exit codes for PatternistSDK utilities.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT ExitCode
    {
    public:
        /**
         * Exit codes for programs part of PatternistSDK. The values for the enums are specified
         * to make it easily understandable what number a symbol corresponds to.
         */
        enum Code
        {
            Success             = 0,
            InvalidArgCount     = 1,
            InvalidArgs         = 2,

            /**
             * Used in @c patternistrunsuite
             */
            InvalidCatalogFile  = 3,
            FileNotExists       = 4,
            OpenError           = 5,
            WriteError          = 6,

            /**
             * Used in the test program used for comparing files on the byte level.
             */
            NotEqual            = 7,
            UnevenTestCount     = 8,
            InternalError       = 9,
            Regression          = 10
        };

    private:
        /**
         * This constructor is private and has no implementation, because this
         * class is not supposed to be instantiated.
         */
        inline ExitCode();

        Q_DISABLE_COPY(ExitCode)
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
