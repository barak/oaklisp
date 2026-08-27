;;; This file is part of Oaklisp.
;;;
;;; runtime.scm -- the native part of the Guile-hosted Oaklisp: the
;;; world builder's types, the proto locale, and the primitives and
;;; standard operations that the world's own sources either cannot
;;; provide here (they are written against the emulator) or must exist
;;; before those sources can be loaded.
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

;;; Method procedures take the base pointer first; NLAMBDA hides it.
(define-syntax nlambda
  (syntax-rules ()
    ((_ (f ...) b ...) (lambda (bp% f ...) b ...))
    ((_ (f ... . r) b ...) (lambda (bp% f ... . r) b ...))
    ((_ r b ...) (lambda (bp% . r) b ...))))

;;;==========================================================================
;;; The world builder's types (kernel0.oak, kernel0types.oak)
;;;==========================================================================

(define *unbound-marker* (list 'unbound))
(define *self-evaluatory-mixin* #f)

;;; A coercable type as kernel.oak's INITIALIZE for COERCABLE-TYPE would
;;; make it: laid out, with a coercer that is the identity on its own
;;; instances.
(define (make-coercable-type ivars supers)
  (let ((t (raw-type *coercable-type* 10)))
    (type-initialize! t ivars supers)
    (let ((co-op (make-native-op *operation* #f)))
      (oak-add-method! co-op t (nlambda (self) self))
      (vector-set! (oak-obj-v t)
		   (+ (bp-offset *coercable-type* *coercable-type*)
		      (ivar-index *coercable-type* 'CO-OP))
		   co-op))
    t))

(define (kernel-bootstrap-types!)
  ;; TYPE
  (let ((type0 (%make-oak-obj (make-vector TYPE-SIZE '()) #f)))
    (let ((v (oak-obj-v type0)))
      (vector-set! v 0 type0)
      (vector-set! v TYPE-INSTANCE-LENGTH 9)
      (vector-set! v TYPE-VAR-LEN? '())
      (vector-set! v TYPE-IVAR-LIST
		   '(INSTANCE-LENGTH VARIABLE-LENGTH? SUPERTYPE-LIST IVAR-LIST
		     IVAR-COUNT TYPE-BP-ALIST OPERATION-METHOD-ALIST TOP-WIRED?))
      (vector-set! v TYPE-IVAR-COUNT 8)
      (vector-set! v TYPE-BP-ALIST '())
      (vector-set! v TYPE-OP-METH-ALIST '())
      (vector-set! v TYPE-TOP-WIRED? #t))
    (set! *type* type0))
  ;; OBJECT
  (let ((object0 (%allocate *type* TYPE-SIZE)))
    (let ((v (oak-obj-v object0)))
      (vector-set! v TYPE-INSTANCE-LENGTH 1)
      (vector-set! v TYPE-VAR-LEN? '())
      (vector-set! v TYPE-SUPERS '())
      (vector-set! v TYPE-IVAR-LIST '())
      (vector-set! v TYPE-IVAR-COUNT 0)
      (vector-set! v TYPE-BP-ALIST (list (cons object0 1)))
      (vector-set! v TYPE-OP-METH-ALIST '())
      (vector-set! v TYPE-TOP-WIRED? '()))
    (set! *object* object0))
  (vector-set! (oak-obj-v *type*) TYPE-SUPERS (list *object*))
  (vector-set! (oak-obj-v *type*) TYPE-BP-ALIST (list (cons *type* 1) (cons *object* 9)))
  ;; OPERATION
  (let ((operation0 (%allocate *type* TYPE-SIZE)))
    (let ((v (oak-obj-v operation0)))
      (vector-set! v TYPE-INSTANCE-LENGTH 5)
      (vector-set! v TYPE-VAR-LEN? '())
      (vector-set! v TYPE-SUPERS (list *object*))
      (vector-set! v TYPE-IVAR-LIST '(LAMBDA? CACHE-TYPE CACHE-METHOD CACHE-TYPE-OFFSET))
      (vector-set! v TYPE-IVAR-COUNT 4)
      (vector-set! v TYPE-BP-ALIST (list (cons operation0 1) (cons *object* 0)))
      (vector-set! v TYPE-OP-METH-ALIST '())
      (vector-set! v TYPE-TOP-WIRED? #t))
    (set! *operation* operation0))
  (note-type-name! *type* 'TYPE)
  (note-type-name! *object* 'OBJECT)
  (note-type-name! *operation* 'OPERATION)
  ;; The operation subtypes, as operations.oak will define them.
  (set! *settable-operation* (make-type '(THE-SETTER) (list *operation*)))
  (set! *locatable-operation* (make-type '(THE-LOCATER) (list *settable-operation*)))
  (note-type-name! *settable-operation* 'SETTABLE-OPERATION)
  (note-type-name! *locatable-operation* 'LOCATABLE-OPERATION)
  ;; COERCABLE-TYPE as kernel.oak defines it.
  (set! *coercable-type* (make-type '(CO-OP) (list *type*)))
  (note-type-name! *coercable-type* 'COERCABLE-TYPE)
  ;; The crude types of kernel0types.oak.
  (define (sort-of-init name)
    ;; Allocated with room for a coercable type's CO-OP slot, since
    ;; kernel.oak retypes STRING that way (COERCABLE_TYPE_SIZE).
    (let ((t (raw-type *type* 10)))
      (vector-set! (oak-obj-v t) TYPE-SUPERS (list *object*))
      (vector-set! (oak-obj-v t) TYPE-BP-ALIST (list (cons t 1)))
      (note-type-name! t name)
      t))
  (set! *%code-vector* (sort-of-init '%CODE-VECTOR))
  (set! *cons-pair* (sort-of-init 'CONS-PAIR))
  (set! *string* (sort-of-init 'STRING))
  (set! *%method* (sort-of-init '%METHOD))
  (set! *%closed-environment* (sort-of-init '%CLOSED-ENVIRONMENT))
  (set! *null-type* (sort-of-init 'NULL-TYPE))
  (set! *fixnum* (sort-of-init 'FIXNUM))
  (set! *locative* (sort-of-init 'LOCATIVE))
  ;; Types the world defines with DEFINE-INSTANCE, made here exactly as
  ;; it will so that it keeps these objects (methods on symbols exist
  ;; before kernel.oak defines SYMBOL).
  (set! *self-evaluatory-mixin* (make-type '() '()))
  (note-type-name! *self-evaluatory-mixin* 'SELF-EVALUATORY-MIXIN)
  (set! *symbol* (make-coercable-type '(PRINT-NAME) (list *object*)))
  (note-type-name! *symbol* 'SYMBOL)
  (set! *character* (make-coercable-type '() (list *self-evaluatory-mixin* *object*)))
  (note-type-name! *character* 'CHARACTER)
  ;; Placeholders the world redefines; hooks keep the host in step.
  (set! *simple-vector* (sort-of-init 'SIMPLE-VECTOR))
  (set! *truths* (sort-of-init 'TRUTHS))
  (set! *proto-locale-type* (sort-of-init 'PROTO-LOCALE)))

;;;==========================================================================
;;; The proto locale
;;;==========================================================================
;;; Holds globals and macros until locales.oak and make-locales.oak have
;;; been loaded, when its contents move to SYSTEM-LOCALE.

(define-record-type <proto-locale-data>
  (make-proto-locale-data vars macros frozen)
  proto-locale-data?
  (vars proto-vars)
  (macros proto-macros)
  (frozen proto-frozen set-proto-frozen!))

(define (proto-data loc) (oak-obj-aux loc))

(define (make-proto-locale)
  (let ((loc (%allocate *proto-locale-type* 1)))
    (set-oak-obj-aux! loc (make-proto-locale-data (make-hash-table) (make-hash-table) '()))
    loc))

;;; Globals: define or assign in the proto locale (used by the host
;;; before the world's locales exist and to install natives).

(define (defglobal! name value)
  (oak-global-set! (resolve-global *proto-locale* name) value))

(define (global-ref name)
  (global-value (current-locale) name))

;;;==========================================================================
;;; Native operations
;;;==========================================================================

;;; Define global NAME as a native operation of TYPE whose every call
;;; runs PROC (an NLAMBDA); redefinitions by the world are locked to
;;; the same PROC.
(define (native-locked! name type proc)
  (let ((op (make-native-op type #f)))
    (lock-native! op proc)
    (on-define! name (lambda (v)
		       (when (oak-op? v) (lock-native! v proc))
		       v))
    (defglobal! name op)
    op))

;;; A plain native lambda that the world may later redefine.
(define (native-fn! name proc)
  (defglobal! name (make-native-op *operation* proc)))

;;; An operation object that define-instance will keep, initially with
;;; no methods.
(define (native-op! name type)
  (defglobal! name (make-native-op type #f)))

(define (native-method! op-name type proc)
  (let ((op (global-ref op-name)))
    (unless (oak-op? op)
      (oak-host-error "native-method!: ~A is not an operation" op-name))
    (oak-add-method! op type proc)))

(define (native-setter-method! op-name type proc)
  (oak-add-method! (op-setter (global-ref op-name)) type proc))

(define (native-locater-method! op-name type proc)
  (oak-add-method! (op-locater (global-ref op-name)) type proc))

;;;==========================================================================
;;; The proto locale's methods
;;;==========================================================================

(define (install-locale-ops!)
  (let ((variable? (make-native-op *settable-operation* #f))
	(variable-here? (make-native-op *settable-operation* #f))
	(macro? (make-native-op *settable-operation* #f))
	(macro-here? (make-native-op *settable-operation* #f))
	(frozen? (make-native-op *settable-operation* #f))
	(frozen-here? (make-native-op *settable-operation* #f))
	(find-locale (make-native-op *operation* #f))
	(pt *proto-locale-type*))
    (set! op-variable? variable?)
    (set! op-variable-here? variable-here?)
    (set! op-macro? macro?)
    (set! op-macro-here? macro-here?)
    (define (lookup-var loc sym)
      (or (hashq-ref (proto-vars (proto-data loc)) sym) '()))
    (define (set-var! loc sym loci)
      (cond ((null? loci)
	     (hashq-remove! (proto-vars (proto-data loc)) sym)
	     '())
	    (else
	     (let ((cell (if (eq? loci #t) (make-cell-loc (list 'unbound sym)) loci)))
	       (hashq-set! (proto-vars (proto-data loc)) sym cell)
	       cell))))
    (oak-add-method! variable? pt (nlambda (loc sym) (lookup-var loc sym)))
    (oak-add-method! (op-setter variable?) pt (nlambda (loc sym loci) (set-var! loc sym loci)))
    (oak-add-method! variable-here? pt (nlambda (loc sym) (lookup-var loc sym)))
    (oak-add-method! (op-setter variable-here?) pt
		     (nlambda (loc sym loci) (set-var! loc sym loci)))
    (define (lookup-macro loc sym)
      (or (hashq-ref (proto-macros (proto-data loc)) sym) '()))
    (define (set-macro! loc sym expander)
      (if (null? expander)
	  (hashq-remove! (proto-macros (proto-data loc)) sym)
	  (hashq-set! (proto-macros (proto-data loc)) sym expander))
      expander)
    (oak-add-method! macro? pt (nlambda (loc sym) (lookup-macro loc sym)))
    (oak-add-method! (op-setter macro?) pt (nlambda (loc sym e) (set-macro! loc sym e)))
    (oak-add-method! macro-here? pt (nlambda (loc sym) (lookup-macro loc sym)))
    (oak-add-method! (op-setter macro-here?) pt (nlambda (loc sym e) (set-macro! loc sym e)))
    (define (frozen loc sym)
      (oak-bool (memq sym (proto-frozen (proto-data loc)))))
    (define (set-frozen! loc sym new)
      (let ((d (proto-data loc)))
	(if (oak-true? new)
	    (unless (memq sym (proto-frozen d))
	      (set-proto-frozen! d (cons sym (proto-frozen d))))
	    (set-proto-frozen! d (delq sym (proto-frozen d))))
	new))
    (oak-add-method! frozen? pt (nlambda (loc sym) (frozen loc sym)))
    (oak-add-method! (op-setter frozen?) pt (nlambda (loc sym n) (set-frozen! loc sym n)))
    (oak-add-method! frozen-here? pt (nlambda (loc sym) (frozen loc sym)))
    (oak-add-method! (op-setter frozen-here?) pt (nlambda (loc sym n) (set-frozen! loc sym n)))
    (oak-add-method! find-locale pt
		     (nlambda (loc sym pred)
		       (if (oak-true? (oak-call pred loc sym)) loc '())))
    ;; Now that VARIABLE? works, the globals can be defined.
    (defglobal! 'VARIABLE? variable?)
    (defglobal! 'VARIABLE-HERE? variable-here?)
    (defglobal! 'MACRO? macro?)
    (defglobal! 'MACRO-HERE? macro-here?)
    (defglobal! 'FROZEN? frozen?)
    (defglobal! 'FROZEN-HERE? frozen-here?)
    (defglobal! 'FIND-LOCALE find-locale)))

;;; Move everything from the proto locale into a real locale, using the
;;; world's own (SETTER VARIABLE-HERE?) etc.
(define (migrate-proto-locale! target)
  (let ((d (proto-data *proto-locale*)))
    (hash-for-each (lambda (sym cell)
		     (oak-call (op-setter op-variable-here?) target sym cell))
		   (proto-vars d))
    (hash-for-each (lambda (sym expander)
		     (oak-call (op-setter op-macro-here?) target sym expander))
		   (proto-macros d))
    (for-each (lambda (sym)
		(oak-call (op-setter (global-ref 'FROZEN-HERE?)) target sym #t))
	      (reverse (proto-frozen d)))))

;;;==========================================================================
;;; Kernel natives
;;;==========================================================================

(define (as-list-of-types x)
  (let loop ((l x) (acc '()))
    (if (pair? l) (loop (cdr l) (cons (car l) acc)) (reverse acc))))

(define (host-make type . args)
  ;; MAKE before kernel1-make.oak is loaded.
  (cond ((oak-true? (oak-subtype? type *variable-length-mixin-or-nothing*))
	 (apply host-varlen-make type args))
	(else
	 (let ((obj (%allocate type (type-instance-length type))))
	   (apply oak-call (global-ref 'INITIALIZE) obj args)))))

(define *variable-length-mixin-or-nothing* #f)

(define (host-varlen-make type ncells . args)
  (let ((obj (%varlen-allocate type (+ (type-instance-length type) ncells))))
    (apply oak-call (global-ref 'INITIALIZE) obj ncells args)))

(define (%varlen-allocate type total)
  (cond ((eq? type *simple-vector*)
	 (make-vector (- total 2) '()))
	(else
	 (let ((obj (%allocate type total)))
	   (vector-set! (oak-obj-v obj) 1 total)
	   obj))))

;;; (%SLOT n): a locatable operation on raw slots, cached per n.
(define *slot-ops* (make-hash-table))
(define (slot-op n)
  (or (hashv-ref *slot-ops* n)
      (let ((op (make-native-op *locatable-operation* #f)))
	(lock-native! op (nlambda (obj) (%slot-ref obj n)))
	(lock-native! (op-setter op) (nlambda (obj val) (%slot-set! obj n val)))
	(lock-native! (op-locater op)
		      (nlambda (obj)
			(make-loc (lambda () (%slot-ref obj n))
				  (lambda (x) (%slot-set! obj n x)))))
	(hashv-set! *slot-ops* n op)
	op)))

;;; (%REGISTER 'name): settable operations on a scratch register file.
(define *register-ops* (make-hash-table))
(define (register-op name)
  (or (hashq-ref *register-ops* name)
      (let ((op (make-native-op *settable-operation* #f)))
	(lock-native! op (nlambda () (or (hashq-ref *registers* name) '())))
	(lock-native! (op-setter op) (nlambda (val) (hashq-set! *registers* name val) val))
	(hashq-set! *register-ops* name op)
	op)))

(define *halt-ops* (make-hash-table))
(define (halt-op status)
  (or (hashv-ref *halt-ops* status)
      (let ((op (make-native-op *operation* #f)))
	(lock-native! op (nlambda args
			   (format (current-error-port) "~%((%HALT ~A))~%" status)
			   (exit status)))
	(hashv-set! *halt-ops* status op)
	op)))

(define (coercable? type)
  (oak-true? (oak-subtype? (oak-get-type type) *coercable-type*)))

(define (type-co-op type)
  (unless (coercable? type)
    (oak-host-error "~A is not a coercable type" (oak-describe type)))
  (vector-ref (oak-obj-v type)
	      (+ (bp-offset (oak-get-type type) *coercable-type*)
		 (ivar-index *coercable-type* 'CO-OP))))

(define (the-setter-index op)
  (+ (bp-offset (oak-get-type op) *settable-operation*)
     (ivar-index *settable-operation* 'THE-SETTER)))

(define (the-locater-index op)
  (+ (bp-offset (oak-get-type op) *locatable-operation*)
     (ivar-index *locatable-operation* 'THE-LOCATER)))

(define (settable-op? op)
  (oak-true? (oak-subtype? (oak-get-type op) *settable-operation*)))
(define (locatable-op? op)
  (oak-true? (oak-subtype? (oak-get-type op) *locatable-operation*)))

;;; CONTENTS and its setter and locater are always native, even after
;;; the world replaces them: the world's own methods for them are
;;; written in terms of the open coded instructions.
(define (install-contents! op)
  (lock-native! op (nlambda (loc) (loc-contents loc)))
  (let ((s (vector-ref (oak-obj-v op) (the-setter-index op))))
    (when (oak-obj? s)
      (lock-native! s (nlambda (loc v) (oak-global-set! loc v)))))
  (let ((l (vector-ref (oak-obj-v op) (the-locater-index op))))
    (when (oak-obj? l)
      (lock-native! l (nlambda (loc) loc))))
  op)

;;; (SET! (SETTER op) new) and (SET! (LOCATER op) new).
(define (set-the-setter! op new)
  (vector-set! (oak-obj-v op) (the-setter-index op) new)
  (when (eq? op *contents-op*) (install-contents! op))
  new)

(define (set-the-locater! op new)
  (vector-set! (oak-obj-v op) (the-locater-index op) new)
  (when (eq? op *contents-op*) (install-contents! op))
  new)

;;; The SETTER operation: native, with its own setter and locater.
(define (install-setter-op! v)
  (lock-native! v (nlambda (op) (op-setter op)))
  (when (settable-op? v)
    (let ((s (vector-ref (oak-obj-v v) (the-setter-index v))))
      (when (oak-obj? s)
	(lock-native! s (nlambda (op new) (set-the-setter! op new))))))
  (when (locatable-op? v)
    (let ((l (vector-ref (oak-obj-v v) (the-locater-index v))))
      (when (oak-obj? l)
	(lock-native! l (nlambda (op) (make-slot-loc op (the-setter-index op)))))))
  v)

(define (install-locater-op! v)
  (lock-native! v (nlambda (op) (op-locater op)))
  (when (settable-op? v)
    (let ((s (vector-ref (oak-obj-v v) (the-setter-index v))))
      (when (oak-obj? s)
	(lock-native! s (nlambda (op new) (set-the-locater! op new))))))
  (when (locatable-op? v)
    (let ((l (vector-ref (oak-obj-v v) (the-locater-index v))))
      (when (oak-obj? l)
	(lock-native! l (nlambda (op) (make-slot-loc op (the-locater-index op)))))))
  v)

(define (install-kernel-natives!)
  (native-locked! '%ALLOCATE *operation* (nlambda (type n) (%allocate type n)))
  (native-locked! '%VARLEN-ALLOCATE *operation* (nlambda (type n) (%varlen-allocate type n)))
  ;; Until subprimitive.oak defines the real ones, whose products are
  ;; open coded operations the fallback in oak-call interprets.
  (native-fn! '%SLOT (nlambda (n) (slot-op n)))
  (native-fn! '%REGISTER (nlambda (name) (register-op name)))
  (native-fn! '%HALT (nlambda (status) (halt-op status)))
  (on-define! 'OPEN-CODED-MIXIN (lambda (v) (when (oak-obj? v) (set! *open-coded-mixin* v)) v))
  (native-locked! 'GET-TYPE *operation* (nlambda (x) (oak-get-type x)))
  (native-locked! '%TAG *operation* (nlambda (x) (oak-%tag x)))
  (native-locked! '%DATA *operation* (nlambda (x) (oak-%data x)))
  (native-locked! '%POINTER *operation* (nlambda (x) (oak-%data x)))
  (native-locked! '%CRUNCH *operation* (nlambda (d t) (oak-%crunch d t)))
  (native-locked! '%ASSQ *operation* (nlambda (k l) (or (assq k l) '())))
  (native-locked! '%WRITE-CHAR *operation*
		  (nlambda (c) (write-char c (current-error-port)) c))
  (native-locked! '%READ-CHAR *operation* (nlambda () '()))
  (native-locked! '%BIG-ENDIAN? *operation* (nlambda () '()))
  (native-locked! '%LOAD-PROCESS *operation* (nlambda () 0))
  (native-locked! 'ACQUIRE-MUTEX *operation* (nlambda (m) m))
  (native-locked! 'RELEASE-MUTEX *operation* (nlambda (m) m))
  (native-locked! 'GET-TIME *operation*
		  (nlambda () (let ((t (gettimeofday))) (+ (* 1000 (car t)) (quotient (cdr t) 1000)))))
  (native-locked! '%MAKE-CELL *operation* (nlambda (v) (make-cell-loc v)))
  (native-locked! 'OBJECT-HASH *operation* (nlambda (x) (oak-object-hash x)))
  (native-locked! 'OBJECT-UNHASH *operation*
		  (nlambda (n) (oak-host-error "OBJECT-UNHASH not supported")))
  (native-locked! '%THROW *operation* (nlambda (tag val) (oak-throw tag val)))
  (native-locked! '%FILLTAG *operation* (nlambda (e) (oak-filltag e)))
  (native-locked! '%RETURN *operation* (nlambda () '()))
  (native-locked! 'LISTIFY-ARGS *operation* (nlambda (op . rest) (oak-call op rest)))
  (native-locked! 'BACKWARDS-LISTIFY-ARGS *operation*
		  (nlambda (op . rest) (oak-call op (reverse rest))))
  (native-locked! 'CONSUME-ARGS *operation* (nlambda (val . rest) val))
  (native-locked! 'APPLY *operation*
		  (nlambda (op . args)
		    (let loop ((a args) (acc '()))
		      (if (null? (cdr a))
			  (apply oak-call op (append (reverse acc) (car a)))
			  (loop (cdr a) (cons (car a) acc))))))
  (native-locked! '^SUPER *operation* (nlambda (type op self . args)
				       (apply oak-super type op self args)))
  (native-locked! 'SUBTYPE? *operation* (nlambda (t s) (oak-subtype? t s)))
  (native-locked! 'IS-A? *operation* (nlambda (o t) (oak-is-a? o t)))
  (native-locked! 'EQ? *operation* (nlambda (a b) (oak-bool (eq? a b))))
  (native-locked! 'NULL? *operation* (nlambda (a) (oak-bool (null? a))))
  (native-locked! 'NOT *operation* (nlambda (a) (oak-bool (null? a))))
  (native-locked! 'IDENTITY *operation* (nlambda (a) a))
  (native-locked! 'SECOND-ARG *operation* (nlambda (a b . rest) b))
  (native-locked! '%PUSH *operation* (nlambda args 0))
  (native-locked! 'SETUP-TAG-TRAPS *operation* (nlambda () '()))
  (native-locked! 'EXIT *operation*
		  (nlambda (status . rest)
		    (when (pair? rest)
		      (apply oak-format (current-error-port) rest))
		    (exit status)))
  ;; MAKE / INITIALIZE, replaced by kernel1-make.oak.
  (native-fn! 'MAKE (nlambda (type . args) (apply host-make type args)))
  (native-fn! '%VARLEN-MAKE (nlambda (type . args) (apply host-varlen-make type args)))
  (native-op! 'INITIALIZE *operation*)
  (native-method! 'INITIALIZE *object* (nlambda (self) self))
  (native-method! 'INITIALIZE *type*
		  (nlambda (self ivars supers) (type-initialize! self ivars supers)))
  (native-method! 'INITIALIZE *operation*
		  (nlambda (self) (init-op-slots! self) self))
  (native-method! 'INITIALIZE *settable-operation*
		  (nlambda (self) (init-op-slots! self) self))
  (native-method! 'INITIALIZE *locatable-operation*
		  (nlambda (self) (init-op-slots! self) self))
  ;; The method installers of kernel1-install.oak; ADD-METHOD is a
  ;; special form here, but code-vector.oak adds a method to this one.
  (native-op! '%INSTALL-METHOD-WITH-ENV *operation*)
  (native-op! '%INSTALL-METHOD *operation*)
  (native-op! '%INSTALL-LAMBDA *operation*)
  (native-op! '%INSTALL-LAMBDA-WITH-ENV *operation*)
  ;; COERCER before coerce.oak (create-accessors uses #^symbol).
  (native-op! 'COERCER *settable-operation*)
  (native-method! 'COERCER *coercable-type* (nlambda (t) (type-co-op t)))
  (native-setter-method! 'COERCER *coercable-type*
			 (nlambda (t op)
			   (vector-set! (oak-obj-v t)
					(+ (bp-offset (oak-get-type t) *coercable-type*)
					   (ivar-index *coercable-type* 'CO-OP))
					op)
			   op))
  (native-locked! '%YOUR-TOP-WIRED *operation*
		  (nlambda (t) (vector-set! (oak-obj-v t) TYPE-TOP-WIRED? #t) t))
  (native-fn! '%LENGTH (nlambda (l) (length l)))
  (native-fn! '%MEMQ (nlambda (x l) (or (memq x l) '())))
  (native-fn! '%APPEND (nlambda (a b) (append a b)))
  ;; SETTER / LOCATER / CONTENTS
  (native-locked! 'SETTER *operation* (nlambda (op) (op-setter op)))
  (on-define! 'SETTER (lambda (v) (when (oak-op? v) (install-setter-op! v)) v))
  (native-locked! 'LOCATER *operation* (nlambda (op) (op-locater op)))
  (on-define! 'LOCATER (lambda (v) (when (oak-op? v) (install-locater-op! v)) v))
  (let ((contents (make-native-op *locatable-operation* #f)))
    (install-contents! contents)
    (on-define! 'CONTENTS (lambda (v) (when (oak-op? v) (install-contents! v)) v))
    (defglobal! 'CONTENTS contents)
    (set! *contents-op* contents))
  (set! *setter-op* (global-ref 'SETTER))
  (on-define! 'SETTER (let ((old (hashq-ref *global-hooks* 'SETTER)))
			(lambda (v) (let ((v (old v))) (when (oak-op? v) (set! *setter-op* v)) v))))
  (on-define! 'CONTENTS (let ((old (hashq-ref *global-hooks* 'CONTENTS)))
			  (lambda (v) (let ((v (old v))) (when (oak-op? v) (set! *contents-op* v)) v))))
  ;; Type hooks: keep the host's view of the world's types current.
  (on-define! 'SYMBOL (lambda (v) (set! *symbol* v) (note-type-name! v 'SYMBOL) v))
  (on-define! 'UNDEFINED (lambda (v) (when (oak-obj? v) (note-type-name! v 'UNDEFINED)) v))
  (on-define! 'CHARACTER (lambda (v) (set! *character* v) (note-type-name! v 'CHARACTER) v))
  (on-define! 'SIMPLE-VECTOR (lambda (v) (set! *simple-vector* v) (note-type-name! v 'SIMPLE-VECTOR) v))
  (on-define! 'BIGNUM (lambda (v) (set! *bignum* v) v))
  (on-define! 'FRACTION (lambda (v) (set! *fraction* v) v))
  (on-define! 'VARIABLE-LENGTH-MIXIN
	      (lambda (v)
		(when (oak-obj? v)
		  (set! *variable-length-mixin* v)
		  (set! *variable-length-mixin-or-nothing* v)
		  (note-type-name! v 'VARIABLE-LENGTH-MIXIN))
		v))
  (on-define! 'MAKE (lambda (v) v))
  ;; Every type gets a name for error messages.
  (set! *variable-length-mixin-or-nothing* (raw-type *type*))
  (defglobal! 'TYPE *type*)
  (defglobal! 'OBJECT *object*)
  (defglobal! 'OPERATION *operation*)
  (defglobal! 'SETTABLE-OPERATION *settable-operation*)
  (defglobal! 'LOCATABLE-OPERATION *locatable-operation*)
  (defglobal! 'COERCABLE-TYPE *coercable-type*)
  (defglobal! '%CODE-VECTOR *%code-vector*)
  (defglobal! 'CONS-PAIR *cons-pair*)
  (defglobal! 'STRING *string*)
  (defglobal! '%METHOD *%method*)
  (defglobal! '%CLOSED-ENVIRONMENT *%closed-environment*)
  (defglobal! 'NULL-TYPE *null-type*)
  (defglobal! 'FIXNUM *fixnum*)
  (defglobal! 'LOCATIVE *locative*)
  (defglobal! 'SYMBOL *symbol*)
  (defglobal! 'SELF-EVALUATORY-MIXIN *self-evaluatory-mixin*)
  (defglobal! 'CHARACTER *character*)
  (defglobal! 'NIL '())
  (defglobal! 'T #t)
  (defglobal! '%SIMPLE-OPERATION-LENGTH 5)
  (defglobal! '%EMPTY-ENVIRONMENT (%varlen-allocate *%closed-environment* 2))
  (defglobal! '%ARGLESS-TAG-TRAP-TABLE (make-vector 128 '()))
  (defglobal! '%ARGED-TAG-TRAP-TABLE (make-vector 128 '()))
  (defglobal! 'MONITOR-FOR-BRUCE '())
  (defglobal! '*LOCALE-MUTEX* '())
  (defglobal! '*ADD-METHOD-MUTEX* '())
  (set! *make-cell* (oak-loc-var (resolve-global *proto-locale* 'MAKE)))
  (set! *variable-length-mixin-cell* (oak-loc-var (resolve-global *proto-locale* 'VARIABLE-LENGTH-MIXIN)))
  (set! *fluid-binding-list-cell* (oak-loc-var (resolve-global *proto-locale* 'FLUID-BINDING-LIST)))
  (set! *wind-count-cell* (oak-loc-var (resolve-global *proto-locale* '%WIND-COUNT)))
  (variable-set! *wind-count-cell* 0)
  (variable-set! *fluid-binding-list-cell* *host-fluid-bindings*))

;;;==========================================================================
;;; Fluids (fluid.oak provides the real ones; these come first)
;;;==========================================================================

(define (install-fluid-natives!)
  (native-fn! 'GET-CURRENT-FLUID-BINDINGS (nlambda () (fluid-bindings)))
  (native-fn! 'SET-CURRENT-FLUID-BINDINGS (nlambda (l) (set-fluid-bindings! l) l))
  (let ((op (make-native-op *locatable-operation* #f)))
    (oak-add-method! op *symbol* (nlambda (sym) (fluid-ref sym)))
    (oak-add-method! (op-setter op) *symbol* (nlambda (sym val) (fluid-set! sym val)))
    (oak-add-method! (op-locater op) *symbol*
		     (nlambda (sym)
		       (let ((p (assq sym (fluid-bindings))))
			 (unless p (oak-host-error "Locative to (FLUID ~A) not found." sym))
			 (make-loc (lambda () (cdr p)) (lambda (x) (set-cdr! p x))))))
    (defglobal! '%FLUID op))
  (install-host-fluids!))

;;; Fluids the skipped files (print-list.oak, symbols.oak, ...) would
;;; set; re-run after fluid.oak replaces the binding list.
(define (install-host-fluids!)
  (fluid-set! 'PRINT-LEVEL '())
  (fluid-set! 'PRINT-LENGTH '())
  (fluid-set! 'PRINT-ESCAPE #t)
  (fluid-set! 'PRINT-RADIX 10)
  (fluid-set! 'SYMBOL-SLASHIFICATION-STYLE '())
  (fluid-set! 'FANCY-REFERENCES '())
  (fluid-set! 'ERROR-HANDLERS '())
  (fluid-set! 'DEBUG-LEVEL 0)
  (fluid-set! 'FEATURES '(OAKLISP SCHEME)))

;;;==========================================================================
;;; Lists
;;;==========================================================================

(define (oak-nth l n)
  (cond ((pair? l) (list-ref l n))
	((vector? l) (vector-ref l n))
	((string? l) (string-ref l n))
	(else (oak-host-error "NTH of ~S" (oak-describe l)))))

(define (oak-set-nth! l n v)
  (cond ((pair? l) (set-car! (list-tail l n) v) v)
	((vector? l) (vector-set! l n v) v)
	((string? l) (string-set! l n v) v)
	(else (oak-host-error "(SETTER NTH) of ~S" (oak-describe l)))))

(define (oak-map1 op l)
  (let loop ((l l) (acc '()))
    (if (pair? l)
	(loop (cdr l) (cons (oak-call op (car l)) acc))
	(reverse! acc))))

(define (oak-mapn op ls)
  (let loop ((ls ls) (acc '()))
    (if (any null? ls)
	(reverse! acc)
	(loop (map cdr ls) (cons (apply oak-call op (map car ls)) acc)))))

(define (oak-every? pred l)
  (let loop ((l l))
    (cond ((null? l) #t)
	  ((null? (cdr l)) (oak-call pred (car l)))
	  ((oak-true? (oak-call pred (car l))) (loop (cdr l)))
	  (else '()))))

(define (oak-any? pred l)
  (let loop ((l l))
    (cond ((null? l) '())
	  (else (let ((v (oak-call pred (car l))))
		  (if (oak-true? v) v (loop (cdr l))))))))

(define (oak-append2 a b)
  (cond ((null? a) b)
	((pair? a) (append a b))
	((string? a) (string-append a b))
	(else (oak-host-error "APPEND of ~S" (oak-describe a)))))

(define (oak-append . args)
  (cond ((null? args) '())
	((null? (cdr args)) (car args))
	(else (oak-append2 (car args) (apply oak-append (cdr args))))))

(define (oak-subseq seq index len)
  (cond ((string? seq) (substring seq index (+ index len)))
	((vector? seq) (let ((v (make-vector len '())))
			 (do ((i 0 (+ i 1))) ((= i len) v)
			   (vector-set! v i (vector-ref seq (+ index i))))))
	(else (list-head (list-tail seq index) len))))

(define (oak-last l)
  (cond ((string? l) (string-ref l (- (string-length l) 1)))
	((vector? l) (vector-ref l (- (vector-length l) 1)))
	(else (car (last-pair l)))))

(define (oak-setequal? a b)
  (oak-bool (and (every (lambda (x) (memq x b)) a)
		 (every (lambda (x) (memq x a)) b))))

(define (install-list-natives!)
(native-locked! 'CONS *operation* (nlambda (a b) (cons a b)))
  (let ((car-op (make-native-op *locatable-operation* #f)))
    (define (install-car! op)
      (lock-native! op (nlambda (p) (if (pair? p) (car p) (oak-host-error "CAR of ~S" (oak-describe p)))))
      (lock-native! (op-setter op) (nlambda (p v) (set-car! p v) v))
      (lock-native! (op-locater op)
		    (nlambda (p) (make-loc (lambda () (car p)) (lambda (x) (set-car! p x)))))
      op)
    (install-car! car-op)
    (on-define! 'CAR (lambda (v) (when (oak-op? v) (install-car! v)) v))
    (defglobal! 'CAR car-op))
  (let ((cdr-op (make-native-op *locatable-operation* #f)))
    (define (install-cdr! op)
      (lock-native! op (nlambda (p) (if (pair? p) (cdr p) (oak-host-error "CDR of ~S" (oak-describe p)))))
      (lock-native! (op-setter op) (nlambda (p v) (set-cdr! p v) v))
      (lock-native! (op-locater op)
		    (nlambda (p) (make-loc (lambda () (cdr p)) (lambda (x) (set-cdr! p x)))))
      op)
    (install-cdr! cdr-op)
    (on-define! 'CDR (lambda (v) (when (oak-op? v) (install-cdr! v)) v))
    (defglobal! 'CDR cdr-op))
  ;; c[ad]{2,4}r
  (for-each
   (lambda (name)
     (let* ((path (reverse (string->list (substring (symbol->string name) 1
						    (- (string-length (symbol->string name)) 1)))))
	    (get (lambda (x)
		   (let loop ((p path) (x x))
		     (if (null? p) x
			 (loop (cdr p) (if (char=? (car p) #\A) (car x) (cdr x)))))))
	    (parent (lambda (x)
		      (let loop ((p (cdr (reverse path))) (x x))
			(if (null? p) x
			    (loop (cdr p) (if (char=? (car p) #\A) (car x) (cdr x)))))))
	    (last-a? (char=? (car (reverse path)) #\A)))
       (define (install! op)
	 (lock-native! op (nlambda (x) (get x)))
	 (lock-native! (op-setter op)
		       (nlambda (x v) (let ((p (parent x)))
					(if last-a? (set-car! p v) (set-cdr! p v)) v)))
	 (lock-native! (op-locater op)
		       (nlambda (x) (let ((p (parent x)))
				      (if last-a?
					  (make-loc (lambda () (car p)) (lambda (v) (set-car! p v)))
					  (make-loc (lambda () (cdr p)) (lambda (v) (set-cdr! p v)))))))
	 op)
       (let ((op (make-native-op *locatable-operation* #f)))
	 (install! op)
	 (on-define! name (lambda (v) (when (oak-op? v) (install! v)) v))
	 (defglobal! name op))))
   '(CAAR CADR CDAR CDDR CAAAR CAADR CADAR CADDR CDAAR CDADR CDDAR CDDDR
     CAAAAR CAAADR CAADAR CAADDR CADAAR CADADR CADDAR CADDDR
     CDAAAR CDAADR CDADAR CDADDR CDDAAR CDDADR CDDDAR CDDDDR))
  (native-fn! 'FIRST (nlambda (l) (list-ref l 0)))
  (native-fn! 'SECOND (nlambda (l) (list-ref l 1)))
  (native-fn! 'THIRD (nlambda (l) (list-ref l 2)))
  (native-fn! 'FOURTH (nlambda (l) (list-ref l 3)))
  (native-fn! 'FIFTH (nlambda (l) (list-ref l 4)))
  (native-locked! 'LIST *operation* (nlambda args args))
  (native-locked! 'LIST* *operation* (nlambda args (apply cons* args)))
  (native-op! 'APPEND *operation*)
  (lock-native! (global-ref 'APPEND) (nlambda args (apply oak-append args)))
  (native-op! 'APPEND! *operation*)
  (lock-native! (global-ref 'APPEND!) (nlambda args (apply oak-append args)))
  (native-op! 'REVERSE *operation*)
  (native-op! 'REVERSE! *operation*)
  (native-op! 'LENGTH *settable-operation*)
  (native-op! 'NTH *locatable-operation*)
  (native-op! 'LAST *locatable-operation*)
  (native-op! 'LAST-PAIR *operation*)
  (native-op! 'TAIL *locatable-operation*)
  (native-op! 'HEAD *locatable-operation*)
  (native-op! 'SUBSEQ *operation*)
  (native-op! 'COPY *operation*)
  (native-op! 'MAP *operation*)
  (lock-native! (global-ref 'MAP)
		(nlambda (op l . more)
		  (if (null? more) (oak-map1 op l) (oak-mapn op (cons l more)))))
  (native-op! 'MAP! *operation*)
  (lock-native! (global-ref 'MAP!)
		(nlambda (op l . more)
		  (if (null? more)
		      (let loop ((p l))
			(when (pair? p) (set-car! p (oak-call op (car p))) (loop (cdr p))))
		      (let loop ((p l) (rest more))
			(when (and (pair? p) (not (any null? rest)))
			  (set-car! p (apply oak-call op (car p) (map car rest)))
			  (loop (cdr p) (map cdr rest)))))
		  l))
  (native-op! 'FOR-EACH *operation*)
  (lock-native! (global-ref 'FOR-EACH)
		(nlambda (op l . more)
		  (if (null? more)
		      (let loop ((p l)) (when (pair? p) (oak-call op (car p)) (loop (cdr p))))
		      (let loop ((ls (cons l more)))
			(unless (any null? ls)
			  (apply oak-call op (map car ls))
			  (loop (map cdr ls)))))
		  '()))
  (native-op! 'MAP-AND-REVERSE *operation*)
  (lock-native! (global-ref 'MAP-AND-REVERSE)
		(nlambda (op l . more)
		  (if (null? more)
		      (reverse! (oak-map1 op l))
		      (reverse! (oak-mapn op (cons l more))))))
  (native-op! 'EVERY? *operation*)
  (lock-native! (global-ref 'EVERY?) (nlambda (pred l) (oak-every? pred l)))
  (native-op! 'ANY? *operation*)
  (lock-native! (global-ref 'ANY?) (nlambda (pred l) (oak-any? pred l)))
  (native-fn! 'MEMQ (nlambda (x l) (or (memq x l) '())))
  (native-fn! 'ASSQ (nlambda (x l) (or (assq x l) '())))
  (native-fn! 'ASSV (nlambda (x l) (or (assv x l) '())))
  (native-fn! 'RASSQ (nlambda (x l) (let loop ((l l)) (cond ((null? l) '()) ((eq? (cdar l) x) (car l)) (else (loop (cdr l)))))))
  (native-op! 'ASS *operation*)
  (lock-native! (global-ref 'ASS)
		(nlambda (pred x l)
		  (let loop ((l l))
		    (cond ((null? l) '())
			  ((oak-true? (oak-call pred x (caar l))) (car l))
			  (else (loop (cdr l)))))))
  (native-op! 'MEM *operation*)
  (lock-native! (global-ref 'MEM)
		(nlambda (pred x l)
		  (let loop ((l l))
		    (cond ((null? l) '())
			  ((oak-true? (oak-call pred x (car l))) l)
			  (else (loop (cdr l)))))))
  (native-op! 'DEL *operation*)
  (lock-native! (global-ref 'DEL)
		(nlambda (pred x l)
		  (let loop ((l l) (acc '()))
		    (cond ((null? l) (reverse! acc))
			  ((oak-true? (oak-call pred x (car l))) (loop (cdr l) acc))
			  (else (loop (cdr l) (cons (car l) acc)))))))
  (native-op! 'DEL! *operation*)
  (lock-native! (global-ref 'DEL!)
		(nlambda (pred x l)
		  (let loop ((l l) (acc '()))
		    (cond ((null? l) (reverse! acc))
			  ((oak-true? (oak-call pred x (car l))) (loop (cdr l) acc))
			  (else (loop (cdr l) (cons (car l) acc)))))))
  (native-fn! 'DELQ (nlambda (x l) (delq x l)))
  (native-op! 'SETEQUAL? *operation*)
  (native-op! 'UNION *operation*)
  (lock-native! (global-ref 'UNION)
		(nlambda (a b)
		  (let loop ((a (reverse a)) (acc b))
		    (cond ((null? a) acc)
			  ((memq (car a) acc) (loop (cdr a) acc))
			  (else (loop (cdr a) (cons (car a) acc)))))))
  (native-fn! 'POSITION-IN-LIST
	      (nlambda (x l)
		(let loop ((l l) (i 0))
		  (cond ((null? l) '())
			((eq? x (car l)) i)
			(else (loop (cdr l) (+ i 1)))))))
  (native-fn! 'IOTA (nlambda (n) (iota n 1)))
  (native-fn! 'IOTA0 (nlambda (n) (iota n 0)))
  (native-fn! 'SPLICE (nlambda (l) (apply append l)))
  (native-op! 'EQUAL? *operation*)
  (lock-native! (global-ref 'EQUAL?) (nlambda (a b) (oak-bool (oak-equal? a b))))
  (native-locked! 'EQV? *operation* (nlambda (a b) (oak-bool (eqv? a b))))
  (native-fn! 'ATOM? (nlambda (x) (oak-bool (not (pair? x)))))
  (native-fn! 'PAIR? (nlambda (x) (oak-bool (pair? x))))
  (native-fn! 'LIST? (nlambda (x) (oak-bool (or (null? x) (pair? x)))))
  (native-fn! 'SYMBOL? (nlambda (x) (oak-bool (symbol? x))))
  (native-fn! 'STRING? (nlambda (x) (oak-bool (string? x))))
  (native-fn! 'CHAR? (nlambda (x) (oak-bool (char? x))))
  (native-fn! 'VECTOR? (nlambda (x) (oak-bool (vector? x))))
  (native-fn! 'NUMBER? (nlambda (x) (oak-bool (number? x))))
  (native-fn! 'INTEGER? (nlambda (x) (oak-bool (exact-integer? x))))
  (native-fn! 'FIXNUM? (nlambda (x) (oak-bool (exact-integer? x))))
  (native-fn! 'PROCEDURE? (nlambda (x) (oak-bool (oak-op? x))))
  (install-list-methods!))

(define (install-list-methods!)
  (native-method! 'REVERSE *cons-pair* (nlambda (l) (reverse l)))
  (native-method! 'REVERSE *null-type* (nlambda (l) '()))
  (native-method! 'REVERSE! *cons-pair* (nlambda (l) (reverse! l)))
  (native-method! 'REVERSE! *null-type* (nlambda (l) '()))
  (native-method! 'LENGTH *cons-pair* (nlambda (l) (length l)))
  (native-method! 'LENGTH *null-type* (nlambda (l) 0))
  (native-method! 'NTH *cons-pair* (nlambda (l n) (list-ref l n)))
  (native-setter-method! 'NTH *cons-pair* (nlambda (l n v) (oak-set-nth! l n v)))
  (native-locater-method! 'NTH *cons-pair*
			  (nlambda (l n) (let ((p (list-tail l n)))
					   (make-loc (lambda () (car p)) (lambda (v) (set-car! p v))))))
  (native-method! 'LAST *cons-pair* (nlambda (l) (oak-last l)))
  (native-method! 'LAST-PAIR *cons-pair* (nlambda (l) (last-pair l)))
  (native-method! 'TAIL *cons-pair* (nlambda (l n) (list-tail l n)))
  (native-method! 'TAIL *null-type* (nlambda (l n) (if (= n 0) '() (oak-host-error "TAIL past end"))))
  (native-method! 'HEAD *cons-pair* (nlambda (l n) (list-head l n)))
  (native-method! 'HEAD *null-type* (nlambda (l n) '()))
  (native-method! 'SUBSEQ *cons-pair* (nlambda (l i n) (oak-subseq l i n)))
  (native-method! 'SUBSEQ *null-type* (nlambda (l i n) '()))
  (native-method! 'COPY *cons-pair* (nlambda (l) (list-copy l)))
  (native-method! 'COPY *null-type* (nlambda (l) '()))
  (native-method! 'SETEQUAL? *cons-pair* (nlambda (a b) (oak-setequal? a b)))
  (native-method! 'SETEQUAL? *null-type* (nlambda (a b) (oak-setequal? a b))))

(define (oak-equal? a b)
  (cond ((eq? a b) #t)
	((and (pair? a) (pair? b))
	 (and (oak-equal? (car a) (car b)) (oak-equal? (cdr a) (cdr b))))
	((and (string? a) (string? b)) (string=? a b))
	((and (number? a) (number? b)) (= a b))
	((and (vector? a) (vector? b))
	 (and (= (vector-length a) (vector-length b))
	      (let loop ((i 0))
		(or (= i (vector-length a))
		    (and (oak-equal? (vector-ref a i) (vector-ref b i))
			 (loop (+ i 1)))))))
	((oak-obj? a)
	 ;; user EQUAL? methods (e.g. on AST constant nodes)
	 (let ((m (lookup-method (global-ref 'EQUAL?) (oak-get-type a))))
	   (and m (oak-true? ((car m) (cdr m) a b)))))
	(else #f)))

;;;==========================================================================
;;; Numbers
;;;==========================================================================

(define (num-args-check name args)
  (for-each (lambda (a) (unless (number? a)
			  (oak-host-error "~A: not a number: ~S" name (oak-describe a))))
	    args))

(define (oak-< a b)
  (cond ((and (number? a) (number? b)) (< a b))
	((and (char? a) (char? b)) (char<? a b))
	((and (string? a) (string? b)) (string<? a b))
	(else (oak-host-error "< of ~S and ~S" (oak-describe a) (oak-describe b)))))

(define (oak-= a b)
  (cond ((and (number? a) (number? b)) (= a b))
	((and (char? a) (char? b)) (char=? a b))
	((and (string? a) (string? b)) (string=? a b))
	((or (number? a) (number? b) (char? a) (char? b)) #f)
	(else (oak-host-error "= of ~S and ~S" (oak-describe a) (oak-describe b)))))

;;; The emulator's ROT instruction: rotate the raw tagged word (the
;;; fixnum shifted up by the two tag bits) within a 64 bit word, then
;;; clear the tag bits.  Width is 62 on the 64 bit build.
(define *word-bits* 64)
(define (oak-rot x n)
  (let* ((width (- *word-bits* 2))
	 (b (modulo n width))
	 (mask (- (expt 2 *word-bits*) 1))
	 (a (logand (* x 4) mask))
	 (r (if (= b 0)
		a
		(logand (logior (ash a b) (ash a (- (- width b)))) mask)))
	 (r (logand r (lognot 3)))
	 (signed (if (>= r (expt 2 (- *word-bits* 1))) (- r (expt 2 *word-bits*)) r)))
    (quotient signed 4)))

(define (install-number-natives!)
  (native-locked! '+ *operation* (nlambda args (num-args-check '+ args) (apply + args)))
  (native-locked! '- *operation* (nlambda args (num-args-check '- args) (apply - args)))
  (native-locked! '* *operation* (nlambda args (num-args-check '* args) (apply * args)))
  (native-locked! '/ *operation* (nlambda args (num-args-check '/ args) (apply / args)))
  (native-locked! '1+ *operation* (nlambda (x) (+ x 1)))
  (native-locked! 'MINUS *operation* (nlambda (x) (- x)))
  (native-locked! 'QUOTIENT *operation* (nlambda (a b) (quotient a b)))
  (native-locked! 'QUOTIENTM *operation* (nlambda (a b) (floor-quotient a b)))
  (native-locked! 'REMAINDER *operation* (nlambda (a b) (remainder a b)))
  (native-locked! 'MODULO *operation* (nlambda (a b) (modulo a b)))
  (native-locked! 'ZERO? *operation* (nlambda (x) (oak-bool (and (number? x) (zero? x)))))
  (native-locked! 'NEGATIVE? *operation* (nlambda (x) (oak-bool (negative? x))))
  (native-locked! 'POSITIVE? *operation* (nlambda (x) (oak-bool (positive? x))))
  (native-locked! '= *operation* (nlambda (a b) (oak-bool (oak-= a b))))
  (native-locked! '< *operation* (nlambda (a b) (oak-bool (oak-< a b))))
  (native-locked! '> *operation* (nlambda (a b) (oak-bool (oak-< b a))))
  (native-locked! '<= *operation* (nlambda (a b) (oak-bool (not (oak-< b a)))))
  (native-locked! '>= *operation* (nlambda (a b) (oak-bool (not (oak-< a b)))))
  (native-locked! '!= *operation* (nlambda (a b) (oak-bool (not (oak-= a b)))))
  (native-locked! 'ASH-LEFT *operation* (nlambda (x n) (ash x n)))
  (native-locked! 'ASH-RIGHT *operation* (nlambda (x n) (ash x (- n))))
  (native-locked! 'ROT-LEFT *operation* (nlambda (x n) (oak-rot x n)))
  (native-locked! 'ROT-RIGHT *operation* (nlambda (x n) (oak-rot x (- n))))
  (native-locked! 'BIT-AND *operation* (nlambda (a b) (logand a b)))
  (native-locked! 'BIT-OR *operation* (nlambda (a b) (logior a b)))
  (native-locked! 'BIT-XOR *operation* (nlambda (a b) (logxor a b)))
  (native-locked! 'BIT-NOT *operation* (nlambda (a) (lognot a)))
  (native-locked! 'BIT-NAND *operation* (nlambda (a b) (lognot (logand a b))))
  (native-locked! 'BIT-NOR *operation* (nlambda (a b) (lognot (logior a b))))
  (native-locked! 'BIT-EQUIV *operation* (nlambda (a b) (lognot (logxor a b))))
  (native-locked! 'BIT-ANDCA *operation* (nlambda (a b) (logand a (lognot b))))
  (native-locked! 'ABS *operation* (nlambda (x) (abs x)))
  (native-locked! 'EXPT *operation* (nlambda (x y) (expt x y)))
  (native-locked! 'MAX *operation* (nlambda args (apply max args)))
  (native-locked! 'MIN *operation* (nlambda args (apply min args)))
  (native-locked! 'EVEN? *operation* (nlambda (x) (oak-bool (even? x))))
  (native-locked! 'ODD? *operation* (nlambda (x) (oak-bool (odd? x))))
  (native-locked! 'MOST-NEGATIVE-FIXNUM? *operation* (nlambda (x) '()))
  (native-locked! 'FX-PLUS *operation* (nlambda (a b) (+ a b)))
  (native-locked! 'FX-TIMES *operation* (nlambda (a b) (* a b)))
  (native-locked! 'GET-ARGLINE-CHAR *operation* (nlambda (a b) '()))
  (native-locked! 'NUMERATOR *operation* (nlambda (x) (numerator x)))
  (native-locked! 'DENOMINATOR *operation* (nlambda (x) (denominator x)))
  (native-locked! 'FLOOR *operation* (nlambda (x) (floor x)))
  (native-locked! 'TRUNCATE *operation* (nlambda (x) (truncate x))))

;;;==========================================================================
;;; Symbols
;;;==========================================================================

(define *gensym-oak-counter* 0)

(define (oak-gensym x)
  (let ((s (string-append (if (symbol? x) (symbol->string x) x)
			  (number->string *gensym-oak-counter*))))
    (set! *gensym-oak-counter* (+ *gensym-oak-counter* 1))
    (string->symbol s)))

(define (install-symbol-natives!)
  (native-locked! 'GENSYM *operation* (nlambda (x) (oak-gensym x)))
  (native-locked! 'GENERATE-SYMBOL *operation* (nlambda (x) (oak-gensym x)))
  (native-locked! 'GENVAR *operation* (nlambda () (oak-gensym "v")))
  (native-locked! 'GENVAR/1 *operation* (nlambda (ignored) (oak-gensym "v")))
  (native-op! 'INTERN *settable-operation*)
  (native-method! 'INTERN *string* (nlambda (s) (string->symbol s)))
  (native-op! 'REQUIRES-SLASHIFICATION? *operation*)
  (native-method! 'REQUIRES-SLASHIFICATION? *string*
		  (nlambda (s) (oak-bool (string-requires-slashification? s)))))

;;; Native methods on the SYMBOL type; re-installed whenever the world
;;; redefines SYMBOL.
(define (install-symbol-methods! type)
  (native-method! 'REQUIRES-SLASHIFICATION? type
		  (nlambda (s) (oak-bool (symbol-requires-slashification? (symbol->string s)))))
  (native-method! 'INTERN type (nlambda (s) s))
  (native-method! 'PRINT type (nlambda (s stream) (print-symbol s stream) s)))

;;; From symbols.oak: which symbols must be escaped when printed.
(define (symbol-requires-slashification? name)
  (let ((l (string-length name))
	(base (fluid-ref 'PRINT-RADIX)))
    (define (digit? c) (and (oak-digit-value c base) #t))
    (define (constituent? c)
      (memq (oak-char-syntax c) '(constituent nonterminating-macro)))
    (define (bad-char? c)
      ;; not a constituent, or lower case
      (or (not (constituent? c))
	  (char-lower-case? c)))
    (define (scan i)
      (cond ((= i l) #f)
	    ((bad-char? (string-ref name i)) #t)
	    (else (scan (+ i 1)))))
    ;; the number DFA
    (define (n1 i)          ; before first digit
      (if (= i l) #f
	  (let ((c (string-ref name i)))
	    (n1.5 i c))))
    (define (n1.5 i c)
      (cond ((and (constituent? c) (digit? c)) (n2 (+ i 1)))
	    ((char=? c #\.) (n3 (+ i 1)))
	    (else (scan.5 i c))))
    (define (n2 i)          ; in first block of digits
      (if (= i l) #t
	  (let ((c (string-ref name i)))
	    (cond ((and (constituent? c) (digit? c)) (n2 (+ i 1)))
		  ((char=? c #\/) (n3 (+ i 1)))
		  ((char=? c #\.) (n4 (+ i 1)))
		  (else (scan.5 i c))))))
    (define (n3 i)          ; before required digit of last block
      (if (= i l) #t
	  (let ((c (string-ref name i)))
	    (if (and (constituent? c) (digit? c)) (n4 (+ i 1)) (scan.5 i c)))))
    (define (n4 i)          ; before optional digit of last block
      (if (= i l) #t
	  (let ((c (string-ref name i)))
	    (if (and (constituent? c) (digit? c)) (n4 (+ i 1)) (scan.5 i c)))))
    (define (scan.5 i c)
      (if (bad-char? c) #t (scan (+ i 1))))
    (or (string=? name "")
	(string=? name ".")
	(string=? name "...")
	(let ((c (string-ref name 0)))
	  (or (not (eq? (oak-char-syntax c) 'constituent))
	      (if (char=? c #\-)
		  (n1 1)
		  (n1.5 0 c)))))))

(define (string-requires-slashification? s)
  (let loop ((i 0))
    (cond ((= i (string-length s)) #f)
	  ((memv (string-ref s i) '(#\" #\\)) #t)
	  (else (loop (+ i 1))))))

;;;==========================================================================
;;; Characters
;;;==========================================================================

(define (install-char-methods! type)
  (native-method! 'PRINT type (nlambda (c stream) (print-char c stream) c))
  (native-method! 'UPCASE type (nlambda (c) (char-upcase c)))
  (native-method! 'DOWNCASE type (nlambda (c) (char-downcase c)))
  (native-method! 'GRAPHIC? type
		  (nlambda (c) (oak-bool (and (>= (char->integer c) 33) (<= (char->integer c) 126))))))

;;; Errors, signals

(define (oak-error-message fmt args)
  (apply oak-format #f fmt args))

(define *in-error* #f)

(define (oak-error fmt . args)
  ;; Look for a handler in #*ERROR-HANDLERS; otherwise die.
  (let ((handlers (if (fluid-bound? 'ERROR-HANDLERS) (fluid-ref 'ERROR-HANDLERS) '()))
	(general-error (and (fluid-bound? 'CURRENT-LOCALE)
			    (let ((loc (resolve-global (current-locale) 'GENERAL-ERROR)))
			      (let ((v (loc-contents loc)))
				(and (oak-obj? v) v))))))
    (let ((msg (if (string? fmt) (oak-error-message fmt args) (format #f "~A" fmt))))
      (let loop ((h handlers))
	(cond ((null? h)
	       (throw 'oak-error msg))
	      ((and general-error
		    (oak-obj? (caar h))
		    (oak-true? (oak-subtype? general-error (caar h)))
		    (not (eq? (cdar h) (global-ref 'INVOKE-DEBUGGER))))
	       ;; Build an error object of type general-error carrying the message.
	       (let ((err (%allocate general-error (type-instance-length general-error))))
		 (set-oak-obj-aux! err msg)
		 (oak-call (cdar h) err)))
	      (else (loop (cdr h))))))))

(define (install-error-natives!)
  (native-locked! 'ERROR *operation* (nlambda (fmt . args) (apply oak-error fmt args)))
  (native-locked! 'CERROR *operation*
		  (nlambda (proceed fmt . args) (apply oak-error fmt args)))
  (native-fn! 'WARNING (nlambda (fmt . args)
			 (apply oak-format (global-ref 'STANDARD-ERROR)
				(string-append "~&Warning: " fmt) args)
			 '()))
  (native-locked! 'SIGNAL *operation*
		  (nlambda (type . args)
		    (oak-error "Signal ~A ~A" (oak-describe type) (map oak-describe args))))
  (native-locked! 'SIGNAL-DESTRUCTURE-ERROR *operation*
		  (nlambda (found required)
		    (oak-error "While destructuring, ~S was found where ~A is required."
			       found required)))
  (native-locked! 'POISON *operation*
		  (nlambda args (oak-error "The poison function was called with args ~S." args)))
  (native-locked! 'INVOKE-DEBUGGER *operation*
		  (nlambda (err) (throw 'oak-error (format #f "error ~A" (oak-describe err)))))
  (native-op! 'REPORT *operation*)
  (native-fn! 'READ-EVAL-PRINT-LOOP (nlambda args (throw 'oak-error "REPL entered"))))
