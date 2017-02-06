// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_NFC_NFC_PEER_H_
#define DEVICE_NFC_NFC_PEER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "device/nfc/nfc_ndef_record.h"

namespace device {

// NfcPeer represents a remote NFC adapter that is available for P2P
// communication with the local adapter. Instances of NfcPeer allow two
// kinds of P2P interaction that is supported by NFC:
//
//    - NDEF. Specifically, reading NDEF records found on the peer device and
//      pushing NDEF records to it (e.g. via SNEP or Android Beam), in the form
//      of an NDEF message as specified by the NFC forum.
//    - Initiating a handover. On platforms that support it, handover can be
//      used to quickly bootstrap a Bluetooth or WiFi based connection between
//      the two devices over NFC.
class NfcPeer {
 public:
  // NFC handover types.
  enum HandoverType {
    kHandoverTypeBluetooth,
    kHandoverTypeWiFi
  };

  // Interface for observing changes from NFC peer devices.
  class Observer {
   public:
    virtual ~Observer() {}

    // This method will be called when an NDEF record |record| from the peer
    // device |peer| is received. Users can use this method to be notified of
    // new records on the device and when the initial set of records are
    // received from it, if any. All records received from |peer| can be
    // accessed by calling |peer->GetNdefMessage()|.
    virtual void RecordReceived(NfcPeer* peer, const NfcNdefRecord* record) {}
  };

  // The ErrorCallback is used by methods to asynchronously report errors.
  typedef base::Closure ErrorCallback;

  virtual ~NfcPeer();

  // Adds and removes observers for events on this NFC peer. If monitoring
  // multiple peers, check the |peer| parameter of observer methods to
  // determine which peer is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the unique identifier assigned to this peer.
  virtual std::string GetIdentifier() const = 0;

  // Returns all NDEF records that were received from the peer device in the
  // form of a NDEF message. If the returned NDEF message contains no records,
  // this only means that no records have yet been received from the device.
  // Users should use this method in conjunction with the Observer methods
  // to be notified when the records are ready.
  virtual const NfcNdefMessage& GetNdefMessage() const = 0;

  // Sends the NDEF records contained in |message| to the peer device. On
  // success, |callback| will be invoked. On failure, |error_callback| will be
  // invoked.
  virtual void PushNdef(const NfcNdefMessage& message,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) = 0;

  // Initiates WiFi or Bluetooth pairing with the NFC peer device based on
  // |handover_type|. On success, |callback| will be invoked. On failure,
  // |error_callback| will be invoked.
  virtual void StartHandover(HandoverType handover_type,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) = 0;

 protected:
  NfcPeer();

 private:
  DISALLOW_COPY_AND_ASSIGN(NfcPeer);
};

}  // namespace device

#endif  // DEVICE_NFC_NFC_PEER_H_
