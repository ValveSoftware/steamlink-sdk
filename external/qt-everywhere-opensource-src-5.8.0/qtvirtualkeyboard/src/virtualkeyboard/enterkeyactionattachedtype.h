/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef ENTERKEYACTIONATTACHEDTYPE_H
#define ENTERKEYACTIONATTACHEDTYPE_H

#include <QObject>
#include "enterkeyaction.h"

namespace QtVirtualKeyboard {

class EnterKeyActionAttachedType : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int actionId READ actionId WRITE setActionId NOTIFY actionIdChanged)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit EnterKeyActionAttachedType(QObject *parent);

    int actionId() const;
    void setActionId(int actionId);
    QString label() const;
    void setLabel(const QString& label);
    bool enabled() const;
    void setEnabled(bool enabled);

signals:
    void actionIdChanged();
    void labelChanged();
    void enabledChanged();

private:
    int m_actionId;
    QString m_label;
    bool m_enabled;
};

} // namespace QtVirtualKeyboard

#endif
