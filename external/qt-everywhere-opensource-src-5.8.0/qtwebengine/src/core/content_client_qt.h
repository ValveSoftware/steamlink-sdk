/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#ifndef CONTENT_CLIENT_QT_H
#define CONTENT_CLIENT_QT_H

#include "base/strings/string_piece.h"
#include "content/public/common/content_client.h"
#include "ui/base/layout.h"
#include <QtCore/qcompilerdetection.h> // Needed for Q_DECL_OVERRIDE

namespace QtWebEngineCore {

class ContentClientQt : public content::ContentClient {
public:
    static std::string getUserAgent();

#if defined(ENABLE_PLUGINS)
    virtual void AddPepperPlugins(std::vector<content::PepperPluginInfo>* plugins) Q_DECL_OVERRIDE;
#endif

    virtual base::StringPiece GetDataResource(int, ui::ScaleFactor) const Q_DECL_OVERRIDE;
    virtual base::RefCountedMemory* GetDataResourceBytes(int resource_id) const  Q_DECL_OVERRIDE;
    virtual std::string GetUserAgent() const Q_DECL_OVERRIDE { return getUserAgent(); }
    virtual base::string16 GetLocalizedString(int message_id) const Q_DECL_OVERRIDE;
    virtual std::string GetProduct() const Q_DECL_OVERRIDE;
};

} // namespace QtWebEngineCore

#endif // CONTENT_CLIENT_QT_H
