/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/clipboard/Clipboard.h"

#include "core/HTMLNames.h"
#include "core/clipboard/DataObject.h"
#include "core/clipboard/DataTransferItem.h"
#include "core/clipboard/DataTransferItemList.h"
#include "core/editing/markup.h"
#include "core/fetch/ImageResource.h"
#include "core/fileapi/FileList.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLImageElement.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "platform/DragImage.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/clipboard/ClipboardMimeTypes.h"
#include "platform/clipboard/ClipboardUtilities.h"

namespace WebCore {

// These "conversion" methods are called by both WebCore and WebKit, and never make sense to JS, so we don't
// worry about security for these. They don't allow access to the pasteboard anyway.
static DragOperation dragOpFromIEOp(const String& op)
{
    // yep, it's really just this fixed set
    if (op == "uninitialized")
        return DragOperationEvery;
    if (op == "none")
        return DragOperationNone;
    if (op == "copy")
        return DragOperationCopy;
    if (op == "link")
        return DragOperationLink;
    if (op == "move")
        return (DragOperation)(DragOperationGeneric | DragOperationMove);
    if (op == "copyLink")
        return (DragOperation)(DragOperationCopy | DragOperationLink);
    if (op == "copyMove")
        return (DragOperation)(DragOperationCopy | DragOperationGeneric | DragOperationMove);
    if (op == "linkMove")
        return (DragOperation)(DragOperationLink | DragOperationGeneric | DragOperationMove);
    if (op == "all")
        return DragOperationEvery;
    return DragOperationPrivate; // really a marker for "no conversion"
}

static String IEOpFromDragOp(DragOperation op)
{
    bool moveSet = !!((DragOperationGeneric | DragOperationMove) & op);

    if ((moveSet && (op & DragOperationCopy) && (op & DragOperationLink))
        || (op == DragOperationEvery))
        return "all";
    if (moveSet && (op & DragOperationCopy))
        return "copyMove";
    if (moveSet && (op & DragOperationLink))
        return "linkMove";
    if ((op & DragOperationCopy) && (op & DragOperationLink))
        return "copyLink";
    if (moveSet)
        return "move";
    if (op & DragOperationCopy)
        return "copy";
    if (op & DragOperationLink)
        return "link";
    return "none";
}

// We provide the IE clipboard types (URL and Text), and the clipboard types specified in the WHATWG Web Applications 1.0 draft
// see http://www.whatwg.org/specs/web-apps/current-work/ Section 6.3.5.3
static String normalizeType(const String& type, bool* convertToURL = 0)
{
    String cleanType = type.stripWhiteSpace().lower();
    if (cleanType == mimeTypeText || cleanType.startsWith(mimeTypeTextPlainEtc))
        return mimeTypeTextPlain;
    if (cleanType == mimeTypeURL) {
        if (convertToURL)
            *convertToURL = true;
        return mimeTypeTextURIList;
    }
    return cleanType;
}

PassRefPtrWillBeRawPtr<Clipboard> Clipboard::create(ClipboardType type, ClipboardAccessPolicy policy, PassRefPtrWillBeRawPtr<DataObject> dataObject)
{
    return adoptRefWillBeNoop(new Clipboard(type, policy , dataObject));
}

Clipboard::~Clipboard()
{
}

void Clipboard::setDropEffect(const String &effect)
{
    if (!isForDragAndDrop())
        return;

    // The attribute must ignore any attempts to set it to a value other than none, copy, link, and move.
    if (effect != "none" && effect != "copy"  && effect != "link" && effect != "move")
        return;

    // FIXME: The spec actually allows this in all circumstances, even though there's no point in
    // setting the drop effect when this condition is not true.
    if (canReadTypes())
        m_dropEffect = effect;
}

void Clipboard::setEffectAllowed(const String &effect)
{
    if (!isForDragAndDrop())
        return;

    if (dragOpFromIEOp(effect) == DragOperationPrivate) {
        // This means that there was no conversion, and the effectAllowed that
        // we are passed isn't a valid effectAllowed, so we should ignore it,
        // and not set m_effectAllowed.

        // The attribute must ignore any attempts to set it to a value other than
        // none, copy, copyLink, copyMove, link, linkMove, move, all, and uninitialized.
        return;
    }


    if (canWriteData())
        m_effectAllowed = effect;
}

void Clipboard::clearData(const String& type)
{
    if (!canWriteData())
        return;

    if (type.isNull())
        m_dataObject->clearAll();
    else
        m_dataObject->clearData(normalizeType(type));
}

String Clipboard::getData(const String& type) const
{
    if (!canReadData())
        return String();

    bool convertToURL = false;
    String data = m_dataObject->getData(normalizeType(type, &convertToURL));
    if (!convertToURL)
        return data;
    return convertURIListToURL(data);
}

bool Clipboard::setData(const String& type, const String& data)
{
    if (!canWriteData())
        return false;

    return m_dataObject->setData(normalizeType(type), data);
}

// extensions beyond IE's API
Vector<String> Clipboard::types() const
{
    Vector<String> types;
    if (!canReadTypes())
        return types;

    ListHashSet<String> typesSet = m_dataObject->types();
    types.appendRange(typesSet.begin(), typesSet.end());
    return types;
}

PassRefPtrWillBeRawPtr<FileList> Clipboard::files() const
{
    RefPtrWillBeRawPtr<FileList> files = FileList::create();
    if (!canReadData())
        return files.release();

    for (size_t i = 0; i < m_dataObject->length(); ++i) {
        if (m_dataObject->item(i)->kind() == DataObjectItem::FileKind) {
            RefPtrWillBeRawPtr<Blob> blob = m_dataObject->item(i)->getAsFile();
            if (blob && blob->isFile())
                files->append(toFile(blob.get()));
        }
    }

    return files.release();
}

void Clipboard::setDragImage(Element* image, int x, int y, ExceptionState& exceptionState)
{
    if (!isForDragAndDrop())
        return;

    if (!image) {
        exceptionState.throwTypeError("setDragImage: Invalid first argument");
        return;
    }
    IntPoint location(x, y);
    if (isHTMLImageElement(*image) && !image->inDocument())
        setDragImageResource(toHTMLImageElement(*image).cachedImage(), location);
    else
        setDragImageElement(image, location);
}

void Clipboard::clearDragImage()
{
    if (!canSetDragImage())
        return;

    m_dragImage = 0;
    m_dragLoc = IntPoint();
    m_dragImageElement = nullptr;
}

void Clipboard::setDragImageResource(ImageResource* img, const IntPoint& loc)
{
    setDragImage(img, 0, loc);
}

void Clipboard::setDragImageElement(Node* node, const IntPoint& loc)
{
    setDragImage(0, node, loc);
}

PassOwnPtr<DragImage> Clipboard::createDragImage(IntPoint& loc, LocalFrame* frame) const
{
    if (m_dragImageElement) {
        loc = m_dragLoc;

        return frame->nodeImage(*m_dragImageElement);
    }
    if (m_dragImage) {
        loc = m_dragLoc;
        return DragImage::create(m_dragImage->image());
    }
    return nullptr;
}

static ImageResource* getImageResource(Element* element)
{
    // Attempt to pull ImageResource from element
    ASSERT(element);
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isImage())
        return 0;

    RenderImage* image = toRenderImage(renderer);
    if (image->cachedImage() && !image->cachedImage()->errorOccurred())
        return image->cachedImage();

    return 0;
}

static void writeImageToDataObject(DataObject* dataObject, Element* element, const KURL& url)
{
    // Shove image data into a DataObject for use as a file
    ImageResource* cachedImage = getImageResource(element);
    if (!cachedImage || !cachedImage->imageForRenderer(element->renderer()) || !cachedImage->isLoaded())
        return;

    SharedBuffer* imageBuffer = cachedImage->imageForRenderer(element->renderer())->data();
    if (!imageBuffer || !imageBuffer->size())
        return;

    String imageExtension = cachedImage->image()->filenameExtension();
    ASSERT(!imageExtension.isEmpty());

    // Determine the filename for the file contents of the image.
    String filename = cachedImage->response().suggestedFilename();
    if (filename.isEmpty())
        filename = url.lastPathComponent();

    String fileExtension;
    if (filename.isEmpty()) {
        filename = element->getAttribute(HTMLNames::altAttr);
    } else {
        // Strip any existing extension. Assume that alt text is usually not a filename.
        int extensionIndex = filename.reverseFind('.');
        if (extensionIndex != -1) {
            fileExtension = filename.substring(extensionIndex + 1);
            filename.truncate(extensionIndex);
        }
    }

    if (!fileExtension.isEmpty() && fileExtension != imageExtension) {
        String imageMimeType = MIMETypeRegistry::getMIMETypeForExtension(imageExtension);
        ASSERT(imageMimeType.startsWith("image/"));
        // Use the file extension only if it has imageMimeType: it's untrustworthy otherwise.
        if (imageMimeType == MIMETypeRegistry::getMIMETypeForExtension(fileExtension))
            imageExtension = fileExtension;
    }

    imageExtension = "." + imageExtension;
    validateFilename(filename, imageExtension);

    dataObject->addSharedBuffer(filename + imageExtension, imageBuffer);
}

void Clipboard::declareAndWriteDragImage(Element* element, const KURL& url, const String& title)
{
    if (!m_dataObject)
        return;

    m_dataObject->setURLAndTitle(url, title);

    // Write the bytes in the image to the file format.
    writeImageToDataObject(m_dataObject.get(), element, url);

    // Put img tag on the clipboard referencing the image
    m_dataObject->setData(mimeTypeTextHTML, createMarkup(element, IncludeNode, 0, ResolveAllURLs));
}

void Clipboard::writeURL(const KURL& url, const String& title)
{
    if (!m_dataObject)
        return;
    ASSERT(!url.isEmpty());

    m_dataObject->setURLAndTitle(url, title);

    // The URL can also be used as plain text.
    m_dataObject->setData(mimeTypeTextPlain, url.string());

    // The URL can also be used as an HTML fragment.
    m_dataObject->setHTMLAndBaseURL(urlToMarkup(url, title), url);
}

void Clipboard::writeRange(Range* selectedRange, LocalFrame* frame)
{
    ASSERT(selectedRange);
    if (!m_dataObject)
        return;

    m_dataObject->setHTMLAndBaseURL(createMarkup(selectedRange, 0, AnnotateForInterchange, false, ResolveNonLocalURLs), frame->document()->url());

    String str = frame->selectedTextForClipboard();
#if OS(WIN)
    replaceNewlinesWithWindowsStyleNewlines(str);
#endif
    replaceNBSPWithSpace(str);
    m_dataObject->setData(mimeTypeTextPlain, str);
}

void Clipboard::writePlainText(const String& text)
{
    if (!m_dataObject)
        return;

    String str = text;
#if OS(WIN)
    replaceNewlinesWithWindowsStyleNewlines(str);
#endif
    replaceNBSPWithSpace(str);

    m_dataObject->setData(mimeTypeTextPlain, str);
}

bool Clipboard::hasData()
{
    ASSERT(isForDragAndDrop());

    return m_dataObject->length() > 0;
}

void Clipboard::setAccessPolicy(ClipboardAccessPolicy policy)
{
    // once you go numb, can never go back
    ASSERT(m_policy != ClipboardNumb || policy == ClipboardNumb);
    m_policy = policy;
}

bool Clipboard::canReadTypes() const
{
    return m_policy == ClipboardReadable || m_policy == ClipboardTypesReadable || m_policy == ClipboardWritable;
}

bool Clipboard::canReadData() const
{
    return m_policy == ClipboardReadable || m_policy == ClipboardWritable;
}

bool Clipboard::canWriteData() const
{
    return m_policy == ClipboardWritable;
}

bool Clipboard::canSetDragImage() const
{
    return m_policy == ClipboardImageWritable || m_policy == ClipboardWritable;
}

DragOperation Clipboard::sourceOperation() const
{
    DragOperation op = dragOpFromIEOp(m_effectAllowed);
    ASSERT(op != DragOperationPrivate);
    return op;
}

DragOperation Clipboard::destinationOperation() const
{
    DragOperation op = dragOpFromIEOp(m_dropEffect);
    ASSERT(op == DragOperationCopy || op == DragOperationNone || op == DragOperationLink || op == (DragOperation)(DragOperationGeneric | DragOperationMove) || op == DragOperationEvery);
    return op;
}

void Clipboard::setSourceOperation(DragOperation op)
{
    ASSERT_ARG(op, op != DragOperationPrivate);
    m_effectAllowed = IEOpFromDragOp(op);
}

void Clipboard::setDestinationOperation(DragOperation op)
{
    ASSERT_ARG(op, op == DragOperationCopy || op == DragOperationNone || op == DragOperationLink || op == DragOperationGeneric || op == DragOperationMove || op == (DragOperation)(DragOperationGeneric | DragOperationMove));
    m_dropEffect = IEOpFromDragOp(op);
}

bool Clipboard::hasDropZoneType(const String& keyword)
{
    if (keyword.startsWith("file:"))
        return hasFileOfType(keyword.substring(5));

    if (keyword.startsWith("string:"))
        return hasStringOfType(keyword.substring(7));

    return false;
}

PassRefPtrWillBeRawPtr<DataTransferItemList> Clipboard::items()
{
    // FIXME: According to the spec, we are supposed to return the same collection of items each
    // time. We now return a wrapper that always wraps the *same* set of items, so JS shouldn't be
    // able to tell, but we probably still want to fix this.
    return DataTransferItemList::create(this, m_dataObject);
}

PassRefPtrWillBeRawPtr<DataObject> Clipboard::dataObject() const
{
    return m_dataObject;
}

Clipboard::Clipboard(ClipboardType type, ClipboardAccessPolicy policy, PassRefPtrWillBeRawPtr<DataObject> dataObject)
    : m_policy(policy)
    , m_dropEffect("uninitialized")
    , m_effectAllowed("uninitialized")
    , m_clipboardType(type)
    , m_dataObject(dataObject)
{
    ScriptWrappable::init(this);
}

void Clipboard::setDragImage(ImageResource* image, Node* node, const IntPoint& loc)
{
    if (!canSetDragImage())
        return;

    m_dragImage = image;
    m_dragLoc = loc;
    m_dragImageElement = node;
}

bool Clipboard::hasFileOfType(const String& type) const
{
    if (!canReadTypes())
        return false;

    RefPtrWillBeRawPtr<FileList> fileList = files();
    if (fileList->isEmpty())
        return false;

    for (unsigned f = 0; f < fileList->length(); f++) {
        if (equalIgnoringCase(fileList->item(f)->type(), type))
            return true;
    }
    return false;
}

bool Clipboard::hasStringOfType(const String& type) const
{
    if (!canReadTypes())
        return false;

    return types().contains(type);
}

DragOperation convertDropZoneOperationToDragOperation(const String& dragOperation)
{
    if (dragOperation == "copy")
        return DragOperationCopy;
    if (dragOperation == "move")
        return DragOperationMove;
    if (dragOperation == "link")
        return DragOperationLink;
    return DragOperationNone;
}

String convertDragOperationToDropZoneOperation(DragOperation operation)
{
    switch (operation) {
    case DragOperationCopy:
        return String("copy");
    case DragOperationMove:
        return String("move");
    case DragOperationLink:
        return String("link");
    default:
        return String("copy");
    }
}

void Clipboard::trace(Visitor* visitor)
{
    visitor->trace(m_dataObject);
    visitor->trace(m_dragImageElement);
}

} // namespace WebCore
