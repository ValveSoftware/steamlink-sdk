// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_NFC_NFC_ADAPTER_H_
#define DEVICE_NFC_NFC_ADAPTER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace device {

class NfcPeer;
class NfcTag;

// NfcAdapter represents a local NFC adapter which may be used to interact with
// NFC tags and remote NFC adapters on platforms that support NFC. Through
// instances of this class, users can obtain important information such as if
// and/or when an adapter is present, supported NFC technologies, and the
// adapter's power and polling state. NfcAdapter instances can be used to power
// up/down the NFC adapter and its Observer interface allows users to get
// notified when new adapters are added/removed and when remote NFC tags and
// devices were detected or lost.
//
// A system can contain more than one NFC adapter (e.g. external USB adapters)
// but Chrome will have only one NfcAdapter instance. This instance will do
// its best to represent all underlying adapters but will only allow
// interacting with only one at a given time. If the currently represented
// adapter is removed from the system, the NfcAdapter instance will update to
// reflect the information from the next available adapter.
class NfcAdapter : public base::RefCounted<NfcAdapter> {
 public:
  // Interface for observing changes from NFC adapters.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the presence of the adapter |adapter| changes. When |present|
    // is true, this indicates that the adapter has now become present, while a
    // value of false indicates that the adapter is no longer available on the
    // current system.
    virtual void AdapterPresentChanged(NfcAdapter* adapter, bool present) {}

    // Called when the radio power state of the adapter |adapter| changes. If
    // |powered| is true, the adapter radio is turned on, otherwise it's turned
    // off.
    virtual void AdapterPoweredChanged(NfcAdapter* adapter, bool powered) {}

    // Called when the "polling" state of the adapter |adapter| changes. If
    // |polling| is true, the adapter is currently polling for remote tags and
    // devices. If false, the adapter isn't polling, either because a poll loop
    // was never started or because a connection with a tag or peer has been
    // established.
    virtual void AdapterPollingChanged(NfcAdapter* adapter, bool polling) {}

    // Called when an NFC tag |tag| has been found by the adapter |adapter|.
    // The observer can use this method to take further action on the tag object
    // |tag|, such as reading its records or writing to it. While |tag| will be
    // valid within the context of this call, its life-time cannot be guaranteed
    // once this call returns, as the object may get destroyed if the connection
    // with the tag is lost. Instead of caching the pointer directly, observers
    // should store the tag's assigned unique identifier instead, which can be
    // used to obtain a pointer to the tag, as long as it exists.
    virtual void TagFound(NfcAdapter* adapter, NfcTag* tag) {}

    // Called when the NFC tag |tag| is no longer known by the adapter
    // |adapter|. |tag| should not be cached.
    virtual void TagLost(NfcAdapter* adapter, NfcTag* tag) {}

    // Called when a remote NFC adapter |peer| has been detected, which is
    // available for peer-to-peer communication over NFC. The observer can use
    // this method to take further action on |peer| such as reading its records
    // or pushing NDEFs to it. While |peer| will be valid within the context of
    // this call, its life-time cannot be guaranteed once this call returns, as
    // the object may get destroyed if the connection with the peer is lost.
    // Instead of caching the pointer directly, observers should store the
    // peer's assigned unique identifier instead, which can be used to obtain a
    // pointer to the peer, as long as it exists.
    virtual void PeerFound(NfcAdapter* adaper, NfcPeer* peer) {}

    // Called when the remote NFC adapter |peer| is no longer known by the
    // adapter |adapter|. |peer| should not be cached.
    virtual void PeerLost(NfcAdapter* adapter, NfcPeer* peer) {}
  };

  // The ErrorCallback is used by methods to asynchronously report errors.
  typedef base::Closure ErrorCallback;

  // Typedefs for lists of NFC peer and NFC tag objects.
  typedef std::vector<NfcPeer*> PeerList;
  typedef std::vector<NfcTag*> TagList;

  // Adds and removes observers for events on this NFC adapter. If monitoring
  // multiple adapters, check the |adapter| parameter of observer methods to
  // determine which adapter is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Indicates whether an underlying adapter is actually present on the
  // system. An adapter that was previously present can become no longer
  // present, for example, if all physical adapters that can back it up were
  // removed from the system.
  virtual bool IsPresent() const = 0;

  // Indicates whether the adapter radio is powered.
  virtual bool IsPowered() const = 0;

  // Indicates whether the adapter is polling for remote NFC tags and peers.
  virtual bool IsPolling() const = 0;

  // Indicates whether the NfcAdapter instance is initialized and ready to use.
  virtual bool IsInitialized() const = 0;

  // Requests a change to the adapter radio power. Setting |powered| to true
  // will turn on the radio and false will turn it off. On success, |callback|
  // will be invoked. On failure, |error_callback| will be invoked, which can
  // happen if the radio power is already in the requested state, or if the
  // underlying adapter is not present.
  virtual void SetPowered(bool powered,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Requests the adapter to begin its poll loop to start looking for remote
  // NFC tags and peers. On success, |callback| will be invoked. On failure,
  // |error_callback| will be invoked. This method can fail for various
  // reasons, including:
  //    - The adapter radio is not powered.
  //    - The adapter is already polling.
  //    - The adapter is busy; it has already established a connection to a
  //      remote tag or peer.
  // Bear in mind that this method may be called by multiple users of the same
  // adapter.
  virtual void StartPolling(const base::Closure& callback,
                            const ErrorCallback& error_callback) = 0;

  // Requests the adapter to stop its current poll loop. On success, |callback|
  // will be invoked. On failure, |error_callback| will be invoked. This method
  // can fail if the adapter is not polling when this method was called. Bear
  // in mind that this method may be called by multiple users of the same
  // adapter and polling may not actually stop if other callers have called
  // StartPolling() in the mean time.
  virtual void StopPolling(const base::Closure& callback,
                           const ErrorCallback& error_callback) = 0;

  // Returns a list containing all known peers in |peer_list|. If |peer_list|
  // was non-empty at the time of the call, it will be cleared. The contents of
  // |peer_list| at the end of this call are owned by the adapter.
  virtual void GetPeers(PeerList* peer_list) const;

  // Returns a list containing all known tags in |tag_list|. If |tag_list| was
  // non-empty at the time of the call, it will be cleared. The contents of
  // |tag_list| at the end of this call are owned by the adapter.
  virtual void GetTags(TagList* tag_list) const;

  // Returns a pointer to the peer with the given identifier |identifier| or
  // NULL if no such peer is known. If a non-NULL pointer is returned, the
  // instance that it points to is owned by this adapter.
  virtual NfcPeer* GetPeer(const std::string& identifier) const;

  // Returns a pointer to the tag with the given identifier |identifier| or
  // NULL if no such tag is known. If a non-NULL pointer is returned, the
  // instance that it points to is owned by this adapter.
  virtual NfcTag* GetTag(const std::string& identifier) const;

 protected:
  friend class base::RefCounted<NfcAdapter>;

  // The default constructor does nothing. The destructor deletes all known
  // NfcPeer and NfcTag instances.
  NfcAdapter();
  virtual ~NfcAdapter();

  // Peers and tags that have been found. The key is the unique identifier
  // assigned to the peer or tag and the value is a pointer to the
  // corresponding NfcPeer or NfcTag object, whose lifetime is managed by the
  // adapter instance.
  typedef std::map<const std::string, NfcPeer*> PeersMap;
  typedef std::map<const std::string, NfcTag*> TagsMap;

  // Set the given tag or peer for |identifier|. If a tag or peer for
  // |identifier| already exists, these methods won't do anything.
  void SetTag(const std::string& identifier, NfcTag* tag);
  void SetPeer(const std::string& identifier, NfcPeer* peer);

  // Removes the tag or peer for |identifier| and returns the removed object.
  // Returns NULL, if no tag or peer for |identifier| was found.
  NfcTag* RemoveTag(const std::string& identifier);
  NfcPeer* RemovePeer(const std::string& identifier);

  // Clear the peer and tag maps. These methods won't delete the tag and peer
  // objects, however after the call to these methods, the peers and tags won't
  // be returned via calls to GetPeers and GetTags.
  void ClearTags();
  void ClearPeers();

 private:
  // Peers and tags that are managed by this adapter.
  PeersMap peers_;
  TagsMap tags_;

  DISALLOW_COPY_AND_ASSIGN(NfcAdapter);
};

}  // namespace device

#endif  // DEVICE_NFC_NFC_ADAPTER_H_
