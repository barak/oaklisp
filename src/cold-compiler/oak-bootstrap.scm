#!/usr/bin/env -S guile3.0 --no-auto-compile -s
!#
;;; This file is part of Oaklisp.
;;;
;;; oak-bootstrap.scm -- compile Oaklisp source files to .oa object
;;; files without a running Oaklisp.
;;;
;;; This hosts enough of Oaklisp in Guile to load the world's own
;;; macros and compiler from their .oak sources (see host/world.scm for
;;; exactly which files), and then runs that compiler.  Because it is
;;; the real compiler that runs, the output is byte for byte what a
;;; native Oaklisp produces, which is what makes the bootstrap
;;; checkable: `make check-bootstrap' in src/world compares it with the
;;; prebuilt objects.
;;;
;;; Usage: oak-bootstrap.scm [--srcdir DIR] [--outdir DIR]
;;;                          [--locale system-locale|compiler-locale]
;;;                          [--verbose] [--noisy N] FILE...
;;;
;;; Each FILE is the base name of DIR/FILE.oak (or, with a slash in it,
;;; a path minus the .oak); FILE.oa is written to OUTDIR.  --locale
;;; gives the locale the files are compiled in (the compiler's own
;;; sources want compiler-locale) and may be repeated between files.
;;; --32bit compiles as a 32-bit Oaklisp would; the cold objects do not
;;; depend on the word size, but bignum.oak probes it.
;;;
;;; Copyright (C) 2026 Blake McBride
;;;
;;; This program is free software; you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation; either version 2 of the License, or
;;; (at your option) any later version.
;;;
;;; This program is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.
;;;
;;; The GNU GPL is available at http://www.gnu.org/licenses/gpl.html
;;; or from the Free Software Foundation, 59 Temple Place - Suite 330,
;;; Boston, MA 02111-1307, USA

(use-modules (srfi srfi-1)
	     (srfi srfi-9)
	     (srfi srfi-13)
	     (ice-9 format)
	     (ice-9 hash-table))

(define host-dir
  (let ((f (current-filename)))
    (string-append (dirname (or f "oak-bootstrap.scm")) "/host")))

(for-each (lambda (f) (load (string-append host-dir "/" f ".scm")))
	  '("reader" "kernel" "translate" "macros" "runtime" "runtime2" "world"))

(define (usage)
  (format (current-error-port)
	  "usage: oak-bootstrap.scm [--srcdir DIR] [--outdir DIR] [--locale L] [--32bit] [--noisy N] [--verbose] [--keep-going] [--eval EXPR] FILE...~%")
  (exit 2))

(define (main args)
  (let loop ((args (cdr args)) (jobs '()) (locale-name 'SYSTEM-LOCALE))
    (cond
     ((null? args)
      (run-jobs (reverse jobs)))
     ((string=? (car args) "--srcdir")
      (set! *srcdir* (cadr args)) (loop (cddr args) jobs locale-name))
     ((string=? (car args) "--outdir")
      (set! *outdir* (cadr args)) (loop (cddr args) jobs locale-name))
     ((string=? (car args) "--verbose")
      (set! *verbose* #t) (loop (cdr args) jobs locale-name))
     ((string=? (car args) "--debug")
      (set! *debug* #t) (loop (cdr args) jobs locale-name))
     ((string=? (car args) "--32bit")
      (set! *word-bits* 32) (loop (cdr args) jobs locale-name))
     ((string=? (car args) "--keep-going")
      (set! *keep-going* #t) (loop (cdr args) jobs locale-name))
     ((string=? (car args) "--trace")
      (set! *trace-eval* #t) (loop (cdr args) jobs locale-name))
     ((string=? (car args) "--noisy")
      (loop (cddr args) (cons (cons 'noisy (string->number (cadr args))) jobs) locale-name))
     ((string=? (car args) "--locale")
      (loop (cddr args) jobs (string->symbol (string-upcase (cadr args)))))
     ((string=? (car args) "--eval")
      (loop (cddr args) (cons (list 'eval (cadr args) locale-name) jobs) locale-name))
     ((string-prefix? "--" (car args)) (usage))
     (else
      (loop (cdr args) (cons (cons locale-name (car args)) jobs) locale-name)))))

(define *debug* #f)

(define (run-jobs jobs)
  (catch 'oak-error
    (lambda ()
     (with-throw-handler #t
      (lambda ()
      (build-world!)
      (for-each
       (lambda (job)
	 (cond ((eq? (car job) 'noisy)
		(fluid-set! 'COMPILER-NOISINESS (cdr job)))
	       ((eq? (car job) 'eval)
		(let ((form (oak-read (open-input-string (cadr job))))
		      (locale (global-ref (caddr job))))
		  (let ((v (with-fluids* (list (cons 'CURRENT-LOCALE locale))
					 (lambda () (oak-eval form locale)))))
		    (oak-print v (global-ref 'STANDARD-OUTPUT))
		    (newline))))
	       (else
		(let ((locale (global-ref (car job))))
		  (format (current-error-port) ";; compiling ~A~%" (cdr job))
		  (if *keep-going*
		      (catch 'oak-error
			(lambda () (compile-world-file (cdr job) locale))
			(lambda (key msg)
			  (format (current-error-port) ";; error compiling ~A: ~A~%" (cdr job) msg)))
		      (compile-world-file (cdr job) locale))))))
       jobs))
      (lambda (key . args)
	(when (and *debug* (not (eq? key 'oak-error)))
	  (format (current-error-port) "~%guile error: ~A ~S~%" key args)
	  (display-backtrace (make-stack #t) (current-error-port))))))
    (lambda (key msg)
      (format (current-error-port) "~%oak-bootstrap: error: ~A~%" msg)
      (when *current-file*
	(format (current-error-port) "  while loading ~A~%" *current-file*))
      (exit 1))
    (lambda (key msg)
      (when *debug*
	(display-backtrace (make-stack #t) (current-error-port))))))

(main (command-line))
