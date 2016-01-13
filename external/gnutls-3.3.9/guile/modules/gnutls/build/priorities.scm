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

;;; Written by Ludovic Courtès <ludo@chbouib.org>

(define-module (gnutls build priorities)
  :use-module (srfi srfi-9)
  :use-module (gnutls build utils)
  :use-module (gnutls build enums)
  :export (output-session-set-priority-function %gnutls-priorities))

;;;
;;; Helpers to generate the `gnutls_XXX_set_priority ()' wrappers.
;;;



;;;
;;; Priority functions.
;;;

(define-record-type <session-priority>
  (make-session-priority enum-type c-setter)
  session-priority?
  (enum-type        session-priority-enum-type)
  (c-setter         session-priority-c-setter)
  (c-getter         session-priority-c-getter))


;;;
;;; C code generation.
;;;

(define (output-session-set-priority-function priority port)
  (let* ((enum   (session-priority-enum-type priority))
         (setter (session-priority-c-setter priority))
         (c-name (scheme-symbol->c-name (enum-type-subsystem enum))))
    (format port "SCM_DEFINE (scm_gnutls_set_session_~a_priority_x,~%"
            c-name)
    (format port "            \"set-session-~a-priority!\", 2, 0, 0,~%"
            (enum-type-subsystem enum))
    (format port "            (SCM session, SCM items),~%")
    (format port "            \"Use @var{items} (a list) as the list of \"~%")
    (format port "            \"preferred ~a for @var{session}.\")~%"
            (enum-type-subsystem enum))
    (format port "#define FUNC_NAME s_scm_gnutls_set_session_~a_priority_x~%"
            c-name)
    (format port "{~%")
    (format port "  gnutls_session_t c_session;~%")
    (format port "  ~a *c_items;~%"
            (enum-type-c-type enum))
    (format port "  long int c_len, i;~%")
    (format port "  scm_c_issue_deprecation_warning \
(\"`set-session-~a-priority!' is deprecated, \
use `set-session-priorities!' instead\");~%" (enum-type-subsystem enum))
    (format port "  c_session = scm_to_gnutls_session (session, 1, FUNC_NAME);~%")
    (format port "  SCM_VALIDATE_LIST_COPYLEN (2, items, c_len);~%")
    (format port "  c_items = alloca (sizeof (* c_items) * (c_len + 1));~%")
    (format port "  for (i = 0; i < c_len; i++, items = SCM_CDR (items))~%")
    (format port "    c_items[i] = ~a (SCM_CAR (items), 2, FUNC_NAME);~%"
            (enum-type-to-c-function enum))
    (format port "  c_items[c_len] = (~a) 0;~%"
            (enum-type-c-type enum))
    (format port "  ~a (c_session, (int *) c_items);~%"
            setter)
    (format port "  return SCM_UNSPECIFIED;~%")
    (format port "}~%")
    (format port "#undef FUNC_NAME~%")))


;;;
;;; Actual priority functions.
;;;

(define %gnutls-priorities
  (map make-session-priority
       (list %cipher-enum %mac-enum %compression-method-enum %kx-enum
             %protocol-enum %certificate-type-enum)
       (list "gnutls_cipher_set_priority" "gnutls_mac_set_priority"
             "gnutls_compression_set_priority" "gnutls_kx_set_priority"
             "gnutls_protocol_set_priority"
             "gnutls_certificate_type_set_priority")))


;;; Local Variables:
;;; mode: scheme
;;; coding: latin-1
;;; End:

;;; arch-tag: a9cdcc92-6dcf-4d63-afec-6dc16334e379
