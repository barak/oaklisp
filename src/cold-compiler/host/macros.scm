;;; This file is part of Oaklisp.
;;;
;;; macros.scm -- bootstrap macros for the Guile-hosted Oaklisp.
;;;
;;; The real macros live in src/world/macros0.oak and friends and are
;;; written in Oaklisp, which means they can only be loaded once the
;;; macros they themselves use exist.  These host versions break that
;;; circle: they are used to evaluate the macro files, after which the
;;; world's own definitions take precedence (the expander looks in the
;;; locale before it looks here).  They cover what those files use and
;;; follow the originals closely; they are never used when compiling.
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

(define (bm-genvar) (tr-gensym '%G))

(define (bm-error form msg . args)
  (oak-host-error "~A in ~S" (apply format #f msg args) form))

(define (oak-list? x) (or (null? x) (pair? x)))

(define-syntax define-bm
  (syntax-rules ()
    ((_ name (form) body ...)
     (define-builtin-macro! 'name (lambda (form) body ...)))))

;;; The current values of MAKE, OPERATION and OBJECT, quoted, as LAMBDA
;;; and ADD-METHOD embed them.
(define (bm-quoted-make) (list 'QUOTE (variable-ref *make-cell*)))
(define (bm-quoted-operation) (list 'QUOTE *operation*))
(define (bm-quoted-object) (list 'QUOTE *object*))

;;;--------------------------------------------------------------------------
;;; macros0.oak

(define-bm MAKE-LOCATIVE (form)
  (let ((place (cadr form)))
    (if (symbol? place)
	`(%MAKE-LOCATIVE ,place)
	(let ((place (oak-expand (current-locale) place)))
	  (if (symbol? place)
	      `(%MAKE-LOCATIVE ,place)
	      `((LOCATER ,(car place)) ,@(cdr place)))))))

(define (bm-set form)
  (let ((place (cadr form)) (value (caddr form)))
    (define (normal place)
      `((SETTER CONTENTS) (MAKE-LOCATIVE ,place) ,value))
    (if (symbol? place)
	(normal place)
	(let ((place (oak-expand (current-locale) place)))
	  (if (symbol? place)
	      (normal place)
	      `((SETTER ,(car place)) ,@(cdr place) ,value))))))

(define-bm SET! (form) (bm-set form))
(define-bm SET (form) (bm-set form))

(define-bm OR (form)
  (let ((clauses (cdr form)))
    (cond ((null? clauses) '(QUOTE ()))
	  ((null? (cdr clauses)) (car clauses))
	  (else
	   (let ((var (bm-genvar)))
	     `(LET ((,var ,(car clauses)))
		(IF ,var ,var (OR ,@(cdr clauses)))))))))

(define-bm AND (form)
  (let ((clauses (cdr form)))
    (cond ((null? clauses) 'T)
	  ((null? (cdr clauses)) (car clauses))
	  (else `(IF ,(car clauses) (AND ,@(cdr clauses)) (QUOTE ()))))))

(define-bm IF (form)
  (cond ((= (length form) 3)
	 `(%IF ,(cadr form) ,(caddr form) IF-UNDEFINED-VALUE))
	((= (length form) 4)
	 `(%IF ,(cadr form) ,(caddr form) ,(cadddr form)))
	(else (bm-error form "malformed IF"))))

(define-bm COND (form)
  (let ((clauses (cdr form)))
    (cond ((null? clauses) 'COND-UNDEFINED-VALUE)
	  (else
	   (let* ((clause (car clauses))
		  (guard (car clause))
		  (rest (cdr clauses)))
	     (cond ((null? (cdr clause))
		    (if (null? rest) guard `(OR ,guard (COND ,@rest))))
		   ((and (eq? (cadr clause) '=>) (= (length clause) 3))
		    (let ((v (bm-genvar)))
		      `(LET ((,v ,guard))
			 (IF ,v (,(caddr clause) ,v) (COND ,@rest)))))
		   (else
		    `(IF ,guard (BLOCK ,@(cdr clause)) (COND ,@rest)))))))))

(define (bm-add-method-expander what)
  (lambda (form)
    (let* ((sig (cadr form))
	   (body (cddr form))
	   (op (car sig))
	   (cdr-info (cdr sig))
	   (bit (and (pair? cdr-info) (oak-list? (car cdr-info))))
	   (type-info (if bit (car cdr-info) (list (bm-quoted-object))))
	   (args (if bit (cdr cdr-info) cdr-info))
	   (typ (car type-info))
	   (ivars (cdr type-info)))
      `(,what (,op (,typ ,@ivars) ,@args) (BLOCK ,@body)))))

(define-builtin-macro! 'ADD-METHOD (bm-add-method-expander '%ADD-METHOD))
(define-builtin-macro! 'NATIVE-ADD-METHOD (bm-add-method-expander '%ADD-METHOD))
(define-builtin-macro! '_ADD-METHOD (bm-add-method-expander '_%ADD-METHOD))

(define-bm LAMBDA (form)
  (let ((varlist (cadr form)) (body (cddr form)))
    `(ADD-METHOD ((,(bm-quoted-make) ,(bm-quoted-operation)) ,@varlist)
       ,@body)))

(define (bm-labels form)
  (let* ((raw-clauses (cadr form))
	 (body (cddr form))
	 (clauses (map (lambda (clause)
			 (if (symbol? (car clause))
			     clause
			     (let ((var (caar clause)) (args (cdar clause)) (b (cdr clause)))
			       `(,var (LAMBDA ,args ,@b)))))
		       raw-clauses)))
    `(%LABELS ,(map (lambda (c) (list (car c) (cadr c))) clauses)
	      (BLOCK ,@body))))

(define-bm LABELS (form) (bm-labels form))
(define-bm LETREC (form) (bm-labels form))

(define-bm LET (form)
  (let ((clauses (cadr form)) (body (cddr form)))
    (cond ((symbol? clauses)
	   `(ITERATE ,clauses ,@body))
	  (else
	   `((LAMBDA ,(map car clauses) ,@body)
	     ,@(map cadr clauses))))))

(define-bm ITERATE (form)
  (let ((label (cadr form)) (clauses (caddr form)) (body (cdddr form)))
    (unless (symbol? label) (bm-error form "ITERATE label must be a symbol"))
    `(LABELS (((,label ,@(map car clauses)) ,@body))
       (,label ,@(map cadr clauses)))))

(define-bm THE-RUNTIME (form)
  `(CONTENTS (IDENTITY (MAKE-LOCATIVE ,(cadr form)))))

(define-bm FLUID (form)
  `(%FLUID (QUOTE ,(cadr form))))

(define-bm BIND (form)
  (let ((clauses (cadr form)) (body (cddr form))
	(old (bm-genvar)))
    `(LET ((,old (GET-CURRENT-FLUID-BINDINGS)))
       ,(if (null? clauses)
	    '()
	    `(SET-CURRENT-FLUID-BINDINGS
	      (LIST* ,@(map (lambda (clause)
			      (let ((var (cadr (car clause))) (val (cadr clause)))
				`(CONS (QUOTE ,var) ,val)))
			    clauses)
		     ,old)))
       (BLOCK0 (BLOCK ,@body)
	       (SET-CURRENT-FLUID-BINDINGS ,old)))))

(define-bm BLOCK0 (form)
  (let ((v (bm-genvar)))
    `(LET ((,v ,(cadr form)))
       ,@(cddr form)
       ,v)))

(define-bm BLOCK (form)
  `(%BLOCK ,@(cdr form)))

(define-bm DOLIST (form)
  (let* ((spec (cadr form))
	 (var (car spec)) (l (cadr spec)) (exit-body (cddr spec))
	 (body (cddr form))
	 (itt (bm-genvar)) (lab (bm-genvar)))
    `(LABELS (((,lab ,itt)
	       (IF (NULL? ,itt) (BLOCK ,@exit-body)
		   (LET ((,var (CAR ,itt)))
		     ,@body
		     (,lab (CDR ,itt))))))
       (,lab ,l))))

;;;--------------------------------------------------------------------------
;;; macros1.oak

(define-bm COMMENT (form) '(QUOTE COMMENT))

(define-bm LET* (form)
  (let ((clauses (cadr form)) (body (cddr form)))
    (if (null? clauses)
	`(BLOCK ,@body)
	`(LET (,(car clauses))
	   (LET* ,(cdr clauses) ,@body)))))

(define-bm DEFINE (form)
  (let ((var (cadr form)))
    (cond ((and (pair? var) (eq? (car var) 'FLUID))
	   (unless (= (length form) 3) (bm-error form "malformed DEFINE"))
	   `(SET! ,@(cdr form)))
	  ((pair? var)
	   `(SET! ,(car var) (LAMBDA ,(cdr var) ,@(cddr form))))
	  (else
	   (unless (= (length form) 3) (bm-error form "malformed DEFINE"))
	   `((SETTER CONTENTS) (MAKE-LOCATIVE ,var) ,(caddr form))))))

(define-bm DEFINE-SYNTAX (form)
  (let ((sym (cadr form)) (def (caddr form)))
    (cond ((pair? sym)
	   (let ((v (bm-genvar)))
	     `(DEFINE-SYNTAX ,(car sym)
		(LAMBDA (,v)
		  (DESTRUCTURE* (#t . ,(cdr sym)) ,v
		    ,def)))))
	  (else
	   `(SET! (MACRO-HERE? (FLUID CURRENT-LOCALE) (QUOTE ,sym)) ,def)))))

(define-bm LOCAL-SYNTAX (form)
  (let ((sym (cadr form)) (def (caddr form)))
    (cond ((pair? sym)
	   (let ((v (bm-genvar)))
	     `(LOCAL-SYNTAX ,(car sym)
		(LAMBDA (,v)
		  (DESTRUCTURE* (#t ,@(cdr sym)) ,v
		    ,def)))))
	  (else
	   (oak-call (op-setter op-macro-here?) (current-locale) sym
		     (oak-eval def (current-locale)))
	   '(QUOTE LOCAL-SYNTAX)))))

(define-bm DEFINE-LOCAL-SYNTAX (form)
  (let ((sym (cadr form)) (def (caddr form)))
    (cond ((pair? sym)
	   (let ((v (bm-genvar)))
	     `(DEFINE-LOCAL-SYNTAX ,(car sym)
		(LAMBDA (,v)
		  (DESTRUCTURE* (#t ,@(cdr sym)) ,v
		    ,def)))))
	  (else
	   (oak-call (op-setter op-macro-here?) (current-locale) sym
		     (oak-eval def (current-locale)))
	   `(SET! (MACRO-HERE? (FLUID CURRENT-LOCALE) (QUOTE ,sym)) ,def)))))

(define-bm DEFINE-INSTANCE (form)
  (let ((loc (cadr form)) (typ (caddr form)) (args (cdddr form))
	(t1 (bm-genvar)) (t2 (bm-genvar)))
    `(LET ((,t1 (MAKE-LOCATIVE ,loc))
	   (,t2 ,typ))
       (IF (EQ? (GET-TYPE (CONTENTS ,t1)) ,t2)
	   (CONTENTS ,t1)
	   (SET! (CONTENTS ,t1) (MAKE ,t2 ,@args))))))

(define-bm DOTIMES (form)
  (let* ((spec (cadr form))
	 (var (car spec)) (limit (cadr spec)) (exit-body (cddr spec))
	 (body (cddr form))
	 (exit-form (if (null? exit-body) '() (car exit-body)))
	 (v-limit (bm-genvar)) (v-label (bm-genvar)))
    `(LET ((,v-limit ,limit))
       (LABELS (((,v-label ,var)
		 (COND ((< ,var ,v-limit) ,@body (,v-label (+ ,var 1)))
		       (ELSE ,exit-form))))
	 (,v-label 0)))))

(define-bm WHEN (form)
  `(IF ,(cadr form) (BLOCK ,@(cddr form)) WHEN-UNDEFINED-VALUE))

(define-bm UNLESS (form)
  `(IF (NOT ,(cadr form)) (BLOCK ,@(cddr form)) UNLESS-UNDEFINED-VALUE))

(define-bm WHILE (form)
  (let ((v (bm-genvar)))
    `(ITERATE ,v ()
       (IF ,(cadr form)
	   (BLOCK ,@(cddr form) (,v))
	   WHILE-UNDEFINED-VALUE))))

(define-bm CASE (form)
  (let ((key (cadr form)) (clauses (cddr form)) (keyvar (bm-genvar)))
    `(LET ((,keyvar ,key))
       (COND
	,@(map (lambda (clause)
		 (cond ((memq (car clause) '(ELSE OTHERWISE)) clause)
		       (else
			`((OR ,@(map (lambda (th)
				       `(,(if (or (exact-integer? th) (not (number? th)))
					      'EQ? 'EQV?)
					 (QUOTE ,th) ,keyvar))
				     (car clause)))
			  ,@(cdr clause)))))
	       clauses)))))

;;;--------------------------------------------------------------------------
;;; macros2.oak

(define-bm WITH-OPERATIONS (form)
  `(LET ,(map (lambda (x) `(,x (MAKE OPERATION))) (cadr form))
     ,@(cddr form)))

(define-bm PUSH (form)
  (let ((location (cadr form)) (expr (caddr form)))
    (if (symbol? location)
	`(SET! ,location (CONS ,expr ,location))
	`(LET ((LOC (MAKE-LOCATIVE ,location)))
	   (SET! (CONTENTS LOC) (CONS ,expr (CONTENTS LOC)))))))

(define-bm POP (form)
  (let ((location (cadr form)))
    `(LET* ((LOC (MAKE-LOCATIVE ,location))
	    (VAL (CONTENTS LOC)))
       (SET! (CONTENTS LOC) (CDR VAL))
       (CAR VAL))))

;;;--------------------------------------------------------------------------
;;; define.oak

(define-bm DEFINE-CONSTANT (form)
  (let ((var (cadr form)) (body (cddr form)))
    `(BLOCK0 (DEFINE ,var ,@body)
	     (FREEZE-IN-CURRENT-LOCALE (QUOTE ,(if (pair? var) (car var) var))))))

(define-bm DEFINE-CONSTANT-INSTANCE (form)
  (let ((var (cadr form)) (typ (caddr form)) (args (cdddr form)))
    `(BLOCK0 (DEFINE-INSTANCE ,var ,typ ,@args)
	     (FREEZE-IN-CURRENT-LOCALE (QUOTE ,var)))))

;;;--------------------------------------------------------------------------
;;; destructure.oak

(define (bm-destructure-2 pattern expr body expr-important noper)
  (cond ((symbol? pattern)
	 `(LET ((,pattern ,expr)) ,body))
	((and (not noper)
	      (or (null? pattern)
		  (eq? #t pattern)
		  (and (pair? pattern) (eq? 'QUOTE (car pattern)))))
	 (if expr-important `(BLOCK ,expr ,body) body))
	((eq? #t pattern)
	 (if expr-important `(BLOCK ,expr ,body) body))
	((null? pattern)
	 (let ((v (bm-genvar)))
	   `(LET ((,v ,expr))
	      (IF (NULL? ,v)
		  ,body
		  ,(noper v '())))))
	((not (pair? pattern))
	 (oak-host-error "bad destructure pattern ~S" pattern))
	((eq? 'QUOTE (car pattern))
	 (let ((v (bm-genvar)))
	   `(LET ((,v ,expr))
	      (IF (EQ? ,v (QUOTE ,(cadr pattern)))
		  ,body
		  ,(noper v (cadr pattern))))))
	(else
	 (let* ((v (bm-genvar))
		(inner (bm-destructure-2 (car pattern) `(CAR ,v)
					 (bm-destructure-2 (cdr pattern) `(CDR ,v)
							   body #f noper)
					 #f noper)))
	   (if noper
	       `(LET ((,v ,expr))
		  (IF (PAIR? ,v) ,inner ,(noper v pattern)))
	       `(LET ((,v ,expr)) ,inner))))))

(define (bm-destr-signaler found desired)
  `(SIGNAL-DESTRUCTURE-ERROR ,found (QUOTE ,desired)))

(define-bm DESTRUCTURE (form)
  (let ((pattern (cadr form)) (expr (caddr form)) (body (cdddr form)))
    (bm-destructure-2 pattern expr `(BLOCK ,@body) #t #f)))

(define-bm DESTRUCTURE* (form)
  (let ((pattern (cadr form)) (expr (caddr form)) (body (cdddr form)))
    (bm-destructure-2 pattern expr `(BLOCK ,@body) #t bm-destr-signaler)))

(define-bm DESTRUCTURE** (form)
  (let* ((expr (cadr form)) (clauses (cddr form))
	 (v (bm-genvar))
	 (tags (map (lambda (c) (bm-genvar)) clauses))
	 (tags0 (append (cdr tags) '(#f))))
    (if (null? clauses)
	`(ERROR "No DESTRUCTURE** clauses, so none match ~S." ,expr)
	`(LET ((,v ,expr))
	   (LABELS
	       ,(map (lambda (this next clause)
		       (let ((pattern (car clause)) (body (cdr clause)))
			 `(,this
			   (LAMBDA ()
			     ,(if (eq? pattern 'OTHERWISE)
				  (if next
				      (bm-error form "Nonterminal OTHERWISE in DESTRUCTURE**")
				      `(BLOCK ,@body))
				  (bm-destructure-2 pattern v `(BLOCK ,@body) #f
						    (if next
							(lambda (a b) `(,next))
							bm-destr-signaler)))))))
		     tags tags0 clauses)
	     (,(car tags)))))))

;;;--------------------------------------------------------------------------
;;; backquote.oak

(define (bm-quotation? x)
  (and (pair? x) (eq? (car x) 'QUOTE) (pair? (cdr x)) (null? (cddr x))))

(define (bm-comma-atsignation? x)
  (and (pair? x) (eq? (car x) 'UNQUOTE-SPLICING) (pair? (cdr x)) (null? (cddr x))))

(define (bm-expand-backquote x)
  (cond ((null? x) '(QUOTE ()))
	((vector? x)
	 (let ((l (vector->list x)))
	   (cond ((any bm-comma-atsignation? l)
		  (list '(COERCER SIMPLE-VECTOR) (bm-expand-backquote l)))
		 (else
		  (let ((l1 (map bm-expand-backquote l)))
		    (if (every bm-quotation? l1)
			(list 'QUOTE x)
			(cons 'VECTOR l1)))))))
	((not (pair? x)) (list 'QUOTE x))
	((eq? (car x) 'QUASIQUOTE)
	 (bm-expand-backquote (bm-expand-backquote (cadr x))))
	((eq? (car x) 'UNQUOTE) (cadr x))
	((and (pair? (car x)) (eq? (caar x) 'UNQUOTE-SPLICING))
	 (let ((splice-in (cadar x))
	       (tail (bm-expand-backquote (cdr x))))
	   (cond ((and (bm-quotation? tail) (null? (cadr tail)))
		  splice-in)
		 (else (list 'APPEND splice-in tail)))))
	(else
	 (let* ((car-x (car x)) (cdr-x (cdr x))
		(a (bm-expand-backquote car-x))
		(d (bm-expand-backquote cdr-x)))
	   (cond ((bm-quotation? d)
		  (let ((cadr-d (cadr d)))
		    (cond ((bm-quotation? a)
			   (let ((cadr-a (cadr a)))
			     (list 'QUOTE
				   (if (and (eq? car-x cadr-a) (eq? cdr-x cadr-d))
				       x
				       (cons cadr-a cadr-d)))))
			  ((null? cadr-d) (list 'LIST a))
			  (else (list 'LIST* a d)))))
		 ((and (pair? d) (memq (car d) '(LIST LIST*)))
		  (cons* (car d) a (cdr d)))
		 (else (list 'LIST* a d)))))))

(define-bm QUASIQUOTE (form)
  (bm-expand-backquote (cadr form)))

;;;--------------------------------------------------------------------------
;;; catch.oak / error3.oak, host flavoured: used only before the real
;;; ones are loaded.

(define-bm NATIVE-CATCH (form)
  (let ((var (cadr form)) (body (cddr form)))
    `(%CATCH
      (LET ((,var (%FILLTAG (%ALLOCATE ESCAPE-OBJECT 5))))
	,@body))))

(define-bm CATCH (form)
  (let ((v (cadr form)) (body (cddr form)))
    `(NATIVE-CATCH ,v
       (LET ((,v (LAMBDA (VAL) (%THROW ,v VAL))))
	 ,@body))))

;;;--------------------------------------------------------------------------
;;; unwind-protect.oak / error2.oak / error3.oak, host flavoured: these
;;; macros are defined late in the cold load but used earlier; the
;;; real ones take over once loaded.

(define-bm WIND-PROTECT (form)
  (let ((before (cadr form)) (during (caddr form)) (after (cadddr form))
	(a (bm-genvar)) (b (bm-genvar)))
    `(LET ((,b (LAMBDA () ,before)) (,a (LAMBDA () ,after)))
       (,b)
       (BLOCK0 ,during (,a)))))

(define-bm FUNNY-WIND-PROTECT (form)
  (let ((normal-before (cadr form)) (during (cadddr form))
	(normal-after (car (cddddr form))))
    `(BLOCK ,normal-before (BLOCK0 ,during ,normal-after))))

(define-bm BIND-ERROR-HANDLER (form)
  (let ((spec (cadr form)) (body (cddr form)))
    `(BIND (((FLUID ERROR-HANDLERS)
	     (CONS (CONS ,(car spec) ,(cadr spec)) (FLUID ERROR-HANDLERS))))
       ,@body)))

(define-bm ERROR-RETURN (form)
  (let ((tag (bm-genvar)))
    `(NATIVE-CATCH ,tag ,@(cddr form))))

(define-bm ERROR-RESTART (form)
  (let ((variables (caddr form)) (body (cdddr form)))
    `(LET ,variables ,@body)))

(define-bm CATCH-ERRORS (form)
  (let* ((spec (cadr form)) (error-type (car spec)) (more (cdr spec)) (body (cddr form))
	 (v (bm-genvar)) (m (bm-genvar)) (r (bm-genvar)))
    (cond ((null? more)
	   `(NATIVE-CATCH ,v
	      (BIND-ERROR-HANDLER (,error-type (LAMBDA (ERR) (%THROW ,v (QUOTE ()))))
		,@body)))
	  ((null? (cdr more))
	   `(LET ((,m (CONS (QUOTE ()) (QUOTE ()))))
	      (LET ((,r (NATIVE-CATCH ,v
			  (BIND-ERROR-HANDLER (,error-type (LAMBDA (ERR) (%THROW ,v (CONS ,m ERR))))
			    ,@body))))
		(IF (AND (PAIR? ,r) (EQ? (CAR ,r) ,m))
		    (,(car more) (CDR ,r))
		    ,r))))
	  (else
	   `(LET ((,m (CONS (QUOTE ()) (QUOTE ()))))
	      (LET ((,r (NATIVE-CATCH ,v
			  (BIND-ERROR-HANDLER (,error-type (LAMBDA (ERR) (%THROW ,v (CONS ,m ERR))))
			    ,@body))))
		(IF (AND (PAIR? ,r) (EQ? (CAR ,r) ,m))
		    (,(car more) (CDR ,r))
		    (,(cadr more) ,r))))))))
