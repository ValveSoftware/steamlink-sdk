// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItem_h
#define DisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/graphics/ContiguousContainer.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/Noncopyable.h"

#ifndef NDEBUG
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"
#endif

class SkPictureGpuAnalyzer;

namespace blink {

class GraphicsContext;
class IntRect;
class WebDisplayItemList;

class PLATFORM_EXPORT DisplayItem {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    enum {
        // Must be kept in sync with core/paint/PaintPhase.h.
        PaintPhaseMax = 11,
    };

    // A display item type uniquely identifies a display item of a client.
    // Some display item types can be categorized using the following directives:
    // - In enum Type:
    //   - enum value <Category>First;
    //   - enum values of the category, first of which should equal <Category>First;
    //     (for ease of maintenance, the values should be in alphabetic order)
    //   - enum value <Category>Last which should be equal to the last of the enum values of the category
    // - DEFINE_CATEGORY_METHODS(<Category>) to define is<Category>Type(Type) and is<Category>() methods.
    //
    // A category or subset of a category can contain types each of which corresponds to a PaintPhase:
    // - In enum Type:
    //   - enum value <Category>[<Subset>]PaintPhaseFirst;
    //   - enum value <Category>[<Subset>]PaintPhaseLast = <Category>[<Subset>]PaintPhaseFirst + PaintPhaseMax;
    // - DEFINE_PAINT_PHASE_CONVERSION_METHOD(<Category>[<Subset>]) to define
    //   paintPhaseTo<Category>[<Subset>]Type(PaintPhase) method.
    //
    // A category can be derived from another category, containing types each of which corresponds to a
    // value of the latter category:
    // - In enum Type:
    //   - enum value <Category>First;
    //   - enum value <Category>Last = <Category>First + <BaseCategory>Last - <BaseCategory>First;
    // - DEFINE_CONVERSION_METHODS(<Category>, <category>, <BaseCategory>, <baseCategory>) to define methods to
    //   convert types between the categories;
    enum Type {
        DrawingFirst,
        DrawingPaintPhaseFirst = DrawingFirst,
        DrawingPaintPhaseLast = DrawingFirst + PaintPhaseMax,
        BoxDecorationBackground,
        Caret,
        DragCaret,
        ColumnRules,
        DebugDrawing,
        DebugRedFill,
        DocumentBackground,
        DragImage,
        SVGImage,
        LinkHighlight,
        ImageAreaFocusRing,
        PageOverlay,
        PageWidgetDelegateBackgroundFallback,
        PopupContainerBorder,
        PopupListBoxBackground,
        PopupListBoxRow,
        PrintedContentBackground,
        PrintedContentDestinationLocations,
        PrintedContentLineBoundary,
        PrintedContentPDFURLRect,
        Resizer,
        SVGClip,
        SVGFilter,
        SVGMask,
        ScrollbarBackButtonEnd,
        ScrollbarBackButtonStart,
        ScrollbarBackground,
        ScrollbarBackTrack,
        ScrollbarCorner,
        ScrollbarForwardButtonEnd,
        ScrollbarForwardButtonStart,
        ScrollbarForwardTrack,
        ScrollbarThumb,
        ScrollbarTickmarks,
        ScrollbarTrackBackground,
        ScrollbarCompositedScrollbar,
        SelectionTint,
        TableCellBackgroundFromColumnGroup,
        TableCellBackgroundFromColumn,
        TableCellBackgroundFromSection,
        TableCellBackgroundFromRow,
        // Table collapsed borders can be painted together (e.g., left & top) but there are at most 4 phases of collapsed
        // border painting for a single cell. To disambiguate these phases of collapsed border painting, a mask is used.
        // TableCollapsedBorderBase can be larger than TableCollapsedBorderUnalignedBase to ensure the base lower bits are 0's.
        TableCollapsedBorderUnalignedBase,
        TableCollapsedBorderBase = (((TableCollapsedBorderUnalignedBase - 1) >> 4) + 1) << 4,
        TableCollapsedBorderLast = TableCollapsedBorderBase + 0x0f,
        TableSectionBoxShadowInset,
        TableSectionBoxShadowNormal,
        TableRowBoxShadowInset,
        TableRowBoxShadowNormal,
        VideoBitmap,
        WebPlugin,
        WebFont,
        ReflectionMask,
        DrawingLast = ReflectionMask,

        CachedDrawingFirst,
        CachedDrawingLast = CachedDrawingFirst + DrawingLast - DrawingFirst,

        ForeignLayerFirst,
        ForeignLayerPlugin = ForeignLayerFirst,
        ForeignLayerLast = ForeignLayerPlugin,

        ClipFirst,
        ClipBoxPaintPhaseFirst = ClipFirst,
        ClipBoxPaintPhaseLast = ClipBoxPaintPhaseFirst + PaintPhaseMax,
        ClipColumnBoundsPaintPhaseFirst,
        ClipColumnBoundsPaintPhaseLast = ClipColumnBoundsPaintPhaseFirst + PaintPhaseMax,
        ClipLayerFragmentPaintPhaseFirst,
        ClipLayerFragmentPaintPhaseLast = ClipLayerFragmentPaintPhaseFirst + PaintPhaseMax,
        ClipFileUploadControlRect,
        ClipFrameToVisibleContentRect,
        ClipFrameScrollbars,
        ClipLayerBackground,
        ClipLayerColumnBounds,
        ClipLayerFilter,
        ClipLayerForeground,
        ClipLayerParent,
        ClipLayerOverflowControls,
        ClipNodeImage,
        ClipPopupListBoxFrame,
        ClipScrollbarsToBoxBounds,
        ClipSelectionImage,
        PageWidgetDelegateClip,
        ClipPrintedPage,
        ClipLast = ClipPrintedPage,

        EndClipFirst,
        EndClipLast = EndClipFirst + ClipLast - ClipFirst,

        FloatClipFirst,
        FloatClipPaintPhaseFirst = FloatClipFirst,
        FloatClipPaintPhaseLast = FloatClipFirst + PaintPhaseMax,
        FloatClipLast = FloatClipPaintPhaseLast,
        EndFloatClipFirst,
        EndFloatClipLast = EndFloatClipFirst + FloatClipLast - FloatClipFirst,

        ScrollFirst,
        ScrollPaintPhaseFirst = ScrollFirst,
        ScrollPaintPhaseLast = ScrollPaintPhaseFirst + PaintPhaseMax,
        ScrollOverflowControls,
        ScrollLast = ScrollOverflowControls,
        EndScrollFirst,
        EndScrollLast = EndScrollFirst + ScrollLast - ScrollFirst,

        Transform3DFirst,
        Transform3DElementTransform = Transform3DFirst,
        Transform3DLast = Transform3DElementTransform,
        EndTransform3DFirst,
        EndTransform3DLast = EndTransform3DFirst + Transform3DLast - Transform3DFirst,

        BeginFilter,
        EndFilter,
        BeginCompositing,
        EndCompositing,
        BeginTransform,
        EndTransform,
        BeginClipPath,
        EndClipPath,

        Subsequence,
        EndSubsequence,
        CachedSubsequence,

        UninitializedType,
        TypeLast = UninitializedType
    };

    static_assert(TableCollapsedBorderBase >= TableCollapsedBorderUnalignedBase, "TableCollapsedBorder types overlap with other types");
    static_assert((TableCollapsedBorderBase & 0xf) == 0, "The lowest 4 bits of TableCollapsedBorderBase should be zero");
    // Bits or'ed onto TableCollapsedBorderBase to generate a real table collapsed border type.
    enum TableCollapsedBorderSides {
        TableCollapsedBorderTop = 1 << 0,
        TableCollapsedBorderRight = 1 << 1,
        TableCollapsedBorderBottom = 1 << 2,
        TableCollapsedBorderLeft = 1 << 3,
    };

    DisplayItem(const DisplayItemClient& client, Type type, size_t derivedSize)
        : m_client(&client)
        , m_type(type)
        , m_derivedSize(derivedSize)
        , m_skippedCache(false)
#ifndef NDEBUG
        , m_clientDebugString(client.debugName())
#endif
    {
        // derivedSize must fit in m_derivedSize.
        // If it doesn't, enlarge m_derivedSize and fix this assert.
        ASSERT_WITH_SECURITY_IMPLICATION(derivedSize < (1 << 8));
        ASSERT_WITH_SECURITY_IMPLICATION(derivedSize >= sizeof(*this));
    }

    virtual ~DisplayItem() { }

    // Ids are for matching new DisplayItems with existing DisplayItems.
    struct Id {
        STACK_ALLOCATED();
        Id(const DisplayItemClient& client, const Type type)
            : client(client)
            , type(type) { }

        bool matches(const DisplayItem& item) const
        {
            // We should always convert to non-cached types before matching.
            ASSERT(!isCachedType(item.m_type));
            ASSERT(!isCachedType(type));
            return &client == item.m_client && type == item.m_type;
        }

        const DisplayItemClient& client;
        const Type type;
    };

    // Convert cached type to non-cached type (e.g., Type::CachedSVGImage -> Type::SVGImage).
    static Type nonCachedType(Type type)
    {
        if (isCachedDrawingType(type))
            return cachedDrawingTypeToDrawingType(type);
        if (type == CachedSubsequence)
            return Subsequence;
        return type;
    }

    // Return the Id with cached type converted to non-cached type.
    Id nonCachedId() const
    {
        return Id(*m_client, nonCachedType(m_type));
    }

    virtual void replay(GraphicsContext&) const { }

    const DisplayItemClient& client() const { ASSERT(m_client); return *m_client; }
    Type getType() const { return m_type; }

    // Size of this object in memory, used to move it with memcpy.
    // This is not sizeof(*this), because it needs to account for the size of
    // the derived class (i.e. runtime type). Derived classes are expected to
    // supply this to the DisplayItem constructor.
    size_t derivedSize() const { return m_derivedSize; }

    // For PaintController only. Painters should use DisplayItemCacheSkipper instead.
    void setSkippedCache() { m_skippedCache = true; }
    bool skippedCache() const { return m_skippedCache; }

    virtual void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const { }

    // See comments of enum Type for usage of the following macros.
#define DEFINE_CATEGORY_METHODS(Category) \
    static bool is##Category##Type(Type type) { return type >= Category##First && type <= Category##Last; } \
    bool is##Category() const { return is##Category##Type(getType()); }

#define DEFINE_CONVERSION_METHODS(Category1, category1, Category2, category2) \
    static Type category1##TypeTo##Category2##Type(Type type) \
    { \
        static_assert(Category1##Last - Category1##First == Category2##Last - Category2##First, \
            "Categories " #Category1 " and " #Category2 " should have same number of enum values. See comments of DisplayItem::Type"); \
        ASSERT(is##Category1##Type(type)); \
        return static_cast<Type>(type - Category1##First + Category2##First); \
    } \
    static Type category2##TypeTo##Category1##Type(Type type) \
    { \
        ASSERT(is##Category2##Type(type)); \
        return static_cast<Type>(type - Category2##First + Category1##First); \
    }

#define DEFINE_PAIRED_CATEGORY_METHODS(Category, category) \
    DEFINE_CATEGORY_METHODS(Category) \
    DEFINE_CATEGORY_METHODS(End##Category) \
    DEFINE_CONVERSION_METHODS(Category, category, End##Category, end##Category)

#define DEFINE_PAINT_PHASE_CONVERSION_METHOD(Category) \
    static Type paintPhaseTo##Category##Type(int paintPhase) \
    { \
        static_assert(Category##PaintPhaseLast - Category##PaintPhaseFirst == PaintPhaseMax, \
            "Invalid paint-phase-based category " #Category ". See comments of DisplayItem::Type"); \
        return static_cast<Type>(paintPhase + Category##PaintPhaseFirst); \
    }

    DEFINE_CATEGORY_METHODS(Drawing)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(Drawing)
    DEFINE_CATEGORY_METHODS(CachedDrawing)
    DEFINE_CONVERSION_METHODS(Drawing, drawing, CachedDrawing, cachedDrawing)

    DEFINE_CATEGORY_METHODS(ForeignLayer)

    DEFINE_PAIRED_CATEGORY_METHODS(Clip, clip)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(ClipLayerFragment)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(ClipBox)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(ClipColumnBounds)

    DEFINE_PAIRED_CATEGORY_METHODS(FloatClip, floatClip)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(FloatClip)

    DEFINE_PAIRED_CATEGORY_METHODS(Scroll, scroll)
    DEFINE_PAINT_PHASE_CONVERSION_METHOD(Scroll)

    DEFINE_PAIRED_CATEGORY_METHODS(Transform3D, transform3D)

    static bool isCachedType(Type type) { return isCachedDrawingType(type) || type == CachedSubsequence; }
    bool isCached() const { return isCachedType(m_type); }
    static bool isCacheableType(Type type) { return isDrawingType(type) || type == Subsequence; }
    bool isCacheable() const { return !skippedCache() && isCacheableType(m_type); }

    virtual bool isBegin() const { return false; }
    virtual bool isEnd() const { return false; }

#if DCHECK_IS_ON()
    virtual bool isEndAndPairedWith(DisplayItem::Type otherType) const { return false; }
    virtual bool equals(const DisplayItem& other) const
    {
        return m_client == other.m_client
            && m_type == other.m_type
            && m_derivedSize == other.m_derivedSize
            && m_skippedCache == other.m_skippedCache;
    }
#endif

    // True if the client is non-null. Because m_client is const, this should
    // never be false except when we explicitly create a tombstone/"dead display
    // item" as part of moving an item from one list to another (see:
    // DisplayItemList::appendByMoving).
    bool hasValidClient() const { return m_client; }

    virtual bool drawsContent() const { return false; }

    // Override to implement specific analysis strategies.
    virtual void analyzeForGpuRasterization(SkPictureGpuAnalyzer&) const { }

#ifndef NDEBUG
    static WTF::String typeAsDebugString(DisplayItem::Type);
    const WTF::String clientDebugString() const { return m_clientDebugString; }
    void setClientDebugString(const WTF::String& s) { m_clientDebugString = s; }
    WTF::String asDebugString() const;
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const;
#endif

private:
    // The default DisplayItem constructor is only used by
    // ContiguousContainer::appendByMoving where an invalid DisplaItem is
    // constructed at the source location.
    template <typename T, unsigned alignment> friend class ContiguousContainer;

    DisplayItem()
        : m_client(nullptr)
        , m_type(UninitializedType)
        , m_derivedSize(sizeof(*this))
        , m_skippedCache(false)
    { }

    const DisplayItemClient* m_client;
    static_assert(TypeLast < (1 << 16), "DisplayItem::Type should fit in 16 bits");
    const Type m_type : 16;
    const unsigned m_derivedSize : 8; // size of the actual derived class
    unsigned m_skippedCache : 1;

#ifndef NDEBUG
    WTF::String m_clientDebugString;
#endif
};

class PLATFORM_EXPORT PairedBeginDisplayItem : public DisplayItem {
protected:
    PairedBeginDisplayItem(const DisplayItemClient& client, Type type, size_t derivedSize) : DisplayItem(client, type, derivedSize) { }

private:
    bool isBegin() const final { return true; }
};

class PLATFORM_EXPORT PairedEndDisplayItem : public DisplayItem {
protected:
    PairedEndDisplayItem(const DisplayItemClient& client, Type type, size_t derivedSize) : DisplayItem(client, type, derivedSize) { }

#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const override = 0;
#endif

private:
    bool isEnd() const final { return true; }
};

} // namespace blink

#endif // DisplayItem_h
