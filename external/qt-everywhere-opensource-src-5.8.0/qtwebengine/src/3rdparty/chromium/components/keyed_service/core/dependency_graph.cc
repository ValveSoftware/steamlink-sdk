// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/dependency_graph.h"

#include <stddef.h>

#include <algorithm>
#include <deque>
#include <iterator>

#include "base/strings/string_piece.h"

namespace {

// Escapes |id| to be a valid ID in the DOT format [1]. This is implemented as
// enclosing the string in quotation marks, and escaping any quotation marks
// found with backslashes.
// [1] http://www.graphviz.org/content/dot-language
std::string Escape(base::StringPiece id) {
  std::string result = "\"";
  result.reserve(id.size() + 2);  // +2 for the enclosing quotes.
  size_t after_last_quot = 0;
  size_t next_quot = id.find('"');
  while (next_quot != base::StringPiece::npos) {
    result.append(id.data() + after_last_quot, next_quot - after_last_quot);
    result.append("\"");
    after_last_quot = next_quot + 1;
    next_quot = id.find('"', after_last_quot);
  }
  result.append(id.data() + after_last_quot, id.size() - after_last_quot);
  result.append("\"");
  return result;
}

}  // namespace

DependencyGraph::DependencyGraph() {}

DependencyGraph::~DependencyGraph() {}

void DependencyGraph::AddNode(DependencyNode* node) {
  all_nodes_.push_back(node);
  construction_order_.clear();
}

void DependencyGraph::RemoveNode(DependencyNode* node) {
  all_nodes_.erase(std::remove(all_nodes_.begin(), all_nodes_.end(), node),
                   all_nodes_.end());

  // Remove all dependency edges that contain this node.
  EdgeMap::iterator it = edges_.begin();
  while (it != edges_.end()) {
    EdgeMap::iterator temp = it;
    ++it;

    if (temp->first == node || temp->second == node)
      edges_.erase(temp);
  }

  construction_order_.clear();
}

void DependencyGraph::AddEdge(DependencyNode* depended,
                              DependencyNode* dependee) {
  edges_.insert(std::make_pair(depended, dependee));
  construction_order_.clear();
}

bool DependencyGraph::GetConstructionOrder(
    std::vector<DependencyNode*>* order) {
  if (construction_order_.empty() && !BuildConstructionOrder())
    return false;

  *order = construction_order_;
  return true;
}

bool DependencyGraph::GetDestructionOrder(std::vector<DependencyNode*>* order) {
  if (construction_order_.empty() && !BuildConstructionOrder())
    return false;

  *order = construction_order_;

  // Destroy nodes in reverse order.
  std::reverse(order->begin(), order->end());

  return true;
}

bool DependencyGraph::BuildConstructionOrder() {
  // Step 1: Build a set of nodes with no incoming edges.
  std::deque<DependencyNode*> queue;
  std::copy(all_nodes_.begin(), all_nodes_.end(), std::back_inserter(queue));

  std::deque<DependencyNode*>::iterator queue_end = queue.end();
  for (EdgeMap::const_iterator it = edges_.begin(); it != edges_.end(); ++it) {
    queue_end = std::remove(queue.begin(), queue_end, it->second);
  }
  queue.erase(queue_end, queue.end());

  // Step 2: Do the Kahn topological sort.
  std::vector<DependencyNode*> output;
  EdgeMap edges(edges_);
  while (!queue.empty()) {
    DependencyNode* node = queue.front();
    queue.pop_front();
    output.push_back(node);

    std::pair<EdgeMap::iterator, EdgeMap::iterator> range =
        edges.equal_range(node);
    EdgeMap::iterator it = range.first;
    while (it != range.second) {
      DependencyNode* dest = it->second;
      EdgeMap::iterator temp = it;
      it++;
      edges.erase(temp);

      bool has_incoming_edges = false;
      for (EdgeMap::iterator jt = edges.begin(); jt != edges.end(); ++jt) {
        if (jt->second == dest) {
          has_incoming_edges = true;
          break;
        }
      }

      if (!has_incoming_edges)
        queue.push_back(dest);
    }
  }

  if (!edges.empty()) {
    // Dependency graph has a cycle.
    return false;
  }

  construction_order_ = output;
  return true;
}

std::string DependencyGraph::DumpAsGraphviz(
    const std::string& toplevel_name,
    const base::Callback<std::string(DependencyNode*)>& node_name_callback)
    const {
  std::string result("digraph {\n");
  std::string escaped_toplevel_name = Escape(toplevel_name);

  // Make a copy of all nodes.
  std::deque<DependencyNode*> nodes;
  std::copy(all_nodes_.begin(), all_nodes_.end(), std::back_inserter(nodes));

  // State all dependencies and remove |second| so we don't generate an
  // implicit dependency on the top level node.
  std::deque<DependencyNode*>::iterator nodes_end(nodes.end());
  result.append("  /* Dependencies */\n");
  for (EdgeMap::const_iterator it = edges_.begin(); it != edges_.end(); ++it) {
    result.append("  ");
    result.append(Escape(node_name_callback.Run(it->second)));
    result.append(" -> ");
    result.append(Escape(node_name_callback.Run(it->first)));
    result.append(";\n");

    nodes_end = std::remove(nodes.begin(), nodes_end, it->second);
  }
  nodes.erase(nodes_end, nodes.end());

  // Every node that doesn't depend on anything else will implicitly depend on
  // the top level node.
  result.append("\n  /* Toplevel attachments */\n");
  for (std::deque<DependencyNode*>::const_iterator it = nodes.begin();
       it != nodes.end();
       ++it) {
    result.append("  ");
    result.append(Escape(node_name_callback.Run(*it)));
    result.append(" -> ");
    result.append(escaped_toplevel_name);
    result.append(";\n");
  }

  result.append("\n  /* Toplevel node */\n");
  result.append("  ");
  result.append(escaped_toplevel_name);
  result.append(" [shape=box];\n");

  result.append("}\n");
  return result;
}
