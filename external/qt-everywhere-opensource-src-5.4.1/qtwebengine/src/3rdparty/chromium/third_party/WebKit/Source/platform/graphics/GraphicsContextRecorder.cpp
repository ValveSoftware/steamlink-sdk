/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/graphics/GraphicsContextRecorder.h"

#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/ImageSource.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/ImageFrame.h"
#include "platform/image-encoders/skia/PNGImageEncoder.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkStream.h"
#include "wtf/HexNumber.h"
#include "wtf/text/Base64.h"
#include "wtf/text/TextEncoding.h"

namespace WebCore {

GraphicsContext* GraphicsContextRecorder::record(const IntSize& size, bool isCertainlyOpaque)
{
    ASSERT(!m_picture);
    ASSERT(!m_recorder);
    ASSERT(!m_context);
    m_isCertainlyOpaque = isCertainlyOpaque;
    m_recorder = adoptPtr(new SkPictureRecorder);
    SkCanvas* canvas = m_recorder->beginRecording(size.width(), size.height(), 0, 0);
    m_context = adoptPtr(new GraphicsContext(canvas));
    m_context->setTrackOpaqueRegion(isCertainlyOpaque);
    m_context->setCertainlyOpaque(isCertainlyOpaque);
    return m_context.get();
}

PassRefPtr<GraphicsContextSnapshot> GraphicsContextRecorder::stop()
{
    m_context.clear();
    m_picture = adoptRef(m_recorder->endRecording());
    m_recorder.clear();
    return adoptRef(new GraphicsContextSnapshot(m_picture.release(), m_isCertainlyOpaque));
}

GraphicsContextSnapshot::GraphicsContextSnapshot(PassRefPtr<SkPicture> picture, bool isCertainlyOpaque)
    : m_picture(picture)
    , m_isCertainlyOpaque(isCertainlyOpaque)
{
}


class SnapshotPlayer : public SkDrawPictureCallback {
    WTF_MAKE_NONCOPYABLE(SnapshotPlayer);
public:
    explicit SnapshotPlayer(PassRefPtr<SkPicture> picture, SkCanvas* canvas)
        : m_picture(picture)
        , m_canvas(canvas)
    {
    }

    SkCanvas* canvas() { return m_canvas; }

    void play()
    {
        m_picture->draw(m_canvas, this);
    }

private:
    RefPtr<SkPicture> m_picture;
    SkCanvas* m_canvas;
};

class FragmentSnapshotPlayer : public SnapshotPlayer {
public:
    FragmentSnapshotPlayer(PassRefPtr<SkPicture> picture, SkCanvas* canvas)
        : SnapshotPlayer(picture, canvas)
    {
    }

    void play(unsigned fromStep, unsigned toStep)
    {
        m_fromStep = fromStep;
        m_toStep = toStep;
        m_stepCount = 0;
        SnapshotPlayer::play();
    }

    virtual bool abortDrawing() OVERRIDE
    {
        ++m_stepCount;
        if (m_stepCount == m_fromStep) {
            const SkBitmap& bitmap = canvas()->getDevice()->accessBitmap(true);
            bitmap.eraseARGB(0, 0, 0, 0); // FIXME: get layers background color, it might make resulting image a bit more plausable.
        }
        return m_toStep && m_stepCount > m_toStep;
    }

private:
    unsigned m_fromStep;
    unsigned m_toStep;
    unsigned m_stepCount;
};

class ProfilingSnapshotPlayer : public SnapshotPlayer {
public:
    ProfilingSnapshotPlayer(PassRefPtr<SkPicture> picture, SkCanvas* canvas)
        : SnapshotPlayer(picture, canvas)
    {
    }

    void play(GraphicsContextSnapshot::Timings* timingsVector, unsigned minRepeatCount, double minDuration)
    {
        m_timingsVector = timingsVector;
        m_timingsVector->reserveCapacity(minRepeatCount);

        double now = WTF::monotonicallyIncreasingTime();
        double stopTime = now + minDuration;
        for (unsigned step = 0; step < minRepeatCount || now < stopTime; ++step) {
            m_timingsVector->append(Vector<double>());
            m_currentTimings = &m_timingsVector->last();
            if (m_timingsVector->size() > 1)
                m_currentTimings->reserveCapacity(m_timingsVector->begin()->size());
            SnapshotPlayer::play();
            now = WTF::monotonicallyIncreasingTime();
            m_currentTimings->append(now);
        }
    }

    virtual bool abortDrawing() OVERRIDE
    {
        m_currentTimings->append(WTF::monotonicallyIncreasingTime());
        return false;
    }

    const GraphicsContextSnapshot::Timings& timingsVector() const { return *m_timingsVector; }

private:
    GraphicsContextSnapshot::Timings* m_timingsVector;
    Vector<double>* m_currentTimings;
};

class LoggingCanvas : public SkCanvas {
public:
    LoggingCanvas(int width, int height) : SkCanvas(width, height)
    {
        m_log = JSONArray::create();
    }

    void clear(SkColor color) OVERRIDE
    {
        addItemWithParams("clear")->setString("color", stringForSkColor(color));
    }

    void drawPaint(const SkPaint& paint) OVERRIDE
    {
        addItemWithParams("drawPaint")->setObject("paint", objectForSkPaint(paint));
    }

    void drawPoints(PointMode mode, size_t count, const SkPoint pts[], const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawPoints");
        params->setString("pointMode", pointModeName(mode));
        params->setArray("points", arrayForSkPoints(count, pts));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void drawRect(const SkRect& rect, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawRect");
        params->setObject("rect", objectForSkRect(rect));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void drawOval(const SkRect& oval, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawOval");
        params->setObject("oval", objectForSkRect(oval));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void drawRRect(const SkRRect& rrect, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawRRect");
        params->setObject("rrect", objectForSkRRect(rrect));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void drawPath(const SkPath& path, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawPath");
        params->setObject("path", objectForSkPath(path));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void drawBitmap(const SkBitmap& bitmap, SkScalar left, SkScalar top, const SkPaint* paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawBitmap");
        params->setNumber("left", left);
        params->setNumber("top", top);
        params->setObject("bitmap", objectForSkBitmap(bitmap));
        params->setObject("paint", objectForSkPaint(*paint));
    }

    void drawBitmapRectToRect(const SkBitmap& bitmap, const SkRect* src, const SkRect& dst, const SkPaint* paint, DrawBitmapRectFlags flags) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawBitmapRectToRect");
        params->setObject("bitmap", objectForSkBitmap(bitmap));
        params->setObject("src", objectForSkRect(*src));
        params->setObject("dst", objectForSkRect(dst));
        params->setObject("paint", objectForSkPaint(*paint));
        params->setNumber("flags", flags);
    }

    void drawBitmapMatrix(const SkBitmap& bitmap, const SkMatrix& m, const SkPaint* paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawBitmapMatrix");
        params->setObject("bitmap", objectForSkBitmap(bitmap));
        params->setArray("matrix", arrayForSkMatrix(m));
        params->setObject("paint", objectForSkPaint(*paint));
    }

    void drawBitmapNine(const SkBitmap& bitmap, const SkIRect& center, const SkRect& dst, const SkPaint* paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawBitmapNine");
        params->setObject("bitmap", objectForSkBitmap(bitmap));
        params->setObject("center", objectForSkIRect(center));
        params->setObject("dst", objectForSkRect(dst));
        params->setObject("paint", objectForSkPaint(*paint));
    }

    void drawSprite(const SkBitmap& bitmap, int left, int top, const SkPaint* paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawSprite");
        params->setObject("bitmap", objectForSkBitmap(bitmap));
        params->setNumber("left", left);
        params->setNumber("top", top);
        params->setObject("paint", objectForSkPaint(*paint));
    }

    void drawVertices(VertexMode vmode, int vertexCount, const SkPoint vertices[], const SkPoint texs[], const SkColor colors[], SkXfermode* xmode,
        const uint16_t indices[], int indexCount, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawVertices");
        params->setObject("paint", objectForSkPaint(paint));
    }

    void drawData(const void* data, size_t length) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawData");
        params->setNumber("length", length);
    }

    void beginCommentGroup(const char* description) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("beginCommentGroup");
        params->setString("description", description);
    }

    void addComment(const char* keyword, const char* value) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("addComment");
        params->setString("key", keyword);
        params->setString("value", value);
    }

    void endCommentGroup() OVERRIDE
    {
        addItem("endCommentGroup");
    }

    void onDrawDRRect(const SkRRect& outer, const SkRRect& inner, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawDRRect");
        params->setObject("outer", objectForSkRRect(outer));
        params->setObject("inner", objectForSkRRect(inner));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void onDrawText(const void* text, size_t byteLength, SkScalar x, SkScalar y, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawText");
        params->setString("text", stringForText(text, byteLength, paint));
        params->setNumber("x", x);
        params->setNumber("y", y);
        params->setObject("paint", objectForSkPaint(paint));
    }

    void onDrawPosText(const void* text, size_t byteLength, const SkPoint pos[], const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawPosText");
        params->setString("text", stringForText(text, byteLength, paint));
        size_t pointsCount = paint.countText(text, byteLength);
        params->setArray("pos", arrayForSkPoints(pointsCount, pos));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void onDrawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[], SkScalar constY, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawPosTextH");
        params->setString("text", stringForText(text, byteLength, paint));
        size_t pointsCount = paint.countText(text, byteLength);
        params->setArray("xpos", arrayForSkScalars(pointsCount, xpos));
        params->setNumber("constY", constY);
        params->setObject("paint", objectForSkPaint(paint));
    }

    void onDrawTextOnPath(const void* text, size_t byteLength, const SkPath& path, const SkMatrix* matrix, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawTextOnPath");
        params->setString("text", stringForText(text, byteLength, paint));
        params->setObject("path", objectForSkPath(path));
        params->setArray("matrix", arrayForSkMatrix(*matrix));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void onPushCull(const SkRect& cullRect) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("pushCull");
        params->setObject("cullRect", objectForSkRect(cullRect));
    }

    void onPopCull() OVERRIDE
    {
        addItem("popCull");
    }

    void onClipRect(const SkRect& rect, SkRegion::Op op, ClipEdgeStyle style) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("clipRect");
        params->setObject("rect", objectForSkRect(rect));
        params->setString("SkRegion::Op", regionOpName(op));
        params->setBoolean("softClipEdgeStyle", kSoft_ClipEdgeStyle == style);
    }

    void onClipRRect(const SkRRect& rrect, SkRegion::Op op, ClipEdgeStyle style) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("clipRRect");
        params->setObject("rrect", objectForSkRRect(rrect));
        params->setString("SkRegion::Op", regionOpName(op));
        params->setBoolean("softClipEdgeStyle", kSoft_ClipEdgeStyle == style);
    }

    void onClipPath(const SkPath& path, SkRegion::Op op, ClipEdgeStyle style) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("clipPath");
        params->setObject("path", objectForSkPath(path));
        params->setString("SkRegion::Op", regionOpName(op));
        params->setBoolean("softClipEdgeStyle", kSoft_ClipEdgeStyle == style);
    }

    void onClipRegion(const SkRegion& region, SkRegion::Op op) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("clipRegion");
        params->setString("op", regionOpName(op));
    }

    void onDrawPicture(const SkPicture* picture) OVERRIDE
    {
        addItemWithParams("drawPicture")->setObject("picture", objectForSkPicture(*picture));
    }

    void didSetMatrix(const SkMatrix& matrix) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("setMatrix");
        params->setArray("matrix", arrayForSkMatrix(matrix));
        this->SkCanvas::didSetMatrix(matrix);
    }

    void didConcat(const SkMatrix& matrix) OVERRIDE
    {
        switch (matrix.getType()) {
        case SkMatrix::kTranslate_Mask:
            translate(matrix.getTranslateX(), matrix.getTranslateY());
            break;
        case SkMatrix::kScale_Mask:
            scale(matrix.getScaleX(), matrix.getScaleY());
            break;
        default:
            concat(matrix);
        }
        this->SkCanvas::didConcat(matrix);
    }

    void willRestore() OVERRIDE
    {
        addItem("restore");
        this->SkCanvas::willRestore();
    }

    SaveLayerStrategy willSaveLayer(const SkRect* bounds, const SkPaint* paint, SaveFlags flags) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("saveLayer");
        if (bounds)
            params->setObject("bounds", objectForSkRect(*bounds));
        params->setObject("paint", objectForSkPaint(*paint));
        params->setString("saveFlags", saveFlagsToString(flags));
        this->SkCanvas::willSaveLayer(bounds, paint, flags);
        return kNoLayer_SaveLayerStrategy;
    }

    void willSave() OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("save");
        this->SkCanvas::willSave();
    }

    bool isClipEmpty() const OVERRIDE
    {
        return false;
    }

    bool isClipRect() const OVERRIDE
    {
        return true;
    }

#ifdef SK_SUPPORT_LEGACY_GETCLIPTYPE
    ClipType getClipType() const OVERRIDE
    {
        return kRect_ClipType;
    }
#endif

    bool getClipBounds(SkRect* bounds) const OVERRIDE
    {
        if (bounds)
            bounds->setXYWH(0, 0, SkIntToScalar(this->imageInfo().fWidth), SkIntToScalar(this->imageInfo().fHeight));
        return true;
    }

    bool getClipDeviceBounds(SkIRect* bounds) const OVERRIDE
    {
        if (bounds)
            bounds->setLargest();
        return true;
    }

    PassRefPtr<JSONArray> log()
    {
        return m_log;
    }

private:
    RefPtr<JSONArray> m_log;

    PassRefPtr<JSONObject> addItem(const String& name)
    {
        RefPtr<JSONObject> item = JSONObject::create();
        item->setString("method", name);
        m_log->pushObject(item);
        return item.release();
    }

    PassRefPtr<JSONObject> addItemWithParams(const String& name)
    {
        RefPtr<JSONObject> item = addItem(name);
        RefPtr<JSONObject> params = JSONObject::create();
        item->setObject("params", params);
        return params.release();
    }

    PassRefPtr<JSONObject> objectForSkRect(const SkRect& rect)
    {
        RefPtr<JSONObject> rectItem = JSONObject::create();
        rectItem->setNumber("left", rect.left());
        rectItem->setNumber("top", rect.top());
        rectItem->setNumber("right", rect.right());
        rectItem->setNumber("bottom", rect.bottom());
        return rectItem.release();
    }

    PassRefPtr<JSONObject> objectForSkIRect(const SkIRect& rect)
    {
        RefPtr<JSONObject> rectItem = JSONObject::create();
        rectItem->setNumber("left", rect.left());
        rectItem->setNumber("top", rect.top());
        rectItem->setNumber("right", rect.right());
        rectItem->setNumber("bottom", rect.bottom());
        return rectItem.release();
    }

    String pointModeName(PointMode mode)
    {
        switch (mode) {
        case SkCanvas::kPoints_PointMode: return "Points";
        case SkCanvas::kLines_PointMode: return "Lines";
        case SkCanvas::kPolygon_PointMode: return "Polygon";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    PassRefPtr<JSONObject> objectForSkPoint(const SkPoint& point)
    {
        RefPtr<JSONObject> pointItem = JSONObject::create();
        pointItem->setNumber("x", point.x());
        pointItem->setNumber("y", point.y());
        return pointItem.release();
    }

    PassRefPtr<JSONArray> arrayForSkPoints(size_t count, const SkPoint points[])
    {
        RefPtr<JSONArray> pointsArrayItem = JSONArray::create();
        for (size_t i = 0; i < count; ++i)
            pointsArrayItem->pushObject(objectForSkPoint(points[i]));
        return pointsArrayItem.release();
    }

    PassRefPtr<JSONObject> objectForSkPicture(const SkPicture& picture)
    {
        RefPtr<JSONObject> pictureItem = JSONObject::create();
        pictureItem->setNumber("width", picture.width());
        pictureItem->setNumber("height", picture.height());
        return pictureItem.release();
    }

    PassRefPtr<JSONObject> objectForRadius(const SkRRect& rrect, SkRRect::Corner corner)
    {
        RefPtr<JSONObject> radiusItem = JSONObject::create();
        SkVector radius = rrect.radii(corner);
        radiusItem->setNumber("xRadius", radius.x());
        radiusItem->setNumber("yRadius", radius.y());
        return radiusItem.release();
    }

    String rrectTypeName(SkRRect::Type type)
    {
        switch (type) {
        case SkRRect::kEmpty_Type: return "Empty";
        case SkRRect::kRect_Type: return "Rect";
        case SkRRect::kOval_Type: return "Oval";
        case SkRRect::kSimple_Type: return "Simple";
        case SkRRect::kNinePatch_Type: return "Nine-patch";
        case SkRRect::kComplex_Type: return "Complex";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    String radiusName(SkRRect::Corner corner)
    {
        switch (corner) {
        case SkRRect::kUpperLeft_Corner: return "upperLeftRadius";
        case SkRRect::kUpperRight_Corner: return "upperRightRadius";
        case SkRRect::kLowerRight_Corner: return "lowerRightRadius";
        case SkRRect::kLowerLeft_Corner: return "lowerLeftRadius";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        }
    }

    PassRefPtr<JSONObject> objectForSkRRect(const SkRRect& rrect)
    {
        RefPtr<JSONObject> rrectItem = JSONObject::create();
        rrectItem->setString("type", rrectTypeName(rrect.type()));
        rrectItem->setNumber("left", rrect.rect().left());
        rrectItem->setNumber("top", rrect.rect().top());
        rrectItem->setNumber("right", rrect.rect().right());
        rrectItem->setNumber("bottom", rrect.rect().bottom());
        for (int i = 0; i < 4; ++i)
            rrectItem->setObject(radiusName((SkRRect::Corner) i), objectForRadius(rrect, (SkRRect::Corner) i));
        return rrectItem.release();
    }

    String fillTypeName(SkPath::FillType type)
    {
        switch (type) {
        case SkPath::kWinding_FillType: return "Winding";
        case SkPath::kEvenOdd_FillType: return "EvenOdd";
        case SkPath::kInverseWinding_FillType: return "InverseWinding";
        case SkPath::kInverseEvenOdd_FillType: return "InverseEvenOdd";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    String convexityName(SkPath::Convexity convexity)
    {
        switch (convexity) {
        case SkPath::kUnknown_Convexity: return "Unknown";
        case SkPath::kConvex_Convexity: return "Convex";
        case SkPath::kConcave_Convexity: return "Concave";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    String verbName(SkPath::Verb verb)
    {
        switch (verb) {
        case SkPath::kMove_Verb: return "Move";
        case SkPath::kLine_Verb: return "Line";
        case SkPath::kQuad_Verb: return "Quad";
        case SkPath::kConic_Verb: return "Conic";
        case SkPath::kCubic_Verb: return "Cubic";
        case SkPath::kClose_Verb: return "Close";
        case SkPath::kDone_Verb: return "Done";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    struct VerbParams {
        String name;
        unsigned pointCount;
        unsigned pointOffset;

        VerbParams(const String& name, unsigned pointCount, unsigned pointOffset)
            : name(name)
            , pointCount(pointCount)
            , pointOffset(pointOffset) { }
    };

    VerbParams segmentParams(SkPath::Verb verb)
    {
        switch (verb) {
        case SkPath::kMove_Verb: return VerbParams("Move", 1, 0);
        case SkPath::kLine_Verb: return VerbParams("Line", 1, 1);
        case SkPath::kQuad_Verb: return VerbParams("Quad", 2, 1);
        case SkPath::kConic_Verb: return VerbParams("Conic", 2, 1);
        case SkPath::kCubic_Verb: return VerbParams("Cubic", 3, 1);
        case SkPath::kClose_Verb: return VerbParams("Close", 0, 0);
        case SkPath::kDone_Verb: return VerbParams("Done", 0, 0);
        default:
            ASSERT_NOT_REACHED();
            return VerbParams("?", 0, 0);
        };
    }

    PassRefPtr<JSONObject> objectForSkPath(const SkPath& path)
    {
        RefPtr<JSONObject> pathItem = JSONObject::create();
        pathItem->setString("fillType", fillTypeName(path.getFillType()));
        pathItem->setString("convexity", convexityName(path.getConvexity()));
        pathItem->setBoolean("isRect", path.isRect(0));
        SkPath::Iter iter(path, false);
        SkPoint points[4];
        RefPtr<JSONArray> pathPointsArray = JSONArray::create();
        for (SkPath::Verb verb = iter.next(points, false); verb != SkPath::kDone_Verb; verb = iter.next(points, false)) {
            VerbParams verbParams = segmentParams(verb);
            RefPtr<JSONObject> pathPointItem = JSONObject::create();
            pathPointItem->setString("verb", verbParams.name);
            ASSERT(verbParams.pointCount + verbParams.pointOffset <= WTF_ARRAY_LENGTH(points));
            pathPointItem->setArray("points", arrayForSkPoints(verbParams.pointCount, points + verbParams.pointOffset));
            if (SkPath::kConic_Verb == verb)
                pathPointItem->setNumber("conicWeight", iter.conicWeight());
            pathPointsArray->pushObject(pathPointItem);
        }
        pathItem->setArray("pathPoints", pathPointsArray);
        pathItem->setObject("bounds", objectForSkRect(path.getBounds()));
        return pathItem.release();
    }

    String configName(SkBitmap::Config config)
    {
        switch (config) {
        case SkBitmap::kNo_Config: return "None";
        case SkBitmap::kA8_Config: return "A8";
        case SkBitmap::kIndex8_Config: return "Index8";
        case SkBitmap::kRGB_565_Config: return "RGB565";
        case SkBitmap::kARGB_4444_Config: return "ARGB4444";
        case SkBitmap::kARGB_8888_Config: return "ARGB8888";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    PassRefPtr<JSONObject> objectForBitmapData(const SkBitmap& bitmap)
    {
        RefPtr<JSONObject> dataItem = JSONObject::create();
        Vector<unsigned char> output;
        WebCore::PNGImageEncoder::encode(bitmap, &output);
        dataItem->setString("base64", WTF::base64Encode(reinterpret_cast<char*>(output.data()), output.size()));
        dataItem->setString("mimeType", "image/png");
        return dataItem.release();
    }

    PassRefPtr<JSONObject> objectForSkBitmap(const SkBitmap& bitmap)
    {
        RefPtr<JSONObject> bitmapItem = JSONObject::create();
        bitmapItem->setNumber("width", bitmap.width());
        bitmapItem->setNumber("height", bitmap.height());
        bitmapItem->setString("config", configName(bitmap.config()));
        bitmapItem->setBoolean("opaque", bitmap.isOpaque());
        bitmapItem->setBoolean("immutable", bitmap.isImmutable());
        bitmapItem->setBoolean("volatile", bitmap.isVolatile());
        bitmapItem->setNumber("genID", bitmap.getGenerationID());
        bitmapItem->setObject("data", objectForBitmapData(bitmap));
        return bitmapItem.release();
    }

    PassRefPtr<JSONObject> objectForSkShader(const SkShader& shader)
    {
        RefPtr<JSONObject> shaderItem = JSONObject::create();
        const SkMatrix localMatrix = shader.getLocalMatrix();
        if (!localMatrix.isIdentity())
            shaderItem->setArray("localMatrix", arrayForSkMatrix(localMatrix));
        return shaderItem.release();
    }

    String stringForSkColor(const SkColor& color)
    {
        String colorString = "#";
        appendUnsignedAsHex(color, colorString);
        return colorString;
    }

    void appendFlagToString(String* flagsString, bool isSet, const String& name)
    {
        if (!isSet)
            return;
        if (flagsString->length())
            flagsString->append("|");
        flagsString->append(name);
    }

    String stringForSkPaintFlags(const SkPaint& paint)
    {
        if (!paint.getFlags())
            return "none";
        String flagsString = "";
        appendFlagToString(&flagsString, paint.isAntiAlias(), "AntiAlias");
        appendFlagToString(&flagsString, paint.isDither(), "Dither");
        appendFlagToString(&flagsString, paint.isUnderlineText(), "UnderlinText");
        appendFlagToString(&flagsString, paint.isStrikeThruText(), "StrikeThruText");
        appendFlagToString(&flagsString, paint.isFakeBoldText(), "FakeBoldText");
        appendFlagToString(&flagsString, paint.isLinearText(), "LinearText");
        appendFlagToString(&flagsString, paint.isSubpixelText(), "SubpixelText");
        appendFlagToString(&flagsString, paint.isDevKernText(), "DevKernText");
        appendFlagToString(&flagsString, paint.isLCDRenderText(), "LCDRenderText");
        appendFlagToString(&flagsString, paint.isEmbeddedBitmapText(), "EmbeddedBitmapText");
        appendFlagToString(&flagsString, paint.isAutohinted(), "Autohinted");
        appendFlagToString(&flagsString, paint.isVerticalText(), "VerticalText");
        appendFlagToString(&flagsString, paint.getFlags() & SkPaint::kGenA8FromLCD_Flag, "GenA8FromLCD");
        return flagsString;
    }

    String filterLevelName(SkPaint::FilterLevel filterLevel)
    {
        switch (filterLevel) {
        case SkPaint::kNone_FilterLevel: return "None";
        case SkPaint::kLow_FilterLevel: return "Low";
        case SkPaint::kMedium_FilterLevel: return "Medium";
        case SkPaint::kHigh_FilterLevel: return "High";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    String textAlignName(SkPaint::Align align)
    {
        switch (align) {
        case SkPaint::kLeft_Align: return "Left";
        case SkPaint::kCenter_Align: return "Center";
        case SkPaint::kRight_Align: return "Right";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    String strokeCapName(SkPaint::Cap cap)
    {
        switch (cap) {
        case SkPaint::kButt_Cap: return "Butt";
        case SkPaint::kRound_Cap: return "Round";
        case SkPaint::kSquare_Cap: return "Square";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    String strokeJoinName(SkPaint::Join join)
    {
        switch (join) {
        case SkPaint::kMiter_Join: return "Miter";
        case SkPaint::kRound_Join: return "Round";
        case SkPaint::kBevel_Join: return "Bevel";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    String styleName(SkPaint::Style style)
    {
        switch (style) {
        case SkPaint::kFill_Style: return "Fill";
        case SkPaint::kStroke_Style: return "Stroke";
        case SkPaint::kStrokeAndFill_Style: return "StrokeAndFill";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    String textEncodingName(SkPaint::TextEncoding encoding)
    {
        switch (encoding) {
        case SkPaint::kUTF8_TextEncoding: return "UTF-8";
        case SkPaint::kUTF16_TextEncoding: return "UTF-16";
        case SkPaint::kUTF32_TextEncoding: return "UTF-32";
        case SkPaint::kGlyphID_TextEncoding: return "GlyphID";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    String hintingName(SkPaint::Hinting hinting)
    {
        switch (hinting) {
        case SkPaint::kNo_Hinting: return "None";
        case SkPaint::kSlight_Hinting: return "Slight";
        case SkPaint::kNormal_Hinting: return "Normal";
        case SkPaint::kFull_Hinting: return "Full";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    PassRefPtr<JSONObject> objectForSkPaint(const SkPaint& paint)
    {
        RefPtr<JSONObject> paintItem = JSONObject::create();
        paintItem->setNumber("textSize", paint.getTextSize());
        paintItem->setNumber("textScaleX", paint.getTextScaleX());
        paintItem->setNumber("textSkewX", paint.getTextSkewX());
        if (SkShader* shader = paint.getShader())
            paintItem->setObject("shader", objectForSkShader(*shader));
        paintItem->setString("color", stringForSkColor(paint.getColor()));
        paintItem->setNumber("strokeWidth", paint.getStrokeWidth());
        paintItem->setNumber("strokeMiter", paint.getStrokeMiter());
        paintItem->setString("flags", stringForSkPaintFlags(paint));
        paintItem->setString("filterLevel", filterLevelName(paint.getFilterLevel()));
        paintItem->setString("textAlign", textAlignName(paint.getTextAlign()));
        paintItem->setString("strokeCap", strokeCapName(paint.getStrokeCap()));
        paintItem->setString("strokeJoin", strokeJoinName(paint.getStrokeJoin()));
        paintItem->setString("styleName", styleName(paint.getStyle()));
        paintItem->setString("textEncoding", textEncodingName(paint.getTextEncoding()));
        paintItem->setString("hinting", hintingName(paint.getHinting()));
        return paintItem.release();
    }

    PassRefPtr<JSONArray> arrayForSkMatrix(const SkMatrix& matrix)
    {
        RefPtr<JSONArray> matrixArray = JSONArray::create();
        for (int i = 0; i < 9; ++i)
            matrixArray->pushNumber(matrix[i]);
        return matrixArray.release();
    }

    PassRefPtr<JSONArray> arrayForSkScalars(size_t n, const SkScalar scalars[])
    {
        RefPtr<JSONArray> scalarsArray = JSONArray::create();
        for (size_t i = 0; i < n; ++i)
            scalarsArray->pushNumber(scalars[i]);
        return scalarsArray.release();
    }

    String regionOpName(SkRegion::Op op)
    {
        switch (op) {
        case SkRegion::kDifference_Op: return "kDifference_Op";
        case SkRegion::kIntersect_Op: return "kIntersect_Op";
        case SkRegion::kUnion_Op: return "kUnion_Op";
        case SkRegion::kXOR_Op: return "kXOR_Op";
        case SkRegion::kReverseDifference_Op: return "kReverseDifference_Op";
        case SkRegion::kReplace_Op: return "kReplace_Op";
        default: return "Unknown type";
        };
    }

    void translate(SkScalar dx, SkScalar dy)
    {
        RefPtr<JSONObject> params = addItemWithParams("translate");
        params->setNumber("dx", dx);
        params->setNumber("dy", dy);
    }

    void scale(SkScalar scaleX, SkScalar scaleY)
    {
        RefPtr<JSONObject> params = addItemWithParams("scale");
        params->setNumber("scaleX", scaleX);
        params->setNumber("scaleY", scaleY);
    }

    void concat(const SkMatrix& matrix)
    {
        RefPtr<JSONObject> params = addItemWithParams("concat");
        params->setArray("matrix", arrayForSkMatrix(matrix));
    }

    String saveFlagsToString(SkCanvas::SaveFlags flags)
    {
        String flagsString = "";
        if (flags & SkCanvas::kHasAlphaLayer_SaveFlag)
            flagsString.append("kHasAlphaLayer_SaveFlag ");
        if (flags & SkCanvas::kFullColorLayer_SaveFlag)
            flagsString.append("kFullColorLayer_SaveFlag ");
        if (flags & SkCanvas::kClipToLayer_SaveFlag)
            flagsString.append("kClipToLayer_SaveFlag ");
        return flagsString;
    }

    String textEncodingCanonicalName(SkPaint::TextEncoding encoding)
    {
        String name = textEncodingName(encoding);
        if (encoding == SkPaint::kUTF16_TextEncoding || encoding == SkPaint::kUTF32_TextEncoding)
            name.append("LE");
        return name;
    }

    String stringForUTFText(const void* text, size_t length, SkPaint::TextEncoding encoding)
    {
        return WTF::TextEncoding(textEncodingCanonicalName(encoding)).decode((const char*)text, length);
    }

    String stringForText(const void* text, size_t byteLength, const SkPaint& paint)
    {
        SkPaint::TextEncoding encoding = paint.getTextEncoding();
        switch (encoding) {
        case SkPaint::kUTF8_TextEncoding:
        case SkPaint::kUTF16_TextEncoding:
        case SkPaint::kUTF32_TextEncoding:
            return stringForUTFText(text, byteLength, encoding);
        case SkPaint::kGlyphID_TextEncoding: {
            WTF::Vector<SkUnichar> dataVector(byteLength / 2);
            SkUnichar* textData = dataVector.data();
            paint.glyphsToUnichars(static_cast<const uint16_t*>(text), byteLength / 2, textData);
            return WTF::UTF32LittleEndianEncoding().decode(reinterpret_cast<const char*>(textData), byteLength * 2);
        }
        default:
            ASSERT_NOT_REACHED();
            return "?";
        }
    }
};

static bool decodeBitmap(const void* data, size_t length, SkBitmap* result)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::create(static_cast<const char*>(data), length);
    OwnPtr<ImageDecoder> imageDecoder = ImageDecoder::create(*buffer, ImageSource::AlphaPremultiplied, ImageSource::GammaAndColorProfileIgnored);
    if (!imageDecoder)
        return false;
    imageDecoder->setData(buffer.get(), true);
    ImageFrame* frame = imageDecoder->frameBufferAtIndex(0);
    if (!frame)
        return true;
    *result = frame->getSkBitmap();
    return true;
}

PassRefPtr<GraphicsContextSnapshot> GraphicsContextSnapshot::load(const char* data, size_t size)
{
    SkMemoryStream stream(data, size);
    RefPtr<SkPicture> picture = adoptRef(SkPicture::CreateFromStream(&stream, decodeBitmap));
    if (!picture)
        return nullptr;
    return adoptRef(new GraphicsContextSnapshot(picture, false));
}

PassOwnPtr<ImageBuffer> GraphicsContextSnapshot::replay(unsigned fromStep, unsigned toStep) const
{
    OwnPtr<ImageBuffer> imageBuffer = createImageBuffer();
    FragmentSnapshotPlayer player(m_picture, imageBuffer->context()->canvas());
    player.play(fromStep, toStep);
    return imageBuffer.release();
}

PassOwnPtr<GraphicsContextSnapshot::Timings> GraphicsContextSnapshot::profile(unsigned minRepeatCount, double minDuration) const
{
    OwnPtr<GraphicsContextSnapshot::Timings> timings = adoptPtr(new GraphicsContextSnapshot::Timings());
    OwnPtr<ImageBuffer> imageBuffer = createImageBuffer();
    ProfilingSnapshotPlayer player(m_picture, imageBuffer->context()->canvas());
    player.play(timings.get(), minRepeatCount, minDuration);
    return timings.release();
}

PassOwnPtr<ImageBuffer> GraphicsContextSnapshot::createImageBuffer() const
{
    return ImageBuffer::create(IntSize(m_picture->width(), m_picture->height()), m_isCertainlyOpaque ? Opaque : NonOpaque);
}

PassRefPtr<JSONArray> GraphicsContextSnapshot::snapshotCommandLog() const
{
    LoggingCanvas canvas(m_picture->width(), m_picture->height());
    FragmentSnapshotPlayer player(m_picture, &canvas);
    player.play(0, 0);
    return canvas.log();
}

}
