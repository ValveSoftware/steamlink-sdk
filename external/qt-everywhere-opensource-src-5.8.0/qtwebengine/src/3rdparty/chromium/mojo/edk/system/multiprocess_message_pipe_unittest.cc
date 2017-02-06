// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/handle_signals_state.h"
#include "mojo/edk/system/test_utils.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/edk/test/test_utils.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/types.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace mojo {
namespace edk {
namespace {

class MultiprocessMessagePipeTest : public test::MojoTestBase {
 protected:
  // Convenience class for tests which will control command-driven children.
  // See the CommandDrivenClient definition below.
  class CommandDrivenClientController {
   public:
    explicit CommandDrivenClientController(MojoHandle h) : h_(h) {}

    void Send(const std::string& command) {
      WriteMessage(h_, command);
      EXPECT_EQ("ok", ReadMessage(h_));
    }

    void SendHandle(const std::string& name, MojoHandle p) {
      WriteMessageWithHandles(h_, "take:" + name, &p, 1);
      EXPECT_EQ("ok", ReadMessage(h_));
    }

    MojoHandle RetrieveHandle(const std::string& name) {
      WriteMessage(h_, "return:" + name);
      MojoHandle p;
      EXPECT_EQ("ok", ReadMessageWithHandles(h_, &p, 1));
      return p;
    }

    void Exit() { WriteMessage(h_, "exit"); }

   private:
    MojoHandle h_;
  };
};

// For each message received, sends a reply message with the same contents
// repeated twice, until the other end is closed or it receives "quitquitquit"
// (which it doesn't reply to). It'll return the number of messages received,
// not including any "quitquitquit" message, modulo 100.
DEFINE_TEST_CLIENT_WITH_PIPE(EchoEcho, MultiprocessMessagePipeTest, h) {
  const std::string quitquitquit("quitquitquit");
  int rv = 0;
  for (;; rv = (rv + 1) % 100) {
    // Wait for our end of the message pipe to be readable.
    HandleSignalsState hss;
    MojoResult result =
        MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
                 MOJO_DEADLINE_INDEFINITE, &hss);
    if (result != MOJO_RESULT_OK) {
      // It was closed, probably.
      CHECK_EQ(result, MOJO_RESULT_FAILED_PRECONDITION);
      CHECK_EQ(hss.satisfied_signals, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
      CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
      break;
    } else {
      CHECK((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
      CHECK((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));
    }

    std::string read_buffer(1000, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(MojoReadMessage(h, &read_buffer[0],
                             &read_buffer_size, nullptr,
                             0, MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    VLOG(2) << "Child got: " << read_buffer;

    if (read_buffer == quitquitquit) {
      VLOG(2) << "Child quitting.";
      break;
    }

    std::string write_buffer = read_buffer + read_buffer;
    CHECK_EQ(MojoWriteMessage(h, write_buffer.data(),
                              static_cast<uint32_t>(write_buffer.size()),
                              nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
  }

  return rv;
}

TEST_F(MultiprocessMessagePipeTest, Basic) {
  RUN_CHILD_ON_PIPE(EchoEcho, h)
    std::string hello("hello");
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(h, hello.data(),
                               static_cast<uint32_t>(hello.size()), nullptr, 0u,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_DEADLINE_INDEFINITE, &hss));
    // The child may or may not have closed its end of the message pipe and died
    // (and we may or may not know it yet), so our end may or may not appear as
    // writable.
    EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
    EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

    std::string read_buffer(1000, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(MojoReadMessage(h, &read_buffer[0],
                             &read_buffer_size, nullptr, 0,
                             MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    VLOG(2) << "Parent got: " << read_buffer;
    ASSERT_EQ(hello + hello, read_buffer);

    std::string quitquitquit("quitquitquit");
    CHECK_EQ(MojoWriteMessage(h, quitquitquit.data(),
                              static_cast<uint32_t>(quitquitquit.size()),
                              nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
  END_CHILD_AND_EXPECT_EXIT_CODE(1 % 100);
}

TEST_F(MultiprocessMessagePipeTest, QueueMessages) {
  static const size_t kNumMessages = 1001;
  RUN_CHILD_ON_PIPE(EchoEcho, h)
    for (size_t i = 0; i < kNumMessages; i++) {
      std::string write_buffer(i, 'A' + (i % 26));
      ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(h, write_buffer.data(),
                               static_cast<uint32_t>(write_buffer.size()),
                               nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE));
    }

    for (size_t i = 0; i < kNumMessages; i++) {
      HandleSignalsState hss;
      ASSERT_EQ(MOJO_RESULT_OK,
                MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
                         MOJO_DEADLINE_INDEFINITE, &hss));
      // The child may or may not have closed its end of the message pipe and
      // died (and we may or may not know it yet), so our end may or may not
      // appear as writable.
      ASSERT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
      ASSERT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

      std::string read_buffer(kNumMessages * 2, '\0');
      uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
      ASSERT_EQ(MojoReadMessage(h, &read_buffer[0],
                                &read_buffer_size, nullptr, 0,
                                MOJO_READ_MESSAGE_FLAG_NONE),
               MOJO_RESULT_OK);
      read_buffer.resize(read_buffer_size);

      ASSERT_EQ(std::string(i * 2, 'A' + (i % 26)), read_buffer);
    }

    const std::string quitquitquit("quitquitquit");
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(h, quitquitquit.data(),
                               static_cast<uint32_t>(quitquitquit.size()),
                               nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for it to become readable, which should fail (since we sent
    // "quitquitquit").
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_DEADLINE_INDEFINITE, &hss));
    ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
    ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);
  END_CHILD_AND_EXPECT_EXIT_CODE(static_cast<int>(kNumMessages % 100));
}

DEFINE_TEST_CLIENT_WITH_PIPE(CheckSharedBuffer, MultiprocessMessagePipeTest,
                             h) {
  // Wait for the first message from our parent.
  HandleSignalsState hss;
  CHECK_EQ(MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  // In this test, the parent definitely doesn't close its end of the message
  // pipe before we do.
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                    MOJO_HANDLE_SIGNAL_WRITABLE |
                                    MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  // It should have a shared buffer.
  std::string read_buffer(100, '\0');
  uint32_t num_bytes = static_cast<uint32_t>(read_buffer.size());
  MojoHandle handles[10];
  uint32_t num_handlers = arraysize(handles);  // Maximum number to receive
  CHECK_EQ(MojoReadMessage(h, &read_buffer[0],
                           &num_bytes, &handles[0],
                           &num_handlers, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(num_bytes);
  CHECK_EQ(read_buffer, std::string("go 1"));
  CHECK_EQ(num_handlers, 1u);

  // Make a mapping.
  void* buffer;
  CHECK_EQ(MojoMapBuffer(handles[0], 0, 100, &buffer,
                         MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE),
           MOJO_RESULT_OK);

  // Write some stuff to the shared buffer.
  static const char kHello[] = "hello";
  memcpy(buffer, kHello, sizeof(kHello));

  // We should be able to close the dispatcher now.
  MojoClose(handles[0]);

  // And send a message to signal that we've written stuff.
  const std::string go2("go 2");
  CHECK_EQ(MojoWriteMessage(h, go2.data(),
                            static_cast<uint32_t>(go2.size()), nullptr, 0u,
                            MOJO_WRITE_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);

  // Now wait for our parent to send us a message.
  hss = HandleSignalsState();
  CHECK_EQ(MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
                    MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                        MOJO_HANDLE_SIGNAL_WRITABLE |
                                        MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  read_buffer = std::string(100, '\0');
  num_bytes = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(h, &read_buffer[0], &num_bytes,
                           nullptr, 0, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(num_bytes);
  CHECK_EQ(read_buffer, std::string("go 3"));

  // It should have written something to the shared buffer.
  static const char kWorld[] = "world!!!";
  CHECK_EQ(memcmp(buffer, kWorld, sizeof(kWorld)), 0);

  // And we're done.

  return 0;
}

TEST_F(MultiprocessMessagePipeTest, SharedBufferPassing) {
  RUN_CHILD_ON_PIPE(CheckSharedBuffer, h)
    // Make a shared buffer.
    MojoCreateSharedBufferOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE;

    MojoHandle shared_buffer;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoCreateSharedBuffer(&options, 100, &shared_buffer));

    // Send the shared buffer.
    const std::string go1("go 1");

    MojoHandle duplicated_shared_buffer;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoDuplicateBufferHandle(
                  shared_buffer,
                  nullptr,
                  &duplicated_shared_buffer));
    MojoHandle handles[1];
    handles[0] = duplicated_shared_buffer;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(h, &go1[0],
                               static_cast<uint32_t>(go1.size()), &handles[0],
                               arraysize(handles),
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for a message from the child.
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_DEADLINE_INDEFINITE, &hss));
    EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
    EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

    std::string read_buffer(100, '\0');
    uint32_t num_bytes = static_cast<uint32_t>(read_buffer.size());
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoReadMessage(h, &read_buffer[0],
                              &num_bytes, nullptr, 0,
                              MOJO_READ_MESSAGE_FLAG_NONE));
    read_buffer.resize(num_bytes);
    ASSERT_EQ(std::string("go 2"), read_buffer);

    // After we get it, the child should have written something to the shared
    // buffer.
    static const char kHello[] = "hello";
    void* buffer;
    CHECK_EQ(MojoMapBuffer(shared_buffer, 0, 100, &buffer,
                           MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE),
             MOJO_RESULT_OK);
    ASSERT_EQ(0, memcmp(buffer, kHello, sizeof(kHello)));

    // Now we'll write some stuff to the shared buffer.
    static const char kWorld[] = "world!!!";
    memcpy(buffer, kWorld, sizeof(kWorld));

    // And send a message to signal that we've written stuff.
    const std::string go3("go 3");
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(h, &go3[0],
                               static_cast<uint32_t>(go3.size()), nullptr, 0u,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for |h| to become readable, which should fail.
    hss = HandleSignalsState();
    ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_DEADLINE_INDEFINITE, &hss));
    ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
    ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);
  END_CHILD()
}

DEFINE_TEST_CLIENT_WITH_PIPE(CheckPlatformHandleFile,
                             MultiprocessMessagePipeTest, h) {
  HandleSignalsState hss;
  CHECK_EQ(MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
                    MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                        MOJO_HANDLE_SIGNAL_WRITABLE |
                                        MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  std::string read_buffer(100, '\0');
  uint32_t num_bytes = static_cast<uint32_t>(read_buffer.size());
  MojoHandle handles[255];  // Maximum number to receive.
  uint32_t num_handlers = arraysize(handles);

  CHECK_EQ(MojoReadMessage(h, &read_buffer[0],
                           &num_bytes, &handles[0],
                           &num_handlers, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);

  read_buffer.resize(num_bytes);
  char hello[32];
  int num_handles = 0;
  sscanf(read_buffer.c_str(), "%s %d", hello, &num_handles);
  CHECK_EQ(std::string("hello"), std::string(hello));
  CHECK_GT(num_handles, 0);

  for (int i = 0; i < num_handles; ++i) {
    ScopedPlatformHandle h;
    CHECK_EQ(PassWrappedPlatformHandle(handles[i], &h), MOJO_RESULT_OK);
    CHECK(h.is_valid());
    MojoClose(handles[i]);

    base::ScopedFILE fp(test::FILEFromPlatformHandle(std::move(h), "r"));
    CHECK(fp);
    std::string fread_buffer(100, '\0');
    size_t bytes_read =
        fread(&fread_buffer[0], 1, fread_buffer.size(), fp.get());
    fread_buffer.resize(bytes_read);
    CHECK_EQ(fread_buffer, "world");
  }

  return 0;
}

class MultiprocessMessagePipeTestWithPipeCount
    : public MultiprocessMessagePipeTest,
      public testing::WithParamInterface<size_t> {};

TEST_P(MultiprocessMessagePipeTestWithPipeCount, PlatformHandlePassing) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  RUN_CHILD_ON_PIPE(CheckPlatformHandleFile, h)
    std::vector<MojoHandle> handles;

    size_t pipe_count = GetParam();
    for (size_t i = 0; i < pipe_count; ++i) {
      base::FilePath unused;
      base::ScopedFILE fp(
          CreateAndOpenTemporaryFileInDir(temp_dir.path(), &unused));
      const std::string world("world");
      CHECK_EQ(fwrite(&world[0], 1, world.size(), fp.get()), world.size());
      fflush(fp.get());
      rewind(fp.get());
      MojoHandle handle;
      ASSERT_EQ(
          CreatePlatformHandleWrapper(
              ScopedPlatformHandle(test::PlatformHandleFromFILE(std::move(fp))),
              &handle),
          MOJO_RESULT_OK);
      handles.push_back(handle);
    }

    char message[128];
    sprintf(message, "hello %d", static_cast<int>(pipe_count));
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(h, message,
                               static_cast<uint32_t>(strlen(message)),
                               &handles[0],
                               static_cast<uint32_t>(handles.size()),
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for it to become readable, which should fail.
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_DEADLINE_INDEFINITE, &hss));
    ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
    ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);
  END_CHILD()
}

// Android multi-process tests are not executing the new process. This is flaky.
#if !defined(OS_ANDROID)
INSTANTIATE_TEST_CASE_P(PipeCount,
                        MultiprocessMessagePipeTestWithPipeCount,
                        // TODO: Re-enable the 140-pipe case when ChannelPosix
                        // has support for sending lots of handles.
                        testing::Values(1u, 128u/*, 140u*/));
#endif

DEFINE_TEST_CLIENT_WITH_PIPE(CheckMessagePipe, MultiprocessMessagePipeTest, h) {
  // Wait for the first message from our parent.
  HandleSignalsState hss;
  CHECK_EQ(MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
                    MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  // In this test, the parent definitely doesn't close its end of the message
  // pipe before we do.
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                    MOJO_HANDLE_SIGNAL_WRITABLE |
                                    MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  // It should have a message pipe.
  MojoHandle handles[10];
  uint32_t num_handlers = arraysize(handles);
  CHECK_EQ(MojoReadMessage(h, nullptr,
                           nullptr, &handles[0],
                           &num_handlers, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  CHECK_EQ(num_handlers, 1u);

  // Read data from the received message pipe.
  CHECK_EQ(MojoWait(handles[0], MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                    MOJO_HANDLE_SIGNAL_WRITABLE |
                                    MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  std::string read_buffer(100, '\0');
  uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(handles[0], &read_buffer[0],
                           &read_buffer_size, nullptr,
                           0, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(read_buffer_size);
  CHECK_EQ(read_buffer, std::string("hello"));

  // Now write some data into the message pipe.
  std::string write_buffer = "world";
  CHECK_EQ(MojoWriteMessage(handles[0], write_buffer.data(),
                            static_cast<uint32_t>(write_buffer.size()), nullptr,
                            0u, MOJO_WRITE_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  MojoClose(handles[0]);
  return 0;
}

TEST_F(MultiprocessMessagePipeTest, MessagePipePassing) {
  RUN_CHILD_ON_PIPE(CheckMessagePipe, h)
    MojoCreateSharedBufferOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE;

    MojoHandle mp1, mp2;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoCreateMessagePipe(nullptr, &mp1, &mp2));

    // Write a string into one end of the new message pipe and send the other
    // end.
    const std::string hello("hello");
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(mp1, &hello[0],
                               static_cast<uint32_t>(hello.size()), nullptr, 0,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(h, nullptr, 0, &mp2, 1,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for a message from the child.
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWait(mp1, MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_DEADLINE_INDEFINITE, &hss));
    EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
    EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

    std::string read_buffer(100, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(MojoReadMessage(mp1, &read_buffer[0],
                             &read_buffer_size, nullptr,
                             0, MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    CHECK_EQ(read_buffer, std::string("world"));

    MojoClose(mp1);
  END_CHILD()
}

TEST_F(MultiprocessMessagePipeTest, MessagePipeTwoPassing) {
  RUN_CHILD_ON_PIPE(CheckMessagePipe, h)
    MojoHandle mp1, mp2;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoCreateMessagePipe(nullptr, &mp2, &mp1));

    // Write a string into one end of the new message pipe and send the other
    // end.
    const std::string hello("hello");
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(mp1, &hello[0],
                               static_cast<uint32_t>(hello.size()), nullptr, 0u,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(h, nullptr, 0u, &mp2, 1u,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for a message from the child.
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWait(mp1, MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_DEADLINE_INDEFINITE, &hss));
    EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
    EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

    std::string read_buffer(100, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(MojoReadMessage(mp1, &read_buffer[0],
                             &read_buffer_size, nullptr,
                             0, MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    CHECK_EQ(read_buffer, std::string("world"));
  END_CHILD();
}

DEFINE_TEST_CLIENT_WITH_PIPE(DataPipeConsumer, MultiprocessMessagePipeTest, h) {
  // Wait for the first message from our parent.
  HandleSignalsState hss;
  CHECK_EQ(MojoWait(h, MOJO_HANDLE_SIGNAL_READABLE,
                    MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  // In this test, the parent definitely doesn't close its end of the message
  // pipe before we do.
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                    MOJO_HANDLE_SIGNAL_WRITABLE |
                                    MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  // It should have a message pipe.
  MojoHandle handles[10];
  uint32_t num_handlers = arraysize(handles);
  CHECK_EQ(MojoReadMessage(h, nullptr,
                           nullptr, &handles[0],
                           &num_handlers, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  CHECK_EQ(num_handlers, 1u);

  // Read data from the received message pipe.
  CHECK_EQ(MojoWait(handles[0], MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                    MOJO_HANDLE_SIGNAL_WRITABLE |
                                    MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  std::string read_buffer(100, '\0');
  uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(handles[0], &read_buffer[0],
                           &read_buffer_size, nullptr,
                           0, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(read_buffer_size);
  CHECK_EQ(read_buffer, std::string("hello"));

  // Now write some data into the message pipe.
  std::string write_buffer = "world";
  CHECK_EQ(MojoWriteMessage(handles[0], write_buffer.data(),
                            static_cast<uint32_t>(write_buffer.size()),
                            nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE),
            MOJO_RESULT_OK);
  MojoClose(handles[0]);
  return 0;
}

TEST_F(MultiprocessMessagePipeTest, DataPipeConsumer) {
  RUN_CHILD_ON_PIPE(DataPipeConsumer, h)
    MojoCreateSharedBufferOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE;

    MojoHandle mp1, mp2;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoCreateMessagePipe(nullptr, &mp2, &mp1));

    // Write a string into one end of the new message pipe and send the other
    // end.
    const std::string hello("hello");
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(mp1, &hello[0],
                               static_cast<uint32_t>(hello.size()), nullptr, 0u,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(h, nullptr, 0, &mp2, 1u,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for a message from the child.
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWait(mp1, MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_DEADLINE_INDEFINITE, &hss));
    EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
    EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

    std::string read_buffer(100, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(MojoReadMessage(mp1, &read_buffer[0],
                             &read_buffer_size, nullptr,
                             0, MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    CHECK_EQ(read_buffer, std::string("world"));

    MojoClose(mp1);
  END_CHILD();
}

TEST_F(MultiprocessMessagePipeTest, CreateMessagePipe) {
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);
  VerifyTransmission(p0, p1, "hey man");
  VerifyTransmission(p1, p0, "slow down");
  VerifyTransmission(p0, p1, std::string(10 * 1024 * 1024, 'a'));
  VerifyTransmission(p1, p0, std::string(10 * 1024 * 1024, 'e'));

  CloseHandle(p0);
  CloseHandle(p1);
}

TEST_F(MultiprocessMessagePipeTest, PassMessagePipeLocal) {
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);
  VerifyTransmission(p0, p1, "testing testing");
  VerifyTransmission(p1, p0, "one two three");

  MojoHandle p2, p3;

  CreateMessagePipe(&p2, &p3);
  VerifyTransmission(p2, p3, "testing testing");
  VerifyTransmission(p3, p2, "one two three");

  // Pass p2 over p0 to p1.
  const std::string message = "ceci n'est pas une pipe";
  WriteMessageWithHandles(p0, message, &p2, 1);
  EXPECT_EQ(message, ReadMessageWithHandles(p1, &p2, 1));

  CloseHandle(p0);
  CloseHandle(p1);

  // Verify that the received handle (now in p2) still works.
  VerifyTransmission(p2, p3, "Easy come, easy go; will you let me go?");
  VerifyTransmission(p3, p2, "Bismillah! NO! We will not let you go!");

  CloseHandle(p2);
  CloseHandle(p3);
}

// Echos the primordial channel until "exit".
DEFINE_TEST_CLIENT_WITH_PIPE(ChannelEchoClient, MultiprocessMessagePipeTest,
                             h) {
  for (;;) {
    std::string message = ReadMessage(h);
    if (message == "exit")
      break;
    WriteMessage(h, message);
  }
  return 0;
}

TEST_F(MultiprocessMessagePipeTest, MultiprocessChannelPipe) {
  RUN_CHILD_ON_PIPE(ChannelEchoClient, h)
    VerifyEcho(h, "in an interstellar burst");
    VerifyEcho(h, "i am back to save the universe");
    VerifyEcho(h, std::string(10 * 1024 * 1024, 'o'));

    WriteMessage(h, "exit");
  END_CHILD()
}

// Receives a pipe handle from the primordial channel and echos on it until
// "exit". Used to test simple pipe transfer across processes via channels.
DEFINE_TEST_CLIENT_WITH_PIPE(EchoServiceClient, MultiprocessMessagePipeTest,
                             h) {
  MojoHandle p;
  ReadMessageWithHandles(h, &p, 1);
  for (;;) {
    std::string message = ReadMessage(p);
    if (message == "exit")
      break;
    WriteMessage(p, message);
  }
  return 0;
}

TEST_F(MultiprocessMessagePipeTest, PassMessagePipeCrossProcess) {
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);
  RUN_CHILD_ON_PIPE(EchoServiceClient, h)

    // Pass one end of the pipe to the other process.
    WriteMessageWithHandles(h, "here take this", &p1, 1);

    VerifyEcho(p0, "and you may ask yourself");
    VerifyEcho(p0, "where does that highway go?");
    VerifyEcho(p0, std::string(20 * 1024 * 1024, 'i'));

    WriteMessage(p0, "exit");
  END_CHILD()
  CloseHandle(p0);
}

// Receives a pipe handle from the primordial channel and reads new handles
// from it. Each read handle establishes a new echo channel.
DEFINE_TEST_CLIENT_WITH_PIPE(EchoServiceFactoryClient,
                             MultiprocessMessagePipeTest, h) {
  MojoHandle p;
  ReadMessageWithHandles(h, &p, 1);

  std::vector<MojoHandle> handles(2);
  handles[0] = h;
  handles[1] = p;
  std::vector<MojoHandleSignals> signals(2, MOJO_HANDLE_SIGNAL_READABLE);
  for (;;) {
    uint32_t index;
    CHECK_EQ(MojoWaitMany(handles.data(), signals.data(),
                          static_cast<uint32_t>(handles.size()),
                          MOJO_DEADLINE_INDEFINITE, &index, nullptr),
             MOJO_RESULT_OK);
    DCHECK_LE(index, handles.size());
    if (index == 0) {
      // If data is available on the first pipe, it should be an exit command.
      EXPECT_EQ(std::string("exit"), ReadMessage(h));
      break;
    } else if (index == 1) {
      // If the second pipe, it should be a new handle requesting echo service.
      MojoHandle echo_request;
      ReadMessageWithHandles(p, &echo_request, 1);
      handles.push_back(echo_request);
      signals.push_back(MOJO_HANDLE_SIGNAL_READABLE);
    } else {
      // Otherwise it was one of our established echo pipes. Echo!
      WriteMessage(handles[index], ReadMessage(handles[index]));
    }
  }

  for (size_t i = 1; i < handles.size(); ++i)
    CloseHandle(handles[i]);

  return 0;
}

TEST_F(MultiprocessMessagePipeTest, PassMoarMessagePipesCrossProcess) {
  MojoHandle echo_factory_proxy, echo_factory_request;
  CreateMessagePipe(&echo_factory_proxy, &echo_factory_request);

  MojoHandle echo_proxy_a, echo_request_a;
  CreateMessagePipe(&echo_proxy_a, &echo_request_a);

  MojoHandle echo_proxy_b, echo_request_b;
  CreateMessagePipe(&echo_proxy_b, &echo_request_b);

  MojoHandle echo_proxy_c, echo_request_c;
  CreateMessagePipe(&echo_proxy_c, &echo_request_c);

  RUN_CHILD_ON_PIPE(EchoServiceFactoryClient, h)
    WriteMessageWithHandles(
        h, "gief factory naow plz", &echo_factory_request, 1);

    WriteMessageWithHandles(echo_factory_proxy, "give me an echo service plz!",
                           &echo_request_a, 1);
    WriteMessageWithHandles(echo_factory_proxy, "give me one too!",
                           &echo_request_b, 1);

    VerifyEcho(echo_proxy_a, "i came here for an argument");
    VerifyEcho(echo_proxy_a, "shut your festering gob");
    VerifyEcho(echo_proxy_a, "mumble mumble mumble");

    VerifyEcho(echo_proxy_b, "wubalubadubdub");
    VerifyEcho(echo_proxy_b, "wubalubadubdub");

    WriteMessageWithHandles(echo_factory_proxy, "hook me up also thanks",
                           &echo_request_c, 1);

    VerifyEcho(echo_proxy_a, "the frobinators taste like frobinators");
    VerifyEcho(echo_proxy_b, "beep bop boop");
    VerifyEcho(echo_proxy_c, "zzzzzzzzzzzzzzzzzzzzzzzzzz");

    WriteMessage(h, "exit");
  END_CHILD()

  CloseHandle(echo_factory_proxy);
  CloseHandle(echo_proxy_a);
  CloseHandle(echo_proxy_b);
  CloseHandle(echo_proxy_c);
}

TEST_F(MultiprocessMessagePipeTest, ChannelPipesWithMultipleChildren) {
  RUN_CHILD_ON_PIPE(ChannelEchoClient, a)
    RUN_CHILD_ON_PIPE(ChannelEchoClient, b)
      VerifyEcho(a, "hello child 0");
      VerifyEcho(b, "hello child 1");

      WriteMessage(a, "exit");
      WriteMessage(b, "exit");
    END_CHILD()
  END_CHILD()
}

// Reads and turns a pipe handle some number of times to create lots of
// transient proxies.
DEFINE_TEST_CLIENT_TEST_WITH_PIPE(PingPongPipeClient,
                                  MultiprocessMessagePipeTest, h) {
  const size_t kNumBounces = 50;
  MojoHandle p0, p1;
  ReadMessageWithHandles(h, &p0, 1);
  ReadMessageWithHandles(h, &p1, 1);
  for (size_t i = 0; i < kNumBounces; ++i) {
    WriteMessageWithHandles(h, "", &p1, 1);
    ReadMessageWithHandles(h, &p1, 1);
  }
  WriteMessageWithHandles(h, "", &p0, 1);
  WriteMessage(p1, "bye");
  MojoClose(p1);
  EXPECT_EQ("quit", ReadMessage(h));
}

TEST_F(MultiprocessMessagePipeTest, PingPongPipe) {
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);

  RUN_CHILD_ON_PIPE(PingPongPipeClient, h)
    const size_t kNumBounces = 50;
    WriteMessageWithHandles(h, "", &p0, 1);
    WriteMessageWithHandles(h, "", &p1, 1);
    for (size_t i = 0; i < kNumBounces; ++i) {
      ReadMessageWithHandles(h, &p1, 1);
      WriteMessageWithHandles(h, "", &p1, 1);
    }
    ReadMessageWithHandles(h, &p0, 1);
    WriteMessage(h, "quit");
  END_CHILD()

  EXPECT_EQ("bye", ReadMessage(p0));

  // We should still be able to observe peer closure from the other end.
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWait(p0, MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                     MOJO_DEADLINE_INDEFINITE, nullptr));
}

// Parses commands from the parent pipe and does whatever it's asked to do.
DEFINE_TEST_CLIENT_WITH_PIPE(CommandDrivenClient, MultiprocessMessagePipeTest,
                             h) {
  base::hash_map<std::string, MojoHandle> named_pipes;
  for (;;) {
    MojoHandle p;
    auto parts = base::SplitString(ReadMessageWithOptionalHandle(h, &p), ":",
                                   base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    CHECK(!parts.empty());
    std::string command = parts[0];
    if (command == "take") {
      // Take a pipe.
      CHECK_EQ(parts.size(), 2u);
      CHECK_NE(p, MOJO_HANDLE_INVALID);
      named_pipes[parts[1]] = p;
      WriteMessage(h, "ok");
    } else if (command == "return") {
      // Return a pipe.
      CHECK_EQ(parts.size(), 2u);
      CHECK_EQ(p, MOJO_HANDLE_INVALID);
      p = named_pipes[parts[1]];
      CHECK_NE(p, MOJO_HANDLE_INVALID);
      named_pipes.erase(parts[1]);
      WriteMessageWithHandles(h, "ok", &p, 1);
    } else if (command == "say") {
      // Say something to a named pipe.
      CHECK_EQ(parts.size(), 3u);
      CHECK_EQ(p, MOJO_HANDLE_INVALID);
      p = named_pipes[parts[1]];
      CHECK_NE(p, MOJO_HANDLE_INVALID);
      CHECK(!parts[2].empty());
      WriteMessage(p, parts[2]);
      WriteMessage(h, "ok");
    } else if (command == "hear") {
      // Expect to read something from a named pipe.
      CHECK_EQ(parts.size(), 3u);
      CHECK_EQ(p, MOJO_HANDLE_INVALID);
      p = named_pipes[parts[1]];
      CHECK_NE(p, MOJO_HANDLE_INVALID);
      CHECK(!parts[2].empty());
      CHECK_EQ(parts[2], ReadMessage(p));
      WriteMessage(h, "ok");
    } else if (command == "pass") {
      // Pass one named pipe over another named pipe.
      CHECK_EQ(parts.size(), 3u);
      CHECK_EQ(p, MOJO_HANDLE_INVALID);
      p = named_pipes[parts[1]];
      MojoHandle carrier = named_pipes[parts[2]];
      CHECK_NE(p, MOJO_HANDLE_INVALID);
      CHECK_NE(carrier, MOJO_HANDLE_INVALID);
      named_pipes.erase(parts[1]);
      WriteMessageWithHandles(carrier, "got a pipe for ya", &p, 1);
      WriteMessage(h, "ok");
    } else if (command == "catch") {
      // Expect to receive one named pipe from another named pipe.
      CHECK_EQ(parts.size(), 3u);
      CHECK_EQ(p, MOJO_HANDLE_INVALID);
      MojoHandle carrier = named_pipes[parts[2]];
      CHECK_NE(carrier, MOJO_HANDLE_INVALID);
      ReadMessageWithHandles(carrier, &p, 1);
      CHECK_NE(p, MOJO_HANDLE_INVALID);
      named_pipes[parts[1]] = p;
      WriteMessage(h, "ok");
    } else if (command == "exit") {
      CHECK_EQ(parts.size(), 1u);
      break;
    }
  }

  for (auto& pipe: named_pipes)
    CloseHandle(pipe.second);

  return 0;
}

TEST_F(MultiprocessMessagePipeTest, ChildToChildPipes) {
  RUN_CHILD_ON_PIPE(CommandDrivenClient, h0)
    RUN_CHILD_ON_PIPE(CommandDrivenClient, h1)
      CommandDrivenClientController a(h0);
      CommandDrivenClientController b(h1);

      // Create a pipe and pass each end to a different client.
      MojoHandle p0, p1;
      CreateMessagePipe(&p0, &p1);
      a.SendHandle("x", p0);
      b.SendHandle("y", p1);

      // Make sure they can talk.
      a.Send("say:x:hello sir");
      b.Send("hear:y:hello sir");

      b.Send("say:y:i love multiprocess pipes!");
      a.Send("hear:x:i love multiprocess pipes!");

      a.Exit();
      b.Exit();
    END_CHILD()
  END_CHILD()
}

TEST_F(MultiprocessMessagePipeTest, MoreChildToChildPipes) {
  RUN_CHILD_ON_PIPE(CommandDrivenClient, h0)
    RUN_CHILD_ON_PIPE(CommandDrivenClient, h1)
      RUN_CHILD_ON_PIPE(CommandDrivenClient, h2)
        RUN_CHILD_ON_PIPE(CommandDrivenClient, h3)
          CommandDrivenClientController a(h0), b(h1), c(h2), d(h3);

          // Connect a to b and c to d

          MojoHandle p0, p1;

          CreateMessagePipe(&p0, &p1);
          a.SendHandle("b_pipe", p0);
          b.SendHandle("a_pipe", p1);

          MojoHandle p2, p3;

          CreateMessagePipe(&p2, &p3);
          c.SendHandle("d_pipe", p2);
          d.SendHandle("c_pipe", p3);

          // Connect b to c via a and d
          MojoHandle p4, p5;
          CreateMessagePipe(&p4, &p5);
          a.SendHandle("d_pipe", p4);
          d.SendHandle("a_pipe", p5);

          // Have |a| pass its new |d|-pipe to |b|. It will eventually connect
          // to |c|.
          a.Send("pass:d_pipe:b_pipe");
          b.Send("catch:c_pipe:a_pipe");

          // Have |d| pass its new |a|-pipe to |c|. It will now be connected to
          // |b|.
          d.Send("pass:a_pipe:c_pipe");
          c.Send("catch:b_pipe:d_pipe");

          // Make sure b and c and talk.
          b.Send("say:c_pipe:it's a beautiful day");
          c.Send("hear:b_pipe:it's a beautiful day");

          // Create x and y and have b and c exchange them.
          MojoHandle x, y;
          CreateMessagePipe(&x, &y);
          b.SendHandle("x", x);
          c.SendHandle("y", y);
          b.Send("pass:x:c_pipe");
          c.Send("pass:y:b_pipe");
          b.Send("catch:y:c_pipe");
          c.Send("catch:x:b_pipe");

          // Make sure the pipe still works in both directions.
          b.Send("say:y:hello");
          c.Send("hear:x:hello");
          c.Send("say:x:goodbye");
          b.Send("hear:y:goodbye");

          // Take both pipes back.
          y = c.RetrieveHandle("x");
          x = b.RetrieveHandle("y");

          VerifyTransmission(x, y, "still works");
          VerifyTransmission(y, x, "in both directions");

          CloseHandle(x);
          CloseHandle(y);

          a.Exit();
          b.Exit();
          c.Exit();
          d.Exit();
        END_CHILD()
      END_CHILD()
    END_CHILD()
  END_CHILD()
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceivePipeWithClosedPeer,
                                  MultiprocessMessagePipeTest, h) {
  MojoHandle p;
  EXPECT_EQ("foo", ReadMessageWithHandles(h, &p, 1));

  EXPECT_EQ(MOJO_RESULT_OK, MojoWait(p, MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                                     MOJO_DEADLINE_INDEFINITE, nullptr));
}

TEST_F(MultiprocessMessagePipeTest, SendPipeThenClosePeer) {
  RUN_CHILD_ON_PIPE(ReceivePipeWithClosedPeer, h)
    MojoHandle a, b;
    CreateMessagePipe(&a, &b);

    // Send |a| and immediately close |b|. The child should observe closure.
    WriteMessageWithHandles(h, "foo", &a, 1);
    MojoClose(b);
  END_CHILD()
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(SendOtherChildPipeWithClosedPeer,
                                  MultiprocessMessagePipeTest, h) {
  // Create a new pipe and send one end to the parent, who will connect it to
  // a client running ReceivePipeWithClosedPeerFromOtherChild.
  MojoHandle application_proxy, application_request;
  CreateMessagePipe(&application_proxy, &application_request);
  WriteMessageWithHandles(h, "c2a plz", &application_request, 1);

  // Create another pipe and send one end to the remote "application".
  MojoHandle service_proxy, service_request;
  CreateMessagePipe(&service_proxy, &service_request);
  WriteMessageWithHandles(application_proxy, "c2s lol", &service_request, 1);

  // Immediately close the service proxy. The "application" should detect this.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(service_proxy));

  // Wait for quit.
  EXPECT_EQ("quit", ReadMessage(h));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceivePipeWithClosedPeerFromOtherChild,
                                  MultiprocessMessagePipeTest, h) {
  // Receive a pipe from the parent. This is akin to an "application request".
  MojoHandle application_client;
  EXPECT_EQ("c2a", ReadMessageWithHandles(h, &application_client, 1));

  // Receive a pipe from the "application" "client".
  MojoHandle service_client;
  EXPECT_EQ("c2s lol",
            ReadMessageWithHandles(application_client, &service_client, 1));

  // Wait for the service client to signal closure.
  EXPECT_EQ(MOJO_RESULT_OK, MojoWait(service_client,
                                     MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                                     MOJO_DEADLINE_INDEFINITE, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(service_client));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(application_client));
}

#if defined(OS_ANDROID)
// Android multi-process tests are not executing the new process. This is flaky.
#define MAYBE_SendPipeWithClosedPeerBetweenChildren \
    DISABLED_SendPipeWithClosedPeerBetweenChildren
#else
#define MAYBE_SendPipeWithClosedPeerBetweenChildren \
    SendPipeWithClosedPeerBetweenChildren
#endif
TEST_F(MultiprocessMessagePipeTest,
       MAYBE_SendPipeWithClosedPeerBetweenChildren) {
  RUN_CHILD_ON_PIPE(SendOtherChildPipeWithClosedPeer, kid_a)
    RUN_CHILD_ON_PIPE(ReceivePipeWithClosedPeerFromOtherChild, kid_b)
      // Receive an "application request" from the first child and forward it
      // to the second child.
      MojoHandle application_request;
      EXPECT_EQ("c2a plz",
                ReadMessageWithHandles(kid_a, &application_request, 1));

      WriteMessageWithHandles(kid_b, "c2a", &application_request, 1);
    END_CHILD()

    WriteMessage(kid_a, "quit");
  END_CHILD()
}


TEST_F(MultiprocessMessagePipeTest, SendClosePeerSend) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  // Send |a| over |c|, immediately close |b|, then send |a| back over |d|.
  WriteMessageWithHandles(c, "foo", &a, 1);
  EXPECT_EQ("foo", ReadMessageWithHandles(d, &a, 1));
  WriteMessageWithHandles(d, "bar", &a, 1);
  EXPECT_EQ("bar", ReadMessageWithHandles(c, &a, 1));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));

  // We should be able to detect peer closure on |a|.
  EXPECT_EQ(MOJO_RESULT_OK, MojoWait(a, MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                                     MOJO_DEADLINE_INDEFINITE, nullptr));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(WriteCloseSendPeerClient,
                                  MultiprocessMessagePipeTest, h) {
  MojoHandle pipe[2];
  EXPECT_EQ("foo", ReadMessageWithHandles(h, pipe, 2));

  // Write some messages to the first endpoint and then close it.
  WriteMessage(pipe[0], "baz");
  WriteMessage(pipe[0], "qux");
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe[0]));

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  // Pass the orphaned endpoint over another pipe before passing it back to
  // the parent, just for some extra proxying goodness.
  WriteMessageWithHandles(c, "foo", &pipe[1], 1);
  EXPECT_EQ("foo", ReadMessageWithHandles(d, &pipe[1], 1));

  // And finally pass it back to the parent.
  WriteMessageWithHandles(h, "bar", &pipe[1], 1);

  EXPECT_EQ("quit", ReadMessage(h));
}

TEST_F(MultiprocessMessagePipeTest, WriteCloseSendPeer) {
  MojoHandle pipe[2];
  CreateMessagePipe(&pipe[0], &pipe[1]);

  RUN_CHILD_ON_PIPE(WriteCloseSendPeerClient, h)
    // Pass the pipe to the child.
    WriteMessageWithHandles(h, "foo", pipe, 2);

    // Read back an endpoint which should have messages on it.
    MojoHandle p;
    EXPECT_EQ("bar", ReadMessageWithHandles(h, &p, 1));

    EXPECT_EQ("baz", ReadMessage(p));
    EXPECT_EQ("qux", ReadMessage(p));

    // Expect to have peer closure signaled.
    EXPECT_EQ(MOJO_RESULT_OK, MojoWait(p, MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                                       MOJO_DEADLINE_INDEFINITE, nullptr));

    WriteMessage(h, "quit");
  END_CHILD()
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(BootstrapMessagePipeAsyncClient,
                                  MultiprocessMessagePipeTest, parent) {
  // Receive one end of a platform channel from the parent.
  MojoHandle channel_handle;
  EXPECT_EQ("hi", ReadMessageWithHandles(parent, &channel_handle, 1));
  ScopedPlatformHandle channel;
  EXPECT_EQ(MOJO_RESULT_OK,
            edk::PassWrappedPlatformHandle(channel_handle, &channel));
  ASSERT_TRUE(channel.is_valid());

  // Create a new pipe using our end of the channel.
  ScopedMessagePipeHandle pipe = edk::CreateMessagePipe(std::move(channel));

  // Ensure that we can read and write on the new pipe.
  VerifyEcho(pipe.get().value(), "goodbye");
}

TEST_F(MultiprocessMessagePipeTest, BootstrapMessagePipeAsync) {
  // Tests that new cross-process message pipes can be created synchronously
  // using asynchronous negotiation over an arbitrary platform channel.
  RUN_CHILD_ON_PIPE(BootstrapMessagePipeAsyncClient, child)
    // Pass one end of a platform channel to the child.
    PlatformChannelPair platform_channel;
    MojoHandle client_channel_handle;
    EXPECT_EQ(MOJO_RESULT_OK,
              CreatePlatformHandleWrapper(platform_channel.PassClientHandle(),
                                          &client_channel_handle));
    WriteMessageWithHandles(child, "hi", &client_channel_handle, 1);

    // Create a new pipe using our end of the channel.
    ScopedMessagePipeHandle pipe =
        edk::CreateMessagePipe(platform_channel.PassServerHandle());

    // Ensure that we can read and write on the new pipe.
    VerifyEcho(pipe.get().value(), "goodbye");
  END_CHILD()
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(BadMessageClient, MultiprocessMessagePipeTest,
                                  parent) {
  MojoHandle pipe;
  EXPECT_EQ("hi", ReadMessageWithHandles(parent, &pipe, 1));
  WriteMessage(pipe, "derp");
  EXPECT_EQ("bye", ReadMessage(parent));
}

void OnProcessError(std::string* out_error, const std::string& error) {
  *out_error = error;
}

TEST_F(MultiprocessMessagePipeTest, NotifyBadMessage) {
  const std::string kFirstErrorMessage = "everything is terrible!";
  const std::string kSecondErrorMessage = "not the bits you're looking for";

  std::string first_process_error;
  std::string second_process_error;

  set_process_error_callback(base::Bind(&OnProcessError, &first_process_error));
  RUN_CHILD_ON_PIPE(BadMessageClient, child1)
    set_process_error_callback(base::Bind(&OnProcessError,
                                          &second_process_error));
    RUN_CHILD_ON_PIPE(BadMessageClient, child2)
      MojoHandle a, b, c, d;
      CreateMessagePipe(&a, &b);
      CreateMessagePipe(&c, &d);
      WriteMessageWithHandles(child1, "hi", &b, 1);
      WriteMessageWithHandles(child2, "hi", &d, 1);

      // Read a message from the pipe we sent to child1 and flag it as bad.
      ASSERT_EQ(MOJO_RESULT_OK, MojoWait(a, MOJO_HANDLE_SIGNAL_READABLE,
                                         MOJO_DEADLINE_INDEFINITE, nullptr));
      uint32_t num_bytes = 0;
      MojoMessageHandle message;
      ASSERT_EQ(MOJO_RESULT_OK,
                MojoReadMessageNew(a, &message, &num_bytes, nullptr, 0,
                                   MOJO_READ_MESSAGE_FLAG_NONE));
      EXPECT_EQ(MOJO_RESULT_OK,
                MojoNotifyBadMessage(message, kFirstErrorMessage.data(),
                                     kFirstErrorMessage.size()));
      EXPECT_EQ(MOJO_RESULT_OK, MojoFreeMessage(message));

      // Read a message from the pipe we sent to child2 and flag it as bad.
      ASSERT_EQ(MOJO_RESULT_OK, MojoWait(c, MOJO_HANDLE_SIGNAL_READABLE,
                                         MOJO_DEADLINE_INDEFINITE, nullptr));
      ASSERT_EQ(MOJO_RESULT_OK,
                MojoReadMessageNew(c, &message, &num_bytes, nullptr, 0,
                                   MOJO_READ_MESSAGE_FLAG_NONE));
      EXPECT_EQ(MOJO_RESULT_OK,
                MojoNotifyBadMessage(message, kSecondErrorMessage.data(),
                                     kSecondErrorMessage.size()));
      EXPECT_EQ(MOJO_RESULT_OK, MojoFreeMessage(message));

      WriteMessage(child2, "bye");
    END_CHILD();

    WriteMessage(child1, "bye");
  END_CHILD()

  // The error messages should match the processes which triggered them.
  EXPECT_NE(std::string::npos, first_process_error.find(kFirstErrorMessage));
  EXPECT_NE(std::string::npos, second_process_error.find(kSecondErrorMessage));
}

}  // namespace
}  // namespace edk
}  // namespace mojo
