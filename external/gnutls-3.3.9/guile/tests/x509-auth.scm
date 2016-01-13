;;; GnuTLS --- Guile bindings for GnuTLS.
;;; Copyright (C) 2007-2013 Free Software Foundation, Inc.
;;;
;;; GnuTLS is free software; you can redistribute it and/or
;;; modify it under the terms of the GNU Lesser General Public
;;; License as published by the Free Software Foundation; either
;;; version 2.1 of the License, or (at your option) any later version.
;;;
;;; GnuTLS is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;;; Lesser General Public License for more details.
;;;
;;; You should have received a copy of the GNU Lesser General Public
;;; License along with GnuTLS; if not, write to the Free Software
;;; Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

;;; Written by Ludovic Courtès <ludo@chbouib.org>.


;;;
;;; Test session establishment using X.509 certificate authentication.
;;; Based on `openpgp-auth.scm'.
;;;

(use-modules (gnutls)
             (gnutls build tests)
             (srfi srfi-4))


;; TLS session settings (using the deprecated method).
(define %protos  (list protocol/tls-1.0))
(define %certs   (list certificate-type/x509))
(define %ciphers (list cipher/null cipher/arcfour cipher/aes-128-cbc
                       cipher/aes-256-cbc))
(define %kx      (list kx/rsa kx/rsa-export kx/dhe-dss kx/dhe-dss))
(define %macs    (list mac/sha1 mac/rmd160 mac/md5))

;; Message sent by the client.
(define %message
  (cons "hello, world!" (iota 4444)))

(define (import-something import-proc file fmt)
  (let* ((path (search-path %load-path file))
         (size (stat:size (stat path)))
         (raw  (make-u8vector size)))
    (uniform-vector-read! raw (open-input-file path))
    (import-proc raw fmt)))

(define (import-key import-proc file)
  (import-something import-proc file x509-certificate-format/pem))

(define (import-rsa-params file)
  (import-something pkcs1-import-rsa-parameters file
                    x509-certificate-format/pem))

(define (import-dh-params file)
  (import-something pkcs3-import-dh-parameters file
                    x509-certificate-format/pem))

;; Debugging.
;; (set-log-level! 3)
;; (set-log-procedure! (lambda (level str)
;;                       (format #t "[~a|~a] ~a" (getpid) level str)))

(run-test
    (lambda ()
      (let ((socket-pair (socketpair PF_UNIX SOCK_STREAM 0))
            (pub         (import-key import-x509-certificate
                                     "x509-certificate.pem"))
            (sec         (import-key import-x509-private-key
                                     "x509-key.pem")))
        (let ((pid (primitive-fork)))
          (if (= 0 pid)

              (let ((client (make-session connection-end/client))
                    (cred   (make-certificate-credentials)))
                ;; client-side (child process)
                (set-session-default-priority! client)
                (set-session-certificate-type-priority! client %certs)
                (set-session-kx-priority! client %kx)
                (set-session-protocol-priority! client %protos)
                (set-session-cipher-priority! client %ciphers)
                (set-session-mac-priority! client %macs)

                (set-certificate-credentials-x509-keys! cred (list pub) sec)
                (set-session-credentials! client cred)
                (set-session-dh-prime-bits! client 1024)

                (set-session-transport-fd! client (port->fdes (car socket-pair)))

                (handshake client)
                (write %message (session-record-port client))
                (bye client close-request/rdwr)

                (primitive-exit))

              (let ((server (make-session connection-end/server))
                    (rsa    (import-rsa-params "rsa-parameters.pem"))
                    (dh     (import-dh-params "dh-parameters.pem")))
                ;; server-side
                (set-session-default-priority! server)
                (set-session-certificate-type-priority! server %certs)
                (set-session-kx-priority! server %kx)
                (set-session-protocol-priority! server %protos)
                (set-session-cipher-priority! server %ciphers)
                (set-session-mac-priority! server %macs)
                (set-server-session-certificate-request! server
                         certificate-request/require)

                (set-session-transport-fd! server (port->fdes (cdr socket-pair)))
                (let ((cred (make-certificate-credentials))
                      (trust-file (search-path %load-path
                                               "x509-certificate.pem"))
                      (trust-fmt  x509-certificate-format/pem))
                  (set-certificate-credentials-dh-parameters! cred dh)
                  (set-certificate-credentials-rsa-export-parameters! cred rsa)
                  (set-certificate-credentials-x509-keys! cred (list pub) sec)
                  (set-certificate-credentials-x509-trust-file! cred
                                                                trust-file
                                                                trust-fmt)
                  (set-session-credentials! server cred))
                (set-session-dh-prime-bits! server 1024)

                (handshake server)
                (let ((msg (read (session-record-port server)))
                      (auth-type (session-authentication-type server)))
                  (bye server close-request/rdwr)
                  (and (eq? auth-type credentials/certificate)
                       (equal? msg %message)))))))))

;;; arch-tag: 1f88f835-a5c8-4fd6-94b6-5a13571ba03d
