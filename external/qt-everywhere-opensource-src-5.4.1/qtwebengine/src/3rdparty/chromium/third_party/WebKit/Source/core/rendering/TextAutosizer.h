/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TextAutosizer_h
#define TextAutosizer_h

#include "core/HTMLNames.h"
#include "platform/text/WritingMode.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class Document;
class RenderBlock;
class RenderObject;
struct TextAutosizingWindowInfo;

// Represents cluster related data. Instances should not persist between calls to processSubtree.
struct TextAutosizingClusterInfo {
    explicit TextAutosizingClusterInfo(RenderBlock* root)
        : root(root)
        , blockContainingAllText(0)
        , maxAllowedDifferenceFromTextWidth(150)
    {
    }

    RenderBlock* root;
    const RenderBlock* blockContainingAllText;

    // Upper limit on the difference between the width of the cluster's block containing all
    // text and that of a narrow child before the child becomes a separate cluster.
    float maxAllowedDifferenceFromTextWidth;

    // Descendants of the cluster that are narrower than the block containing all text and must be
    // processed together.
    Vector<TextAutosizingClusterInfo> narrowDescendants;
};

class TextAutosizer FINAL {
    WTF_MAKE_NONCOPYABLE(TextAutosizer);

public:
    static PassOwnPtr<TextAutosizer> create(Document* document) { return adoptPtr(new TextAutosizer(document)); }

    bool processSubtree(RenderObject* layoutRoot);
    void recalculateMultipliers();

    static float computeAutosizedFontSize(float specifiedSize, float multiplier);

private:
    friend class FastTextAutosizer;

    enum TraversalDirection {
        FirstToLast,
        LastToFirst
    };

    explicit TextAutosizer(Document*);

    bool isApplicable() const;
    float clusterMultiplier(WritingMode, const TextAutosizingWindowInfo&, float textWidth) const;

    void processClusterInternal(TextAutosizingClusterInfo&, RenderBlock* container, RenderObject* subtreeRoot, const TextAutosizingWindowInfo&, float multiplier);
    void processCluster(TextAutosizingClusterInfo&, RenderBlock* container, RenderObject* subtreeRoot, const TextAutosizingWindowInfo&);
    void processCompositeCluster(Vector<TextAutosizingClusterInfo>&, const TextAutosizingWindowInfo&);
    void processContainer(float multiplier, RenderBlock* container, TextAutosizingClusterInfo&, RenderObject* subtreeRoot, const TextAutosizingWindowInfo&);

    void setMultiplier(RenderObject*, float);
    void setMultiplierForList(RenderObject* renderer, float multiplier);

    unsigned getCachedHash(const RenderObject* renderer, bool putInCacheIfAbsent);

    static bool isAutosizingContainer(const RenderObject*);
    static bool isNarrowDescendant(const RenderBlock*, TextAutosizingClusterInfo& parentClusterInfo);
    static bool isWiderDescendant(const RenderBlock*, const TextAutosizingClusterInfo& parentClusterInfo);
    static bool isIndependentDescendant(const RenderBlock*);
    static bool isAutosizingCluster(const RenderBlock*, TextAutosizingClusterInfo& parentClusterInfo);

    static bool containerShouldBeAutosized(const RenderBlock* container);
    static bool containerContainsOneOfTags(const RenderBlock* cluster, const Vector<QualifiedName>& tags);
    static bool containerIsRowOfLinks(const RenderObject* container);
    static bool contentHeightIsConstrained(const RenderBlock* container);
    static bool compositeClusterShouldBeAutosized(Vector<TextAutosizingClusterInfo>&, float blockWidth);
    static void measureDescendantTextWidth(const RenderBlock* container, TextAutosizingClusterInfo&, float minTextWidth, float& textWidth);
    unsigned computeCompositeClusterHash(Vector<TextAutosizingClusterInfo>&);
    float computeMultiplier(Vector<TextAutosizingClusterInfo>&, const TextAutosizingWindowInfo&, float textWidth);

    // Use to traverse the tree of descendants, excluding descendants of containers (but returning the containers themselves).
    static RenderObject* nextInPreOrderSkippingDescendantsOfContainers(const RenderObject*, const RenderObject* stayWithin);

    static const RenderBlock* findDeepestBlockContainingAllText(const RenderBlock* cluster);

    // Depending on the traversal direction specified, finds the first or the last leaf text node child that doesn't
    // belong to any cluster.
    static const RenderObject* findFirstTextLeafNotInCluster(const RenderObject*, size_t& depth, TraversalDirection);

    // Returns groups of narrow descendants of a given autosizing cluster. The groups are combined
    // by the difference between the width of the descendant and the width of the parent cluster's
    // |blockContainingAllText|.
    static void getNarrowDescendantsGroupedByWidth(const TextAutosizingClusterInfo& parentClusterInfo, Vector<Vector<TextAutosizingClusterInfo> >&);

    void addNonAutosizedCluster(unsigned key, TextAutosizingClusterInfo& value);
    void secondPassProcessStaleNonAutosizedClusters();
    void processStaleContainer(float multiplier, RenderBlock* cluster, TextAutosizingClusterInfo&);

    Document* m_document;

    HashMap<const RenderObject*, unsigned> m_hashCache;

    // Mapping from all autosized (i.e. multiplier > 1) cluster hashes to their respective multipliers.
    HashMap<unsigned, float> m_hashToMultiplier;
    Vector<unsigned> m_hashesToAutosizeSecondPass;

    // Mapping from a cluster hash to the corresponding cluster infos which have not been autosized yet.
    HashMap<unsigned, OwnPtr<Vector<TextAutosizingClusterInfo> > > m_nonAutosizedClusters;

    bool m_previouslyAutosized;
};

} // namespace WebCore

#endif // TextAutosizer_h
