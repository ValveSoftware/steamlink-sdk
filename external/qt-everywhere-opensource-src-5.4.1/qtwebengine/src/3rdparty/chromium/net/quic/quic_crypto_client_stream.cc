// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_client_stream.h"

#include "net/quic/crypto/channel_id.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/quic_client_session_base.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"

namespace net {

QuicCryptoClientStream::ProofVerifierCallbackImpl::ProofVerifierCallbackImpl(
    QuicCryptoClientStream* stream)
    : stream_(stream) {}

QuicCryptoClientStream::ProofVerifierCallbackImpl::
~ProofVerifierCallbackImpl() {}

void QuicCryptoClientStream::ProofVerifierCallbackImpl::Run(
    bool ok,
    const string& error_details,
    scoped_ptr<ProofVerifyDetails>* details) {
  if (stream_ == NULL) {
    return;
  }

  stream_->verify_ok_ = ok;
  stream_->verify_error_details_ = error_details;
  stream_->verify_details_.reset(details->release());
  stream_->proof_verify_callback_ = NULL;
  stream_->DoHandshakeLoop(NULL);

  // The ProofVerifier owns this object and will delete it when this method
  // returns.
}

void QuicCryptoClientStream::ProofVerifierCallbackImpl::Cancel() {
  stream_ = NULL;
}

QuicCryptoClientStream::QuicCryptoClientStream(
    const QuicServerId& server_id,
    QuicClientSessionBase* session,
    ProofVerifyContext* verify_context,
    QuicCryptoClientConfig* crypto_config)
    : QuicCryptoStream(session),
      next_state_(STATE_IDLE),
      num_client_hellos_(0),
      crypto_config_(crypto_config),
      server_id_(server_id),
      generation_counter_(0),
      proof_verify_callback_(NULL),
      verify_context_(verify_context) {
}

QuicCryptoClientStream::~QuicCryptoClientStream() {
  if (proof_verify_callback_) {
    proof_verify_callback_->Cancel();
  }
}

void QuicCryptoClientStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  QuicCryptoStream::OnHandshakeMessage(message);

  DoHandshakeLoop(&message);
}

bool QuicCryptoClientStream::CryptoConnect() {
  next_state_ = STATE_INITIALIZE;
  DoHandshakeLoop(NULL);
  return true;
}

int QuicCryptoClientStream::num_sent_client_hellos() const {
  return num_client_hellos_;
}

// kMaxClientHellos is the maximum number of times that we'll send a client
// hello. The value 3 accounts for:
//   * One failure due to an incorrect or missing source-address token.
//   * One failure due the server's certificate chain being unavailible and the
//     server being unwilling to send it without a valid source-address token.
static const int kMaxClientHellos = 3;

void QuicCryptoClientStream::DoHandshakeLoop(
    const CryptoHandshakeMessage* in) {
  CryptoHandshakeMessage out;
  QuicErrorCode error;
  string error_details;
  QuicCryptoClientConfig::CachedState* cached =
      crypto_config_->LookupOrCreate(server_id_);

  if (in != NULL) {
    DVLOG(1) << "Client: Received " << in->DebugString();
  }

  for (;;) {
    const State state = next_state_;
    next_state_ = STATE_IDLE;
    switch (state) {
      case STATE_INITIALIZE: {
        if (!cached->IsEmpty() && !cached->signature().empty() &&
            server_id_.is_https()) {
          DCHECK(crypto_config_->proof_verifier());
          // If the cached state needs to be verified, do it now.
          next_state_ = STATE_VERIFY_PROOF;
        } else {
          next_state_ = STATE_SEND_CHLO;
        }
        break;
      }
      case STATE_SEND_CHLO: {
        // Send the client hello in plaintext.
        session()->connection()->SetDefaultEncryptionLevel(ENCRYPTION_NONE);
        if (num_client_hellos_ > kMaxClientHellos) {
          CloseConnection(QUIC_CRYPTO_TOO_MANY_REJECTS);
          return;
        }
        num_client_hellos_++;

        if (!cached->IsComplete(session()->connection()->clock()->WallNow())) {
          crypto_config_->FillInchoateClientHello(
              server_id_,
              session()->connection()->supported_versions().front(),
              cached, &crypto_negotiated_params_, &out);
          // Pad the inchoate client hello to fill up a packet.
          const size_t kFramingOverhead = 50;  // A rough estimate.
          const size_t max_packet_size =
              session()->connection()->max_packet_length();
          if (max_packet_size <= kFramingOverhead) {
            DLOG(DFATAL) << "max_packet_length (" << max_packet_size
                         << ") has no room for framing overhead.";
            CloseConnection(QUIC_INTERNAL_ERROR);
            return;
          }
          if (kClientHelloMinimumSize > max_packet_size - kFramingOverhead) {
            DLOG(DFATAL) << "Client hello won't fit in a single packet.";
            CloseConnection(QUIC_INTERNAL_ERROR);
            return;
          }
          out.set_minimum_size(max_packet_size - kFramingOverhead);
          next_state_ = STATE_RECV_REJ;
          DVLOG(1) << "Client: Sending " << out.DebugString();
          SendHandshakeMessage(out);
          return;
        }
        session()->config()->ToHandshakeMessage(&out);

        scoped_ptr<ChannelIDKey> channel_id_key;
        bool do_channel_id = false;
        if (crypto_config_->channel_id_source()) {
          const CryptoHandshakeMessage* scfg = cached->GetServerConfig();
          DCHECK(scfg);
          const QuicTag* their_proof_demands;
          size_t num_their_proof_demands;
          if (scfg->GetTaglist(kPDMD, &their_proof_demands,
                               &num_their_proof_demands) == QUIC_NO_ERROR) {
            for (size_t i = 0; i < num_their_proof_demands; i++) {
              if (their_proof_demands[i] == kCHID) {
                do_channel_id = true;
                break;
              }
            }
          }
        }
        if (do_channel_id) {
          QuicAsyncStatus status =
              crypto_config_->channel_id_source()->GetChannelIDKey(
                  server_id_.host(), &channel_id_key, NULL);
          if (status != QUIC_SUCCESS) {
            CloseConnectionWithDetails(QUIC_INVALID_CHANNEL_ID_SIGNATURE,
                                       "Channel ID lookup failed");
            return;
          }
        }

        error = crypto_config_->FillClientHello(
            server_id_,
            session()->connection()->connection_id(),
            session()->connection()->supported_versions().front(),
            cached,
            session()->connection()->clock()->WallNow(),
            session()->connection()->random_generator(),
            channel_id_key.get(),
            &crypto_negotiated_params_,
            &out,
            &error_details);
        if (error != QUIC_NO_ERROR) {
          // Flush the cached config so that, if it's bad, the server has a
          // chance to send us another in the future.
          cached->InvalidateServerConfig();
          CloseConnectionWithDetails(error, error_details);
          return;
        }
        if (cached->proof_verify_details()) {
          client_session()->OnProofVerifyDetailsAvailable(
              *cached->proof_verify_details());
        }
        next_state_ = STATE_RECV_SHLO;
        DVLOG(1) << "Client: Sending " << out.DebugString();
        SendHandshakeMessage(out);
        // Be prepared to decrypt with the new server write key.
        session()->connection()->SetAlternativeDecrypter(
            crypto_negotiated_params_.initial_crypters.decrypter.release(),
            ENCRYPTION_INITIAL,
            true /* latch once used */);
        // Send subsequent packets under encryption on the assumption that the
        // server will accept the handshake.
        session()->connection()->SetEncrypter(
            ENCRYPTION_INITIAL,
            crypto_negotiated_params_.initial_crypters.encrypter.release());
        session()->connection()->SetDefaultEncryptionLevel(
            ENCRYPTION_INITIAL);
        if (!encryption_established_) {
          encryption_established_ = true;
          session()->OnCryptoHandshakeEvent(
              QuicSession::ENCRYPTION_FIRST_ESTABLISHED);
        } else {
          session()->OnCryptoHandshakeEvent(
              QuicSession::ENCRYPTION_REESTABLISHED);
        }
        return;
      }
      case STATE_RECV_REJ:
        // We sent a dummy CHLO because we didn't have enough information to
        // perform a handshake, or we sent a full hello that the server
        // rejected. Here we hope to have a REJ that contains the information
        // that we need.
        if (in->tag() != kREJ) {
          CloseConnectionWithDetails(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                     "Expected REJ");
          return;
        }
        error = crypto_config_->ProcessRejection(
            *in, session()->connection()->clock()->WallNow(), cached,
            &crypto_negotiated_params_, &error_details);
        if (error != QUIC_NO_ERROR) {
          CloseConnectionWithDetails(error, error_details);
          return;
        }
        if (!cached->proof_valid()) {
          if (!server_id_.is_https()) {
            // We don't check the certificates for insecure QUIC connections.
            SetCachedProofValid(cached);
          } else if (!cached->signature().empty()) {
            next_state_ = STATE_VERIFY_PROOF;
            break;
          }
        }
        next_state_ = STATE_SEND_CHLO;
        break;
      case STATE_VERIFY_PROOF: {
        ProofVerifier* verifier = crypto_config_->proof_verifier();
        DCHECK(verifier);
        next_state_ = STATE_VERIFY_PROOF_COMPLETE;
        generation_counter_ = cached->generation_counter();

        ProofVerifierCallbackImpl* proof_verify_callback =
            new ProofVerifierCallbackImpl(this);

        verify_ok_ = false;

        QuicAsyncStatus status = verifier->VerifyProof(
            server_id_.host(),
            cached->server_config(),
            cached->certs(),
            cached->signature(),
            verify_context_.get(),
            &verify_error_details_,
            &verify_details_,
            proof_verify_callback);

        switch (status) {
          case QUIC_PENDING:
            proof_verify_callback_ = proof_verify_callback;
            DVLOG(1) << "Doing VerifyProof";
            return;
          case QUIC_FAILURE:
            delete proof_verify_callback;
            break;
          case QUIC_SUCCESS:
            delete proof_verify_callback;
            verify_ok_ = true;
            break;
        }
        break;
      }
      case STATE_VERIFY_PROOF_COMPLETE:
        if (!verify_ok_) {
          client_session()->OnProofVerifyDetailsAvailable(*verify_details_);
          CloseConnectionWithDetails(
              QUIC_PROOF_INVALID, "Proof invalid: " + verify_error_details_);
          return;
        }
        // Check if generation_counter has changed between STATE_VERIFY_PROOF
        // and STATE_VERIFY_PROOF_COMPLETE state changes.
        if (generation_counter_ != cached->generation_counter()) {
          next_state_ = STATE_VERIFY_PROOF;
        } else {
          SetCachedProofValid(cached);
          cached->SetProofVerifyDetails(verify_details_.release());
          next_state_ = STATE_SEND_CHLO;
        }
        break;
      case STATE_RECV_SHLO: {
        // We sent a CHLO that we expected to be accepted and now we're hoping
        // for a SHLO from the server to confirm that.
        if (in->tag() == kREJ) {
          // alternative_decrypter will be NULL if the original alternative
          // decrypter latched and became the primary decrypter. That happens
          // if we received a message encrypted with the INITIAL key.
          if (session()->connection()->alternative_decrypter() == NULL) {
            // The rejection was sent encrypted!
            CloseConnectionWithDetails(QUIC_CRYPTO_ENCRYPTION_LEVEL_INCORRECT,
                                       "encrypted REJ message");
            return;
          }
          next_state_ = STATE_RECV_REJ;
          break;
        }
        if (in->tag() != kSHLO) {
          CloseConnectionWithDetails(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                     "Expected SHLO or REJ");
          return;
        }
        // alternative_decrypter will be NULL if the original alternative
        // decrypter latched and became the primary decrypter. That happens
        // if we received a message encrypted with the INITIAL key.
        if (session()->connection()->alternative_decrypter() != NULL) {
          // The server hello was sent without encryption.
          CloseConnectionWithDetails(QUIC_CRYPTO_ENCRYPTION_LEVEL_INCORRECT,
                                     "unencrypted SHLO message");
          return;
        }
        error = crypto_config_->ProcessServerHello(
            *in, session()->connection()->connection_id(),
            session()->connection()->server_supported_versions(),
            cached, &crypto_negotiated_params_, &error_details);

        if (error != QUIC_NO_ERROR) {
          CloseConnectionWithDetails(
              error, "Server hello invalid: " + error_details);
          return;
        }
        error =
            session()->config()->ProcessPeerHello(*in, SERVER, &error_details);
        if (error != QUIC_NO_ERROR) {
          CloseConnectionWithDetails(
              error, "Server hello invalid: " + error_details);
          return;
        }
        session()->OnConfigNegotiated();

        CrypterPair* crypters =
            &crypto_negotiated_params_.forward_secure_crypters;
        // TODO(agl): we don't currently latch this decrypter because the idea
        // has been floated that the server shouldn't send packets encrypted
        // with the FORWARD_SECURE key until it receives a FORWARD_SECURE
        // packet from the client.
        session()->connection()->SetAlternativeDecrypter(
            crypters->decrypter.release(), ENCRYPTION_FORWARD_SECURE,
            false /* don't latch */);
        session()->connection()->SetEncrypter(
            ENCRYPTION_FORWARD_SECURE, crypters->encrypter.release());
        session()->connection()->SetDefaultEncryptionLevel(
            ENCRYPTION_FORWARD_SECURE);

        handshake_confirmed_ = true;
        session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
        return;
      }
      case STATE_IDLE:
        // This means that the peer sent us a message that we weren't expecting.
        CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE);
        return;
    }
  }
}

void QuicCryptoClientStream::SetCachedProofValid(
    QuicCryptoClientConfig::CachedState* cached) {
  cached->SetProofValid();
  client_session()->OnProofValid(*cached);
}

QuicClientSessionBase* QuicCryptoClientStream::client_session() {
  return reinterpret_cast<QuicClientSessionBase*>(session());
}

}  // namespace net
