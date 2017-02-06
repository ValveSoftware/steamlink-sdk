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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "clipboard_qt.h"
#include "ui/base/clipboard/clipboard.h"

#include "type_conversion.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/custom_data_helper.h"

#include <QGuiApplication>
#include <QImage>
#include <QMimeData>

namespace QtWebEngineCore {

static void registerMetaTypes()
{
    qRegisterMetaType<QClipboard::Mode>("QClipboard::Mode");
}

Q_CONSTRUCTOR_FUNCTION(registerMetaTypes)

Q_GLOBAL_STATIC(ClipboardChangeObserver, clipboardChangeObserver)

ClipboardChangeObserver::ClipboardChangeObserver()
{
    connect(QGuiApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)), SLOT(trackChange(QClipboard::Mode)));
}

void ClipboardChangeObserver::trackChange(QClipboard::Mode mode)
{
    ++sequenceNumber[mode];
}

} // namespace QtWebEngineCore

using namespace QtWebEngineCore;

namespace {

const char kMimeTypeBitmap[] = "image/bmp";
const char kMimeTypeMozillaURL[] = "text/x-moz-url";
const char kMimeTypeWebCustomDataCopy[] = "chromium/x-web-custom-data";
const char kMimeTypePepperCustomData[] = "chromium/x-pepper-custom-data";
const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";

QScopedPointer<QMimeData> uncommittedData;
QMimeData *getUncommittedData()
{
    if (!uncommittedData)
        uncommittedData.reset(new QMimeData);
    return uncommittedData.data();
}

}  // namespace

namespace ui {

// Factory function
Clipboard* Clipboard::Create() {
    return new ClipboardQt;
}

Clipboard::FormatType Clipboard::GetFormatType(const std::string& format_string)
{
    return FormatType::Deserialize(format_string);
}

const Clipboard::FormatType& Clipboard::GetPlainTextFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeText));
    return type;
}

const Clipboard::FormatType& Clipboard::GetPlainTextWFormatType()
{
    return GetPlainTextFormatType();
}

const Clipboard::FormatType& Clipboard::GetUrlFormatType()
{
    return GetPlainTextFormatType();
}

const Clipboard::FormatType& Clipboard::GetUrlWFormatType()
{
    return GetPlainTextWFormatType();
}

const Clipboard::FormatType& Clipboard::GetHtmlFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeHTML));
    return type;
}

const Clipboard::FormatType& Clipboard::GetRtfFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeRTF));
    return type;
}

const Clipboard::FormatType& Clipboard::GetBitmapFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeBitmap));
    return type;
}

const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebkitSmartPaste));
    return type;
}

const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebCustomDataCopy));
    return type;
}

const Clipboard::FormatType& Clipboard::GetPepperCustomDataFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypePepperCustomData));
    return type;
}


Clipboard::FormatType::FormatType()
{
}

Clipboard::FormatType::FormatType(const std::string& format_string)
    : data_(format_string)
{
}

Clipboard::FormatType::~FormatType()
{
}

std::string Clipboard::FormatType::Serialize() const
{
  return data_;
}

Clipboard::FormatType Clipboard::FormatType::Deserialize(const std::string& serialization)
{
  return FormatType(serialization);
}

bool Clipboard::FormatType::Equals(const FormatType& other) const
{
  return data_ == other.data_;
}

#if defined(OS_WIN) || defined(USE_AURA)
bool Clipboard::FormatType::operator<(const FormatType& other) const
{
    return data_.compare(other.data_) < 0;
}
#endif

} // namespace ui

namespace QtWebEngineCore {

void ClipboardQt::WriteObjects(ui::ClipboardType type, const ObjectMap& objects)
{
    DCHECK(CalledOnValidThread());
    DCHECK(IsSupportedClipboardType(type));

    for (ObjectMap::const_iterator iter = objects.begin(); iter != objects.end(); ++iter)
        DispatchObject(static_cast<ObjectType>(iter->first), iter->second);

    // Commit the accumulated data.
    if (uncommittedData)
        QGuiApplication::clipboard()->setMimeData(uncommittedData.take(), type == ui::CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);

    if (type == ui::CLIPBOARD_TYPE_COPY_PASTE && IsSupportedClipboardType(ui::CLIPBOARD_TYPE_SELECTION)) {
        ObjectMap::const_iterator text_iter = objects.find(CBF_TEXT);
        if (text_iter != objects.end()) {
            // Copy text and SourceTag to the selection clipboard.
            ObjectMap::const_iterator next_iter = text_iter;
            WriteObjects(ui::CLIPBOARD_TYPE_SELECTION, ObjectMap(text_iter, ++next_iter));
        }
    }
}

void ClipboardQt::WriteText(const char* text_data, size_t text_len)
{
    getUncommittedData()->setText(QString::fromUtf8(text_data, text_len));
}

void ClipboardQt::WriteHTML(const char* markup_data, size_t markup_len, const char* url_data, size_t url_len)
{
    getUncommittedData()->setHtml(QString::fromUtf8(markup_data, markup_len));
}

void ClipboardQt::WriteRTF(const char* rtf_data, size_t data_len)
{
    getUncommittedData()->setData(QString::fromLatin1(kMimeTypeRTF), QByteArray(rtf_data, data_len));
}

void ClipboardQt::WriteWebSmartPaste()
{
    getUncommittedData()->setData(QString::fromLatin1(kMimeTypeWebkitSmartPaste), QByteArray());
}

void ClipboardQt::WriteBitmap(const SkBitmap& bitmap)
{
    getUncommittedData()->setImageData(toQImage(bitmap).copy());
}

void ClipboardQt::WriteBookmark(const char* title_data, size_t title_len, const char* url_data, size_t url_len)
{
    // FIXME: Untested, seems to be used only for drag-n-drop.
    // Write as a mozilla url (UTF16: URL, newline, title).
    QString url = QString::fromUtf8(url_data, url_len);
    QString title = QString::fromUtf8(title_data, title_len);

    QByteArray data;
    data.append(reinterpret_cast<const char*>(url.utf16()), url.size() * 2);
    data.append('\n');
    data.append(reinterpret_cast<const char*>(title.utf16()), title.size() * 2);
    getUncommittedData()->setData(QString::fromLatin1(kMimeTypeMozillaURL), data);
}

void ClipboardQt::WriteData(const FormatType& format, const char* data_data, size_t data_len)
{
    getUncommittedData()->setData(QString::fromStdString(format.ToString()), QByteArray(data_data, data_len));
}

bool ClipboardQt::IsFormatAvailable(const ui::Clipboard::FormatType& format, ui::ClipboardType type) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == ui::CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    return mimeData && mimeData->hasFormat(QString::fromStdString(format.ToString()));
}

void ClipboardQt::Clear(ui::ClipboardType type)
{
    QGuiApplication::clipboard()->clear(type == ui::CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
}

void ClipboardQt::ReadAvailableTypes(ui::ClipboardType type, std::vector<base::string16>* types, bool* contains_filenames) const
{
    if (!types || !contains_filenames) {
        NOTREACHED();
        return;
    }

    types->clear();
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == ui::CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    if (!mimeData)
        return;
    if (mimeData->hasImage())
        types->push_back(toString16(QStringLiteral("image/png")));
    Q_FOREACH (const QString &mimeType, mimeData->formats())
        types->push_back(toString16(mimeType));
    *contains_filenames = false;

    const QByteArray customData = mimeData->data(QString::fromLatin1(kMimeTypeWebCustomDataCopy));
    ui::ReadCustomDataTypes(customData.constData(), customData.size(), types);
}


void ClipboardQt::ReadText(ui::ClipboardType type, base::string16* result) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == ui::CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    if (mimeData)
        *result = toString16(mimeData->text());
}

void ClipboardQt::ReadAsciiText(ui::ClipboardType type, std::string* result) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == ui::CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    if (mimeData)
        *result = mimeData->text().toStdString();
}

void ClipboardQt::ReadHTML(ui::ClipboardType type, base::string16* markup, std::string* src_url, uint32_t* fragment_start, uint32_t* fragment_end) const
{
    markup->clear();
    if (src_url)
        src_url->clear();
    *fragment_start = 0;
    *fragment_end = 0;

    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == ui::CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    if (!mimeData)
        return;
    *markup = toString16(mimeData->html());
    *fragment_end = static_cast<uint32_t>(markup->length());
}

void ClipboardQt::ReadRTF(ui::ClipboardType type, std::string* result) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == ui::CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    if (!mimeData)
        return;
    const QByteArray byteArray = mimeData->data(QString::fromLatin1(kMimeTypeRTF));
    *result = std::string(byteArray.constData(), byteArray.length());
}

SkBitmap ClipboardQt::ReadImage(ui::ClipboardType type) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == ui::CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    if (!mimeData)
        return SkBitmap();
    QImage image = qvariant_cast<QImage>(mimeData->imageData());

    image = image.convertToFormat(QImage::Format_ARGB32);
    SkBitmap bitmap;
    bitmap.setInfo(SkImageInfo::MakeN32(image.width(), image.height(), kOpaque_SkAlphaType));
    bitmap.setPixels(const_cast<uchar*>(image.constBits()));

    // Return a deep copy of the pixel data.
    SkBitmap copy;
    bitmap.copyTo(&copy, kN32_SkColorType);
    return copy;
}

void ClipboardQt::ReadCustomData(ui::ClipboardType clipboard_type, const base::string16& type, base::string16* result) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(clipboard_type == ui::CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    if (!mimeData)
        return;
    const QByteArray customData = mimeData->data(QString::fromLatin1(kMimeTypeWebCustomDataCopy));
    ui::ReadCustomDataForType(customData.constData(), customData.size(), type, result);
}

void ClipboardQt::ReadBookmark(base::string16* title, std::string* url) const
{
    NOTIMPLEMENTED();
}

void ClipboardQt::ReadData(const FormatType& format, std::string* result) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    if (!mimeData)
        return;
    const QByteArray byteArray = mimeData->data(QString::fromStdString(format.ToString()));
    *result = std::string(byteArray.constData(), byteArray.length());
}

uint64_t ClipboardQt::GetSequenceNumber(ui::ClipboardType type) const
{
    return clipboardChangeObserver()->getSequenceNumber(type == ui::CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
}

}  // namespace QtWebEngineCore
