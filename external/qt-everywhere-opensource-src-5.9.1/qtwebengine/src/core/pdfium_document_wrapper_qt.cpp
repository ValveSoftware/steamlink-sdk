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
#if defined (ENABLE_PDF)
#include "pdfium_document_wrapper_qt.h"

#include <QtCore/qhash.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainter.h>

#include "third_party/pdfium/public/fpdf_doc.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace QtWebEngineCore {
int PdfiumDocumentWrapperQt::m_libraryUsers = 0;

class QWEBENGINE_EXPORT PdfiumPageWrapperQt {
public:
    PdfiumPageWrapperQt(void *data, int pageIndex, int targetWidth, int targetHeight)
        : m_pageData(FPDF_LoadPage(data, pageIndex))
        , m_width(FPDF_GetPageWidth(m_pageData))
        , m_height(FPDF_GetPageHeight(m_pageData))
        , m_index(pageIndex)
        , m_image(createImage(targetWidth, targetHeight))
    {
    }

    PdfiumPageWrapperQt()
        : m_pageData(nullptr)
        , m_width(-1)
        , m_height(-1)
        , m_index(-1)
        , m_image(QImage())
    {
    }

    virtual ~PdfiumPageWrapperQt()
    {
        FPDF_ClosePage(m_pageData);
    }

    QImage image()
    {
        return m_image;
    }

private:
    QImage createImage(int targetWidth, int targetHeight)
    {
        Q_ASSERT(m_pageData);
        if (targetWidth <= 0)
            targetWidth = m_width;

        if (targetHeight <= 0)
            targetHeight = m_height;

        QImage image(targetWidth, targetHeight, QImage::Format_RGBA8888);
        Q_ASSERT(!image.isNull());
        image.fill(0xFFFFFFFF);

        FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(image.width(), image.height(),
                                                 FPDFBitmap_BGRA,
                                                 image.scanLine(0), image.bytesPerLine());
        Q_ASSERT(bitmap);

        FPDF_RenderPageBitmap(bitmap, m_pageData,
                              0, 0, image.width(), image.height(),
                              0, 0);
        FPDFBitmap_Destroy(bitmap);
        bitmap = nullptr;

        // Map BGRA to RGBA as PDFium currently does not support RGBA bitmaps directly
        for (int i = 0; i < image.height(); i++) {
            uchar *pixels = image.scanLine(i);
            for (int j = 0; j < image.width(); j++) {
                qSwap(pixels[0], pixels[2]);
                pixels += 4;
            }
        }
        return image;
    }

private:
    void *m_pageData;
    int m_width;
    int m_height;
    int m_index;
    QImage m_image;
};


PdfiumDocumentWrapperQt::PdfiumDocumentWrapperQt(const void *pdfData, size_t size, const QSize& imageSize, const char *password)
    : m_imageSize(imageSize * 2.0)
{
    Q_ASSERT(pdfData);
    Q_ASSERT(size);
    if (m_libraryUsers++ == 0)
        FPDF_InitLibrary();

    m_documentHandle = FPDF_LoadMemDocument(pdfData, static_cast<int>(size), password);
    m_pageCount = FPDF_GetPageCount(m_documentHandle);
}

QImage PdfiumDocumentWrapperQt::pageAsQImage(size_t index)
{
    if (!m_documentHandle || !m_pageCount) {
        qWarning("Failure to generate QImage from invalid or empty PDF document.");
        return QImage();
    }

    if (static_cast<int>(index) >= m_pageCount) {
        qWarning("Failure to generate QImage from PDF data: index out of bounds.");
        return QImage();
    }

    PdfiumPageWrapperQt *pageWrapper = nullptr;
    if (!m_cachedPages.contains(index)) {
        pageWrapper = new PdfiumPageWrapperQt(m_documentHandle, index,
                                              m_imageSize.width(), m_imageSize.height());
        m_cachedPages.insert(index, pageWrapper);
    } else {
        pageWrapper = m_cachedPages.value(index);
    }

    return pageWrapper->image();
}

PdfiumDocumentWrapperQt::~PdfiumDocumentWrapperQt()
{
    qDeleteAll(m_cachedPages);
    FPDF_CloseDocument(m_documentHandle);
    if (--m_libraryUsers == 0)
        FPDF_DestroyLibrary();
}

}
#endif // defined (ENABLE_PDF)
