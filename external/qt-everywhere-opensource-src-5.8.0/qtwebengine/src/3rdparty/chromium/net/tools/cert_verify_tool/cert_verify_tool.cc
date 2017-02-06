// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "net/tools/cert_verify_tool/cert_verify_tool_util.h"
#include "net/tools/cert_verify_tool/verify_using_cert_verify_proc.h"

namespace {

void PrintUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0 << " [flags] <target/chain>\n";
  std::cerr << " <target/chain> should be a file containing a single DER cert "
               "or a PEM certificate chain (target first).\n";
  std::cerr << "Flags:\n";
  std::cerr << " --hostname=<hostname>\n";
  std::cerr << " --roots=<certs path>\n";
  std::cerr << " --intermediates=<certs path>\n";
  std::cerr << " <certs path> should be a file containing a single DER cert or "
               "one or more PEM CERTIFICATE blocks.\n";
  std::cerr << " --dump=<file prefix>\n";
  std::cerr << " Dumps the verified chain to PEM files starting with <file "
               "prefix>.\n";
  // TODO(mattm): allow <certs path> to be a directory containing DER/PEM files?
  // TODO(mattm): allow target to specify an HTTPS URL to check the cert of?
  // TODO(mattm): allow target to be a verify_certificate_chain_unittest PEM
  // file?
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;
  if (!base::CommandLine::Init(argc, argv)) {
    std::cerr << "ERROR in CommandLine::Init\n";
    return 1;
  }
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  base::CommandLine::StringVector args = command_line.GetArgs();
  if (args.size() != 1U || command_line.HasSwitch("help")) {
    PrintUsage(argv[0]);
    return 1;
  }

  std::string hostname = command_line.GetSwitchValueASCII("hostname");
  if (hostname.empty()) {
    std::cerr << "ERROR: --hostname is required\n";
    return 1;
  }

  base::FilePath roots_path = command_line.GetSwitchValuePath("roots");
  base::FilePath intermediates_path =
      command_line.GetSwitchValuePath("intermediates");
  base::FilePath target_path = base::FilePath(args[0]);

  base::FilePath dump_prefix_path = command_line.GetSwitchValuePath("dump");

  std::vector<CertInput> root_der_certs;
  std::vector<CertInput> intermediate_der_certs;
  CertInput target_der_cert;

  if (!roots_path.empty())
    ReadCertificatesFromFile(roots_path, &root_der_certs);
  if (!intermediates_path.empty())
    ReadCertificatesFromFile(intermediates_path, &intermediate_der_certs);
  ReadChainFromFile(target_path, &target_der_cert, &intermediate_der_certs);

  if (target_der_cert.der_cert.empty()) {
    std::cerr << "ERROR: no target cert\n";
    return 1;
  }

  std::cout << "CertVerifyProc:\n";
  bool verify_ok = VerifyUsingCertVerifyProc(target_der_cert, hostname,
                                             intermediate_der_certs,
                                             root_der_certs, dump_prefix_path);

  return verify_ok ? 0 : 1;
}
