;;; GnuTLS --- Guile bindings for GnuTLS.
;;; Copyright (C) 2011-2012 Free Software Foundation, Inc.
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

;;; Written by Ludovic Courtès <ludo@gnu.org>.

(define-module (gnutls build tests)
  #:export (run-test))

(define (run-test thunk)
  "Call `(exit (THUNK))'.  If THUNK raises an exception, then call `(exit 1)' and
display a backtrace.  Otherwise, return THUNK's return value."
  (exit
   (catch #t
     thunk
     (lambda (key . args)
       ;; Never reached.
       (exit 1))
     (lambda (key . args)
       (dynamic-wind ;; to be on the safe side
         (lambda () #t)
         (lambda ()
           (format (current-error-port)
                   "~%throw to `~a' with args ~s~%" key args)
           (display-backtrace (make-stack #t) (current-output-port)))
         (lambda ()
           (exit 1)))
       (exit 1)))))
