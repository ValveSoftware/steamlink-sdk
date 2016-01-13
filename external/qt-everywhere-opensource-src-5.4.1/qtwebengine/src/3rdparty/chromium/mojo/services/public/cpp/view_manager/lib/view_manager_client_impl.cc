// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_manager_client_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/public/cpp/view_manager/lib/node_private.h"
#include "mojo/services/public/cpp/view_manager/lib/view_private.h"
#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace mojo {
namespace view_manager {

Id MakeTransportId(ConnectionSpecificId connection_id,
                   ConnectionSpecificId local_id) {
  return (connection_id << 16) | local_id;
}

// Helper called to construct a local node/view object from transport data.
Node* AddNodeToViewManager(ViewManagerClientImpl* client,
                           Node* parent,
                           Id node_id,
                           Id view_id,
                           const gfx::Rect& bounds) {
  // We don't use the ctor that takes a ViewManager here, since it will call
  // back to the service and attempt to create a new node.
  Node* node = NodePrivate::LocalCreate();
  NodePrivate private_node(node);
  private_node.set_view_manager(client);
  private_node.set_id(node_id);
  private_node.LocalSetBounds(gfx::Rect(), bounds);
  if (parent)
    NodePrivate(parent).LocalAddChild(node);
  client->AddNode(node);

  // View.
  if (view_id != 0) {
    View* view = ViewPrivate::LocalCreate();
    ViewPrivate private_view(view);
    private_view.set_view_manager(client);
    private_view.set_id(view_id);
    private_view.set_node(node);
    // TODO(beng): this broadcasts notifications locally... do we want this? I
    //             don't think so. same story for LocalAddChild above!
    private_node.LocalSetActiveView(view);
    client->AddView(view);
  }
  return node;
}

Node* BuildNodeTree(ViewManagerClientImpl* client,
                    const Array<NodeDataPtr>& nodes) {
  std::vector<Node*> parents;
  Node* root = NULL;
  Node* last_node = NULL;
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (last_node && nodes[i]->parent_id == last_node->id()) {
      parents.push_back(last_node);
    } else if (!parents.empty()) {
      while (parents.back()->id() != nodes[i]->parent_id)
        parents.pop_back();
    }
    Node* node = AddNodeToViewManager(
        client,
        !parents.empty() ? parents.back() : NULL,
        nodes[i]->node_id,
        nodes[i]->view_id,
        nodes[i]->bounds.To<gfx::Rect>());
    if (!last_node)
      root = node;
    last_node = node;
  }
  return root;
}

// Responsible for removing a root from the ViewManager when that node is
// destroyed.
class RootObserver : public NodeObserver {
 public:
  explicit RootObserver(Node* root) : root_(root) {}
  virtual ~RootObserver() {}

 private:
  // Overridden from NodeObserver:
  virtual void OnNodeDestroy(Node* node,
                             DispositionChangePhase phase) OVERRIDE {
    DCHECK_EQ(node, root_);
    if (phase != NodeObserver::DISPOSITION_CHANGED)
      return;
    static_cast<ViewManagerClientImpl*>(
        NodePrivate(root_).view_manager())->RemoveRoot(root_);
    delete this;
  }

  Node* root_;

  DISALLOW_COPY_AND_ASSIGN(RootObserver);
};

class ViewManagerTransaction {
 public:
  virtual ~ViewManagerTransaction() {}

  void Commit() {
    DCHECK(!committed_);
    DoCommit();
    committed_ = true;
  }

  bool committed() const { return committed_; }

 protected:
  explicit ViewManagerTransaction(ViewManagerClientImpl* client)
      : committed_(false),
        client_(client) {
  }

  // Overridden to perform transaction-specific commit actions.
  virtual void DoCommit() = 0;

  // Overridden to perform transaction-specific cleanup on commit ack from the
  // service.
  virtual void DoActionCompleted(bool success) = 0;

  ViewManagerService* service() { return client_->service_; }

  Id GetAndAdvanceNextServerChangeId() {
    return client_->next_server_change_id_++;
  }

  base::Callback<void(bool)> ActionCompletedCallback() {
    return base::Bind(&ViewManagerTransaction::OnActionCompleted,
                      base::Unretained(this));
  }

 private:
  // General callback to be used for commits to the service.
  void OnActionCompleted(bool success) {
    DCHECK(success);
    DoActionCompleted(success);
    client_->RemoveFromPendingQueue(this);
  }

  bool committed_;
  ViewManagerClientImpl* client_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerTransaction);
};

class CreateViewTransaction : public ViewManagerTransaction {
 public:
  CreateViewTransaction(Id view_id, ViewManagerClientImpl* client)
      : ViewManagerTransaction(client),
        view_id_(view_id) {}
  virtual ~CreateViewTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->CreateView(view_id_, ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): failure.
  }

  const Id view_id_;

  DISALLOW_COPY_AND_ASSIGN(CreateViewTransaction);
};

class DestroyViewTransaction : public ViewManagerTransaction {
 public:
  DestroyViewTransaction(Id view_id, ViewManagerClientImpl* client)
      : ViewManagerTransaction(client),
        view_id_(view_id) {}
  virtual ~DestroyViewTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->DeleteView(view_id_, ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const Id view_id_;

  DISALLOW_COPY_AND_ASSIGN(DestroyViewTransaction);
};

class CreateNodeTransaction : public ViewManagerTransaction {
 public:
  CreateNodeTransaction(Id node_id, ViewManagerClientImpl* client)
      : ViewManagerTransaction(client),
        node_id_(node_id) {}
  virtual ~CreateNodeTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->CreateNode(node_id_, ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): Failure means we tried to create with an extant id for this
    //             connection. It also could mean we tried to do something
    //             invalid, or we tried applying a change out of order. Figure
    //             out what to do.
  }

  const Id node_id_;

  DISALLOW_COPY_AND_ASSIGN(CreateNodeTransaction);
};

class DestroyNodeTransaction : public ViewManagerTransaction {
 public:
  DestroyNodeTransaction(Id node_id, ViewManagerClientImpl* client)
      : ViewManagerTransaction(client),
        node_id_(node_id) {}
  virtual ~DestroyNodeTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->DeleteNode(node_id_,
                          GetAndAdvanceNextServerChangeId(),
                          ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const Id node_id_;
  DISALLOW_COPY_AND_ASSIGN(DestroyNodeTransaction);
};

class AddChildTransaction : public ViewManagerTransaction {
 public:
  AddChildTransaction(Id child_id,
                      Id parent_id,
                      ViewManagerClientImpl* client)
      : ViewManagerTransaction(client),
        child_id_(child_id),
        parent_id_(parent_id) {}
  virtual ~AddChildTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->AddNode(parent_id_,
                       child_id_,
                       GetAndAdvanceNextServerChangeId(),
                       ActionCompletedCallback());
  }

  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const Id child_id_;
  const Id parent_id_;

  DISALLOW_COPY_AND_ASSIGN(AddChildTransaction);
};

class RemoveChildTransaction : public ViewManagerTransaction {
 public:
  RemoveChildTransaction(Id child_id, ViewManagerClientImpl* client)
      : ViewManagerTransaction(client),
        child_id_(child_id) {}
  virtual ~RemoveChildTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->RemoveNodeFromParent(
        child_id_,
        GetAndAdvanceNextServerChangeId(),
        ActionCompletedCallback());
  }

  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const Id child_id_;

  DISALLOW_COPY_AND_ASSIGN(RemoveChildTransaction);
};

class ReorderNodeTransaction : public ViewManagerTransaction {
 public:
  ReorderNodeTransaction(Id node_id,
                         Id relative_id,
                         OrderDirection direction,
                         ViewManagerClientImpl* client)
      : ViewManagerTransaction(client),
        node_id_(node_id),
        relative_id_(relative_id),
        direction_(direction) {}
  virtual ~ReorderNodeTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->ReorderNode(node_id_,
                           relative_id_,
                           direction_,
                           GetAndAdvanceNextServerChangeId(),
                           ActionCompletedCallback());
  }

  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const Id node_id_;
  const Id relative_id_;
  const OrderDirection direction_;

  DISALLOW_COPY_AND_ASSIGN(ReorderNodeTransaction);
};

class SetActiveViewTransaction : public ViewManagerTransaction {
 public:
  SetActiveViewTransaction(Id node_id,
                           Id view_id,
                           ViewManagerClientImpl* client)
      : ViewManagerTransaction(client),
        node_id_(node_id),
        view_id_(view_id) {}
  virtual ~SetActiveViewTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->SetView(node_id_, view_id_, ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const Id node_id_;
  const Id view_id_;

  DISALLOW_COPY_AND_ASSIGN(SetActiveViewTransaction);
};

class SetBoundsTransaction : public ViewManagerTransaction {
 public:
  SetBoundsTransaction(Id node_id,
                       const gfx::Rect& bounds,
                       ViewManagerClientImpl* client)
      : ViewManagerTransaction(client),
        node_id_(node_id),
        bounds_(bounds) {}
  virtual ~SetBoundsTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->SetNodeBounds(
        node_id_, Rect::From(bounds_), ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const Id node_id_;
  const gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(SetBoundsTransaction);
};

class SetViewContentsTransaction : public ViewManagerTransaction {
 public:
  SetViewContentsTransaction(Id view_id,
                             const SkBitmap& contents,
                             ViewManagerClientImpl* client)
      : ViewManagerTransaction(client),
        view_id_(view_id),
        contents_(contents) {}
  virtual ~SetViewContentsTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    std::vector<unsigned char> data;
    gfx::PNGCodec::EncodeBGRASkBitmap(contents_, false, &data);

    void* memory = NULL;
    ScopedSharedBufferHandle duped;
    bool result = CreateMapAndDupSharedBuffer(data.size(),
                                              &memory,
                                              &shared_state_handle_,
                                              &duped);
    if (!result)
      return;

    memcpy(memory, &data[0], data.size());

    service()->SetViewContents(view_id_, duped.Pass(),
                               static_cast<uint32_t>(data.size()),
                               ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  bool CreateMapAndDupSharedBuffer(size_t size,
                                   void** memory,
                                   ScopedSharedBufferHandle* handle,
                                   ScopedSharedBufferHandle* duped) {
    MojoResult result = CreateSharedBuffer(NULL, size, handle);
    if (result != MOJO_RESULT_OK)
      return false;
    DCHECK(handle->is_valid());

    result = DuplicateBuffer(handle->get(), NULL, duped);
    if (result != MOJO_RESULT_OK)
      return false;
    DCHECK(duped->is_valid());

    result = MapBuffer(
        handle->get(), 0, size, memory, MOJO_MAP_BUFFER_FLAG_NONE);
    if (result != MOJO_RESULT_OK)
      return false;
    DCHECK(*memory);

    return true;
  }

  const Id view_id_;
  const SkBitmap contents_;
  ScopedSharedBufferHandle shared_state_handle_;

  DISALLOW_COPY_AND_ASSIGN(SetViewContentsTransaction);
};

class EmbedTransaction : public ViewManagerTransaction {
 public:
  EmbedTransaction(const String& url,
                   Id node_id,
                   ViewManagerClientImpl* client)
      : ViewManagerTransaction(client),
        url_(url),
        node_id_(node_id) {}
  virtual ~EmbedTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    std::vector<Id> ids;
    ids.push_back(node_id_);
    service()->Embed(url_, Array<Id>::From(ids), ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const String url_;
  const Id node_id_;

  DISALLOW_COPY_AND_ASSIGN(EmbedTransaction);
};

class SetFocusTransaction : public ViewManagerTransaction {
 public:
  SetFocusTransaction(Id node_id, ViewManagerClientImpl* client)
      : ViewManagerTransaction(client),
        node_id_(node_id) {}
  virtual ~SetFocusTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->SetFocus(node_id_, ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const Id node_id_;

  DISALLOW_COPY_AND_ASSIGN(SetFocusTransaction);
};

ViewManagerClientImpl::ViewManagerClientImpl(ViewManagerDelegate* delegate)
    : connected_(false),
      connection_id_(0),
      next_id_(1),
      next_server_change_id_(0),
      delegate_(delegate) {}

ViewManagerClientImpl::~ViewManagerClientImpl() {
  while (!nodes_.empty()) {
    IdToNodeMap::iterator it = nodes_.begin();
    if (OwnsNode(it->second->id()))
      it->second->Destroy();
    else
      nodes_.erase(it);
  }
  while (!views_.empty()) {
    IdToViewMap::iterator it = views_.begin();
    if (OwnsView(it->second->id()))
      it->second->Destroy();
    else
      views_.erase(it);
  }
}

Id ViewManagerClientImpl::CreateNode() {
  DCHECK(connected_);
  const Id node_id(MakeTransportId(connection_id_, ++next_id_));
  pending_transactions_.push_back(new CreateNodeTransaction(node_id, this));
  Sync();
  return node_id;
}

void ViewManagerClientImpl::DestroyNode(Id node_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(new DestroyNodeTransaction(node_id, this));
  Sync();
}

Id ViewManagerClientImpl::CreateView() {
  DCHECK(connected_);
  const Id view_id(MakeTransportId(connection_id_, ++next_id_));
  pending_transactions_.push_back(new CreateViewTransaction(view_id, this));
  Sync();
  return view_id;
}

void ViewManagerClientImpl::DestroyView(Id view_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(new DestroyViewTransaction(view_id, this));
  Sync();
}

void ViewManagerClientImpl::AddChild(Id child_id,
                                     Id parent_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new AddChildTransaction(child_id, parent_id, this));
  Sync();
}

void ViewManagerClientImpl::RemoveChild(Id child_id, Id parent_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(new RemoveChildTransaction(child_id, this));
  Sync();
}

void ViewManagerClientImpl::Reorder(
    Id node_id,
    Id relative_node_id,
    OrderDirection direction) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new ReorderNodeTransaction(node_id, relative_node_id, direction, this));
  Sync();
}

bool ViewManagerClientImpl::OwnsNode(Id id) const {
  return HiWord(id) == connection_id_;
}

bool ViewManagerClientImpl::OwnsView(Id id) const {
  return HiWord(id) == connection_id_;
}

void ViewManagerClientImpl::SetActiveView(Id node_id, Id view_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new SetActiveViewTransaction(node_id, view_id, this));
  Sync();
}

void ViewManagerClientImpl::SetBounds(Id node_id, const gfx::Rect& bounds) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new SetBoundsTransaction(node_id, bounds, this));
  Sync();
}

void ViewManagerClientImpl::SetViewContents(Id view_id,
                                            const SkBitmap& contents) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new SetViewContentsTransaction(view_id, contents, this));
  Sync();
}

void ViewManagerClientImpl::SetFocus(Id node_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(new SetFocusTransaction(node_id, this));
  Sync();
}

void ViewManagerClientImpl::Embed(const String& url, Id node_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(new EmbedTransaction(url, node_id, this));
  Sync();
}

void ViewManagerClientImpl::AddNode(Node* node) {
  DCHECK(nodes_.find(node->id()) == nodes_.end());
  nodes_[node->id()] = node;
}

void ViewManagerClientImpl::RemoveNode(Id node_id) {
  IdToNodeMap::iterator it = nodes_.find(node_id);
  if (it != nodes_.end())
    nodes_.erase(it);
}

void ViewManagerClientImpl::AddView(View* view) {
  DCHECK(views_.find(view->id()) == views_.end());
  views_[view->id()] = view;
}

void ViewManagerClientImpl::RemoveView(Id view_id) {
  IdToViewMap::iterator it = views_.find(view_id);
  if (it != views_.end())
    views_.erase(it);
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerClientImpl, ViewManager implementation:

const std::string& ViewManagerClientImpl::GetEmbedderURL() const {
  return creator_url_;
}

const std::vector<Node*>& ViewManagerClientImpl::GetRoots() const {
  return roots_;
}

Node* ViewManagerClientImpl::GetNodeById(Id id) {
  IdToNodeMap::const_iterator it = nodes_.find(id);
  return it != nodes_.end() ? it->second : NULL;
}

View* ViewManagerClientImpl::GetViewById(Id id) {
  IdToViewMap::const_iterator it = views_.find(id);
  return it != views_.end() ? it->second : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerClientImpl, InterfaceImpl overrides:

void ViewManagerClientImpl::OnConnectionEstablished() {
  service_ = client();
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerClientImpl, ViewManagerClient implementation:

void ViewManagerClientImpl::OnViewManagerConnectionEstablished(
    ConnectionSpecificId connection_id,
    const String& creator_url,
    Id next_server_change_id,
    Array<NodeDataPtr> nodes) {
  connected_ = true;
  connection_id_ = connection_id;
  creator_url_ = TypeConverter<String, std::string>::ConvertFrom(creator_url);
  next_server_change_id_ = next_server_change_id;

  DCHECK(pending_transactions_.empty());
  AddRoot(BuildNodeTree(this, nodes));
}

void ViewManagerClientImpl::OnRootsAdded(Array<NodeDataPtr> nodes) {
  AddRoot(BuildNodeTree(this, nodes));
}

void ViewManagerClientImpl::OnServerChangeIdAdvanced(
    Id next_server_change_id) {
  next_server_change_id_ = next_server_change_id;
}

void ViewManagerClientImpl::OnNodeBoundsChanged(Id node_id,
                                                RectPtr old_bounds,
                                                RectPtr new_bounds) {
  Node* node = GetNodeById(node_id);
  NodePrivate(node).LocalSetBounds(old_bounds.To<gfx::Rect>(),
                                   new_bounds.To<gfx::Rect>());
}

void ViewManagerClientImpl::OnNodeHierarchyChanged(
    Id node_id,
    Id new_parent_id,
    Id old_parent_id,
    Id server_change_id,
    mojo::Array<NodeDataPtr> nodes) {
  next_server_change_id_ = server_change_id + 1;

  BuildNodeTree(this, nodes);

  Node* new_parent = GetNodeById(new_parent_id);
  Node* old_parent = GetNodeById(old_parent_id);
  Node* node = GetNodeById(node_id);
  if (new_parent)
    NodePrivate(new_parent).LocalAddChild(node);
  else
    NodePrivate(old_parent).LocalRemoveChild(node);
}

void ViewManagerClientImpl::OnNodeReordered(Id node_id,
                                            Id relative_node_id,
                                            OrderDirection direction,
                                            Id server_change_id) {
  next_server_change_id_ = server_change_id + 1;

  Node* node = GetNodeById(node_id);
  Node* relative_node = GetNodeById(relative_node_id);
  if (node && relative_node) {
    NodePrivate(node).LocalReorder(relative_node, direction);
  }
}

void ViewManagerClientImpl::OnNodeDeleted(Id node_id, Id server_change_id) {
  next_server_change_id_ = server_change_id + 1;

  Node* node = GetNodeById(node_id);
  if (node)
    NodePrivate(node).LocalDestroy();
}

void ViewManagerClientImpl::OnNodeViewReplaced(Id node_id,
                                               Id new_view_id,
                                               Id old_view_id) {
  Node* node = GetNodeById(node_id);
  View* new_view = GetViewById(new_view_id);
  if (!new_view && new_view_id != 0) {
    // This client wasn't aware of this View until now.
    new_view = ViewPrivate::LocalCreate();
    ViewPrivate private_view(new_view);
    private_view.set_view_manager(this);
    private_view.set_id(new_view_id);
    private_view.set_node(node);
    AddView(new_view);
  }
  View* old_view = GetViewById(old_view_id);
  DCHECK_EQ(old_view, node->active_view());
  NodePrivate(node).LocalSetActiveView(new_view);
}

void ViewManagerClientImpl::OnViewDeleted(Id view_id) {
  View* view = GetViewById(view_id);
  if (view)
    ViewPrivate(view).LocalDestroy();
}

void ViewManagerClientImpl::OnViewInputEvent(
    Id view_id,
    EventPtr event,
    const Callback<void()>& ack_callback) {
  View* view = GetViewById(view_id);
  if (view) {
    FOR_EACH_OBSERVER(ViewObserver,
                      *ViewPrivate(view).observers(),
                      OnViewInputEvent(view, event));
  }
  ack_callback.Run();
}

void ViewManagerClientImpl::DispatchOnViewInputEvent(Id view_id,
                                                     EventPtr event) {
  // For now blindly bounce the message back to the server. Doing this means the
  // event is sent to the correct target (|view_id|).
  // Note: This function is only invoked on the window manager.
  service_->DispatchOnViewInputEvent(view_id, event.Pass());
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerClientImpl, private:

void ViewManagerClientImpl::Sync() {
  // The service connection may not be set up yet. OnConnectionEstablished()
  // will schedule another sync when it is.
  if (!connected_)
    return;

  Transactions::const_iterator it = pending_transactions_.begin();
  for (; it != pending_transactions_.end(); ++it) {
    if (!(*it)->committed())
      (*it)->Commit();
  }
}

void ViewManagerClientImpl::RemoveFromPendingQueue(
    ViewManagerTransaction* transaction) {
  DCHECK_EQ(transaction, pending_transactions_.front());
  pending_transactions_.erase(pending_transactions_.begin());
  if (pending_transactions_.empty() && !changes_acked_callback_.is_null())
    changes_acked_callback_.Run();
}

void ViewManagerClientImpl::AddRoot(Node* root) {
  // A new root must not already exist as a root or be contained by an existing
  // hierarchy visible to this view manager.
  std::vector<Node*>::const_iterator it = roots_.begin();
  for (; it != roots_.end(); ++it) {
    if (*it == root || (*it)->Contains(root))
      return;
  }
  roots_.push_back(root);
  root->AddObserver(new RootObserver(root));
  delegate_->OnRootAdded(this, root);
}

void ViewManagerClientImpl::RemoveRoot(Node* root) {
  std::vector<Node*>::iterator it =
      std::find(roots_.begin(), roots_.end(), root);
  if (it != roots_.end())
    roots_.erase(it);
}

////////////////////////////////////////////////////////////////////////////////
// ViewManager, public:

// static
void ViewManager::Create(Application* application,
                         ViewManagerDelegate* delegate) {
  application->AddService<ViewManagerClientImpl>(delegate);
}

}  // namespace view_manager
}  // namespace mojo
