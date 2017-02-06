/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the config.tests in the Qt ImageFormats module.
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

#include <stdio.h>
#include <libmng.h>

int main(int, char **)
{
    mng_handle hMNG;
    mng_cleanup(&hMNG);

#if defined(MNG_VERSION_MAJOR)
#if MNG_VERSION_MAJOR < 1 || (MNG_VERSION_MAJOR == 1 && MNG_VERSION_MINOR == 0 && MNG_VERSION_RELEASE < 9)
#error System libmng version is less than 1.0.9; using built-in version instead.
#endif
#endif

    return 0;
}
