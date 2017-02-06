// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree_node_blame_context.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/trace_event_analyzer.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event_argument.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"

namespace content {

namespace {

bool EventPointerCompare(const trace_analyzer::TraceEvent* lhs,
                         const trace_analyzer::TraceEvent* rhs) {
  CHECK(lhs);
  CHECK(rhs);
  return *lhs < *rhs;
}

void OnTraceDataCollected(base::Closure quit_closure,
                          base::trace_event::TraceResultBuffer* buffer,
                          const scoped_refptr<base::RefCountedString>& json,
                          bool has_more_events) {
  buffer->AddFragment(json->data());
  if (!has_more_events)
    quit_closure.Run();
}

void ExpectFrameTreeNodeObject(const trace_analyzer::TraceEvent* event) {
  EXPECT_EQ("navigation", event->category);
  EXPECT_EQ("FrameTreeNode", event->name);
}

void ExpectFrameTreeNodeSnapshot(const trace_analyzer::TraceEvent* event) {
  ExpectFrameTreeNodeObject(event);
  EXPECT_TRUE(event->HasArg("snapshot"));
  EXPECT_TRUE(event->arg_values.at("snapshot")
                  ->IsType(base::Value::Type::TYPE_DICTIONARY));
}

std::string GetParentNodeID(const trace_analyzer::TraceEvent* event) {
  const base::Value* arg_snapshot = event->arg_values.at("snapshot").get();
  const base::DictionaryValue* snapshot;
  EXPECT_TRUE(arg_snapshot->GetAsDictionary(&snapshot));
  if (!snapshot->HasKey("parent"))
    return std::string();
  const base::DictionaryValue* parent;
  EXPECT_TRUE(snapshot->GetDictionary("parent", &parent));
  std::string parent_id;
  EXPECT_TRUE(parent->GetString("id_ref", &parent_id));
  return parent_id;
}

std::string GetSnapshotURL(const trace_analyzer::TraceEvent* event) {
  const base::Value* arg_snapshot = event->arg_values.at("snapshot").get();
  const base::DictionaryValue* snapshot;
  EXPECT_TRUE(arg_snapshot->GetAsDictionary(&snapshot));
  if (!snapshot->HasKey("url"))
    return std::string();
  std::string url;
  EXPECT_TRUE(snapshot->GetString("url", &url));
  return url;
}

}  // namespace

class FrameTreeNodeBlameContextTest : public RenderViewHostImplTestHarness {
 public:
  FrameTree* tree() { return contents()->GetFrameTree(); }
  FrameTreeNode* root() { return tree()->root(); }
  int process_id() {
    return root()->current_frame_host()->GetProcess()->GetID();
  }

  // Creates a frame tree specified by |shape|, which is a string of paired
  // parentheses. Each pair of parentheses represents a FrameTreeNode, and the
  // nesting of parentheses represents the parent-child relation between nodes.
  // Nodes represented by outer-most parentheses are children of the root node.
  // NOTE: Each node can have at most 9 child nodes, and the tree height (i.e.,
  // max # of edges in any root-to-leaf path) must be at most 9.
  // See the test cases for sample usage.
  void CreateFrameTree(const char* shape) {
    main_test_rfh()->InitializeRenderFrameIfNeeded();
    CreateSubframes(root(), 1, shape);
  }

  void RemoveAllNonRootFrames() {
    while (root()->child_count())
      tree()->RemoveFrame(root()->child_at(0));
  }

  void StartTracing() {
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        base::trace_event::TraceConfig("*"),
        base::trace_event::TraceLog::RECORDING_MODE);
  }

  void StopTracing() {
    base::trace_event::TraceLog::GetInstance()->SetDisabled();
  }

  std::unique_ptr<trace_analyzer::TraceAnalyzer> CreateTraceAnalyzer() {
    base::trace_event::TraceResultBuffer buffer;
    base::trace_event::TraceResultBuffer::SimpleOutput trace_output;
    buffer.SetOutputCallback(trace_output.GetCallback());
    base::RunLoop run_loop;
    buffer.Start();
    base::trace_event::TraceLog::GetInstance()->Flush(
        base::Bind(&OnTraceDataCollected, run_loop.QuitClosure(),
                   base::Unretained(&buffer)));
    run_loop.Run();
    buffer.Finish();

    return base::WrapUnique(
        trace_analyzer::TraceAnalyzer::Create(trace_output.json_output));
  }

 private:
  int CreateSubframes(FrameTreeNode* node, int self_id, const char* shape) {
    int consumption = 0;
    for (int child_num = 1; shape[consumption++] == '('; ++child_num) {
      int child_id = self_id * 10 + child_num;
      tree()->AddFrame(
          node, process_id(), child_id, blink::WebTreeScopeType::Document,
          std::string(), base::StringPrintf("uniqueName%d", child_id),
          blink::WebSandboxFlags::None, blink::WebFrameOwnerProperties());
      FrameTreeNode* child = node->child_at(child_num - 1);
      consumption += CreateSubframes(child, child_id, shape + consumption);
    }
    return consumption;
  }
};

// Creates a frame tree, tests if (i) the creation of each new frame is
// correctly traced, and (ii) the topology given by the snapshots is correct.
TEST_F(FrameTreeNodeBlameContextTest, FrameCreation) {
  /* Shape of the frame tree to be created:
   *        ()
   *      /    \
   *     ()    ()
   *    /  \   |
   *   ()  ()  ()
   *           |
   *           ()
   */
  const char* tree_shape = "(()())((()))";

  StartTracing();
  CreateFrameTree(tree_shape);
  StopTracing();

  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer =
      CreateTraceAnalyzer();
  trace_analyzer::TraceEventVector events;
  trace_analyzer::Query q =
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_CREATE_OBJECT) ||
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_SNAPSHOT_OBJECT);
  analyzer->FindEvents(q, &events);

  // Two events for each new node: creation and snapshot.
  EXPECT_EQ(12u, events.size());

  std::set<FrameTreeNode*> creation_traced;
  std::set<FrameTreeNode*> snapshot_traced;
  for (auto event : events) {
    ExpectFrameTreeNodeObject(event);
    FrameTreeNode* node =
        tree()->FindByID(strtol(event->id.c_str(), nullptr, 16));
    EXPECT_NE(nullptr, node);
    if (event->HasArg("snapshot")) {
      ExpectFrameTreeNodeSnapshot(event);
      EXPECT_FALSE(ContainsValue(snapshot_traced, node));
      snapshot_traced.insert(node);
      std::string parent_id = GetParentNodeID(event);
      EXPECT_FALSE(parent_id.empty());
      EXPECT_EQ(node->parent(),
                tree()->FindByID(strtol(parent_id.c_str(), nullptr, 16)));
    } else {
      EXPECT_EQ(TRACE_EVENT_PHASE_CREATE_OBJECT, event->phase);
      EXPECT_FALSE(ContainsValue(creation_traced, node));
      creation_traced.insert(node);
    }
  }
}

// Deletes frames from a frame tree, tests if the destruction of each frame is
// correctly traced.
TEST_F(FrameTreeNodeBlameContextTest, FrameDeletion) {
  /* Shape of the frame tree to be created:
   *        ()
   *      /    \
   *     ()    ()
   *    /  \   |
   *   ()  ()  ()
   *           |
   *           ()
   */
  const char* tree_shape = "(()())((()))";

  CreateFrameTree(tree_shape);
  std::set<int> node_ids;
  for (FrameTreeNode* node : tree()->Nodes())
    node_ids.insert(node->frame_tree_node_id());

  StartTracing();
  RemoveAllNonRootFrames();
  StopTracing();

  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer =
      CreateTraceAnalyzer();
  trace_analyzer::TraceEventVector events;
  trace_analyzer::Query q =
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_DELETE_OBJECT);
  analyzer->FindEvents(q, &events);

  // The removal of all non-root nodes should be traced.
  EXPECT_EQ(6u, events.size());
  for (auto event : events) {
    ExpectFrameTreeNodeObject(event);
    int id = strtol(event->id.c_str(), nullptr, 16);
    EXPECT_TRUE(ContainsValue(node_ids, id));
    node_ids.erase(id);
  }
}

// Changes URL of the root node. Tests if URL change is correctly traced.
TEST_F(FrameTreeNodeBlameContextTest, URLChange) {
  main_test_rfh()->InitializeRenderFrameIfNeeded();
  GURL url1("http://a.com/");
  GURL url2("https://b.net/");

  StartTracing();
  root()->SetCurrentURL(url1);
  root()->SetCurrentURL(url2);
  root()->ResetForNewProcess();
  StopTracing();

  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer =
      CreateTraceAnalyzer();
  trace_analyzer::TraceEventVector events;
  trace_analyzer::Query q =
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_SNAPSHOT_OBJECT);
  analyzer->FindEvents(q, &events);
  std::sort(events.begin(), events.end(), EventPointerCompare);

  // Three snapshots are traced, one for each URL change.
  EXPECT_EQ(3u, events.size());
  EXPECT_EQ(url1.spec(), GetSnapshotURL(events[0]));
  EXPECT_EQ(url2.spec(), GetSnapshotURL(events[1]));
  EXPECT_EQ("", GetSnapshotURL(events[2]));
}

}  // namespace content
