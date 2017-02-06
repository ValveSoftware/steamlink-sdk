/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "print_web_view_helper_delegate_qt.h"

#include "third_party/WebKit/public/web/WebElement.h"
#include "web_engine_library_info.h"

namespace QtWebEngineCore {
PrintWebViewHelperDelegateQt::~PrintWebViewHelperDelegateQt()
{

}

bool PrintWebViewHelperDelegateQt::CancelPrerender(content::RenderView* render_view,
                                                   int routing_id)
{
    return true;
}

blink::WebElement PrintWebViewHelperDelegateQt::GetPdfElement(blink::WebLocalFrame* frame)
{
    return blink::WebElement();
}

bool PrintWebViewHelperDelegateQt::IsPrintPreviewEnabled()
{
    return false;
}

bool PrintWebViewHelperDelegateQt::OverridePrint(blink::WebLocalFrame* frame)
{
    return false;
}

}

namespace printing {
// std::string PrintingContextDelegate::GetAppLocale()
std::string getApplicationLocale()
{
    return WebEngineLibraryInfo::getApplicationLocale();
}
}
