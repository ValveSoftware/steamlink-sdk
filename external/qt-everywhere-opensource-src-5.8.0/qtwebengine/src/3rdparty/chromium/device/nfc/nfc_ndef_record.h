// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_NFC_NFC_NDEF_RECORD_H_
#define DEVICE_NFC_NFC_NDEF_RECORD_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/values.h"

namespace device {

// NfcNdefRecord represents an NDEF (NFC Data Exchange Format) record. NDEF is
// a light-weight binary format specified by the NFC Forum for transmission and
// storage of typed data over NFC. NDEF defines two constructs: NDEF records and
// messages. An NDEF record contains typed data, such as MIME-type media, a URI,
// or a custom application payload, whereas an NDEF message is a container for
// one or more NDEF records.
class NfcNdefRecord {
 public:
  // NDEF record types that define the payload of the NDEF record.
  enum Type {
    kTypeHandoverCarrier,
    kTypeHandoverRequest,
    kTypeHandoverSelect,
    kTypeSmartPoster,
    kTypeText,
    kTypeURI,
    kTypeUnknown
  };

  // The following are strings that define a possible field of an NDEF record.
  // These strings are used as the keys in the dictionary returned by |data|.
  // Not all fields are always present in an NDEF record, where the presence
  // of a field depends on the type of the record. While some fields are
  // required for a specific record type, others can be optional and won't
  // always be present.

  // Fields for type "Text".

  // The character encoding. When present, the value is one of |kEncodingUtf8|
  // and |kEncodingUtf16|. Otherwise, this field is optional.
  static const char kFieldEncoding[];

  // The ISO/IANA language code (e.g. "en" or "jp"). This field is optional.
  static const char kFieldLanguageCode[];

  // The human readable representation of a text. This field is mandatory.
  static const char kFieldText[];

  // Fields for type "URI".

  // The complete URI, including the scheme and the resource. This field is
  // required.
  static const char kFieldURI[];

  // The URI object MIME type. This is a description of the MIME type of the
  // object the URI points at. This field is optional.
  static const char kFieldMimeType[];

  // The size of the object the URI points at. This field is optional.
  // If present, the value is an unsigned integer. Since base/values.h does not
  // define an unsigned integer type, use a base::DoubleValue to store this.
  static const char kFieldTargetSize[];

  // Fields for type "SmartPoster". A SmartPoster can contain all possible
  // fields of a "URI" record, in addition to the following:

  // The "title" of the SmartPoster. This is an optional field. If present, the
  // value of this field is a list of dictionaries, where each dictionary
  // contains the possible fields of a "Text" record. If the list contains
  // more than one element, each element usually represents the same "title"
  // text in a different language.
  static const char kFieldTitles[];

  // The suggested course of action. The value of this field is one of
  // |kSmartPosterAction*|. This field is optional.
  static const char kFieldAction[];

  // Possible values for character encoding.
  static const char kEncodingUtf8[];
  static const char kEncodingUtf16[];

  // Possible actions defined by the NFC forum SmartPoster record type. Each
  // action is a suggestion to the application indicating the action it should
  // take with the contents of the record.

  // Do the action. e.g. open a URI, send an SMS, dial a phone number.
  static const char kSmartPosterActionDo[];

  // Store data, e.g. store an SMS, bookmark a URI, etc.
  static const char kSmartPosterActionSave[];

  // Open the data for editing.
  static const char kSmartPosterActionOpen[];

  NfcNdefRecord();
  virtual ~NfcNdefRecord();

  // Returns the type that defines the payload of this NDEF record.
  Type type() const { return type_; }

  // Returns the contents of this record in the form of a mapping from keys
  // declared above to their stored values.
  const base::DictionaryValue& data() const { return data_; }

  // Returns true, if this record has been populated via a call to "Populate".
  bool IsPopulated() const;

  // Populates the record with the contents of |data| and sets its type to
  // |type|. Returns true, if the record was successfully populated. If a
  // failure occurs, e.g. |data| contains values that are not allowed in
  // records of type |type| or if |data| does not contain mandatory fields of
  // |type|, this method returns false. Populating an instance of an
  // NfcNdefRecord is allowed only once and after a successful call to this
  // method, all subsequent calls to this method will fail. Use IsPopulated()
  // to determine if this record can be populated.
  bool Populate(Type type, const base::DictionaryValue* data);

 private:
  // The type of this record.
  Type type_;

  // The contents of the record.
  base::DictionaryValue data_;

  DISALLOW_COPY_AND_ASSIGN(NfcNdefRecord);
};

// NfcNdefMessage represent an NDEF message. An NDEF message, contains one or
// more NDEF records and the order in which the records are stored dictates the
// order in which applications are meant to interpret them. For example, a
// client may decide to dispatch to applications based on the first record in
// the sequence.
class NfcNdefMessage {
 public:
  // Typedef for a list of NDEF records.
  typedef std::vector<NfcNdefRecord*> RecordList;

  NfcNdefMessage();
  virtual ~NfcNdefMessage();

  // The NDEF records that are contained in this message.
  const RecordList& records() const { return records_; }

  // Adds the NDEF record |record| to the sequence of records that this
  // NfcNdefMessage contains. This method simply adds the record to this message
  // and does NOT take ownership of it.
  void AddRecord(NfcNdefRecord* record);

  // Removes the NDEF record |record| from this message. Returns true, if the
  // record was removed, otherwise returns false if |record| was not contained
  // in this message.
  bool RemoveRecord(NfcNdefRecord* record);

 private:
  // The NDEF records that are contained by this message.
  RecordList records_;

  DISALLOW_COPY_AND_ASSIGN(NfcNdefMessage);
};

}  // namespace device

#endif  // DEVICE_NFC_NFC_NDEF_RECORD_H_
