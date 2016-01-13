// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A server side dispatcher which dispatches a given client's data to their
// stream.

#ifndef NET_QUIC_QUIC_DISPATCHER_H_
#define NET_QUIC_QUIC_DISPATCHER_H_

#include <list>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/base/linked_hash_map.h"
#include "net/quic/quic_blocked_writer_interface.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_session.h"
#include "net/quic/quic_time_wait_list_manager.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template <>
struct hash<net::QuicBlockedWriterInterface*> {
  std::size_t operator()(const net::QuicBlockedWriterInterface* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}
#endif

namespace net {

class QuicConfig;
class QuicCryptoServerConfig;
class QuicPacketWriterWrapper;
class QuicSession;
class UDPServerSocket;

namespace test {
class QuicDispatcherPeer;
}  // namespace test

class DeleteSessionsAlarm;

class QuicDispatcher : public QuicServerSessionVisitor {
 public:
  // Ideally we'd have a linked_hash_set: the  boolean is unused.
  typedef linked_hash_map<QuicBlockedWriterInterface*, bool> WriteBlockedList;

  // Due to the way delete_sessions_closure_ is registered, the Dispatcher
  // must live until epoll_server Shutdown. |supported_versions| specifies the
  // list of supported QUIC versions.
  QuicDispatcher(const QuicConfig& config,
                 const QuicCryptoServerConfig& crypto_config,
                 const QuicVersionVector& supported_versions,
                 QuicConnectionHelperInterface* helper);

  virtual ~QuicDispatcher();

  // Takes ownership of the packet writer
  virtual void Initialize(QuicPacketWriter* writer);

  // Process the incoming packet by creating a new session, passing it to
  // an existing session, or passing it to the TimeWaitListManager.
  virtual void ProcessPacket(const IPEndPoint& server_address,
                             const IPEndPoint& client_address,
                             const QuicEncryptedPacket& packet);

  // Called when the socket becomes writable to allow queued writes to happen.
  virtual void OnCanWrite();

  // Returns true if there's anything in the blocked writer list.
  virtual bool HasPendingWrites() const;

  // Sends ConnectionClose frames to all connected clients.
  void Shutdown();

  // QuicServerSessionVisitor interface implementation:
  // Ensure that the closed connection is cleaned up asynchronously.
  virtual void OnConnectionClosed(QuicConnectionId connection_id,
                                  QuicErrorCode error) OVERRIDE;

  // Queues the blocked writer for later resumption.
  virtual void OnWriteBlocked(QuicBlockedWriterInterface* writer) OVERRIDE;

  typedef base::hash_map<QuicConnectionId, QuicSession*> SessionMap;

  // Deletes all sessions on the closed session list and clears the list.
  void DeleteSessions();

  const SessionMap& session_map() const { return session_map_; }

  WriteBlockedList* write_blocked_list() { return &write_blocked_list_; }

 protected:
  virtual QuicSession* CreateQuicSession(QuicConnectionId connection_id,
                                         const IPEndPoint& server_address,
                                         const IPEndPoint& client_address);

  virtual QuicConnection* CreateQuicConnection(
      QuicConnectionId connection_id,
      const IPEndPoint& server_address,
      const IPEndPoint& client_address);

  // Called by |framer_visitor_| when the public header has been parsed.
  virtual bool OnUnauthenticatedPublicHeader(
      const QuicPacketPublicHeader& header);

  // Create and return the time wait list manager for this dispatcher, which
  // will be owned by the dispatcher as time_wait_list_manager_
  virtual QuicTimeWaitListManager* CreateQuicTimeWaitListManager();

  // Replaces the packet writer with |writer|. Takes ownership of |writer|.
  void set_writer(QuicPacketWriter* writer) {
    writer_.reset(writer);
  }

  QuicTimeWaitListManager* time_wait_list_manager() {
    return time_wait_list_manager_.get();
  }

  const QuicVersionVector& supported_versions() const {
    return supported_versions_;
  }

  const QuicVersionVector& supported_versions_no_flow_control() const {
    return supported_versions_no_flow_control_;
  }

  const QuicVersionVector& supported_versions_no_connection_flow_control()
      const {
    return supported_versions_no_connection_flow_control_;
  }

  const IPEndPoint& current_server_address() {
    return current_server_address_;
  }
  const IPEndPoint& current_client_address() {
    return current_client_address_;
  }
  const QuicEncryptedPacket& current_packet() {
    return *current_packet_;
  }

  const QuicConfig& config() const { return config_; }

  const QuicCryptoServerConfig& crypto_config() const { return crypto_config_; }

  QuicFramer* framer() { return &framer_; }

  QuicConnectionHelperInterface* helper() { return helper_; }

  QuicPacketWriter* writer() { return writer_.get(); }

 private:
  class QuicFramerVisitor;
  friend class net::test::QuicDispatcherPeer;

  // Called by |framer_visitor_| when the private header has been parsed
  // of a data packet that is destined for the time wait manager.
  void OnUnauthenticatedHeader(const QuicPacketHeader& header);

  // Removes the session from the session map and write blocked list, and
  // adds the ConnectionId to the time-wait list.
  void CleanUpSession(SessionMap::iterator it);

  bool HandlePacketForTimeWait(const QuicPacketPublicHeader& header);

  const QuicConfig& config_;

  const QuicCryptoServerConfig& crypto_config_;

  // The list of connections waiting to write.
  WriteBlockedList write_blocked_list_;

  SessionMap session_map_;

  // Entity that manages connection_ids in time wait state.
  scoped_ptr<QuicTimeWaitListManager> time_wait_list_manager_;

  // The helper used for all connections. Owned by the server.
  QuicConnectionHelperInterface* helper_;

  // An alarm which deletes closed sessions.
  scoped_ptr<QuicAlarm> delete_sessions_alarm_;

  // The list of closed but not-yet-deleted sessions.
  std::list<QuicSession*> closed_session_list_;

  // The writer to write to the socket with.
  scoped_ptr<QuicPacketWriter> writer_;

  // This vector contains QUIC versions which we currently support.
  // This should be ordered such that the highest supported version is the first
  // element, with subsequent elements in descending order (versions can be
  // skipped as necessary).
  const QuicVersionVector supported_versions_;

  // Versions which do not support flow control (introduced in QUIC_VERSION_17).
  // This is used to construct new QuicConnections when flow control is disabled
  // via flag.
  // TODO(rjshade): Remove this when
  // FLAGS_enable_quic_stream_flow_control_2 is removed.
  QuicVersionVector supported_versions_no_flow_control_;
  // Versions which do not support *connection* flow control (introduced in
  // QUIC_VERSION_19).
  // This is used to construct new QuicConnections when connection flow control
  // is disabled via flag.
  // TODO(rjshade): Remove this when
  // FLAGS_enable_quic_connection_flow_control_2 is removed.
  QuicVersionVector supported_versions_no_connection_flow_control_;

  // Information about the packet currently being handled.
  IPEndPoint current_client_address_;
  IPEndPoint current_server_address_;
  const QuicEncryptedPacket* current_packet_;

  QuicFramer framer_;
  scoped_ptr<QuicFramerVisitor> framer_visitor_;

  DISALLOW_COPY_AND_ASSIGN(QuicDispatcher);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_DISPATCHER_H_
