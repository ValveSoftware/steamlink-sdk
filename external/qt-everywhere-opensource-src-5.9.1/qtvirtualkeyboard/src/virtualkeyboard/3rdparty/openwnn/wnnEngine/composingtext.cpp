/*
 * Qt implementation of OpenWnn library
 * This file is part of the Qt Virtual Keyboard module.
 * Contact: http://www.qt.io/licensing/
 *
 * Copyright (C) 2015  The Qt Company
 * Copyright (C) 2008-2012  OMRON SOFTWARE Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "composingtext.h"

#include <QtCore/private/qobject_p.h>
#include <QDebug>

class ComposingTextPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(ComposingText)
public:
    ComposingTextPrivate(ComposingText *q_ptr) :
        QObjectPrivate(),
        q_ptr(q_ptr)
    {
        memset(mCursor, 0, sizeof(mCursor));
    }

    void modifyUpper(ComposingText::TextLayer layer, int mod_from, int mod_len, int org_len)
    {
        Q_Q(ComposingText);
        if (layer >= (ComposingText::MAX_LAYER - 1)) {
            /* no layer above */
            return;
        }

        ComposingText::TextLayer uplayer = (ComposingText::TextLayer)(layer + 1);
        QList<StrSegment> &strUplayer = mStringLayer[uplayer];
        if (strUplayer.size() <= 0) {
            /*
             * if there is no element on above layer,
             * add a element includes whole elements of the lower layer.
             */
            strUplayer.append(StrSegment(q->toString(layer), 0, mStringLayer[layer].size() - 1));
            modifyUpper(uplayer, 0, 1, 0);
            return;
        }

        int mod_to = mod_from + ((mod_len == 0) ? 0 : (mod_len - 1));
        int org_to = mod_from + ((org_len == 0) ? 0 : (org_len - 1));
        StrSegment &last = strUplayer[strUplayer.size() - 1];
        if (last.to < mod_from) {
            /* add at the tail */
            last.to = mod_to;
            last.string = q->toString(layer, last.from, last.to);
            modifyUpper(uplayer, strUplayer.size()-1, 1, 1);
            return;
        }

        int uplayer_mod_from = -1;
        int uplayer_org_to = -1;
        for (int i = 0; i < strUplayer.size(); i++) {
            const StrSegment &ss = strUplayer.at(i);
            if (ss.from > mod_from) {
                if (ss.to <= org_to) {
                    /* the segment is included */
                    if (uplayer_mod_from < 0) {
                        uplayer_mod_from = i;
                    }
                    uplayer_org_to = i;
                } else {
                    /* included in this segment */
                    uplayer_org_to = i;
                    break;
                }
            } else {
                if (org_len == 0 && ss.from == mod_from) {
                    /* when an element is added */
                    uplayer_mod_from = i - 1;
                    uplayer_org_to   = i - 1;
                    break;
                } else {
                    /* start from this segment */
                    uplayer_mod_from = i;
                    uplayer_org_to = i;
                    if (ss.to >= org_to) {
                        break;
                    }
                }
            }
        }

        int diff = mod_len - org_len;
        if (uplayer_mod_from >= 0) {
            /* update an element */
            StrSegment &ss = strUplayer[uplayer_mod_from];
            int last_to = ss.to;
            int next = uplayer_mod_from + 1;
            for (int i = next; i <= uplayer_org_to; i++) {
                const StrSegment &ss2 = strUplayer.at(next);
                if (last_to > ss2.to) {
                    last_to = ss2.to;
                }
                strUplayer.removeAt(next);
            }
            ss.to = (last_to < mod_to)? mod_to : (last_to + diff);

            ss.string = q->toString(layer, ss.from, ss.to);

            for (int i = next; i < strUplayer.size(); i++) {
                StrSegment &ss2 = strUplayer[i];
                ss2.from += diff;
                ss2.to   += diff;
            }

            modifyUpper(uplayer, uplayer_mod_from, 1, uplayer_org_to - uplayer_mod_from + 1);
        } else {
            /* add an element at the head */
            strUplayer.insert(0, StrSegment(q->toString(layer, mod_from, mod_to), mod_from, mod_to));
            for (int i = 1; i < strUplayer.size(); i++) {
                StrSegment &ss = strUplayer[i];
                ss.from += diff;
                ss.to   += diff;
            }
            modifyUpper(uplayer, 0, 1, 0);
        }
    }

    void deleteStrSegment0(ComposingText::TextLayer layer, int from, int to, int diff)
    {
        QList<StrSegment> &strLayer = mStringLayer[layer];
        if (diff != 0) {
            for (int i = to + 1; i < strLayer.size(); i++) {
                StrSegment &ss = strLayer[i];
                ss.from -= diff;
                ss.to   -= diff;
            }
        }
        for (int i = from; i <= to; i++) {
            strLayer.removeAt(from);
        }
    }

    void replaceStrSegment0(ComposingText::TextLayer layer, const QList<StrSegment> &str, int from, int to)
    {
        QList<StrSegment> &strLayer = mStringLayer[layer];

        if (from < 0 || from > strLayer.size()) {
            from = strLayer.size();
        }
        if (to < 0 || to > strLayer.size()) {
            to = strLayer.size();
        }
        for (int i = from; i <= to; i++) {
            strLayer.removeAt(from);
        }
        for (int i = str.size() - 1; i >= 0; i--) {
            strLayer.insert(from, str.at(i));
        }

        modifyUpper(layer, from, str.size(), to - from + 1);
    }

    ComposingText *q_ptr;
    QList<StrSegment> mStringLayer[ComposingText::MAX_LAYER];
    int mCursor[ComposingText::MAX_LAYER];

};

ComposingText::ComposingText(QObject *parent) :
    QObject(*new ComposingTextPrivate(this), parent)
{

}

ComposingText::~ComposingText()
{

}

StrSegment ComposingText::getStrSegment(TextLayer layer, int pos) const
{
    Q_D(const ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER)
        return StrSegment();

    const QList<StrSegment> &strLayer = d->mStringLayer[layer];
    if (pos < 0) {
        pos = strLayer.size() - 1;
    }
    if (pos >= strLayer.size() || pos < 0) {
        return StrSegment();
    }
    return strLayer.at(pos);
}

void ComposingText::debugout() const
{
    Q_D(const ComposingText);
    for (int i = LAYER0; i < MAX_LAYER; i++) {
        qDebug() << QString("ComposingText[%1]").arg(i);
        qDebug() << "  cur =" << d->mCursor[i];
        QString tmp;
        for (QList<StrSegment>::ConstIterator it = d->mStringLayer[i].constBegin();
             it != d->mStringLayer[i].constEnd(); it++) {
            tmp += QString("(%1,%2,%3)").arg(it->string).arg(it->from).arg(it->to);
        }
        qDebug() << "  str =" << tmp;
    }
}

QString ComposingText::toString(TextLayer layer, int from, int to) const
{
    Q_D(const ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER)
        return QString();

    QString buf;
    const QList<StrSegment> &strLayer = d->mStringLayer[layer];

    for (int i = from; i <= to; i++) {
        const StrSegment &ss = strLayer.at(i);
        buf.append(ss.string);
    }
    return buf;
}

QString ComposingText::toString(TextLayer layer) const
{
    Q_D(const ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER)
        return QString();

    return toString(layer, 0, d->mStringLayer[layer].size() - 1);
}

void ComposingText::insertStrSegment(TextLayer layer, const StrSegment& str)
{
    Q_D(ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER)
        return;

    int cursor = d->mCursor[layer];
    d->mStringLayer[layer].insert(cursor, str);
    d->modifyUpper(layer, cursor, 1, 0);
    setCursor(layer, cursor + 1);
}

void ComposingText::insertStrSegment(TextLayer layer1, TextLayer layer2, const StrSegment &str)
{
    Q_D(ComposingText);

    if (layer1 < LAYER0 || layer1 >= MAX_LAYER || layer2 < LAYER0 || layer2 >= MAX_LAYER)
        return;

    d->mStringLayer[layer1].insert(d->mCursor[layer1], str);
    d->mCursor[layer1]++;

    for (int i = (int)layer1 + 1; i <= (int)layer2; i++) {
        int pos = d->mCursor[i - 1] - 1;
        StrSegment tmp(str.string, pos, pos);
        QList<StrSegment> &strLayer = d->mStringLayer[i];
        strLayer.insert(d->mCursor[i], tmp);
        d->mCursor[i]++;
        for (int j = d->mCursor[i]; j < strLayer.size(); j++) {
            StrSegment &ss = strLayer[j];
            ss.from++;
            ss.to++;
        }
    }
    int cursor = d->mCursor[layer2];
    d->modifyUpper(layer2, cursor - 1, 1, 0);
    setCursor(layer2, cursor);
}

void ComposingText::replaceStrSegment(TextLayer layer, const QList<StrSegment> &str, int num)
{
    Q_D(ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER)
        return;

    int cursor = d->mCursor[layer];
    d->replaceStrSegment0(layer, str, cursor - num, cursor - 1);
    setCursor(layer, cursor + str.size() - num);
}

void ComposingText::deleteStrSegment(TextLayer layer, int from, int to)
{
    Q_D(ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER)
        return;

    int fromL[3] = { -1, -1, -1 };
    int toL[3]   = { -1, -1, -1 };

    QList<StrSegment> &strLayer2 = d->mStringLayer[LAYER2];
    QList<StrSegment> &strLayer1 = d->mStringLayer[LAYER1];

    if (layer == LAYER2) {
        fromL[LAYER2] = from;
        toL[LAYER2]   = to;
        fromL[LAYER1] = strLayer2.at(from).from;
        toL[LAYER1]   = strLayer2.at(to).to;
        fromL[LAYER0] = strLayer1.at(fromL[LAYER1]).from;
        toL[LAYER0]   = strLayer1.at(toL[LAYER1]).to;
    } else if (layer == LAYER1) {
        fromL[LAYER1] = from;
        toL[LAYER1]   = to;
        fromL[LAYER0] = strLayer1.at(from).from;
        toL[LAYER0]   = strLayer1.at(to).to;
    } else {
        fromL[LAYER0] = from;
        toL[LAYER0]   = to;
    }

    int diff = to - from + 1;
    for (int lv = LAYER0; lv < MAX_LAYER; lv++) {
        if (fromL[lv] >= 0) {
            d->deleteStrSegment0((TextLayer)lv, fromL[lv], toL[lv], diff);
        } else {
            int boundary_from = -1;
            int boundary_to   = -1;
            QList<StrSegment> &strLayer = d->mStringLayer[lv];
            for (int i = 0; i < strLayer.size(); i++) {
                const StrSegment &ss = strLayer.at(i);
                if ((ss.from >= fromL[lv-1] && ss.from <= toL[lv-1]) ||
                    (ss.to >= fromL[lv-1] && ss.to <= toL[lv-1]) ) {
                    if (fromL[lv] < 0) {
                        fromL[lv] = i;
                        boundary_from = ss.from;
                    }
                    toL[lv] = i;
                    boundary_to = ss.to;
                } else if (ss.from <= fromL[lv-1] && ss.to >= toL[lv-1]) {
                    boundary_from = ss.from;
                    boundary_to   = ss.to;
                    fromL[lv] = i;
                    toL[lv] = i;
                    break;
                } else if (ss.from > toL[lv-1]) {
                    break;
                }
            }
            if (boundary_from != fromL[lv-1] || boundary_to != toL[lv-1]) {
                d->deleteStrSegment0((TextLayer)lv, fromL[lv] + 1, toL[lv], diff);
                boundary_to -= diff;
                QList<StrSegment> tmp = QList<StrSegment>() <<
                    StrSegment(toString((TextLayer)(lv - 1)), boundary_from, boundary_to);
                d->replaceStrSegment0((TextLayer)lv, tmp, fromL[lv], fromL[lv]);
                return;
            } else {
                d->deleteStrSegment0((TextLayer)lv, fromL[lv], toL[lv], diff);
            }
        }
        diff = toL[lv] - fromL[lv] + 1;
    }
}

int ComposingText::deleteAt(TextLayer layer, bool rightside)
{
    Q_D(ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER)
        return 0;

    int cursor = d->mCursor[layer];
    QList<StrSegment> &strLayer = d->mStringLayer[layer];

    if (!rightside && cursor > 0) {
        deleteStrSegment(layer, cursor - 1, cursor - 1);
        setCursor(layer, cursor - 1);
    } else if (rightside && cursor < strLayer.size()) {
        deleteStrSegment(layer, cursor, cursor);
        setCursor(layer, cursor);
    }
    return strLayer.size();
}

QList<StrSegment> ComposingText::getStringLayer(TextLayer layer) const
{
    Q_D(const ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER)
        return QList<StrSegment>();

    return d->mStringLayer[layer];
}

int ComposingText::included(TextLayer layer, int pos)
{
    Q_D(ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER - 1)
        return 0;

    if (pos == 0) {
        return 0;
    }
    int uplayer = (TextLayer)(layer + 1);
    int i;
    QList<StrSegment> &strLayer = d->mStringLayer[uplayer];
    for (i = 0; i < strLayer.size(); i++) {
        const StrSegment &ss = strLayer.at(i);
        if (ss.from <= pos && pos <= ss.to) {
            break;
        }
    }
    return i;
}

int ComposingText::setCursor(TextLayer layer, int pos)
{
    Q_D(ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER)
        return -1;

    if (pos > d->mStringLayer[layer].size()) {
        pos = d->mStringLayer[layer].size();
    }
    if (pos < 0) {
        pos = 0;
    }
    if (layer == ComposingText::LAYER0) {
        d->mCursor[ComposingText::LAYER0] = pos;
        d->mCursor[ComposingText::LAYER1] = included(ComposingText::LAYER0, pos);
        d->mCursor[ComposingText::LAYER2] = included(ComposingText::LAYER1, d->mCursor[ComposingText::LAYER1]);
    } else if (layer == ComposingText::LAYER1) {
        d->mCursor[ComposingText::LAYER2] = included(ComposingText::LAYER1, pos);
        d->mCursor[ComposingText::LAYER1] = pos;
        d->mCursor[ComposingText::LAYER0] = (pos > 0) ? d->mStringLayer[ComposingText::LAYER1].at(pos - 1).to + 1 : 0;
    } else {
        d->mCursor[ComposingText::LAYER2] = pos;
        d->mCursor[ComposingText::LAYER1] = (pos > 0) ? d->mStringLayer[ComposingText::LAYER2].at(pos - 1).to + 1 : 0;
        d->mCursor[ComposingText::LAYER0] = (d->mCursor[ComposingText::LAYER1] > 0) ? d->mStringLayer[ComposingText::LAYER1].at(d->mCursor[ComposingText::LAYER1] - 1).to + 1 : 0;
    }
    return pos;
}

int ComposingText::moveCursor(TextLayer layer, int diff)
{
    Q_D(ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER)
        return -1;

    int c = d->mCursor[layer] + diff;

    return setCursor(layer, c);
}

int ComposingText::getCursor(TextLayer layer) const
{
    Q_D(const ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER)
        return -1;

    return d->mCursor[layer];
}

int ComposingText::size(TextLayer layer) const
{
    Q_D(const ComposingText);

    if (layer < LAYER0 || layer >= MAX_LAYER)
        return 0;

    return d->mStringLayer[layer].size();
}

void ComposingText::clear()
{
    Q_D(ComposingText);
    for (int i = 0; i < MAX_LAYER; i++) {
        d->mStringLayer[i].clear();
        d->mCursor[i] = 0;
    }
}
