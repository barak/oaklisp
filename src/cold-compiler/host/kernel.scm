;;; This file is part of Oaklisp.
;;;
;;; kernel.scm -- the object system and primitives of the Guile-hosted
;;; Oaklisp used to bootstrap the world from source.
;;;
;;; This replaces the parts of the cold world that are written against
;;; the virtual machine (kernel0.oak, kernel0types.oak, tag-trap.oak,
;;; the string and stream implementations, ...) with Guile code that
;;; presents the same interface, so that the rest of the world -- and
;;; in particular the macros and the compiler -- can be loaded from
;;; their .oak sources unchanged.
;;;
;;; Representation:
;;;   ()            nil / #f              #t          the truth value
;;;   pair          cons-pair             integer     fixnum
;;;   symbol        symbol (upcased)      string      string (mutable)
;;;   char          character             vector      simple-vector
;;;   <oak-obj>     any other instance: a vector of slots, slot 0 being
;;;                 the type, laid out exactly as kernel1-maketype.oak
;;;                 lays out instances so that (%SLOT n) means the same
;;;                 thing here as in the emulator.
;;;   <oak-loc>     a locative
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

;;;==========================================================================
;;; Records
;;;==========================================================================

;;; An instance.  V is the slot vector (slot 0 = type).  AUX holds host
;;; side data: an <opinfo> for operations, a Guile hash table for hash
;;; tables, a port or buffer for streams.
(define-record-type <oak-obj>
  (%make-oak-obj v aux)
  oak-obj?
  (v oak-obj-v)
  (aux oak-obj-aux set-oak-obj-aux!))

;;; A locative.  GET and SET are thunks/procedures; VAR is the Guile
;;; variable behind a global cell (or #f); NAME is the global's name
;;; once it is known, used to trigger native overrides.
(define-record-type <oak-loc>
  (%make-oak-loc get set var name)
  oak-loc?
  (get oak-loc-get)
  (set oak-loc-set)
  (var oak-loc-var)
  (name oak-loc-name set-oak-loc-name!))

;;; Per-operation host data.
(define-record-type <opinfo>
  (%make-opinfo native cache epoch)
  opinfo?
  (native opinfo-native set-opinfo-native!)   ; locked native proc or #f
  (cache opinfo-cache)                         ; hash: type -> (proc . bp)
  (epoch opinfo-epoch set-opinfo-epoch!))

(define (make-loc get set) (%make-oak-loc get set #f #f))

(define (make-cell-loc value)
  (let ((var (make-variable value)))
    (%make-oak-loc (lambda () (variable-ref var))
		   (lambda (x) (variable-set! var x))
		   var #f)))

(define (loc-contents loc) ((oak-loc-get loc)))
(define (loc-set-contents! loc v) ((oak-loc-set loc) v) v)

;;;==========================================================================
;;; Truth
;;;==========================================================================

(define-syntax oak-true?
  (syntax-rules () ((_ x) (not (null? x)))))
(define-syntax oak-bool
  (syntax-rules () ((_ x) (if x #t '()))))

;;;==========================================================================
;;; Errors
;;;==========================================================================

(define (oak-host-error msg . args)
  (throw 'oak-error (apply format #f msg args)))

;;;==========================================================================
;;; Slots and raw allocation
;;;==========================================================================

(define (%slot-ref obj n)
  (cond ((oak-obj? obj) (vector-ref (oak-obj-v obj) n))
	((and (= n 0)) (oak-get-type obj))
	(else (oak-host-error "(%SLOT ~A) of a non-object ~S" n obj))))

(define (%slot-set! obj n val)
  (cond ((oak-obj? obj) (vector-set! (oak-obj-v obj) n val) val)
	((= n 0) (set-immediate-type! obj val) val)
	(else (oak-host-error "(SETTER (%SLOT ~A)) of a non-object ~S" n obj))))

;;; Allocate an instance of TYPE with N slots (N includes the type slot).
(define (%allocate type n)
  (let ((v (make-vector n '())))
    (vector-set! v 0 type)
    (%make-oak-obj v #f)))

;;;==========================================================================
;;; Types
;;;==========================================================================

;;; Slot numbers in a type object (kernel0.oak).
(define TYPE-INSTANCE-LENGTH 1)
(define TYPE-VAR-LEN? 2)
(define TYPE-SUPERS 3)
(define TYPE-IVAR-LIST 4)
(define TYPE-IVAR-COUNT 5)
(define TYPE-BP-ALIST 6)
(define TYPE-OP-METH-ALIST 7)
(define TYPE-TOP-WIRED? 8)
(define TYPE-SIZE 9)

(define (type-supers t) (vector-ref (oak-obj-v t) TYPE-SUPERS))
(define (type-ivar-list t) (vector-ref (oak-obj-v t) TYPE-IVAR-LIST))
(define (type-bp-alist t) (vector-ref (oak-obj-v t) TYPE-BP-ALIST))
(define (type-meth-alist t) (vector-ref (oak-obj-v t) TYPE-OP-METH-ALIST))
(define (type-instance-length t) (vector-ref (oak-obj-v t) TYPE-INSTANCE-LENGTH))

;;; Types that the world builder is expected to have made, plus the
;;; host's own placeholders.  These are host variables so that get-type
;;; is fast; the ones the world redefines are updated by hooks.

(define *type* #f)
(define *object* #f)
(define *operation* #f)
(define *settable-operation* #f)
(define *locatable-operation* #f)
(define *coercable-type* #f)
(define *cons-pair* #f)
(define *null-type* #f)
(define *fixnum* #f)
(define *bignum* #f)
(define *string* #f)
(define *symbol* #f)
(define *character* #f)
(define *locative* #f)
(define *truths* #f)
(define *simple-vector* #f)
(define *%code-vector* #f)
(define *%closed-environment* #f)
(define *%method* #f)
(define *variable-length-mixin* #f)
(define *proto-locale-type* #f)
(define *fraction* #f)

;;; Method cache epoch: bumped whenever any method table or type
;;; relationship changes.
(define *method-epoch* 0)
(define (bump-method-epoch!) (set! *method-epoch* (+ *method-epoch* 1)))

;;; The layout algorithm from kernel1-maketype.oak, verbatim in spirit.

(define (type-initialize! self the-ivar-list the-supertype-list)
  (let ((v (oak-obj-v self))
	(vlm (and *variable-length-mixin-cell*
		  (variable-ref *variable-length-mixin-cell*))))
    (vector-set! v TYPE-VAR-LEN? '())
    (vector-set! v TYPE-SUPERS the-supertype-list)
    (vector-set! v TYPE-IVAR-LIST the-ivar-list)
    (vector-set! v TYPE-IVAR-COUNT (length the-ivar-list))
    (vector-set! v TYPE-OP-METH-ALIST '())
    (vector-set! v TYPE-TOP-WIRED? '())
    (vector-set! v TYPE-INSTANCE-LENGTH 1)
    (vector-set! v TYPE-BP-ALIST '())
    (let nextsuper ((supers-to-do the-supertype-list)
		    (type-table (list self))
		    (top-wired-guy '())
		    (var-len-guy '()))
      (if (pair? supers-to-do)
	  (let ((guy (car supers-to-do)))
	    (cond ((eq? guy vlm)
		   (when (pair? var-len-guy)
		     (oak-host-error "type init: can't include vl-mixin twice"))
		   (vector-set! v TYPE-VAR-LEN? #t)
		   (vector-set! v TYPE-INSTANCE-LENGTH 2)
		   (nextsuper (cdr supers-to-do) type-table top-wired-guy (list guy)))
		  ((oak-true? (vector-ref (oak-obj-v guy) TYPE-TOP-WIRED?))
		   (when (pair? top-wired-guy)
		     (oak-host-error "type init: can't combine two top-wired types"))
		   (nextsuper (cdr supers-to-do) type-table (list guy) var-len-guy))
		  ((memq guy type-table)
		   (nextsuper (cdr supers-to-do) type-table top-wired-guy var-len-guy))
		  (else
		   (nextsuper (append (type-supers guy) (cdr supers-to-do))
			      (cons guy type-table)
			      top-wired-guy var-len-guy))))
	  (begin
	    (when (and (pair? top-wired-guy) (pair? var-len-guy))
	      (oak-host-error "type init: can't have both vl-mixin and a top-wired type"))
	    (let layout ((guys (append top-wired-guy (append type-table var-len-guy))))
	      (if (pair? guys)
		  (let ((guy (car guys)))
		    (vector-set! v TYPE-BP-ALIST
				 (cons (cons guy (vector-ref v TYPE-INSTANCE-LENGTH))
				       (vector-ref v TYPE-BP-ALIST)))
		    (vector-set! v TYPE-INSTANCE-LENGTH
				 (+ (vector-ref v TYPE-INSTANCE-LENGTH)
				    (vector-ref (oak-obj-v guy) TYPE-IVAR-COUNT)))
		    (layout (cdr guys)))
		  (begin (bump-method-epoch!) self))))))))

;;; The cell of the global VARIABLE-LENGTH-MIXIN, set up by the world.
(define *variable-length-mixin-cell* #f)

;;; A fresh, uninitialized type object of meta-type META.
(define* (raw-type meta #:optional (size TYPE-SIZE))
  (let ((t (%allocate meta size)))
    (let ((v (oak-obj-v t)))
      (vector-set! v TYPE-INSTANCE-LENGTH 1)
      (vector-set! v TYPE-VAR-LEN? '())
      (vector-set! v TYPE-SUPERS '())
      (vector-set! v TYPE-IVAR-LIST '())
      (vector-set! v TYPE-IVAR-COUNT 0)
      (vector-set! v TYPE-BP-ALIST (list (cons t 1)))
      (vector-set! v TYPE-OP-METH-ALIST '())
      (vector-set! v TYPE-TOP-WIRED? '()))
    t))

(define (make-type ivars supers)
  (let ((t (raw-type *type*)))
    (type-initialize! t ivars supers)
    t))

;;; SUBTYPE? as kernel1-funs.oak defines it: membership in the bp alist.
(define (oak-subtype? t potential-super)
  (oak-bool (assq potential-super (type-bp-alist t))))

(define (bp-offset self-type meth-type)
  (let ((p (assq meth-type (type-bp-alist self-type))))
    (if p (cdr p) 0)))

;;; Index of ivar NAME in TYPE's own ivar list, or #f.
(define (ivar-index type name)
  (let loop ((l (type-ivar-list type)) (i 0))
    (cond ((null? l) #f)
	  ((eq? (car l) name) i)
	  (else (loop (cdr l) (+ i 1))))))

;;;==========================================================================
;;; The type of a host value
;;;==========================================================================

;;; Immediates whose type can be changed with (SETTER (%SLOT 0)).
(define *immediate-types* '())   ; alist value -> type, for #t only really

(define (set-immediate-type! obj type)
  (cond ((eq? obj #t) (set! *truths* type))
	((null? obj) (set! *null-type* type))
	(else (oak-host-error "can't set the type of ~S" obj))))

(define (oak-get-type x)
  (cond ((pair? x) *cons-pair*)
	((null? x) *null-type*)
	((oak-obj? x) (vector-ref (oak-obj-v x) 0))
	((symbol? x) *symbol*)
	((exact-integer? x) *fixnum*)
	((string? x) *string*)
	((char? x) *character*)
	((eq? x #t) *truths*)
	((vector? x) *simple-vector*)
	((oak-loc? x) *locative*)
	((and (rational? x) (exact? x)) (or *fraction* *fixnum*))
	((procedure? x) *operation*)
	(else (oak-host-error "get-type of unknown host value ~S" x))))

(define (oak-is-a? obj type)
  (oak-subtype? (oak-get-type obj) type))

;;;==========================================================================
;;; Operations and dispatch
;;;==========================================================================

;;; Slot numbers in an operation (kernel0.oak).
(define OP-LAMBDA? 1)

(define (oak-op? x)
  (and (oak-obj? x)
       (oak-true? (oak-subtype? (vector-ref (oak-obj-v x) 0) *operation*))))

(define (op-info op)
  (or (oak-obj-aux op)
      (let ((i (%make-opinfo #f (make-hash-table) *method-epoch*)))
	(set-oak-obj-aux! op i)
	i)))

(define (op-lambda op) (vector-ref (oak-obj-v op) OP-LAMBDA?))

;;; A native operation of the given type with PROC as its lambda.
(define (make-native-op type proc)
  (let ((op (%allocate type (type-instance-length type))))
    (vector-set! (oak-obj-v op) OP-LAMBDA? 0)
    (init-op-slots! op)
    (when proc
      (vector-set! (oak-obj-v op) OP-LAMBDA? proc))
    op))

;;; What INITIALIZE does for operations in operations.oak: fresh setter
;;; and locater operations.
(define (init-op-slots! op)
  (let ((type (vector-ref (oak-obj-v op) 0)))
    (vector-set! (oak-obj-v op) OP-LAMBDA? 0)
    (when (oak-true? (oak-subtype? type *settable-operation*))
      (vector-set! (oak-obj-v op)
		   (+ (bp-offset type *settable-operation*)
		      (ivar-index *settable-operation* 'THE-SETTER))
		   (make-native-op *operation* #f)))
    (when (oak-true? (oak-subtype? type *locatable-operation*))
      (vector-set! (oak-obj-v op)
		   (+ (bp-offset type *locatable-operation*)
		      (ivar-index *locatable-operation* 'THE-LOCATER))
		   (make-native-op *operation* #f)))
    op))

(define (op-setter op)
  (let ((type (vector-ref (oak-obj-v op) 0)))
    (unless (oak-true? (oak-subtype? type *settable-operation*))
      (oak-host-error "SETTER of a non-settable operation ~S" (oak-describe op)))
    (let ((s (vector-ref (oak-obj-v op)
			 (+ (bp-offset type *settable-operation*)
			    (ivar-index *settable-operation* 'THE-SETTER)))))
      (unless (oak-obj? s)
	(oak-host-error "SETTER of ~A is ~A (bp ~A, layout ~A)" (oak-describe op) (oak-describe s)
			(bp-offset type *settable-operation*)
			(map (lambda (p) (cons (oak-describe (car p)) (cdr p))) (type-bp-alist type))))
      s)))

(define (op-locater op)
  (let ((type (vector-ref (oak-obj-v op) 0)))
    (unless (oak-true? (oak-subtype? type *locatable-operation*))
      (oak-host-error "LOCATER of a non-locatable operation ~S" (oak-describe op)))
    (vector-ref (oak-obj-v op)
		(+ (bp-offset type *locatable-operation*)
		   (ivar-index *locatable-operation* 'THE-LOCATER)))))

;;; Lock an operation to a native implementation: every call goes to
;;; PROC (which takes (bp . args)), and later ADD-METHODs are ignored.
(define (lock-native! op proc)
  (unless (oak-obj? op)
    (oak-host-error "lock-native! of a non-operation ~S" (oak-describe op)))
  (set-opinfo-native! (op-info op) proc)
  op)

;;; Method installation, following kernel1-install.oak.
(define (oak-add-method! op type proc)
  (unless (oak-op? op)
    (oak-host-error "ADD-METHOD to non-operation ~S" (oak-describe op)))
  (unless (and (oak-obj? type)
	       (oak-true? (oak-subtype? (vector-ref (oak-obj-v type) 0) *type*)))
    (oak-host-error "ADD-METHOD to non-type ~S" (oak-describe type)))
  (let ((info (op-info op)))
    (cond ((opinfo-native info) op)     ; locked: keep the native version
	  (else
	   (let ((v (oak-obj-v op)))
	     (bump-method-epoch!)
	     (cond ((and (eq? type *object*) (oak-true? (vector-ref v OP-LAMBDA?)))
		    (vector-set! v OP-LAMBDA? proc))
		   (else
		    (let ((l (vector-ref v OP-LAMBDA?)))
		      (when (and (oak-true? l) (not (eqv? l 0)))
			;; Toss it onto OBJECT's alist.
			(let ((ov (oak-obj-v *object*)))
			  (vector-set! ov TYPE-OP-METH-ALIST
				       (cons (cons op l)
					     (vector-ref ov TYPE-OP-METH-ALIST))))))
		    (vector-set! v OP-LAMBDA? '())
		    (let* ((tv (oak-obj-v type))
			   (alist (vector-ref tv TYPE-OP-METH-ALIST))
			   (ass (assq op alist)))
		      (if ass
			  (set-cdr! ass proc)
			  (vector-set! tv TYPE-OP-METH-ALIST
				       (cons (cons op proc) alist)))))))
	   op))))

;;; Depth first search of the type graph, in the emulator's order.
;;; Returns (proc . meth-type) or #f.
(define (find-method op type)
  (let ((ass (assq op (vector-ref (oak-obj-v type) TYPE-OP-METH-ALIST))))
    (if ass
	(cons (cdr ass) type)
	(let loop ((supers (vector-ref (oak-obj-v type) TYPE-SUPERS)))
	  (if (null? supers)
	      #f
	      (or (find-method op (car supers))
		  (loop (cdr supers))))))))

(define (lookup-method op self-type)
  ;; -> (proc . bp) or #f, cached per op.
  (let* ((info (op-info op))
	 (cache (opinfo-cache info)))
    (unless (= (opinfo-epoch info) *method-epoch*)
      (hash-clear! cache)
      (set-opinfo-epoch! info *method-epoch*))
    (let ((hit (hashq-ref cache self-type)))
      (or hit
	  (let ((found (find-method op self-type)))
	    (and found
		 (let ((entry (cons (car found) (bp-offset self-type (cdr found)))))
		   (hashq-set! cache self-type entry)
		   entry)))))))

(define (no-method-error op args)
  (or (open-code-fallback op args)
      (oak-host-error "No method for ~A with args ~A"
		      (oak-describe op)
		      (map oak-describe args))))

;;; Operations such as (%SLOT 3) exist only to be open coded, and have
;;; no methods.  When the host calls one, interpret its byte code list
;;; for the few instructions such operations use.
(define *open-coded-mixin* #f)       ; the world's OPEN-CODED-MIXIN type
(define *registers* (make-hash-table))

(define (open-coded-byte-code op)
  (and *open-coded-mixin*
       (oak-obj? op)
       (let ((type (vector-ref (oak-obj-v op) 0)))
	 (and (oak-true? (oak-subtype? type *open-coded-mixin*))
	      (vector-ref (oak-obj-v op)
			  (+ (bp-offset type *open-coded-mixin*)
			     (ivar-index *open-coded-mixin* 'BYTE-CODE-LIST)))))))

(define (open-code-fallback op args)
  (let ((code (open-coded-byte-code op)))
    (and (pair? code) (null? (cdr code)) (pair? (car code))
	 (let ((instr (car code)))
	   (case (car instr)
	     ((LOAD-SLOT) (and (= (length args) 1)
			       (%slot-ref (car args) (cadr instr))))
	     ((STORE-SLOT) (and (= (length args) 2)
				(%slot-set! (car args) (cadr instr) (cadr args))))
	     ((LOCATE-SLOT) (and (= (length args) 1)
				 (let ((obj (car args)) (n (cadr instr)))
				   (make-loc (lambda () (%slot-ref obj n))
					     (lambda (x) (%slot-set! obj n x))))))
	     ((LOAD-REG) (and (null? args)
			      (or (hashq-ref *registers* (cadr instr)) '())))
	     ((STORE-REG) (and (= (length args) 1)
			       (begin (hashq-set! *registers* (cadr instr) (car args))
				      (car args))))
	     ((HALT) (format (current-error-port) "~%((%HALT ~A))~%" (cadr instr))
	      (exit (cadr instr)))
	     (else #f))))))

;;; Calling an operation.  Fixed arities avoid consing.
(define oak-call
  (case-lambda
    ((op)
     (if (oak-obj? op)
	 (let ((n (and (oak-obj-aux op) (opinfo-native (oak-obj-aux op)))))
	   (cond (n (n 0))
		 ((procedure? (vector-ref (oak-obj-v op) OP-LAMBDA?))
		  ((vector-ref (oak-obj-v op) OP-LAMBDA?) 0))
		 (else (no-method-error op '()))))
	 (bad-call op '())))
    ((op a)
     (if (oak-obj? op)
	 (let ((n (and (oak-obj-aux op) (opinfo-native (oak-obj-aux op)))))
	   (cond (n (n 0 a))
		 ((procedure? (vector-ref (oak-obj-v op) OP-LAMBDA?))
		  ((vector-ref (oak-obj-v op) OP-LAMBDA?) 0 a))
		 (else
		  (let ((m (lookup-method op (oak-get-type a))))
		    (if m ((car m) (cdr m) a) (no-method-error op (list a)))))))
	 (bad-call op (list a))))
    ((op a b)
     (if (oak-obj? op)
	 (let ((n (and (oak-obj-aux op) (opinfo-native (oak-obj-aux op)))))
	   (cond (n (n 0 a b))
		 ((procedure? (vector-ref (oak-obj-v op) OP-LAMBDA?))
		  ((vector-ref (oak-obj-v op) OP-LAMBDA?) 0 a b))
		 (else
		  (let ((m (lookup-method op (oak-get-type a))))
		    (if m ((car m) (cdr m) a b) (no-method-error op (list a b)))))))
	 (bad-call op (list a b))))
    ((op a b c)
     (if (oak-obj? op)
	 (let ((n (and (oak-obj-aux op) (opinfo-native (oak-obj-aux op)))))
	   (cond (n (n 0 a b c))
		 ((procedure? (vector-ref (oak-obj-v op) OP-LAMBDA?))
		  ((vector-ref (oak-obj-v op) OP-LAMBDA?) 0 a b c))
		 (else
		  (let ((m (lookup-method op (oak-get-type a))))
		    (if m ((car m) (cdr m) a b c) (no-method-error op (list a b c)))))))
	 (bad-call op (list a b c))))
    ((op a . rest)
     (if (oak-obj? op)
	 (let ((n (and (oak-obj-aux op) (opinfo-native (oak-obj-aux op)))))
	   (cond (n (apply n 0 a rest))
		 ((procedure? (vector-ref (oak-obj-v op) OP-LAMBDA?))
		  (apply (vector-ref (oak-obj-v op) OP-LAMBDA?) 0 a rest))
		 (else
		  (let ((m (lookup-method op (oak-get-type a))))
		    (if m (apply (car m) (cdr m) a rest)
			(no-method-error op (cons a rest)))))))
	 (bad-call op (cons a rest))))))

(define (bad-call op args)
  (oak-host-error "Call to ~A, which isn't an operation, with args ~A"
		  (oak-describe op) (map oak-describe args)))

(define (oak-apply op args) (apply oak-call op args))

;;; (^SUPER type op self . args): search from TYPE.
(define (oak-super type op self . args)
  (let ((found (find-method op type)))
    (if found
	(apply (car found) (bp-offset (oak-get-type self) (cdr found)) self args)
	(oak-host-error "No ^SUPER method for ~A in ~A"
			(oak-describe op) (oak-describe type)))))

;;;==========================================================================
;;; Global variables (before locales exist) and native overrides
;;;==========================================================================

;;; Native overrides: when the world assigns a global named NAME, the
;;; hook is called with the new value and its result is stored.
(define *global-hooks* (make-hash-table))
(define (on-define! name proc) (hashq-set! *global-hooks* name proc))

(define (run-global-hook name value)
  (let ((h (hashq-ref *global-hooks* name)))
    (if h (h value) value)))

;;; Assignment through a global cell loc, running hooks.
(define (oak-global-set! loc value)
  (let* ((name (oak-loc-name loc))
	 (value (if name (run-global-hook name value) value)))
    ((oak-loc-set loc) value)
    value))

;;;==========================================================================
;;; Fluids
;;;==========================================================================
;;; The binding list lives in the global FLUID-BINDING-LIST once
;;; fluid.oak is loaded; until then in a host variable.  These are the
;;; hooks the world's own code goes through.

(define *fluid-binding-list-cell* #f)   ; set once the global exists
(define *host-fluid-bindings* (list (cons '() '())))

(define (fluid-bindings)
  (if *fluid-binding-list-cell*
      (variable-ref *fluid-binding-list-cell*)
      *host-fluid-bindings*))

(define (set-fluid-bindings! l)
  (if *fluid-binding-list-cell*
      (variable-set! *fluid-binding-list-cell* l)
      (set! *host-fluid-bindings* l)))

(define (fluid-ref sym)
  (let ((p (assq sym (fluid-bindings))))
    (if p (cdr p)
	(oak-host-error "(FLUID ~A) not found." sym))))

(define (fluid-bound? sym)
  (and (assq sym (fluid-bindings)) #t))

(define (fluid-set! sym val)
  (let ((p (assq sym (fluid-bindings))))
    (if p
	(set-cdr! p val)
	;; Add after the head of the top level list so every dynamic
	;; extent sees it, as ADD-TO-CURRENT-FLUID-BINDINGS does.
	(let ((top (let loop ((l (fluid-bindings)))
		     (if (null? (cdr l)) l (loop (cdr l))))))
	  (set-cdr! top (cons (cons sym val) (cdr top)))))
    val))

;;; Host side dynamic binding of fluids.
(define (with-fluids* bindings thunk)
  (let* ((old (fluid-bindings))
	 (new (append bindings old)))
    (dynamic-wind
      (lambda () (set-fluid-bindings! new))
      thunk
      (lambda ()
	;; If the world replaced the whole list meanwhile (as
	;; REVERT-FLUID-BINDING-LIST does) keep its list.
	(when (eq? (fluid-bindings) new)
	  (set-fluid-bindings! old))))))

;;;==========================================================================
;;; Catch and throw
;;;==========================================================================
;;; (%CATCH expr) is compiled by native-catch as
;;;   (%catch (let ((v (%filltag (%allocate escape-object 5)))) ...))
;;; so the %FILLTAG that follows a %CATCH gets the tag of that catch.

(define *pending-catch-tag* #f)

(define (oak-catch thunk)
  (let ((tag (gensym "oak-catch-")))
    (set! *pending-catch-tag* tag)
    (catch tag
      thunk
      (lambda (t value) value))))

(define *wind-count-cell* #f)   ; the global %WIND-COUNT, once it exists

(define (oak-filltag escape-object)
  (let ((tag *pending-catch-tag*))
    (set! *pending-catch-tag* #f)
    (unless tag (oak-host-error "%FILLTAG without a pending %CATCH"))
    (set-oak-obj-aux! escape-object (cons tag (fluid-bindings)))
    ;; What NATIVE-CATCH stores after the filltag; done here so the
    ;; bootstrap NATIVE-CATCH agrees with the real one.
    (let ((v (oak-obj-v escape-object)))
      (when (> (vector-length v) 4)
	(vector-set! v 3 (if *wind-count-cell* (variable-ref *wind-count-cell*) 0))
	(vector-set! v 4 (fluid-bindings))))
    escape-object))

(define (oak-throw escape-object value)
  (let ((info (and (oak-obj? escape-object) (oak-obj-aux escape-object))))
    (unless (and (pair? info) (symbol? (car info)))
      (oak-host-error "%THROW to a non-escape-object ~S" (oak-describe escape-object)))
    (set-fluid-bindings! (cdr info))
    (throw (car info) value)))

;;;==========================================================================
;;; Locatives to ivars and to slots
;;;==========================================================================

(define (make-slot-loc obj index)
  (let ((v (oak-obj-v obj)))
    (make-loc (lambda () (vector-ref v index))
	      (lambda (x) (vector-set! v index x)))))

;;;==========================================================================
;;; Immediate tag hacks: %CRUNCH, %DATA, %TAG
;;;==========================================================================
;;; Characters are immediates with subtag 0: data = code << 6, tag 1.

(define (oak-%tag x)
  (cond ((exact-integer? x) 0)
	((or (char? x) (eq? x #t)) 1)
	((oak-loc? x) 2)
	(else 3)))

(define (oak-%data x)
  (cond ((exact-integer? x) x)
	((char? x) (ash (char->integer x) 6))
	(else (oak-host-error "%DATA of ~S" (oak-describe x)))))

(define (oak-%crunch data tag)
  (cond ((= tag 0) data)
	((= tag 1)
	 (if (= (logand data 63) 0)
	     (integer->char (ash data -6))
	     (oak-host-error "%CRUNCH of non-character immediate ~A" data)))
	(else (oak-host-error "%CRUNCH with tag ~A" tag))))

;;;==========================================================================
;;; Object hash (weak pointer numbers) for ~! printing
;;;==========================================================================

(define *object-hash-table* (make-weak-key-hash-table))
(define *object-hash-counter* 0)

(define (oak-object-hash x)
  (or (hashq-ref *object-hash-table* x)
      (begin (set! *object-hash-counter* (+ *object-hash-counter* 1))
	     (hashq-set! *object-hash-table* x *object-hash-counter*)
	     *object-hash-counter*)))

;;;==========================================================================
;;; Describing objects for error messages
;;;==========================================================================

(define *type-names* (make-hash-table))   ; type object -> name symbol

(define (oak-describe x)
  (cond ((oak-obj? x)
	 (let* ((type (vector-ref (oak-obj-v x) 0))
		(tname (hashq-ref *type-names* type))
		(own (hashq-ref *type-names* x)))
	   (cond (own (format #f "#<~A ~A>" (or tname "Object") own))
		 ((eq? tname 'UNDEFINED)
		  (format #f "#<Undefined ~A>" (vector-ref (oak-obj-v x) 1)))
		 (else (format #f "#<~A ~A>" (or tname "Object") (oak-object-hash x))))))
	((oak-loc? x) "#<Loc>")
	((null? x) "()")
	((pair? x) (format #f "(~A . ~A)" (oak-describe (car x)) (oak-describe (cdr x))))
	(else (format #f "~S" x))))

(define (note-type-name! type name)
  (hashq-set! *type-names* type name))
