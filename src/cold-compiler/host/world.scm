;;; This file is part of Oaklisp.
;;;
;;; world.scm -- building the hosted world: which files of src/world
;;; are loaded from source, which are replaced by the natives, and in
;;; what order.
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

(define *srcdir* ".")
(define *verbose* #f)

(define (world-file name)
  (string-append *srcdir* "/" name ".oak"))

(define *locales-ready* #f)

(define (make-sub-locale locale)
  (if *locales-ready*
      (oak-call (global-ref 'MAKE) (global-ref 'LOCALE) (list locale))
      locale))

(define *keep-going* #f)

(define (load-world-file name locale)
  (when *verbose*
    (format (current-error-port) ";; loading ~A~%" name))
  (if *keep-going*
      (catch 'oak-error
	(lambda () (oak-load-file (world-file name) locale make-sub-locale))
	(lambda (key msg)
	  (format (current-error-port) ";; error loading ~A: ~A~%" name msg)))
      (oak-load-file (world-file name) locale make-sub-locale)))

;;; Files loaded before the macro files, with the bootstrap macros.
(define pre-files '("multi-off" "kernel1-freeze" "fluid"))

;;; The macro files: MISCFILES up to define, plus backquote (cold).
(define macro-files
  '("macros0" "obsolese" "destructure" "macros1" "macros2" "icky-macros"
    "define" "backquote"))

;;; The cold files, in COLDFILESD order, each with what to do:
;;;   load          load from source
;;;   skip          provided natively (or not needed in the host)
;;;   (load . hook) load, then call the hook
(define cold-plan
  `(("cold-booting" . skip)
    ("kernel0" . skip)
    ("kernel0types" . skip)
    ("kernel1-install" . skip)
    ("kernel1-funs" . load)
    ("kernel1-make" . load)
    ("kernel1-freeze" . skip)              ; already loaded
    ("kernel1-maketype" . load)
    ("kernel1-inittypes" . load)
    ("kernel1-segments" . load)
    ("super" . skip)
    ("kernel" load ,(lambda ()
			;; kernel.oak re-initializes the builder's types, which
			;; empties their method tables.
			(install-list-methods!)
			(install-string-methods!)
			(install-print-methods!)
			(install-string-coercions!)
			(install-symbol-methods! *symbol*)
			(install-symbol-coercions! *symbol*)
			(install-list-coercions!)
			(install-hash-coercions!)
			(install-stream-coercions!)))
    ("patch0symbols" . skip)
    ("mix-types" . load)
    ("operations" . load)
    ("ops" . load)
    ("truth" load ,(lambda () (install-print-methods!)))
    ("logops" . load)
    ("consume" . skip)
    ("conses" . load)
    ("coerce" . load)
    ("eqv" . load)
    ("mapping" . load)
    ("fastmap" . load)
    ("multi-off" . skip)                   ; already loaded
    ("fluid" . skip)                       ; already loaded
    ("vector-type" . load)
    ("vl-mixin" load ,(lambda () (install-vector-methods! *simple-vector*)))
    ("numbers" load ,(lambda ()
			 (install-print-methods!)
			 (install-number-print-methods!)))
    ("subtypes" load ,(lambda () (install-char-methods! *character*)))
    ("weak" . load)
    ("strings" forms (DEFINE-CONSTANT %CHARS-PER-WORD 3))
    ("sequences" . load)
    ("undefined" . load)
    ("subprimitive" . load)
    ("gc" . load)
    ("tag-trap" . skip)
    ("code-vector" . load)
    ("hash-table" . skip)
    ("format" . skip)
    ("signal" . load)
    ("error" . load)
    ("symbols" . skip)
    ("print-noise" . skip)
    ("patch-symbols" . skip)
    ("predicates" . load)
    ("print" . load)
    ("print-integer" . skip)
    ("print-list" . skip)
    ("reader-errors" . skip)
    ("reader" . skip)
    ("read-token" . skip)
    ("reader-macros" . skip)
    ("hash-reader" . skip)
    ("read-char" . skip)
    ("locales" . load)
    ("expand" . load)
    ("make-locales" load ,(lambda () (switch-to-real-locales!)))
    ("patch-locales" . skip)
    ("freeze" . load)
    ("bp-alist" . load)
    ("describe" . skip)
    ("warm" . load)
    ("interpreter" . skip)
    ("eval" . skip)
    ("repl" . skip)
    ("system-version" . skip)
    ("top-level" . skip)
    ("booted" . skip)
    ("dump-stack" . skip)
    ("file-errors" . skip)
    ("streams" forms
     (DEFINE-CONSTANT-INSTANCE %STREAM-PRIMITIVE
       (MIX-TYPES OC-MIXER (LIST FOLDABLE-MIXIN OPERATION)))
     (LET ((SP-ALIST (QUOTE ())))
       (ADD-METHOD (%STREAM-PRIMITIVE (OBJECT) N)
	 (LET ((X (ASSQ N SP-ALIST)))
	   (COND (X => CDR)
		 (ELSE
		  (LET ((OP (MAKE (MIX-TYPES OC-MIXER (LIST OPEN-CODED-MIXIN OPERATION))
				  (LIST (LIST (QUOTE STREAM-PRIMITIVE) N))
				  (NTH (QUOTE (0 0 0 2 2 2 1 1 2 1 1 1 2 2 4)) N)
				  1)))
		    (SET! SP-ALIST (CONS (CONS N OP) SP-ALIST))
		    OP)))))))
    ("cold" . skip)
    ("nargs" . skip)
    ("has-method" . load)
    ("op-error" . skip)
    ("error2" . load)
    ("error3" . load)
    ("backquote" . skip)                   ; already loaded
    ("file-io" . load)
    ("fasl" . load)
    ("load-oaf" . load)
    ("load-file" . skip)
    ("string-stream" . skip)
    ("list" . load)
    ("catch" . load)
    ("continuation" . load)
    ("unwind-protect" . load)
    ("bounders" . skip)
    ("anonymous" . skip)
    ("sort" . load)
    ("exit" . skip)
    ("cmdline" . skip)
    ("cmdline-getopt" . skip)
    ("cmdline-options" . skip)
    ("export" . load)
    ("cold-boot-end" . skip)))

;;; The rest of MISCFILES: they define constants and types the compiler
;;; sees (frozen names are inlined, type ivar lists shape methods).
(define misc-files
  '("del" "promise" "bignum" "rational" "complex" "rounding"
    "lazy-cons" "math" "trace" "apropos" "time" "bignum2" "alarm" "multi-em"
    "multiproc" "dump-world"))

(define compiler-files
  '("crunch" "mac-comp-stuff" "mac-compiler-nodes" "mac-compiler1"
    "mac-compiler2" "mac-compiler3" "mac-code" "assembler" "peephole"
    "file-compiler" "compiler-exports"))

;;; Once make-locales.oak has made SYSTEM-LOCALE, move the proto
;;; locale's contents there and start using real locales.
(define (switch-to-real-locales!)
  (let ((system-locale (global-ref 'SYSTEM-LOCALE)))
    (migrate-proto-locale! system-locale)
    (set! *locales-ready* #t)
    (fluid-set! 'CURRENT-LOCALE system-locale)))

(define (system-locale) (global-ref 'SYSTEM-LOCALE))
(define (compiler-locale) (global-ref 'COMPILER-LOCALE))

;;; Natives that must exist before anything is loaded.
(define (install-all-natives!)
  (kernel-bootstrap-types!)
  (set! *proto-locale* (make-proto-locale))
  (install-locale-ops!)
  (install-kernel-natives!)
  (install-fluid-natives!)
  (install-list-natives!)
  (install-number-natives!)
  (install-symbol-natives!)
  (install-error-natives!)
  (install-print-natives!)
  (install-string-natives!)
  (install-vector-natives!)
  (install-hash-natives!)
  (install-stream-natives!)
  (install-format-natives!)
  (install-symbol-methods! *symbol*)
  (install-vector-methods! *simple-vector*)
  (install-char-methods! *character*)
  ;; The expander and evaluator.
  (native-locked! 'EXPAND-GROVELING *operation* (nlambda (locale form) (oak-expand locale form)))
  (native-locked! 'IDIOSYNCRATICALLY-GROVEL *operation*
		  (nlambda (locale form) (idiosyncratically-grovel locale form)))
  (native-locked! 'EVAL *operation* (nlambda (form locale) (oak-eval form locale)))
  (native-locked! 'SUBEVAL *operation* (nlambda (form locale) (oak-eval form locale)))
  (native-fn! 'IMPROPER-LIST? (nlambda (x) (improper-list? x)))
  (native-fn! 'MAKE-PROPER (nlambda (x) (make-proper x)))
  (native-fn! 'FREEZE-IN-CURRENT-LOCALE (nlambda (v) '()))
  (native-fn! 'DEFINE-HASH-MACRO-CHAR (nlambda (c f) '())))

;;; Build the whole hosted world.
(define (build-world!)
  (install-all-natives!)
  (for-each (lambda (f) (load-world-file f *proto-locale*)) pre-files)
  (install-host-fluids!)
  (fluid-set! 'CURRENT-LOCALE *proto-locale*)
  (for-each (lambda (f) (load-world-file f *proto-locale*)) macro-files)
  (for-each
   (lambda (entry)
     (let ((name (car entry)) (action (cdr entry)))
       (cond ((eq? action 'skip) #t)
	     ((eq? action 'load) (load-world-file name (current-locale)))
	     ((and (pair? action) (eq? (car action) 'load))
	      (load-world-file name (current-locale))
	      ((cadr action)))
	     ((and (pair? action) (eq? (car action) 'forms))
	      ;; Just the forms of NAME that define things the compiler
	      ;; sees; the rest of the file is provided natively.
	      (when *verbose* (format (current-error-port) ";; forms from ~A~%" name))
	      (for-each (lambda (form) (oak-load-form form (current-locale) (current-locale)))
			(cdr action)))
	     (else (oak-host-error "bad plan entry ~S" entry)))))
   cold-plan)
  (for-each (lambda (f) (load-world-file f (system-locale))) misc-files)
  ;; The compiler goes into COMPILER-LOCALE.
  (fluid-set! 'CURRENT-LOCALE (compiler-locale))
  (for-each (lambda (f) (load-world-file f (compiler-locale))) compiler-files)
  (fluid-set! 'CURRENT-LOCALE (system-locale))
  #t)

;;; Compile SRCDIR/NAME.oak (or NAME.oak when NAME has a slash in it),
;;; writing NAME.oa under *OUTDIR*.
(define (compile-world-file name locale)
  (with-fluids* (list (cons 'CURRENT-LOCALE locale))
    (lambda ()
      (oak-call (global-ref 'COMPILE-FILE) locale
		(if (string-index name #\/)
		    name
		    (string-append *srcdir* "/" name))))))
