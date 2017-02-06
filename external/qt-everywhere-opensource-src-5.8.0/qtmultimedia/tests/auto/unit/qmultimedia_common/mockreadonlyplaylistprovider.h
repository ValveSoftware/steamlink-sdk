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

#ifndef MOCKREADONLYPLAYLISTPROVIDER_H
#define MOCKREADONLYPLAYLISTPROVIDER_H

#include <private/qmediaplaylistprovider_p.h>

class MockReadOnlyPlaylistProvider : public QMediaPlaylistProvider
{
public:
    MockReadOnlyPlaylistProvider(QObject *parent)
        :QMediaPlaylistProvider(parent)
    {
        m_items.append(QMediaContent(QUrl(QLatin1String("file:///1"))));
        m_items.append(QMediaContent(QUrl(QLatin1String("file:///2"))));
        m_items.append(QMediaContent(QUrl(QLatin1String("file:///3"))));
    }

    int mediaCount() const { return m_items.size(); }
    QMediaContent media(int index) const
    {
        return index >=0 && index < mediaCount() ? m_items.at(index) : QMediaContent();
    }

private:
    QList<QMediaContent> m_items;
};

#endif // MOCKREADONLYPLAYLISTPROVIDER_H
