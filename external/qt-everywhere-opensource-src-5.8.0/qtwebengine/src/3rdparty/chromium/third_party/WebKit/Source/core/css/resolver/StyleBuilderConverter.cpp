/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/css/resolver/StyleBuilderConverter.h"

#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSBasicShapeValues.h"
#include "core/css/CSSContentDistributionValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridAutoRepeatValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSPathValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSValuePair.h"
#include "core/css/resolver/FilterOperationResolver.h"
#include "core/frame/LocalFrame.h"
#include "core/style/ClipPathOperation.h"
#include "core/style/TextSizeAdjust.h"
#include "core/svg/SVGURIReference.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include <algorithm>

namespace blink {

namespace {

static GridLength convertGridTrackBreadth(const StyleResolverState& state, const CSSPrimitiveValue& primitiveValue)
{
    if (primitiveValue.getValueID() == CSSValueMinContent)
        return Length(MinContent);

    if (primitiveValue.getValueID() == CSSValueMaxContent)
        return Length(MaxContent);

    // Fractional unit.
    if (primitiveValue.isFlex())
        return GridLength(primitiveValue.getDoubleValue());

    return StyleBuilderConverter::convertLengthOrAuto(state, primitiveValue);
}

} // namespace

PassRefPtr<StyleReflection> StyleBuilderConverter::convertBoxReflect(StyleResolverState& state, const CSSValue& value)
{
    if (value.isPrimitiveValue()) {
        ASSERT(toCSSPrimitiveValue(value).getValueID() == CSSValueNone);
        return ComputedStyle::initialBoxReflect();
    }

    const CSSReflectValue& reflectValue = toCSSReflectValue(value);
    RefPtr<StyleReflection> reflection = StyleReflection::create();
    reflection->setDirection(reflectValue.direction()->convertTo<CSSReflectionDirection>());
    if (reflectValue.offset())
        reflection->setOffset(reflectValue.offset()->convertToLength(state.cssToLengthConversionData()));
    if (reflectValue.mask()) {
        NinePieceImage mask;
        mask.setMaskDefaults();
        CSSToStyleMap::mapNinePieceImage(state, CSSPropertyWebkitBoxReflect, *reflectValue.mask(), mask);
        reflection->setMask(mask);
    }

    return reflection.release();
}

Color StyleBuilderConverter::convertColor(StyleResolverState& state, const CSSValue& value, bool forVisitedLink)
{
    return state.document().textLinkColors().colorFromCSSValue(value, state.style()->color(), forVisitedLink);
}

AtomicString StyleBuilderConverter::convertFragmentIdentifier(StyleResolverState& state, const CSSValue& value)
{
    if (value.isURIValue())
        return SVGURIReference::fragmentIdentifierFromIRIString(toCSSURIValue(value).value(), state.element()->treeScope());
    return nullAtom;
}

LengthBox StyleBuilderConverter::convertClip(StyleResolverState& state, const CSSValue& value)
{
    const CSSQuadValue& rect = toCSSQuadValue(value);

    return LengthBox(convertLengthOrAuto(state, *rect.top()),
        convertLengthOrAuto(state, *rect.right()),
        convertLengthOrAuto(state, *rect.bottom()),
        convertLengthOrAuto(state, *rect.left()));
}

PassRefPtr<ClipPathOperation> StyleBuilderConverter::convertClipPath(StyleResolverState& state, const CSSValue& value)
{
    if (value.isBasicShapeValue())
        return ShapeClipPathOperation::create(basicShapeForValue(state, value));
    if (value.isURIValue()) {
        String cssURLValue = toCSSURIValue(value).value();
        KURL url = state.document().completeURL(cssURLValue);
        // TODO(fs): Doesn't work with forward or external SVG references (crbug.com/391604, crbug.com/109212, ...)
        return ReferenceClipPathOperation::create(cssURLValue, AtomicString(url.fragmentIdentifier()));
    }
    DCHECK(value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueNone);
    return nullptr;
}

FilterOperations StyleBuilderConverter::convertFilterOperations(StyleResolverState& state, const CSSValue& value)
{
    return FilterOperationResolver::createFilterOperations(state, value);
}

static FontDescription::GenericFamilyType convertGenericFamily(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueWebkitBody:
        return FontDescription::StandardFamily;
    case CSSValueSerif:
        return FontDescription::SerifFamily;
    case CSSValueSansSerif:
        return FontDescription::SansSerifFamily;
    case CSSValueCursive:
        return FontDescription::CursiveFamily;
    case CSSValueFantasy:
        return FontDescription::FantasyFamily;
    case CSSValueMonospace:
        return FontDescription::MonospaceFamily;
    case CSSValueWebkitPictograph:
        return FontDescription::PictographFamily;
    default:
        return FontDescription::NoFamily;
    }
}

static bool convertFontFamilyName(StyleResolverState& state, const CSSValue& value,
    FontDescription::GenericFamilyType& genericFamily, AtomicString& familyName)
{
    if (value.isFontFamilyValue()) {
        genericFamily = FontDescription::NoFamily;
        familyName = AtomicString(toCSSFontFamilyValue(value).value());
    } else if (state.document().settings()) {
        genericFamily = convertGenericFamily(toCSSPrimitiveValue(value).getValueID());
        familyName = state.fontBuilder().genericFontFamilyName(genericFamily);
    }

    return !familyName.isEmpty();
}

FontDescription::FamilyDescription StyleBuilderConverter::convertFontFamily(StyleResolverState& state, const CSSValue& value)
{
    ASSERT(value.isValueList());

    FontDescription::FamilyDescription desc(FontDescription::NoFamily);
    FontFamily* currFamily = nullptr;

    for (auto& family : toCSSValueList(value)) {
        FontDescription::GenericFamilyType genericFamily = FontDescription::NoFamily;
        AtomicString familyName;

        if (!convertFontFamilyName(state, *family, genericFamily, familyName))
            continue;

        if (!currFamily) {
            currFamily = &desc.family;
        } else {
            RefPtr<SharedFontFamily> newFamily = SharedFontFamily::create();
            currFamily->appendFamily(newFamily);
            currFamily = newFamily.get();
        }

        currFamily->setFamily(familyName);

        if (genericFamily != FontDescription::NoFamily)
            desc.genericFamily = genericFamily;
    }

    return desc;
}

PassRefPtr<FontFeatureSettings> StyleBuilderConverter::convertFontFeatureSettings(StyleResolverState& state, const CSSValue& value)
{
    if (value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueNormal)
        return FontBuilder::initialFeatureSettings();

    const CSSValueList& list = toCSSValueList(value);
    RefPtr<FontFeatureSettings> settings = FontFeatureSettings::create();
    int len = list.length();
    for (int i = 0; i < len; ++i) {
        const CSSFontFeatureValue& feature = toCSSFontFeatureValue(list.item(i));
        settings->append(FontFeature(feature.tag(), feature.value()));
    }
    return settings;
}

static float computeFontSize(StyleResolverState& state, const CSSPrimitiveValue& primitiveValue, const FontDescription::Size& parentSize)
{
    if (primitiveValue.isLength())
        return primitiveValue.computeLength<float>(state.fontSizeConversionData());
    if (primitiveValue.isCalculatedPercentageWithLength())
        return primitiveValue.cssCalcValue()->toCalcValue(state.fontSizeConversionData())->evaluate(parentSize.value);

    ASSERT_NOT_REACHED();
    return 0;
}

FontDescription::Size StyleBuilderConverter::convertFontSize(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);

    FontDescription::Size parentSize(0, 0.0f, false);

    // FIXME: Find out when parentStyle could be 0?
    if (state.parentStyle())
        parentSize = state.parentFontDescription().getSize();

    if (CSSValueID valueID = primitiveValue.getValueID()) {
        if (FontSize::isValidValueID(valueID))
            return FontDescription::Size(FontSize::keywordSize(valueID), 0.0f, false);
        if (valueID == CSSValueSmaller)
            return FontDescription::smallerSize(parentSize);
        if (valueID == CSSValueLarger)
            return FontDescription::largerSize(parentSize);
        ASSERT_NOT_REACHED();
        return FontBuilder::initialSize();
    }

    bool parentIsAbsoluteSize = state.parentFontDescription().isAbsoluteSize();

    if (primitiveValue.isPercentage())
        return FontDescription::Size(0, (primitiveValue.getFloatValue() * parentSize.value / 100.0f), parentIsAbsoluteSize);

    return FontDescription::Size(0, computeFontSize(state, primitiveValue, parentSize), parentIsAbsoluteSize || !primitiveValue.isFontRelativeLength());
}

float StyleBuilderConverter::convertFontSizeAdjust(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue.getValueID() == CSSValueNone)
        return FontBuilder::initialSizeAdjust();

    ASSERT(primitiveValue.isNumber());
    return primitiveValue.getFloatValue();
}

FontWeight StyleBuilderConverter::convertFontWeight(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    switch (primitiveValue.getValueID()) {
    case CSSValueBolder:
        return FontDescription::bolderWeight(state.parentStyle()->getFontDescription().weight());
    case CSSValueLighter:
        return FontDescription::lighterWeight(state.parentStyle()->getFontDescription().weight());
    default:
        return primitiveValue.convertTo<FontWeight>();
    }
}

FontDescription::FontVariantCaps StyleBuilderConverter::convertFontVariantCaps(StyleResolverState&, const CSSValue& value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(value.isPrimitiveValue());
    CSSValueID valueID = toCSSPrimitiveValue(value).getValueID();
    switch (valueID) {
    case CSSValueNormal:
        return FontDescription::CapsNormal;
    case CSSValueSmallCaps:
        return FontDescription::SmallCaps;
    case CSSValueAllSmallCaps:
        return FontDescription::AllSmallCaps;
    case CSSValuePetiteCaps:
        return FontDescription::PetiteCaps;
    case CSSValueAllPetiteCaps:
        return FontDescription::AllPetiteCaps;
    case CSSValueUnicase:
        return FontDescription::Unicase;
    case CSSValueTitlingCaps:
        return FontDescription::TitlingCaps;
    default:
        return FontDescription::CapsNormal;
    }
}

FontDescription::VariantLigatures StyleBuilderConverter::convertFontVariantLigatures(StyleResolverState&, const CSSValue& value)
{
    if (value.isValueList()) {
        FontDescription::VariantLigatures ligatures;
        const CSSValueList& valueList = toCSSValueList(value);
        for (size_t i = 0; i < valueList.length(); ++i) {
            const CSSValue& item = valueList.item(i);
            const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(item);
            switch (primitiveValue.getValueID()) {
            case CSSValueNoCommonLigatures:
                ligatures.common = FontDescription::DisabledLigaturesState;
                break;
            case CSSValueCommonLigatures:
                ligatures.common = FontDescription::EnabledLigaturesState;
                break;
            case CSSValueNoDiscretionaryLigatures:
                ligatures.discretionary = FontDescription::DisabledLigaturesState;
                break;
            case CSSValueDiscretionaryLigatures:
                ligatures.discretionary = FontDescription::EnabledLigaturesState;
                break;
            case CSSValueNoHistoricalLigatures:
                ligatures.historical = FontDescription::DisabledLigaturesState;
                break;
            case CSSValueHistoricalLigatures:
                ligatures.historical = FontDescription::EnabledLigaturesState;
                break;
            case CSSValueNoContextual:
                ligatures.contextual = FontDescription::DisabledLigaturesState;
                break;
            case CSSValueContextual:
                ligatures.contextual = FontDescription::EnabledLigaturesState;
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }
        return ligatures;
    }

    ASSERT_WITH_SECURITY_IMPLICATION(value.isPrimitiveValue());

    if (toCSSPrimitiveValue(value).getValueID() == CSSValueNone) {
        return FontDescription::VariantLigatures(FontDescription::DisabledLigaturesState);
    }

    ASSERT(toCSSPrimitiveValue(value).getValueID() == CSSValueNormal);
    return FontDescription::VariantLigatures();
}

FontVariantNumeric StyleBuilderConverter::convertFontVariantNumeric(StyleResolverState&, const CSSValue& value)
{
    if (value.isPrimitiveValue()) {
        ASSERT(toCSSPrimitiveValue(value).getValueID() == CSSValueNormal);
        return FontVariantNumeric();
    }

    FontVariantNumeric variantNumeric;
    for (const CSSValue* feature : toCSSValueList(value)) {
        switch (toCSSPrimitiveValue(feature)->getValueID()) {
        case CSSValueLiningNums:
            variantNumeric.setNumericFigure(FontVariantNumeric::LiningNums);
            break;
        case CSSValueOldstyleNums:
            variantNumeric.setNumericFigure(FontVariantNumeric::OldstyleNums);
            break;
        case CSSValueProportionalNums:
            variantNumeric.setNumericSpacing(FontVariantNumeric::ProportionalNums);
            break;
        case CSSValueTabularNums:
            variantNumeric.setNumericSpacing(FontVariantNumeric::TabularNums);
            break;
        case CSSValueDiagonalFractions:
            variantNumeric.setNumericFraction(FontVariantNumeric::DiagonalFractions);
            break;
        case CSSValueStackedFractions:
            variantNumeric.setNumericFraction(FontVariantNumeric::StackedFractions);
            break;
        case CSSValueOrdinal:
            variantNumeric.setOrdinal(FontVariantNumeric::OrdinalOn);
            break;
        case CSSValueSlashedZero:
            variantNumeric.setSlashedZero(FontVariantNumeric::SlashedZeroOn);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    return variantNumeric;
}

StyleSelfAlignmentData StyleBuilderConverter::convertSelfOrDefaultAlignmentData(StyleResolverState&, const CSSValue& value)
{
    StyleSelfAlignmentData alignmentData = ComputedStyle::initialSelfAlignment();
    if (value.isValuePair()) {
        const CSSValuePair& pair = toCSSValuePair(value);
        if (toCSSPrimitiveValue(pair.first()).getValueID() == CSSValueLegacy) {
            alignmentData.setPositionType(LegacyPosition);
            alignmentData.setPosition(toCSSPrimitiveValue(pair.second()).convertTo<ItemPosition>());
        } else {
            alignmentData.setPosition(toCSSPrimitiveValue(pair.first()).convertTo<ItemPosition>());
            alignmentData.setOverflow(toCSSPrimitiveValue(pair.second()).convertTo<OverflowAlignment>());
        }
    } else {
        alignmentData.setPosition(toCSSPrimitiveValue(value).convertTo<ItemPosition>());
    }
    return alignmentData;
}

StyleContentAlignmentData StyleBuilderConverter::convertContentAlignmentData(StyleResolverState&, const CSSValue& value)
{
    StyleContentAlignmentData alignmentData = ComputedStyle::initialContentAlignment();
    if (!RuntimeEnabledFeatures::cssGridLayoutEnabled()) {
        const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
        switch (primitiveValue.getValueID()) {
        case CSSValueStretch:
        case CSSValueSpaceBetween:
        case CSSValueSpaceAround:
            alignmentData.setDistribution(primitiveValue.convertTo<ContentDistributionType>());
            break;
        case CSSValueFlexStart:
        case CSSValueFlexEnd:
        case CSSValueCenter:
            alignmentData.setPosition(primitiveValue.convertTo<ContentPosition>());
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        return alignmentData;
    }
    const CSSContentDistributionValue& contentValue = toCSSContentDistributionValue(value);
    if (contentValue.distribution()->getValueID() != CSSValueInvalid)
        alignmentData.setDistribution(contentValue.distribution()->convertTo<ContentDistributionType>());
    if (contentValue.position()->getValueID() != CSSValueInvalid)
        alignmentData.setPosition(contentValue.position()->convertTo<ContentPosition>());
    if (contentValue.overflow()->getValueID() != CSSValueInvalid)
        alignmentData.setOverflow(contentValue.overflow()->convertTo<OverflowAlignment>());
    return alignmentData;
}

GridAutoFlow StyleBuilderConverter::convertGridAutoFlow(StyleResolverState&, const CSSValue& value)
{
    const CSSValueList& list = toCSSValueList(value);

    ASSERT(list.length() >= 1);
    const CSSPrimitiveValue& first = toCSSPrimitiveValue(list.item(0));
    const CSSPrimitiveValue* second = list.length() == 2 ? &toCSSPrimitiveValue(list.item(1)) : nullptr;

    switch (first.getValueID()) {
    case CSSValueRow:
        if (second && second->getValueID() == CSSValueDense)
            return AutoFlowRowDense;
        return AutoFlowRow;
    case CSSValueColumn:
        if (second && second->getValueID() == CSSValueDense)
            return AutoFlowColumnDense;
        return AutoFlowColumn;
    case CSSValueDense:
        if (second && second->getValueID() == CSSValueColumn)
            return AutoFlowColumnDense;
        return AutoFlowRowDense;
    default:
        ASSERT_NOT_REACHED();
        return ComputedStyle::initialGridAutoFlow();
    }
}

GridPosition StyleBuilderConverter::convertGridPosition(StyleResolverState&, const CSSValue& value)
{
    // We accept the specification's grammar:
    // 'auto' | [ <integer> || <custom-ident> ] | [ span && [ <integer> || <custom-ident> ] ] | <custom-ident>

    GridPosition position;

    if (value.isCustomIdentValue()) {
        // We translate <custom-ident> to <string> during parsing as it
        // makes handling it more simple.
        position.setNamedGridArea(toCSSCustomIdentValue(value).value());
        return position;
    }

    if (value.isPrimitiveValue()) {
        ASSERT(toCSSPrimitiveValue(value).getValueID() == CSSValueAuto);
        return position;
    }

    const CSSValueList& values = toCSSValueList(value);
    ASSERT(values.length());

    bool isSpanPosition = false;
    // The specification makes the <integer> optional, in which case it default to '1'.
    int gridLineNumber = 1;
    String gridLineName;

    auto it = values.begin();
    const CSSValue* currentValue = it->get();
    if (currentValue->isPrimitiveValue() && toCSSPrimitiveValue(currentValue)->getValueID() == CSSValueSpan) {
        isSpanPosition = true;
        ++it;
        currentValue = it != values.end() ? it->get() : nullptr;
    }

    if (currentValue && currentValue->isPrimitiveValue() && toCSSPrimitiveValue(currentValue)->isNumber()) {
        gridLineNumber = toCSSPrimitiveValue(currentValue)->getIntValue();
        ++it;
        currentValue = it != values.end() ? it->get() : nullptr;
    }

    if (currentValue && currentValue->isCustomIdentValue()) {
        gridLineName = toCSSCustomIdentValue(currentValue)->value();
        ++it;
    }

    ASSERT(it == values.end());
    if (isSpanPosition)
        position.setSpanPosition(gridLineNumber, gridLineName);
    else
        position.setExplicitPosition(gridLineNumber, gridLineName);

    return position;
}

GridTrackSize StyleBuilderConverter::convertGridTrackSize(StyleResolverState& state, const CSSValue& value)
{
    if (value.isPrimitiveValue())
        return GridTrackSize(convertGridTrackBreadth(state, toCSSPrimitiveValue(value)));

    const CSSFunctionValue& minmaxFunction = toCSSFunctionValue(value);
    ASSERT_WITH_SECURITY_IMPLICATION(minmaxFunction.length() == 2);
    GridLength minTrackBreadth(convertGridTrackBreadth(state, toCSSPrimitiveValue(minmaxFunction.item(0))));
    GridLength maxTrackBreadth(convertGridTrackBreadth(state, toCSSPrimitiveValue(minmaxFunction.item(1))));
    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

static void convertGridLineNamesList(const CSSValue& value, size_t currentNamedGridLine, NamedGridLinesMap& namedGridLines, OrderedNamedGridLines& orderedNamedGridLines)
{
    ASSERT(value.isGridLineNamesValue());

    for (auto& namedGridLineValue : toCSSValueList(value)) {
        String namedGridLine = toCSSCustomIdentValue(*namedGridLineValue).value();
        NamedGridLinesMap::AddResult result = namedGridLines.add(namedGridLine, Vector<size_t>());
        result.storedValue->value.append(currentNamedGridLine);
        OrderedNamedGridLines::AddResult orderedInsertionResult = orderedNamedGridLines.add(currentNamedGridLine, Vector<String>());
        orderedInsertionResult.storedValue->value.append(namedGridLine);
    }
}

void StyleBuilderConverter::convertGridTrackList(const CSSValue& value, Vector<GridTrackSize>& trackSizes, NamedGridLinesMap& namedGridLines, OrderedNamedGridLines& orderedNamedGridLines, Vector<GridTrackSize>& autoRepeatTrackSizes, NamedGridLinesMap& autoRepeatNamedGridLines, OrderedNamedGridLines& autoRepeatOrderedNamedGridLines, size_t& autoRepeatInsertionPoint, AutoRepeatType &autoRepeatType, StyleResolverState& state)
{
    if (value.isPrimitiveValue()) {
        ASSERT(toCSSPrimitiveValue(value).getValueID() == CSSValueNone);
        return;
    }

    size_t currentNamedGridLine = 0;
    for (auto currValue : toCSSValueList(value)) {
        if (currValue->isGridLineNamesValue()) {
            convertGridLineNamesList(*currValue, currentNamedGridLine, namedGridLines, orderedNamedGridLines);
            continue;
        }

        if (currValue->isGridAutoRepeatValue()) {
            ASSERT(autoRepeatTrackSizes.isEmpty());
            size_t autoRepeatIndex = 0;
            CSSValueID autoRepeatID = toCSSGridAutoRepeatValue(currValue.get())->autoRepeatID();
            ASSERT(autoRepeatID == CSSValueAutoFill || autoRepeatID == CSSValueAutoFit);
            autoRepeatType = autoRepeatID == CSSValueAutoFill ? AutoFill : AutoFit;
            for (auto autoRepeatValue : toCSSValueList(*currValue)) {
                if (autoRepeatValue->isGridLineNamesValue()) {
                    convertGridLineNamesList(*autoRepeatValue, autoRepeatIndex, autoRepeatNamedGridLines, autoRepeatOrderedNamedGridLines);
                    continue;
                }
                ++autoRepeatIndex;
                autoRepeatTrackSizes.append(convertGridTrackSize(state, *autoRepeatValue));
            }
            autoRepeatInsertionPoint = currentNamedGridLine++;
            continue;
        }

        ++currentNamedGridLine;
        trackSizes.append(convertGridTrackSize(state, *currValue));
    }

    // The parser should have rejected any <track-list> without any <track-size> as
    // this is not conformant to the syntax.
    ASSERT(!trackSizes.isEmpty() || !autoRepeatTrackSizes.isEmpty());
}

void StyleBuilderConverter::convertOrderedNamedGridLinesMapToNamedGridLinesMap(const OrderedNamedGridLines& orderedNamedGridLines, NamedGridLinesMap& namedGridLines)
{
    ASSERT(namedGridLines.size() == 0);

    if (orderedNamedGridLines.size() == 0)
        return;

    for (auto& orderedNamedGridLine : orderedNamedGridLines) {
        for (auto& lineName : orderedNamedGridLine.value) {
            NamedGridLinesMap::AddResult startResult = namedGridLines.add(lineName, Vector<size_t>());
            startResult.storedValue->value.append(orderedNamedGridLine.key);
        }
    }

    for (auto& namedGridLine : namedGridLines) {
        Vector<size_t> gridLineIndexes = namedGridLine.value;
        std::sort(gridLineIndexes.begin(), gridLineIndexes.end());
    }
}

void StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(const NamedGridAreaMap& namedGridAreas, NamedGridLinesMap& namedGridLines, GridTrackSizingDirection direction)
{
    for (const auto& namedGridAreaEntry : namedGridAreas) {
        GridSpan areaSpan = direction == ForRows ? namedGridAreaEntry.value.rows : namedGridAreaEntry.value.columns;
        {
            NamedGridLinesMap::AddResult startResult = namedGridLines.add(namedGridAreaEntry.key + "-start", Vector<size_t>());
            startResult.storedValue->value.append(areaSpan.startLine());
            std::sort(startResult.storedValue->value.begin(), startResult.storedValue->value.end());
        }
        {
            NamedGridLinesMap::AddResult endResult = namedGridLines.add(namedGridAreaEntry.key + "-end", Vector<size_t>());
            endResult.storedValue->value.append(areaSpan.endLine());
            std::sort(endResult.storedValue->value.begin(), endResult.storedValue->value.end());
        }
    }
}

Length StyleBuilderConverter::convertLength(const StyleResolverState& state, const CSSValue& value)
{
    return toCSSPrimitiveValue(value).convertToLength(state.cssToLengthConversionData());
}

UnzoomedLength StyleBuilderConverter::convertUnzoomedLength(const StyleResolverState& state, const CSSValue& value)
{
    return UnzoomedLength(toCSSPrimitiveValue(value).convertToLength(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)));
}

Length StyleBuilderConverter::convertLengthOrAuto(const StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue.getValueID() == CSSValueAuto)
        return Length(Auto);
    return primitiveValue.convertToLength(state.cssToLengthConversionData());
}

Length StyleBuilderConverter::convertLengthSizing(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    switch (primitiveValue.getValueID()) {
    case CSSValueInvalid:
        return convertLength(state, value);
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
        return Length(MinContent);
    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
        return Length(MaxContent);
    case CSSValueWebkitFillAvailable:
        return Length(FillAvailable);
    case CSSValueWebkitFitContent:
    case CSSValueFitContent:
        return Length(FitContent);
    case CSSValueAuto:
        return Length(Auto);
    default:
        ASSERT_NOT_REACHED();
        return Length();
    }
}

Length StyleBuilderConverter::convertLengthMaxSizing(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue.getValueID() == CSSValueNone)
        return Length(MaxSizeNone);
    return convertLengthSizing(state, value);
}

TabSize StyleBuilderConverter::convertLengthOrTabSpaces(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue.isNumber())
        return TabSize(primitiveValue.getIntValue());
    return TabSize(primitiveValue.computeLength<float>(state.cssToLengthConversionData()));
}

static CSSToLengthConversionData lineHeightToLengthConversionData(StyleResolverState& state)
{
    float multiplier = state.style()->effectiveZoom();
    if (LocalFrame* frame = state.document().frame())
        multiplier *= frame->textZoomFactor();
    return state.cssToLengthConversionData().copyWithAdjustedZoom(multiplier);
}

Length StyleBuilderConverter::convertLineHeight(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue.isLength())
        return primitiveValue.computeLength<Length>(lineHeightToLengthConversionData(state));
    if (primitiveValue.isPercentage())
        return Length((state.style()->computedFontSize() * primitiveValue.getIntValue()) / 100.0, Fixed);
    if (primitiveValue.isNumber())
        return Length(primitiveValue.getDoubleValue() * 100.0, Percent);
    if (primitiveValue.isCalculated()) {
        Length zoomedLength = Length(primitiveValue.cssCalcValue()->toCalcValue(lineHeightToLengthConversionData(state)));
        return Length(valueForLength(zoomedLength, LayoutUnit(state.style()->computedFontSize())), Fixed);
    }

    ASSERT(primitiveValue.getValueID() == CSSValueNormal);
    return ComputedStyle::initialLineHeight();
}

float StyleBuilderConverter::convertNumberOrPercentage(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    ASSERT(primitiveValue.isNumber() || primitiveValue.isPercentage());
    if (primitiveValue.isNumber())
        return primitiveValue.getFloatValue();
    return primitiveValue.getFloatValue() / 100.0f;
}

StyleMotionRotation StyleBuilderConverter::convertMotionRotation(StyleResolverState&, const CSSValue& value)
{
    return convertMotionRotation(value);
}

StyleMotionRotation StyleBuilderConverter::convertMotionRotation(const CSSValue& value)
{
    StyleMotionRotation result(0, MotionRotationFixed);

    const CSSValueList& list = toCSSValueList(value);
    ASSERT(list.length() == 1 || list.length() == 2);
    for (const auto& item : list) {
        const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(*item);
        if (primitiveValue.getValueID() == CSSValueAuto) {
            result.type = MotionRotationAuto;
        } else if (primitiveValue.getValueID() == CSSValueReverse) {
            result.type = MotionRotationAuto;
            result.angle += 180;
        } else {
            result.angle += primitiveValue.computeDegrees();
        }
    }

    return result;
}

template <CSSValueID cssValueFor0, CSSValueID cssValueFor100>
Length StyleBuilderConverter::convertPositionLength(StyleResolverState& state, const CSSValue& value)
{
    if (value.isValuePair()) {
        const CSSValuePair& pair = toCSSValuePair(value);
        Length length = StyleBuilderConverter::convertLength(state, pair.second());
        if (toCSSPrimitiveValue(pair.first()).getValueID() == cssValueFor0)
            return length;
        ASSERT(toCSSPrimitiveValue(pair.first()).getValueID() == cssValueFor100);
        return length.subtractFromOneHundredPercent();
    }

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue.isValueID()) {
        switch (primitiveValue.getValueID()) {
        case cssValueFor0:
            return Length(0, Percent);
        case cssValueFor100:
            return Length(100, Percent);
        case CSSValueCenter:
            return Length(50, Percent);
        default:
            ASSERT_NOT_REACHED();
        }
    }

    return StyleBuilderConverter::convertLength(state, primitiveValue);
}

LengthPoint StyleBuilderConverter::convertPosition(StyleResolverState& state, const CSSValue& value)
{
    const CSSValuePair& pair = toCSSValuePair(value);
    return LengthPoint(
        convertPositionLength<CSSValueLeft, CSSValueRight>(state, pair.first()),
        convertPositionLength<CSSValueTop, CSSValueBottom>(state, pair.second())
    );
}

static float convertPerspectiveLength(StyleResolverState& state, const CSSPrimitiveValue& primitiveValue)
{
    return std::max(primitiveValue.computeLength<float>(state.cssToLengthConversionData()), 0.0f);
}

float StyleBuilderConverter::convertPerspective(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue.getValueID() == CSSValueNone)
        return ComputedStyle::initialPerspective();
    return convertPerspectiveLength(state, primitiveValue);
}

EPaintOrder StyleBuilderConverter::convertPaintOrder(StyleResolverState&, const CSSValue& cssPaintOrder)
{
    if (cssPaintOrder.isValueList()) {
        const CSSValueList& orderTypeList = toCSSValueList(cssPaintOrder);
        switch (toCSSPrimitiveValue(orderTypeList.item(0)).getValueID()) {
        case CSSValueFill:
            return orderTypeList.length() > 1 ? PaintOrderFillMarkersStroke : PaintOrderFillStrokeMarkers;
        case CSSValueStroke:
            return orderTypeList.length() > 1 ? PaintOrderStrokeMarkersFill : PaintOrderStrokeFillMarkers;
        case CSSValueMarkers:
            return orderTypeList.length() > 1 ? PaintOrderMarkersStrokeFill : PaintOrderMarkersFillStroke;
        default:
            ASSERT_NOT_REACHED();
            return PaintOrderNormal;
        }
    }

    return PaintOrderNormal;
}

Length StyleBuilderConverter::convertQuirkyLength(StyleResolverState& state, const CSSValue& value)
{
    Length length = convertLengthOrAuto(state, value);
    // This is only for margins which use __qem
    length.setQuirk(toCSSPrimitiveValue(value).isQuirkyEms());
    return length;
}

PassRefPtr<QuotesData> StyleBuilderConverter::convertQuotes(StyleResolverState&, const CSSValue& value)
{
    if (value.isValueList()) {
        const CSSValueList& list = toCSSValueList(value);
        RefPtr<QuotesData> quotes = QuotesData::create();
        for (size_t i = 0; i < list.length(); i += 2) {
            String startQuote = toCSSStringValue(list.item(i)).value();
            String endQuote = toCSSStringValue(list.item(i + 1)).value();
            quotes->addPair(std::make_pair(startQuote, endQuote));
        }
        return quotes.release();
    }
    ASSERT(value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueNone);
    return QuotesData::create();
}

LengthSize StyleBuilderConverter::convertRadius(StyleResolverState& state, const CSSValue& value)
{
    const CSSValuePair& pair = toCSSValuePair(value);
    Length radiusWidth = toCSSPrimitiveValue(pair.first()).convertToLength(state.cssToLengthConversionData());
    Length radiusHeight = toCSSPrimitiveValue(pair.second()).convertToLength(state.cssToLengthConversionData());
    return LengthSize(radiusWidth, radiusHeight);
}

PassRefPtr<ShadowList> StyleBuilderConverter::convertShadow(StyleResolverState& state, const CSSValue& value)
{
    if (value.isPrimitiveValue()) {
        ASSERT(toCSSPrimitiveValue(value).getValueID() == CSSValueNone);
        return PassRefPtr<ShadowList>();
    }

    const CSSValueList& valueList = toCSSValueList(value);
    size_t shadowCount = valueList.length();
    ShadowDataVector shadows;
    for (size_t i = 0; i < shadowCount; ++i) {
        const CSSShadowValue& item = toCSSShadowValue(valueList.item(i));
        float x = item.x->computeLength<float>(state.cssToLengthConversionData());
        float y = item.y->computeLength<float>(state.cssToLengthConversionData());
        float blur = item.blur ? item.blur->computeLength<float>(state.cssToLengthConversionData()) : 0;
        float spread = item.spread ? item.spread->computeLength<float>(state.cssToLengthConversionData()) : 0;
        ShadowStyle shadowStyle = item.style && item.style->getValueID() == CSSValueInset ? Inset : Normal;
        StyleColor color = StyleColor::currentColor();
        if (item.color)
            color = convertStyleColor(state, *item.color);
        shadows.append(ShadowData(FloatPoint(x, y), blur, spread, shadowStyle, color));
    }
    return ShadowList::adopt(shadows);
}

ShapeValue* StyleBuilderConverter::convertShapeValue(StyleResolverState& state, const CSSValue& value)
{
    if (value.isPrimitiveValue()) {
        ASSERT(toCSSPrimitiveValue(value).getValueID() == CSSValueNone);
        return nullptr;
    }

    if (value.isImageValue() || value.isImageGeneratorValue() || value.isImageSetValue())
        return ShapeValue::createImageValue(state.styleImage(CSSPropertyShapeOutside, value));

    RefPtr<BasicShape> shape;
    CSSBoxType cssBox = BoxMissing;
    const CSSValueList& valueList = toCSSValueList(value);
    for (unsigned i = 0; i < valueList.length(); ++i) {
        const CSSValue& value = valueList.item(i);
        if (value.isBasicShapeValue()) {
            shape = basicShapeForValue(state, value);
        } else {
            cssBox = toCSSPrimitiveValue(value).convertTo<CSSBoxType>();
        }
    }

    if (shape)
        return ShapeValue::createShapeValue(shape.release(), cssBox);

    ASSERT(cssBox != BoxMissing);
    return ShapeValue::createBoxShapeValue(cssBox);
}

float StyleBuilderConverter::convertSpacing(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue.getValueID() == CSSValueNormal)
        return 0;
    return primitiveValue.computeLength<float>(state.cssToLengthConversionData());
}

PassRefPtr<SVGDashArray> StyleBuilderConverter::convertStrokeDasharray(StyleResolverState& state, const CSSValue& value)
{
    if (!value.isValueList())
        return SVGComputedStyle::initialStrokeDashArray();

    const CSSValueList& dashes = toCSSValueList(value);

    RefPtr<SVGDashArray> array = SVGDashArray::create();
    size_t length = dashes.length();
    for (size_t i = 0; i < length; ++i) {
        array->append(convertLength(state, toCSSPrimitiveValue(dashes.item(i))));
    }

    return array.release();
}

StyleColor StyleBuilderConverter::convertStyleColor(StyleResolverState& state, const CSSValue& value, bool forVisitedLink)
{
    if (value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueCurrentcolor)
        return StyleColor::currentColor();
    return state.document().textLinkColors().colorFromCSSValue(value, Color(), forVisitedLink);
}

float StyleBuilderConverter::convertTextStrokeWidth(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue.getValueID()) {
        float multiplier = convertLineWidth<float>(state, value);
        return CSSPrimitiveValue::create(multiplier / 48, CSSPrimitiveValue::UnitType::Ems)->computeLength<float>(state.cssToLengthConversionData());
    }
    return primitiveValue.computeLength<float>(state.cssToLengthConversionData());
}

TextSizeAdjust StyleBuilderConverter::convertTextSizeAdjust(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue.getValueID() == CSSValueNone)
        return TextSizeAdjust::adjustNone();
    if (primitiveValue.getValueID() == CSSValueAuto)
        return TextSizeAdjust::adjustAuto();
    DCHECK(primitiveValue.isPercentage());
    return TextSizeAdjust(primitiveValue.getFloatValue() / 100.0f);
}

TransformOrigin StyleBuilderConverter::convertTransformOrigin(StyleResolverState& state, const CSSValue& value)
{
    const CSSValueList& list = toCSSValueList(value);
    ASSERT(list.length() == 3);

    const CSSPrimitiveValue& primitiveValueX = toCSSPrimitiveValue(list.item(0));
    const CSSPrimitiveValue& primitiveValueY = toCSSPrimitiveValue(list.item(1));
    const CSSPrimitiveValue& primitiveValueZ = toCSSPrimitiveValue(list.item(2));

    return TransformOrigin(
        convertPositionLength<CSSValueLeft, CSSValueRight>(state, primitiveValueX),
        convertPositionLength<CSSValueTop, CSSValueBottom>(state, primitiveValueY),
        StyleBuilderConverter::convertComputedLength<float>(state, primitiveValueZ)
    );
}

ScrollSnapPoints StyleBuilderConverter::convertSnapPoints(StyleResolverState& state, const CSSValue& value)
{
    // Handles: none | repeat(<length>)
    ScrollSnapPoints points;
    points.hasRepeat = false;

    if (!value.isFunctionValue())
        return points;

    const CSSFunctionValue& repeatFunction = toCSSFunctionValue(value);
    ASSERT_WITH_SECURITY_IMPLICATION(repeatFunction.length() == 1);
    points.repeatOffset = convertLength(state, toCSSPrimitiveValue(repeatFunction.item(0)));
    points.hasRepeat = true;

    return points;
}

Vector<LengthPoint> StyleBuilderConverter::convertSnapCoordinates(StyleResolverState& state, const CSSValue& value)
{
    // Handles: none | <position>#
    Vector<LengthPoint> coordinates;

    if (!value.isValueList())
        return coordinates;

    const CSSValueList& valueList = toCSSValueList(value);
    coordinates.reserveInitialCapacity(valueList.length());
    for (auto& snapCoordinate : valueList) {
        coordinates.uncheckedAppend(convertPosition(state, *snapCoordinate));
    }

    return coordinates;
}

PassRefPtr<TranslateTransformOperation> StyleBuilderConverter::convertTranslate(StyleResolverState& state, const CSSValue& value)
{
    const CSSValueList& list = toCSSValueList(value);
    ASSERT(list.length() <= 3);
    Length tx = convertLength(state, list.item(0));
    Length ty(0, Fixed);
    double tz = 0;
    if (list.length() >= 2)
        ty = convertLength(state, list.item(1));
    if (list.length() == 3)
        tz = toCSSPrimitiveValue(list.item(2)).computeLength<double>(state.cssToLengthConversionData());

    return TranslateTransformOperation::create(tx, ty, tz, TransformOperation::Translate3D);
}

Rotation StyleBuilderConverter::convertRotation(const CSSValue& value)
{
    const CSSValueList& list = toCSSValueList(value);
    ASSERT(list.length() == 1 || list.length() == 4);
    double angle = toCSSPrimitiveValue(list.item(0)).computeDegrees();
    double x = 0;
    double y = 0;
    double z = 1;
    if (list.length() == 4) {
        x = toCSSPrimitiveValue(list.item(1)).getDoubleValue();
        y = toCSSPrimitiveValue(list.item(2)).getDoubleValue();
        z = toCSSPrimitiveValue(list.item(3)).getDoubleValue();
    }
    return Rotation(FloatPoint3D(x, y, z), angle);
}

PassRefPtr<RotateTransformOperation> StyleBuilderConverter::convertRotate(StyleResolverState& state, const CSSValue& value)
{
    return RotateTransformOperation::create(convertRotation(value), TransformOperation::Rotate3D);
}

PassRefPtr<ScaleTransformOperation> StyleBuilderConverter::convertScale(StyleResolverState& state, const CSSValue& value)
{
    const CSSValueList& list = toCSSValueList(value);
    ASSERT(list.length() <= 3);
    double sx = toCSSPrimitiveValue(list.item(0)).getDoubleValue();
    double sy = sx;
    double sz = 1;
    if (list.length() >= 2)
        sy = toCSSPrimitiveValue(list.item(1)).getDoubleValue();
    if (list.length() == 3)
        sz = toCSSPrimitiveValue(list.item(2)).getDoubleValue();

    return ScaleTransformOperation::create(sx, sy, sz, TransformOperation::Scale3D);
}

RespectImageOrientationEnum StyleBuilderConverter::convertImageOrientation(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    return primitiveValue.getValueID() == CSSValueFromImage ? RespectImageOrientation : DoNotRespectImageOrientation;
}

PassRefPtr<StylePath> StyleBuilderConverter::convertPathOrNone(StyleResolverState& state, const CSSValue& value)
{
    if (value.isPathValue())
        return toCSSPathValue(value).stylePath();
    ASSERT(value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueNone);
    return nullptr;
}

} // namespace blink
