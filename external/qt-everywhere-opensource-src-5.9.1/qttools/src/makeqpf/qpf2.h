/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QPF2_H
#define QPF2_H

#include <private/qfontengine_qpf2_p.h>
#include <qmetatype.h>

QT_BEGIN_NAMESPACE

class QFontEngine;

class QPF
{
public:
    static int debugVerbosity;

    enum GenerationOption {
        IncludeCMap = 0x1,
        RenderGlyphs = 0x2
    };

    struct CharacterRange
    {
        inline CharacterRange() : start(0), end(0x10000) {}
        uint start;
        uint end;
    };

    static QString fileNameForFont(const QFont &f);

    static QByteArray generate(const QFont &font, int options,
                               const QList<CharacterRange> &ranges,
                               QString *originalFontFile = 0);
    static QByteArray generate(QFontEngine *fontEngine, int options, const QList<CharacterRange> &ranges);
    void addHeader(QFontEngine *fontEngine);
    void addCMap(QFontEngine *fontEngine);
    void addGlyphs(QFontEngine *fontEngine, const QList<CharacterRange> &ranges);
    void addBlock(QFontEngineQPF2::BlockTag tag, const QByteArray &data);

    void addTaggedString(QFontEngineQPF2::HeaderTag tag, const QByteArray &string);
    void addTaggedQFixed(QFontEngineQPF2::HeaderTag tag, QFixed value);
    void addTaggedUInt8(QFontEngineQPF2::HeaderTag tag, quint8 value);
    void addTaggedInt8(QFontEngineQPF2::HeaderTag tag, qint8 value);
    void addTaggedUInt16(QFontEngineQPF2::HeaderTag tag, quint16 value);
    void addTaggedUInt32(QFontEngineQPF2::HeaderTag tag, quint32 value);

    static void dump(const QByteArray &qpf);
    const uchar *dumpHeader(const uchar *data);
    const uchar *dumpHeaderTag(const uchar *data);
    void dumpGMapBlock(const quint32 *gmap, int glyphCount);
    void dumpGlyphBlock(const quint32 *gmap, int glyphCount, const uchar *data, const uchar *endPtr);
    void dumpGlyph(const uchar *data, const QFontEngineQPF2::Glyph *glyph);

    void addUInt16(quint16 value) { qToBigEndian(value, addBytes(sizeof(value))); }
    void addUInt32(quint32 value) { qToBigEndian(value, addBytes(sizeof(value))); }
    void addUInt8(quint8 value) { *addBytes(sizeof(value)) = value; }
    void addInt8(qint8 value) { *addBytes(sizeof(value)) = quint8(value); }
    void addByteArray(const QByteArray &string) {
        uchar *data = addBytes(string.length());
        qMemCopy(data, string.constData(), string.length());
    }

    void align4() { while (qpf.size() & 3) { addUInt8('\0'); } }

    uchar *addBytes(int size) {
        const int oldSize = qpf.size();
        qpf.resize(qpf.size() + size);
        return reinterpret_cast<uchar *>(qpf.data() + oldSize);
    }

    QByteArray qpf;
    int options;
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QPF::CharacterRange)

#endif // QPF2_H
