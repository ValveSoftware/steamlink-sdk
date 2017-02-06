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
