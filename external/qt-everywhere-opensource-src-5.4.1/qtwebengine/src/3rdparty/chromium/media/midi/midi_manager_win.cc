// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_win.h"

#include <windows.h>

// Prevent unnecessary functions from being included from <mmsystem.h>
#define MMNODRV
#define MMNOSOUND
#define MMNOWAVE
#define MMNOAUX
#define MMNOMIXER
#define MMNOTIMER
#define MMNOJOY
#define MMNOMCI
#define MMNOMMIO
#include <mmsystem.h>

#include <algorithm>
#include <string>
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "media/midi/midi_message_queue.h"
#include "media/midi/midi_message_util.h"
#include "media/midi/midi_port_info.h"

namespace media {
namespace {

std::string GetInErrorMessage(MMRESULT result) {
  wchar_t text[MAXERRORLENGTH];
  MMRESULT get_result = midiInGetErrorText(result, text, arraysize(text));
  if (get_result != MMSYSERR_NOERROR) {
    DLOG(ERROR) << "Failed to get error message."
                << " original error: " << result
                << " midiInGetErrorText error: " << get_result;
    return std::string();
  }
  return base::WideToUTF8(text);
}

std::string GetOutErrorMessage(MMRESULT result) {
  wchar_t text[MAXERRORLENGTH];
  MMRESULT get_result = midiOutGetErrorText(result, text, arraysize(text));
  if (get_result != MMSYSERR_NOERROR) {
    DLOG(ERROR) << "Failed to get error message."
                << " original error: " << result
                << " midiOutGetErrorText error: " << get_result;
    return std::string();
  }
  return base::WideToUTF8(text);
}

class MIDIHDRDeleter {
 public:
  void operator()(MIDIHDR* header) {
    if (!header)
      return;
    delete[] static_cast<char*>(header->lpData);
    header->lpData = NULL;
    header->dwBufferLength = 0;
    delete header;
  }
};

typedef scoped_ptr<MIDIHDR, MIDIHDRDeleter> ScopedMIDIHDR;

ScopedMIDIHDR CreateMIDIHDR(size_t size) {
  ScopedMIDIHDR header(new MIDIHDR);
  ZeroMemory(header.get(), sizeof(*header));
  header->lpData = new char[size];
  header->dwBufferLength = size;
  return header.Pass();
}

void SendShortMidiMessageInternal(HMIDIOUT midi_out_handle,
                                  const std::vector<uint8>& message) {
  if (message.size() >= 4)
    return;

  DWORD packed_message = 0;
  for (size_t i = 0; i < message.size(); ++i)
    packed_message |= (static_cast<uint32>(message[i]) << (i * 8));
  MMRESULT result = midiOutShortMsg(midi_out_handle, packed_message);
  DLOG_IF(ERROR, result != MMSYSERR_NOERROR)
      << "Failed to output short message: " << GetOutErrorMessage(result);
}

void SendLongMidiMessageInternal(HMIDIOUT midi_out_handle,
                                 const std::vector<uint8>& message) {
  // Implementation note:
  // Sending long MIDI message can be performed synchronously or asynchronously
  // depending on the driver. There are 2 options to support both cases:
  // 1) Call midiOutLongMsg() API and wait for its completion within this
  //   function. In this approach, we can avoid memory copy by directly pointing
  //   |message| as the data buffer to be sent.
  // 2) Allocate a buffer and copy |message| to it, then call midiOutLongMsg()
  //   API. The buffer will be freed in the MOM_DONE event hander, which tells
  //   us that the task of midiOutLongMsg() API is completed.
  // Here we choose option 2) in favor of asynchronous design.

  // Note for built-in USB-MIDI driver:
  // From an observation on Windows 7/8.1 with a USB-MIDI keyboard,
  // midiOutLongMsg() will be always blocked. Sending 64 bytes or less data
  // takes roughly 300 usecs. Sending 2048 bytes or more data takes roughly
  // |message.size() / (75 * 1024)| secs in practice. Here we put 60 KB size
  // limit on SysEx message, with hoping that midiOutLongMsg will be blocked at
  // most 1 sec or so with a typical USB-MIDI device.
  const size_t kSysExSizeLimit = 60 * 1024;
  if (message.size() >= kSysExSizeLimit) {
    DVLOG(1) << "Ingnoreing SysEx message due to the size limit"
             << ", size = " << message.size();
    return;
  }

  ScopedMIDIHDR midi_header(CreateMIDIHDR(message.size()));
  for (size_t i = 0; i < message.size(); ++i)
    midi_header->lpData[i] = static_cast<char>(message[i]);

  MMRESULT result = midiOutPrepareHeader(
      midi_out_handle, midi_header.get(), sizeof(*midi_header));
  if (result != MMSYSERR_NOERROR) {
    DLOG(ERROR) << "Failed to prepare output buffer: "
                << GetOutErrorMessage(result);
    return;
  }

  result = midiOutLongMsg(
      midi_out_handle, midi_header.get(), sizeof(*midi_header));
  if (result != MMSYSERR_NOERROR) {
    DLOG(ERROR) << "Failed to output long message: "
                << GetOutErrorMessage(result);
    result = midiOutUnprepareHeader(
        midi_out_handle, midi_header.get(), sizeof(*midi_header));
    DLOG_IF(ERROR, result != MMSYSERR_NOERROR)
        << "Failed to uninitialize output buffer: "
        << GetOutErrorMessage(result);
    return;
  }

  // The ownership of |midi_header| is moved to MOM_DONE event handler.
  midi_header.release();
}

}  // namespace

class MidiManagerWin::InDeviceInfo {
 public:
  ~InDeviceInfo() {
    Uninitialize();
  }
  void set_port_index(int index) {
    port_index_ = index;
  }
  int port_index() const {
    return port_index_;
  }
  bool device_to_be_closed() const {
    return device_to_be_closed_;
  }
  HMIDIIN midi_handle() const {
    return midi_handle_;
  }

  static scoped_ptr<InDeviceInfo> Create(MidiManagerWin* manager,
                                         UINT device_id) {
    scoped_ptr<InDeviceInfo> obj(new InDeviceInfo(manager));
    if (!obj->Initialize(device_id))
      obj.reset();
    return obj.Pass();
  }

 private:
  static const int kInvalidPortIndex = -1;
  static const size_t kBufferLength = 32 * 1024;

  explicit InDeviceInfo(MidiManagerWin* manager)
      : manager_(manager),
        port_index_(kInvalidPortIndex),
        midi_handle_(NULL),
        started_(false),
        device_to_be_closed_(false) {
  }

  bool Initialize(DWORD device_id) {
    Uninitialize();
    midi_header_ = CreateMIDIHDR(kBufferLength);

    // Here we use |CALLBACK_FUNCTION| to subscribe MIM_DATA, MIM_LONGDATA, and
    // MIM_CLOSE events.
    // - MIM_DATA: This is the only way to get a short MIDI message with
    //     timestamp information.
    // - MIM_LONGDATA: This is the only way to get a long MIDI message with
    //     timestamp information.
    // - MIM_CLOSE: This event is sent when 1) midiInClose() is called, or 2)
    //     the MIDI device becomes unavailable for some reasons, e.g., the cable
    //     is disconnected. As for the former case, HMIDIOUT will be invalidated
    //     soon after the callback is finished. As for the later case, however,
    //     HMIDIOUT continues to be valid until midiInClose() is called.
    MMRESULT result = midiInOpen(&midi_handle_,
                                 device_id,
                                 reinterpret_cast<DWORD_PTR>(&HandleMessage),
                                 reinterpret_cast<DWORD_PTR>(this),
                                 CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
      DLOG(ERROR) << "Failed to open output device. "
                  << " id: " << device_id
                  << " message: " << GetInErrorMessage(result);
      return false;
    }
    result = midiInPrepareHeader(
        midi_handle_, midi_header_.get(), sizeof(*midi_header_));
    if (result != MMSYSERR_NOERROR) {
      DLOG(ERROR) << "Failed to initialize input buffer: "
                  << GetInErrorMessage(result);
      return false;
    }
    result = midiInAddBuffer(
        midi_handle_, midi_header_.get(), sizeof(*midi_header_));
    if (result != MMSYSERR_NOERROR) {
      DLOG(ERROR) << "Failed to attach input buffer: "
                  << GetInErrorMessage(result);
      return false;
    }
    result = midiInStart(midi_handle_);
    if (result != MMSYSERR_NOERROR) {
      DLOG(ERROR) << "Failed to start input port: "
                  << GetInErrorMessage(result);
      return false;
    }
    started_ = true;
    start_time_ = base::TimeTicks::Now();
    return true;
  }

  void Uninitialize() {
    MMRESULT result = MMSYSERR_NOERROR;
    if (midi_handle_ && started_) {
      result = midiInStop(midi_handle_);
      DLOG_IF(ERROR, result != MMSYSERR_NOERROR)
          << "Failed to stop input port: " << GetInErrorMessage(result);
      started_ = false;
      start_time_ = base::TimeTicks();
    }
    if (midi_handle_) {
      // midiInReset flushes pending messages. We ignore these messages.
      device_to_be_closed_ = true;
      result = midiInReset(midi_handle_);
      DLOG_IF(ERROR, result != MMSYSERR_NOERROR)
          << "Failed to reset input port: " << GetInErrorMessage(result);
      result = midiInClose(midi_handle_);
      device_to_be_closed_ = false;
      DLOG_IF(ERROR, result != MMSYSERR_NOERROR)
          << "Failed to close input port: " << GetInErrorMessage(result);
      midi_header_.reset();
      midi_handle_ = NULL;
      port_index_ = kInvalidPortIndex;
    }
  }

  static void CALLBACK HandleMessage(HMIDIIN midi_in_handle,
                                     UINT message,
                                     DWORD_PTR instance,
                                     DWORD_PTR param1,
                                     DWORD_PTR param2) {
    // This method can be called back on any thread depending on Windows
    // multimedia subsystem and underlying MIDI drivers.
    InDeviceInfo* self = reinterpret_cast<InDeviceInfo*>(instance);
    if (!self)
      return;
    if (self->midi_handle() != midi_in_handle)
      return;

    switch (message) {
      case MIM_DATA:
        self->OnShortMessageReceived(static_cast<uint8>(param1 & 0xff),
                                     static_cast<uint8>((param1 >> 8) & 0xff),
                                     static_cast<uint8>((param1 >> 16) & 0xff),
                                     param2);
        return;
      case MIM_LONGDATA:
        self->OnLongMessageReceived(reinterpret_cast<MIDIHDR*>(param1),
                                    param2);
        return;
      case MIM_CLOSE:
        // TODO(yukawa): Implement crbug.com/279097.
        return;
    }
  }

  void OnShortMessageReceived(uint8 status_byte,
                              uint8 first_data_byte,
                              uint8 second_data_byte,
                              DWORD elapsed_ms) {
    if (device_to_be_closed())
      return;
    const size_t len = GetMidiMessageLength(status_byte);
    if (len == 0 || port_index() == kInvalidPortIndex)
      return;
    const uint8 kData[] = { status_byte, first_data_byte, second_data_byte };
    DCHECK_LE(len, arraysize(kData));
    OnMessageReceived(kData, len, elapsed_ms);
  }

  void OnLongMessageReceived(MIDIHDR* header, DWORD elapsed_ms) {
    if (header != midi_header_.get())
      return;
    MMRESULT result = MMSYSERR_NOERROR;
    if (device_to_be_closed()) {
      if (midi_header_ &&
          (midi_header_->dwFlags & MHDR_PREPARED) == MHDR_PREPARED) {
        result = midiInUnprepareHeader(
            midi_handle_, midi_header_.get(), sizeof(*midi_header_));
        DLOG_IF(ERROR, result != MMSYSERR_NOERROR)
            << "Failed to uninitialize input buffer: "
            << GetInErrorMessage(result);
      }
      return;
    }
    if (header->dwBytesRecorded > 0 && port_index() != kInvalidPortIndex) {
      OnMessageReceived(reinterpret_cast<const uint8*>(header->lpData),
                        header->dwBytesRecorded,
                        elapsed_ms);
    }
    result = midiInAddBuffer(midi_handle_, header, sizeof(*header));
    DLOG_IF(ERROR, result != MMSYSERR_NOERROR)
        << "Failed to attach input port: " << GetInErrorMessage(result);
  }

  void OnMessageReceived(const uint8* data, size_t length, DWORD elapsed_ms) {
    // MIM_DATA/MIM_LONGDATA message treats the time when midiInStart() is
    // called as the origin of |elapsed_ms|.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd757284.aspx
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd757286.aspx
    const base::TimeTicks event_time =
        start_time_ + base::TimeDelta::FromMilliseconds(elapsed_ms);
    manager_->ReceiveMidiData(port_index_, data, length, event_time);
  }

  MidiManagerWin* manager_;
  int port_index_;
  HMIDIIN midi_handle_;
  ScopedMIDIHDR midi_header_;
  base::TimeTicks start_time_;
  bool started_;
  bool device_to_be_closed_;
  DISALLOW_COPY_AND_ASSIGN(InDeviceInfo);
};

class MidiManagerWin::OutDeviceInfo {
 public:
  ~OutDeviceInfo() {
    Uninitialize();
  }

  static scoped_ptr<OutDeviceInfo> Create(UINT device_id) {
    scoped_ptr<OutDeviceInfo> obj(new OutDeviceInfo);
    if (!obj->Initialize(device_id))
      obj.reset();
    return obj.Pass();
  }

  HMIDIOUT midi_handle() const {
    return midi_handle_;
  }

  void Quit() {
    quitting_ = true;
  }

  void Send(const std::vector<uint8>& data) {
    // Check if the attached device is still available or not.
    if (!midi_handle_)
      return;

    // Give up sending MIDI messages here if the device is already closed.
    // Note that this check is optional. Regardless of that we check |closed_|
    // or not, nothing harmful happens as long as |midi_handle_| is still valid.
    if (closed_)
      return;

    // MIDI Running status must be filtered out.
    MidiMessageQueue message_queue(false);
    message_queue.Add(data);
    std::vector<uint8> message;
    while (!quitting_) {
      message_queue.Get(&message);
      if (message.empty())
        break;
      // SendShortMidiMessageInternal can send a MIDI message up to 3 bytes.
      if (message.size() <= 3)
        SendShortMidiMessageInternal(midi_handle_, message);
      else
        SendLongMidiMessageInternal(midi_handle_, message);
    }
  }

 private:
  OutDeviceInfo()
      : midi_handle_(NULL),
        closed_(false),
        quitting_(false) {}

  bool Initialize(DWORD device_id) {
    Uninitialize();
    // Here we use |CALLBACK_FUNCTION| to subscribe MOM_DONE and MOM_CLOSE
    // events.
    // - MOM_DONE: SendLongMidiMessageInternal() relies on this event to clean
    //     up the backing store where a long MIDI message is stored.
    // - MOM_CLOSE: This event is sent when 1) midiOutClose() is called, or 2)
    //     the MIDI device becomes unavailable for some reasons, e.g., the cable
    //     is disconnected. As for the former case, HMIDIOUT will be invalidated
    //     soon after the callback is finished. As for the later case, however,
    //     HMIDIOUT continues to be valid until midiOutClose() is called.
    MMRESULT result = midiOutOpen(&midi_handle_,
                                  device_id,
                                  reinterpret_cast<DWORD_PTR>(&HandleMessage),
                                  reinterpret_cast<DWORD_PTR>(this),
                                  CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
      DLOG(ERROR) << "Failed to open output device. "
                  << " id: " << device_id
                  << " message: "<< GetOutErrorMessage(result);
      midi_handle_ = NULL;
      return false;
    }
    return true;
  }

  void Uninitialize() {
    if (!midi_handle_)
      return;

    MMRESULT result = midiOutReset(midi_handle_);
    DLOG_IF(ERROR, result != MMSYSERR_NOERROR)
        << "Failed to reset output port: " << GetOutErrorMessage(result);
    result = midiOutClose(midi_handle_);
    DLOG_IF(ERROR, result != MMSYSERR_NOERROR)
        << "Failed to close output port: " << GetOutErrorMessage(result);
    midi_handle_ = NULL;
    closed_ = true;
  }

  static void CALLBACK HandleMessage(HMIDIOUT midi_out_handle,
                                     UINT message,
                                     DWORD_PTR instance,
                                     DWORD_PTR param1,
                                     DWORD_PTR param2) {
    // This method can be called back on any thread depending on Windows
    // multimedia subsystem and underlying MIDI drivers.

    OutDeviceInfo* self = reinterpret_cast<OutDeviceInfo*>(instance);
    if (!self)
      return;
    if (self->midi_handle() != midi_out_handle)
      return;
    switch (message) {
      case MOM_DONE: {
        // Take ownership of the MIDIHDR object.
        ScopedMIDIHDR header(reinterpret_cast<MIDIHDR*>(param1));
        if (!header)
          return;
        MMRESULT result = midiOutUnprepareHeader(
            self->midi_handle(), header.get(), sizeof(*header));
        DLOG_IF(ERROR, result != MMSYSERR_NOERROR)
            << "Failed to uninitialize output buffer: "
            << GetOutErrorMessage(result);
        return;
      }
      case MOM_CLOSE:
        // No lock is required since this flag is just a hint to avoid
        // unnecessary API calls that will result in failure anyway.
        self->closed_ = true;
        // TODO(yukawa): Implement crbug.com/279097.
        return;
    }
  }

  HMIDIOUT midi_handle_;

  // True if the device is already closed.
  volatile bool closed_;

  // True if the MidiManagerWin is trying to stop the sender thread.
  volatile bool quitting_;

  DISALLOW_COPY_AND_ASSIGN(OutDeviceInfo);
};

MidiManagerWin::MidiManagerWin()
    : send_thread_("MidiSendThread") {
}

void MidiManagerWin::StartInitialization() {
  const UINT num_in_devices = midiInGetNumDevs();
  in_devices_.reserve(num_in_devices);
  for (UINT device_id = 0; device_id < num_in_devices; ++device_id) {
    MIDIINCAPS caps = {};
    MMRESULT result = midiInGetDevCaps(device_id, &caps, sizeof(caps));
    if (result != MMSYSERR_NOERROR) {
      DLOG(ERROR) << "Failed to obtain input device info: "
                  << GetInErrorMessage(result);
      continue;
    }
    scoped_ptr<InDeviceInfo> in_device(InDeviceInfo::Create(this, device_id));
    if (!in_device)
      continue;
    MidiPortInfo info(
        base::IntToString(static_cast<int>(device_id)),
        "",
        base::WideToUTF8(caps.szPname),
        base::IntToString(static_cast<int>(caps.vDriverVersion)));
    AddInputPort(info);
    in_device->set_port_index(input_ports().size() - 1);
    in_devices_.push_back(in_device.Pass());
  }

  const UINT num_out_devices = midiOutGetNumDevs();
  out_devices_.reserve(num_out_devices);
  for (UINT device_id = 0; device_id < num_out_devices; ++device_id) {
    MIDIOUTCAPS caps = {};
    MMRESULT result = midiOutGetDevCaps(device_id, &caps, sizeof(caps));
    if (result != MMSYSERR_NOERROR) {
      DLOG(ERROR) << "Failed to obtain output device info: "
                  << GetOutErrorMessage(result);
      continue;
    }
    scoped_ptr<OutDeviceInfo> out_port(OutDeviceInfo::Create(device_id));
    if (!out_port)
      continue;
    MidiPortInfo info(
        base::IntToString(static_cast<int>(device_id)),
        "",
        base::WideToUTF8(caps.szPname),
        base::IntToString(static_cast<int>(caps.vDriverVersion)));
    AddOutputPort(info);
    out_devices_.push_back(out_port.Pass());
  }

  CompleteInitialization(MIDI_OK);
}

MidiManagerWin::~MidiManagerWin() {
  // Cleanup order is important. |send_thread_| must be stopped before
  // |out_devices_| is cleared.
  for (size_t i = 0; i < output_ports().size(); ++i)
    out_devices_[i]->Quit();
  send_thread_.Stop();

  out_devices_.clear();
  in_devices_.clear();
}

void MidiManagerWin::DispatchSendMidiData(MidiManagerClient* client,
                                          uint32 port_index,
                                          const std::vector<uint8>& data,
                                          double timestamp) {
  if (out_devices_.size() <= port_index)
    return;

  base::TimeDelta delay;
  if (timestamp != 0.0) {
    base::TimeTicks time_to_send =
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(
            timestamp * base::Time::kMicrosecondsPerSecond);
    delay = std::max(time_to_send - base::TimeTicks::Now(), base::TimeDelta());
  }

  if (!send_thread_.IsRunning())
    send_thread_.Start();

  OutDeviceInfo* out_port = out_devices_[port_index].get();
  send_thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&OutDeviceInfo::Send, base::Unretained(out_port), data),
      delay);

  // Call back AccumulateMidiBytesSent() on |send_thread_| to emulate the
  // behavior of MidiManagerMac::SendMidiData.
  // TODO(yukawa): Do this task in a platform-independent way if possible.
  // See crbug.com/325810.
  send_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MidiManagerClient::AccumulateMidiBytesSent,
                 base::Unretained(client), data.size()));
}

MidiManager* MidiManager::Create() {
  return new MidiManagerWin();
}

}  // namespace media
