/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
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

class ContentClientQt : public content::ContentClient {
public:
    static std::string getUserAgent();

    virtual base::StringPiece GetDataResource(int, ui::ScaleFactor) const Q_DECL_OVERRIDE;
    virtual std::string GetUserAgent() const Q_DECL_OVERRIDE { return getUserAgent(); }
    virtual base::string16 GetLocalizedString(int message_id) const Q_DECL_OVERRIDE;
    virtual std::string GetProduct() const Q_DECL_OVERRIDE;
};

#endif // CONTENT_CLIENT_QT_H
