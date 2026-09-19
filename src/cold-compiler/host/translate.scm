;;; This file is part of Oaklisp.
;;;
;;; translate.scm -- macro expansion and translation of Oaklisp core
;;; forms into Guile code, for the Guile-hosted bootstrap.
;;;
;;; After macro expansion (EXPAND-GROVELING, ported from expand.oak) an
;;; Oaklisp form consists only of symbols, constants and the special
;;; forms QUOTE %IF %BLOCK %ADD-METHOD _%ADD-METHOD %MAKE-LOCATIVE
;;; %LABELS %CATCH %SET and combinations.  TRANSLATE turns such a form
;;; into a Guile expression which PRIMITIVE-EVAL then runs.
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
;;; Locales as seen from the host
;;;==========================================================================
;;; These call the world's own VARIABLE? etc. operations, which are
;;; native for the proto locale and come from locales.oak afterwards.

(define op-variable? #f)          ; the operation objects, set by runtime.scm
(define op-variable-here? #f)
(define op-macro? #f)
(define op-macro-here? #f)

(define (current-locale)
  (if (fluid-bound? 'CURRENT-LOCALE)
      (fluid-ref 'CURRENT-LOCALE)
      *proto-locale*))

(define *proto-locale* #f)

;;; The cell (an <oak-loc>) for global SYM in LOCALE, installing it if
;;; it does not exist, as LINK-CODE-SEGMENT does.
(define (resolve-global locale sym)
  (let ((loc (oak-call op-variable? locale sym)))
    (let ((loc (if (oak-true? loc)
		   loc
		   (oak-call (op-setter op-variable-here?) locale sym #t))))
      (unless (oak-loc? loc)
	(oak-host-error "Variable lookup of ~A gave ~S" sym (oak-describe loc)))
      (unless (oak-loc-name loc)
	(set-oak-loc-name! loc sym))
      loc)))

(define (global-value locale sym)
  (loc-contents (resolve-global locale sym)))

;;;==========================================================================
;;; Macro expansion (expand.oak)
;;;==========================================================================

(define *builtin-macros* (make-hash-table))   ; symbol -> (lambda (form) ...)

(define (define-builtin-macro! name proc)
  (hashq-set! *builtin-macros* name proc))

(define (special-form? sym)
  (memq sym '(QUOTE %BLOCK %IF %ADD-METHOD _%ADD-METHOD %MAKE-LOCATIVE %LABELS)))

(define (improper-list? x)
  (if (pair? x)
      (cdr (last-pair x))
      x))

(define (make-proper x)
  (if (pair? x)
      (let ((the-copy (list-copy x)))
	(set-cdr! (last-pair the-copy) '())
	the-copy)
      '()))

(define (map-proper-part op l)
  (let ((improper-part (improper-list? l)))
    (if (oak-true? improper-part)
	(let ((proper-answer (map op (make-proper l))))
	  (set-cdr! (last-pair proper-answer) improper-part)
	  proper-answer)
	(map op l))))

;;; A macro expander for SYM, or #f: the locale chain first, then the
;;; host's bootstrap macros.
(define (macro-lookup locale sym)
  (and (symbol? sym)
       (let ((m (oak-call op-macro? locale sym)))
	 (if (oak-true? m)
	     (lambda (form) (oak-call m form))
	     (hashq-ref *builtin-macros* sym)))))

(define (oak-expand locale form)
  (cond ((not (pair? form)) form)
	((special-form? (car form))
	 (idiosyncratically-grovel locale form))
	(else
	 (let ((m (macro-lookup locale (car form))))
	   (if m
	       (oak-expand locale (m form))
	       (map-proper-part (lambda (x) (oak-expand locale x)) form))))))

(define (idiosyncratically-grovel locale form)
  (let ((s (car form)))
    (cond ((memq s '(%IF %BLOCK %MAKE-LOCATIVE))
	   (cons s (map (lambda (x) (oak-expand locale x)) (cdr form))))
	  ((memq s '(%ADD-METHOD _%ADD-METHOD))
	   (let* ((sig (cadr form))
		  (op (car sig))
		  (typ-ivars (cadr sig))
		  (args (cddr sig))
		  (body (caddr form)))
	     (list s
		   (cons* (oak-expand locale op)
			  (cons (oak-expand locale (car typ-ivars)) (cdr typ-ivars))
			  args)
		   (oak-expand locale body))))
	  ((eq? s '%LABELS)
	   (list s
		 (map (lambda (x) (list (car x) (oak-expand locale (cadr x))))
		      (cadr form))
		 (oak-expand locale (caddr form))))
	  ((eq? s 'QUOTE) form)
	  (else (oak-host-error "Form ~S can't be idiosyncratically groveled." form)))))

;;;==========================================================================
;;; Translation to Guile
;;;==========================================================================

;;; Lexical environment: a list of bindings (sym . binding) where
;;; binding is (local . gsym) or (ivar . k).  A method context carries
;;; the gensyms of the first argument, BP and IVX of the enclosing
;;; %ADD-METHOD so ivar references can be compiled.

(define-record-type <mctx>
  (make-mctx self bp ivx)
  mctx?
  (self mctx-self) (bp mctx-bp) (ivx mctx-ivx))

(define *gensym-counter* 0)
(define (tr-gensym base)
  (set! *gensym-counter* (+ *gensym-counter* 1))
  (string->symbol (string-append (symbol->string base) "%"
				 (number->string *gensym-counter*))))

(define (env-lookup env sym)
  (let ((p (assq sym env)))
    (and p (cdr p))))

;;; The value of global MAKE and the types OPERATION and OBJECT at
;;; translation time, for recognising lambdas.
(define *make-cell* #f)

(define (lambda-op-pattern? op-form typ-form ivars locale)
  ;; ((QUOTE make) (QUOTE operation)) and (QUOTE object) and no ivars.
  (and (null? ivars)
       (pair? op-form)
       (pair? (car op-form))
       (eq? (caar op-form) 'QUOTE)
       (pair? (cdr op-form))
       (pair? (cadr op-form))
       (eq? (car (cadr op-form)) 'QUOTE)
       (null? (cddr op-form))
       (eq? (cadr (car op-form)) (variable-ref *make-cell*))
       (eq? (cadr (cadr op-form)) *operation*)
       (pair? typ-form)
       (eq? (car typ-form) 'QUOTE)
       (eq? (cadr typ-form) *object*)))

;;; Translate a self contained expression.
(define (translate form env ctx locale)
  (cond
   ((symbol? form) (translate-var-ref form env ctx locale))
   ((not (pair? form))
    (if (or (number? form) (string? form) (char? form) (eq? form #t) (null? form)
	    (vector? form))
	(list 'quote form)
	(list 'quote form)))
   (else
    (let ((head (car form)))
      (case head
	((QUOTE) (list 'quote (cadr form)))
	((%IF)
	 (unless (= (length form) 4)
	   (oak-host-error "Malformed %IF ~S" form))
	 (list 'if (list 'oak-true? (translate (cadr form) env ctx locale))
	       (translate (caddr form) env ctx locale)
	       (translate (cadddr form) env ctx locale)))
	((%BLOCK)
	 (if (null? (cdr form))
	     ''()
	     (cons 'begin (map (lambda (x) (translate x env ctx locale)) (cdr form)))))
	((%ADD-METHOD _%ADD-METHOD)
	 (translate-add-method form env ctx locale))
	((%MAKE-LOCATIVE)
	 (translate-make-locative (cadr form) env ctx locale))
	((%LABELS)
	 (translate-labels form env ctx locale))
	((%CATCH)
	 (list 'oak-catch (list 'lambda '() (translate (cadr form) env ctx locale))))
	((%SET)
	 (translate-set (cadr form) (translate (caddr form) env ctx locale)
			env ctx locale))
	((REST-LENGTH)
	 (if (and (pair? (cdr form)) (symbol? (cadr form))
		  (env-lookup env (cadr form)))
	     (list 'length (translate (cadr form) env ctx locale))
	     (translate-call form env ctx locale)))
	(else (translate-call form env ctx locale)))))))

(define (translate-var-ref sym env ctx locale)
  (let ((b (env-lookup env sym)))
    (cond ((not b)
	   (list 'variable-ref (list 'quote (oak-loc-var (resolve-global locale sym)))))
	  ((eq? (car b) 'local) (cdr b))
	  ((eq? (car b) 'ivar)
	   (list 'vector-ref (list 'oak-obj-v (mctx-self ctx))
		 (list '+ (mctx-bp ctx) (list 'vector-ref (mctx-ivx ctx) (cdr b)))))
	  (else (oak-host-error "bad binding ~S" b)))))

(define (translate-set sym value-code env ctx locale)
  (let ((b (env-lookup env sym)))
    (cond ((not b)
	   (list 'oak-global-set! (list 'quote (resolve-global locale sym)) value-code))
	  ((eq? (car b) 'local)
	   (let ((g (tr-gensym 'V)))
	     (list 'let (list (list g value-code))
		   (list 'set! (cdr b) g)
		   g)))
	  ((eq? (car b) 'ivar)
	   (let ((g (tr-gensym 'V)))
	     (list 'let (list (list g value-code))
		   (list 'vector-set! (list 'oak-obj-v (mctx-self ctx))
			 (list '+ (mctx-bp ctx) (list 'vector-ref (mctx-ivx ctx) (cdr b)))
			 g)
		   g)))
	  (else (oak-host-error "bad binding ~S" b)))))

(define (translate-make-locative sym env ctx locale)
  (unless (symbol? sym)
    (oak-host-error "%MAKE-LOCATIVE of non-symbol ~S" sym))
  (let ((b (env-lookup env sym)))
    (cond ((not b)
	   (list 'quote (resolve-global locale sym)))
	  ((eq? (car b) 'local)
	   (let ((g (cdr b)) (x (tr-gensym 'X)))
	     (list 'make-loc
		   (list 'lambda '() g)
		   (list 'lambda (list x) (list 'set! g x)))))
	  ((eq? (car b) 'ivar)
	   (list 'make-slot-loc (mctx-self ctx)
		 (list '+ (mctx-bp ctx) (list 'vector-ref (mctx-ivx ctx) (cdr b)))))
	  (else (oak-host-error "bad binding ~S" b)))))

;;; Bind formals (possibly improper) to gensyms; returns (guile-formals . env).
(define (bind-formals formals env)
  (let loop ((f formals) (gf '()) (env env))
    (cond ((null? f) (cons (reverse gf) env))
	  ((symbol? f)
	   (let ((g (tr-gensym f)))
	     (cons (append (reverse gf) g) (cons (cons f (cons 'local g)) env))))
	  ((pair? f)
	   (unless (symbol? (car f))
	     (oak-host-error "Non-symbol ~S in formals ~S" (car f) formals))
	   (let ((g (tr-gensym (car f))))
	     (loop (cdr f) (cons g gf) (cons (cons (car f) (cons 'local g)) env))))
	  (else (oak-host-error "Bad formals ~S" formals)))))

;;; (%ADD-METHOD (op (type . ivars) . args) body)
(define (translate-add-method form env ctx locale)
  (let* ((sig (cadr form))
	 (op-form (car sig))
	 (typ-form (car (cadr sig)))
	 (ivars (cdr (cadr sig)))
	 (args (cddr sig))
	 (body (caddr form)))
    (if (lambda-op-pattern? op-form typ-form ivars locale)
	(list 'make-lambda-op (translate-method-lambda args '() body env ctx locale #f))
	(let ((ivx (tr-gensym 'IVX)))
	  (list 'oak-install-method!
		(translate op-form env ctx locale)
		(translate typ-form env ctx locale)
		(list 'quote ivars)
		(list 'lambda (list ivx)
		      (translate-method-lambda args ivars body env ctx locale ivx)))))))

;;; (lambda (BP . formals) body) with ivars visible.
(define (translate-method-lambda args ivars body env ctx locale ivx)
  (let* ((bp (tr-gensym 'BP))
	 (bound (bind-formals args env))
	 (gformals (car bound))
	 (env1 (cdr bound))
	 (first-arg (cond ((pair? gformals) (car gformals))
			  ((symbol? gformals) gformals)
			  (else #f)))
	 (ctx2 (if (null? ivars) ctx (make-mctx first-arg bp ivx)))
	 ;; Args shadow ivars, ivars shadow the outer environment, and
	 ;; ENV1 is the arg bindings consed onto ENV, so splice the ivar
	 ;; bindings in between.
	 (env2 (if (null? ivars)
		   env1
		   (let ((nargs (- (length env1) (length env))))
		     (append (list-head env1 nargs)
			     (let loop ((ivs ivars) (k 0) (acc '()))
			       (if (null? ivs) (reverse acc)
				   (loop (cdr ivs) (+ k 1)
					 (cons (cons (car ivs) (cons 'ivar k)) acc))))
			     env)))))
    (when (and (pair? ivars) (not first-arg))
      (oak-host-error "ADD-METHOD with ivars ~S but no arguments" ivars))
    (list 'lambda (cons bp gformals)
	  (translate body env2 ctx2 locale))))

(define (translate-labels form env ctx locale)
  (let* ((clauses (cadr form))
	 (body (caddr form))
	 (names (map car clauses))
	 (gs (map (lambda (n) (tr-gensym n)) names))
	 (env2 (append (map (lambda (n g) (cons n (cons 'local g))) names gs) env)))
    (list 'letrec
	  (map (lambda (g clause) (list g (translate (cadr clause) env2 ctx locale)))
	       gs clauses)
	  (translate body env2 ctx locale))))

;;; Is the global SYM unshadowed and currently bound to OBJ?
(define (global-is? sym obj env locale)
  (and (symbol? sym)
       (not (env-lookup env sym))
       (eq? (global-value locale sym) obj)))

(define *setter-op* #f)      ; the SETTER operation object
(define *contents-op* #f)    ; the CONTENTS operation object

(define (translate-call form env ctx locale)
  (let ((head (car form))
	(args (cdr form)))
    (cond
     ;; ((setter contents) (%make-locative x) v)  =>  assignment
     ((and (pair? head)
	   (= (length head) 2)
	   (global-is? (car head) *setter-op* env locale)
	   (global-is? (cadr head) *contents-op* env locale)
	   (list? args) (= (length args) 2)
	   (pair? (car args))
	   (eq? (caar args) '%MAKE-LOCATIVE)
	   (symbol? (cadar args)))
      (translate-set (cadar args) (translate (cadr args) env ctx locale) env ctx locale))
     ;; ((lambda formals body) args...)  =>  let
     ((and (pair? head)
	   (memq (car head) '(%ADD-METHOD _%ADD-METHOD))
	   (let* ((sig (cadr head))
		  (formals (cddr sig)))
	     (and (lambda-op-pattern? (car sig) (car (cadr sig)) (cdr (cadr sig)) locale)
		  (list? formals)
		  (list? args)
		  (= (length formals) (length args)))))
      (let* ((sig (cadr head))
	     (formals (cddr sig))
	     (body (caddr head))
	     (bound (bind-formals formals env))
	     (gformals (car bound))
	     (env2 (cdr bound)))
	(if (null? formals)
	    (translate body env2 ctx locale)
	    (list 'let (map (lambda (g a) (list g (translate a env ctx locale)))
			    gformals args)
		  (translate body env2 ctx locale)))))
     (else
      (let ((fcode (translate head env ctx locale)))
	(if (list? args)
	    (cons 'oak-call
		  (cons fcode (map (lambda (a) (translate a env ctx locale)) args)))
	    ;; improper: (f a b . rest)
	    (let loop ((a args) (acc '()))
	      (if (pair? a)
		  (loop (cdr a) (cons (translate (car a) env ctx locale) acc))
		  (cons 'apply
			(cons 'oak-call
			      (cons fcode
				    (append (reverse acc)
					    (list (translate a env ctx locale))))))))))))))

;;;==========================================================================
;;; Runtime support referenced by translated code
;;;==========================================================================

(define (make-lambda-op proc)
  (make-native-op *operation* proc))

(define (oak-install-method! op type ivars maker)
  (unless (and (oak-obj? type)
	       (oak-true? (oak-subtype? (vector-ref (oak-obj-v type) 0) *type*)))
    (oak-host-error "ADD-METHOD: ~S is not a type" (oak-describe type)))
  (let ((ivx (list->vector
	      (map (lambda (iv)
		     (or (ivar-index type iv)
			 (oak-host-error "Variable ~A declared in ADD-METHOD isn't in ~A, the ivars of ~A."
					 iv (type-ivar-list type) (oak-describe type))))
		   ivars))))
    (oak-add-method! op type (maker ivx))
    op))

;;;==========================================================================
;;; Eval and load
;;;==========================================================================

(define *trace-eval* #f)

(define (oak-eval form locale)
  (let* ((expanded (oak-expand locale form))
	 (code (translate expanded '() #f locale)))
    (when *trace-eval*
      (format (current-error-port) "~%;; ~S~%;; => ~S~%" form code))
    (primitive-eval code)))

;;; Evaluate FORM in LOCALE the way LOAD-OAK-FILE does: expand in a
;;; sub locale (when locales exist) with #*CURRENT-LOCALE bound to it,
;;; then evaluate with #*CURRENT-LOCALE bound to LOCALE.
(define (oak-load-form form locale sub-locale)
  (let ((expanded (with-fluids* (list (cons 'CURRENT-LOCALE sub-locale))
			       (lambda () (oak-expand sub-locale form)))))
    (with-fluids* (list (cons 'CURRENT-LOCALE locale))
		 (lambda ()
		   (let ((code (translate expanded '() #f locale)))
		     (when *trace-eval*
		       (format (current-error-port) "~%;; ~S~%;; => ~S~%" form code))
		     (primitive-eval code))))))

(define *current-file* #f)

(define (oak-load-file filename locale make-sub-locale)
  (let ((forms (oak-read-file-forms filename))
	(sub (if make-sub-locale (make-sub-locale locale) locale)))
    (set! *current-file* filename)
    (for-each (lambda (form) (oak-load-form form locale sub)) forms)
    #t))
