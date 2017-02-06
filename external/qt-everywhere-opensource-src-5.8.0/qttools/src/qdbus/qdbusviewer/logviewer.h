/****************************************************************************
**
** Copyright (C) 2016 Tasuku Suzuki <stasuku@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef LOGVIEWER_H
#define LOGVIEWER_H

#include <QtWidgets/QTextBrowser>

class LogViewer : public QTextBrowser
{
    Q_OBJECT
public:
    explicit LogViewer(QWidget *parent = 0);

protected:
    virtual void contextMenuEvent(QContextMenuEvent *event);
};

#endif // LOGVIEWER_H
