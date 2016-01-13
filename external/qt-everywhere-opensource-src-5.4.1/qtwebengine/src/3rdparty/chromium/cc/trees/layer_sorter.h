// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_SORTER_H_
#define CC_TREES_LAYER_SORTER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/vector3d_f.h"

#if defined(COMPILER_GCC)
namespace cc { struct GraphEdge; }

namespace BASE_HASH_NAMESPACE {
template <>
struct hash<cc::GraphEdge*> {
  size_t operator()(cc::GraphEdge* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

namespace gfx {
class Transform;
}

namespace cc {
struct GraphEdge;

// Holds various useful properties derived from a layer's 3D outline.
struct CC_EXPORT LayerShape {
  LayerShape();
  LayerShape(float width, float height, const gfx::Transform& draw_transform);
  ~LayerShape();

  float LayerZFromProjectedPoint(const gfx::PointF& p) const;

  gfx::Vector3dF layer_normal;
  gfx::Point3F transform_origin;
  gfx::QuadF projected_quad;
  gfx::RectF projected_bounds;
};

struct GraphNode {
  explicit GraphNode(LayerImpl* layer_impl);
  ~GraphNode();

  LayerImpl* layer;
  LayerShape shape;
  std::vector<GraphEdge*> incoming;
  std::vector<GraphEdge*> outgoing;
  float incoming_edge_weight;
};

struct GraphEdge {
  GraphEdge(GraphNode* from_node, GraphNode* to_node, float weight)
      : from(from_node),
        to(to_node),
        weight(weight) {}

  GraphNode* from;
  GraphNode* to;
  float weight;
};



class CC_EXPORT LayerSorter {
 public:
  LayerSorter();
  ~LayerSorter();

  void Sort(LayerImplList::iterator first, LayerImplList::iterator last);

  enum ABCompareResult {
    ABeforeB,
    BBeforeA,
    None
  };

  static ABCompareResult CheckOverlap(LayerShape* a,
                                      LayerShape* b,
                                      float z_threshold,
                                      float* weight);

 private:
  typedef std::vector<GraphNode> NodeList;
  typedef std::vector<GraphEdge> EdgeList;
  NodeList nodes_;
  EdgeList edges_;
  float z_range_;

  typedef base::hash_map<GraphEdge*, GraphEdge*> EdgeMap;
  EdgeMap active_edges_;

  void CreateGraphNodes(LayerImplList::iterator first,
                        LayerImplList::iterator last);
  void CreateGraphEdges();
  void RemoveEdgeFromList(GraphEdge* graph, std::vector<GraphEdge*>* list);

  DISALLOW_COPY_AND_ASSIGN(LayerSorter);
};

}  // namespace cc
#endif  // CC_TREES_LAYER_SORTER_H_
