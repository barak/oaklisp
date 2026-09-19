;;; This file is part of Oaklisp.
;;;
;;; runtime2.scm -- strings, vectors, hash tables, streams, FORMAT and
;;; PRINT for the Guile-hosted Oaklisp.  These parts of the world are
;;; written against the emulator's memory layout (strings pack three
;;; characters to a word, hash tables index vectors, streams hold file
;;; descriptors), so they are provided natively with the same
;;; interface.
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
;;; Strings
;;;==========================================================================

(define (install-string-natives!)
  (native-locked! 'MAKE-STRING *operation*
		  (nlambda (len . rest) (make-string len (if (pair? rest) (car rest) #\space))))
  (native-op! 'UPCASE *operation*)
  (native-op! 'DOWNCASE *operation*)
  (native-op! 'WRITE-STRING *operation*)
  (native-op! 'GRAPHIC? *operation*)
  (native-op! 'REMOVE *operation*)
  (native-op! 'REMOVE-IF *operation*)
  (native-op! 'FILL! *operation*)
  (native-op! 'SUBSEQUENCE? *operation*)
  (native-fn! '%CHAR? (nlambda (x) (oak-bool (char? x))))
  (defglobal! '%CHARS-PER-WORD 3)
  (native-locked! 'WRITE-STRING-WITH-SLASHES *operation*
		  (nlambda (s delim stream)
		    (string-for-each
		     (lambda (c)
		       (when (or (char=? c delim) (char=? c #\\))
			 (oak-write-char stream #\\))
		       (oak-write-char stream c))
		     s)
		    s))
  (install-string-methods!))

(define (install-string-methods!)
  (let ((t *string*))
    (native-method! 'LENGTH t (nlambda (s) (string-length s)))
    (native-method! 'NTH t (nlambda (s n) (string-ref s n)))
    (native-setter-method! 'NTH t (nlambda (s n c) (string-set! s n c) c))
    (native-locater-method! 'NTH t
			    (nlambda (s n)
			      (make-loc (lambda () (string-ref s n))
					(lambda (c) (string-set! s n c)))))
    (native-method! 'PRINT t (nlambda (s stream) (print-string s stream) s))
    (native-method! 'COPY t (nlambda (s) (string-copy s)))
    (native-method! 'REVERSE t (nlambda (s) (string-reverse s)))
    (native-method! 'SUBSEQ t (nlambda (s i n) (oak-subseq s i n)))
    (native-method! 'UPCASE t (nlambda (s) (string-upcase s)))
    (native-method! 'DOWNCASE t (nlambda (s) (string-downcase s)))
    (native-method! 'REMOVE t (nlambda (s c) (string-delete c s)))
    (native-method! 'WRITE-STRING t (nlambda (s stream) (oak-write-string stream s) s))
    (native-method! 'FILL! t (nlambda (s c) (string-fill! s c) s))
    (native-method! 'LAST t (nlambda (s) (oak-last s)))
    (native-method! 'TAIL t (nlambda (s n) (substring s n)))
    (native-method! 'HEAD t (nlambda (s n) (substring s 0 n)))
    (native-method! 'SUBSEQUENCE? t
		    (nlambda (s1 s2)
		      (let ((i (string-contains s2 s1))) (or i '()))))))

;;; #^STRING, #^SYMBOL, #^LIST-TYPE methods, installed once the world
;;; has made those coercable types.
(define (install-string-coercions!)
  (let ((co (type-co-op *string*)))
    (oak-add-method! co *null-type* (nlambda (l) ""))
    (oak-add-method! co *cons-pair* (nlambda (l) (list->string l)))
    (oak-add-method! co *character* (nlambda (c) (string c)))
    (oak-add-method! co *simple-vector* (nlambda (v) (list->string (vector->list v))))
    (oak-add-method! co *symbol* (nlambda (s) (string-copy (symbol->string s))))))

(define (install-symbol-coercions! type)
  (let ((co (type-co-op type)))
    (oak-add-method! co *string* (nlambda (s) (string->symbol s)))
    (oak-add-method! co *character* (nlambda (c) (or (oak-char->name c) '())))
    (oak-add-method! co *cons-pair* (nlambda (l) (string->symbol (list->string l))))))

(define (install-list-coercions!)
  (let ((co (type-co-op (global-ref 'LIST-TYPE))))
    (oak-add-method! co *string* (nlambda (s) (string->list s)))
    (oak-add-method! co *simple-vector* (nlambda (v) (vector->list v)))))

;;;==========================================================================
;;; Vectors
;;;==========================================================================

(define (install-vector-natives!)
  (native-locked! '%VREF *locatable-operation* (nlambda (v n) (vector-ref v n)))
  (on-define! '%VREF
	      (lambda (v)
		(when (oak-op? v)
		  (lock-native! v (nlambda (v n) (vector-ref v n)))
		  (lock-native! (op-setter v) (nlambda (v n x) (vector-set! v n x) x))
		  (lock-native! (op-locater v)
				(nlambda (v n) (make-loc (lambda () (vector-ref v n))
							 (lambda (x) (vector-set! v n x))))))
		v))
  (lock-native! (op-setter (global-ref '%VREF)) (nlambda (v n x) (vector-set! v n x) x))
  (native-locked! '%LOAD-BP-I *locatable-operation*
		  (nlambda (n) (oak-host-error "%LOAD-BP-I called outside the emulator")))
  (native-fn! 'VECTOR (nlambda args (list->vector args))))

(define (install-vector-methods! type)
  (native-method! 'LENGTH type (nlambda (v) (vector-length v)))
  (native-method! 'NTH type (nlambda (v n) (vector-ref v n)))
  (native-setter-method! 'NTH type (nlambda (v n x) (vector-set! v n x) x))
  (native-locater-method! 'NTH type
			  (nlambda (v n) (make-loc (lambda () (vector-ref v n))
						   (lambda (x) (vector-set! v n x)))))
  (native-method! 'PRINT type (nlambda (v stream) (print-vector v stream) v))
  (native-method! 'COPY type (nlambda (v) (vector-copy v)))
  (native-method! 'SUBSEQ type (nlambda (v i n) (oak-subseq v i n)))
  (native-method! 'INITIALIZE type (nlambda (v n) v))
  (when (coercable? type)
    (let ((co (type-co-op type)))
      (oak-add-method! co *null-type* (nlambda (l) (vector)))
      (oak-add-method! co *cons-pair* (nlambda (l) (list->vector l)))
      (oak-add-method! co *string* (nlambda (s) (list->vector (string->list s)))))))

;;;==========================================================================
;;; Hash tables
;;;==========================================================================

(define *hash-table-type* #f)
(define *generic-hash-table-type* #f)
(define *eq-hash-table-type* #f)
(define *equal-hash-table-type* #f)
(define *string-hash-table-type* #f)

(define (hash-table-of obj)
  (or (oak-obj-aux obj)
      (oak-host-error "not a hash table: ~A" (oak-describe obj))))

(define (install-hash-natives!)
  ;; The types as hash-table.oak defines them, ivar lists included: the
  ;; compiler lays out methods on them by those lists.
  (set! *hash-table-type* (make-type '() '()))
  (note-type-name! *hash-table-type* 'HASH-TABLE)
  (defglobal! 'HASH-TABLE *hash-table-type*)
  (set! *generic-hash-table-type*
	(make-type '(TABLE COUNT SIZE KEY-OP =?) (list *hash-table-type* *object*)))
  (set! *eq-hash-table-type*
	(make-type '(TABLE COUNT SIZE) (list *hash-table-type* *object*)))
  (set! *equal-hash-table-type* (make-type '() (list *generic-hash-table-type*)))
  (set! *string-hash-table-type* *generic-hash-table-type*)
  (note-type-name! *generic-hash-table-type* 'GENERIC-HASH-TABLE)
  (note-type-name! *eq-hash-table-type* 'EQ-HASH-TABLE)
  (note-type-name! *equal-hash-table-type* 'EQUAL-HASH-TABLE)
  (defglobal! 'GENERIC-HASH-TABLE *generic-hash-table-type*)
  (defglobal! 'EQ-HASH-TABLE *eq-hash-table-type*)
  (defglobal! 'EQUAL-HASH-TABLE *equal-hash-table-type*)
  (define (new-table type)
    (let ((obj (%allocate type (type-instance-length type))))
      (set-oak-obj-aux! obj (make-hash-table))
      obj))
  (native-locked! 'MAKE-EQ-HASH-TABLE *operation* (nlambda () (new-table *eq-hash-table-type*)))
  (native-locked! 'MAKE-EQUAL-HASH-TABLE *operation* (nlambda () (new-table *equal-hash-table-type*)))
  (native-locked! 'MAKE-STRING-HASH-TABLE *operation* (nlambda () (new-table *string-hash-table-type*)))
  (native-op! 'PRESENT? *locatable-operation*)
  (native-op! 'TABLE-ENTRY *settable-operation*)
  (native-op! 'RESIZE *operation*)
  (define (present obj key)
    (let ((t (hash-table-of obj)))
      (or (if (eq? (oak-get-type obj) *eq-hash-table-type*)
	      (hashq-ref t key)
	      (hash-ref t key))
	  '())))
  (define (set-present! obj key v)
    (let* ((t (hash-table-of obj))
	   (eq (eq? (oak-get-type obj) *eq-hash-table-type*))
	   (entry (if eq (hashq-ref t key) (hash-ref t key))))
      (cond ((oak-true? v)
	     (if entry
		 (set-cdr! entry v)
		 (if eq (hashq-set! t key (cons key v)) (hash-set! t key (cons key v)))))
	    (else
	     (if eq (hashq-remove! t key) (hash-remove! t key))))
      v))
  (for-each
   (lambda (type)
     (native-method! 'PRESENT? type (nlambda (obj key) (present obj key)))
     (native-setter-method! 'PRESENT? type (nlambda (obj key v) (set-present! obj key v)))
     (native-method! 'TABLE-ENTRY type
		     (nlambda (obj key) (let ((e (present obj key))) (if (pair? e) (cdr e) '()))))
     (native-setter-method! 'TABLE-ENTRY type (nlambda (obj key v) (set-present! obj key v)))
     (native-method! 'LENGTH type (nlambda (obj) (hash-count (const #t) (hash-table-of obj))))
     (native-method! 'RESIZE type (nlambda (obj n) obj))
     (native-method! 'INITIALIZE type
		     (nlambda (obj . args) (set-oak-obj-aux! obj (make-hash-table)) obj))
     (native-method! 'PRINT type
		     (nlambda (obj stream)
		       (oak-format stream "#<hash-table ~D ~!>"
				   (hash-count (const #t) (hash-table-of obj)) obj)
		       obj)))
   (list *eq-hash-table-type* *generic-hash-table-type*)))

(define (install-hash-coercions!)
  (let ((co (type-co-op (global-ref 'LIST-TYPE))))
    (for-each
     (lambda (type)
       (oak-add-method! co type
			(nlambda (obj)
			  (hash-fold (lambda (k entry acc) (cons entry acc))
				     '() (hash-table-of obj)))))
     (list *eq-hash-table-type* *generic-hash-table-type*))))

;;;==========================================================================
;;; Streams
;;;==========================================================================

(define-record-type <hstream>
  (make-hstream port at-line-start string-out?)
  hstream?
  (port hstream-port set-hstream-port!)
  (at-line-start hstream-at-line-start set-hstream-at-line-start!)
  (string-out? hstream-string-out?))

(define *stream-type* #f)
(define *input-stream-type* #f)
(define *output-stream-type* #f)
(define *file-input-stream-type* #f)
(define *file-output-stream-type* #f)
(define *string-output-stream-type* #f)
(define *string-input-stream-type* #f)
(define *eof-token-type* #f)
(define *the-eof-token* #f)

(define (hstream-of obj)
  (if (and (oak-obj? obj) (hstream? (oak-obj-aux obj)))
      (oak-obj-aux obj)
      #f))

(define (make-stream-obj type port string-out?)
  (let ((obj (%allocate type 1)))
    (set-oak-obj-aux! obj (make-hstream port #t string-out?))
    obj))

(define (oak-write-char stream c)
  (let ((hs (hstream-of stream)))
    (if hs
	(begin (write-char c (hstream-port hs))
	       (set-hstream-at-line-start! hs (char=? c #\newline)))
	(oak-call (global-ref 'WRITE-CHAR) stream c))
    c))

(define (oak-write-string stream s)
  (let ((hs (hstream-of stream)))
    (if hs
	(begin (display s (hstream-port hs))
	       (when (> (string-length s) 0)
		 (set-hstream-at-line-start! hs (char=? (string-ref s (- (string-length s) 1))
							#\newline))))
	(string-for-each (lambda (c) (oak-call (global-ref 'WRITE-CHAR) stream c)) s))
    s))

(define (oak-freshline stream)
  (let ((hs (hstream-of stream)))
    (if hs
	(unless (hstream-at-line-start hs) (oak-write-char stream #\newline))
	(oak-call (global-ref 'FRESHLINE) stream))))

;;; Output file redirection: files whose names end in these suffixes
;;; are written under *OUTDIR* when it is set.
(define *outdir* #f)

(define (redirect-path name)
  (if (and *outdir*
	   (or (string-suffix? ".oa" name) (string-suffix? ".oa-tmp" name)))
      (string-append *outdir* "/" (basename name))
      name))

(define (install-stream-natives!)
  (set! *stream-type* (make-type '() '()))
  (set! *input-stream-type* (make-type '() (list *stream-type*)))
  (set! *output-stream-type* (make-type '() (list *stream-type*)))
  (set! *file-input-stream-type* (make-type '() (list *input-stream-type* *object*)))
  (set! *file-output-stream-type* (make-type '() (list *output-stream-type* *object*)))
  (set! *string-output-stream-type* (make-type '() (list *output-stream-type* *object*)))
  (set! *string-input-stream-type* (make-type '() (list *input-stream-type* *object*)))
  (set! *eof-token-type* (make-type '() (list *object*)))
  (for-each (lambda (t n) (note-type-name! t n) (defglobal! n t))
	    (list *stream-type* *input-stream-type* *output-stream-type*
		  *file-input-stream-type* *file-output-stream-type*
		  *string-output-stream-type* *string-input-stream-type* *eof-token-type*)
	    '(STREAM INPUT-STREAM OUTPUT-STREAM FILE-INPUT-STREAM FILE-OUTPUT-STREAM
	      STRING-OUTPUT-STREAM STRING-INPUT-STREAM EOF-TOKEN))
  (set! *the-eof-token* (%allocate *eof-token-type* 1))
  (defglobal! 'THE-EOF-TOKEN *the-eof-token*)
  (defglobal! 'STANDARD-OUTPUT (make-stream-obj *file-output-stream-type* (current-output-port) #f))
  (defglobal! 'STANDARD-ERROR (make-stream-obj *file-output-stream-type* (current-error-port) #f))
  (defglobal! 'STANDARD-INPUT (make-stream-obj *file-input-stream-type* (current-input-port) #f))
  (native-op! 'WRITE-CHAR *operation*)
  (native-op! 'READ-CHAR *operation*)
  (native-op! 'PEEK-CHAR *operation*)
  (native-op! 'UNREAD-CHAR *operation*)
  (native-op! 'NEWLINE *operation*)
  (native-op! 'FRESHLINE *operation*)
  (native-op! 'FLUSH *operation*)
  (native-op! 'CLOSE *operation*)
  (native-op! 'INTERACTIVE? *operation*)
  (native-op! 'READ *operation*)
  (define (oak-read-char-from stream)
    (let ((c (read-char (hstream-port (hstream-of stream)))))
      (if (eof-object? c) *the-eof-token* c)))
  (for-each
   (lambda (type)
     (native-method! 'WRITE-CHAR type (nlambda (s c) (oak-write-char s c)))
     (native-method! 'NEWLINE type (nlambda (s) (oak-write-char s #\newline) '()))
     (native-method! 'FRESHLINE type (nlambda (s) (oak-freshline s) '()))
     (native-method! 'FLUSH type (nlambda (s) (force-output (hstream-port (hstream-of s))) s))
     (native-method! 'CLOSE type
		     (nlambda (s)
		       (let ((hs (hstream-of s)))
			 (unless (hstream-string-out? hs)
			   (close-port (hstream-port hs))))
		       s)))
   (list *file-output-stream-type* *string-output-stream-type*))
  (for-each
   (lambda (type)
     (native-method! 'READ-CHAR type (nlambda (s) (oak-read-char-from s)))
     (native-method! 'PEEK-CHAR type
		     (nlambda (s)
		       (let ((c (peek-char (hstream-port (hstream-of s)))))
			 (if (eof-object? c) *the-eof-token* c))))
     (native-method! 'UNREAD-CHAR type
		     (nlambda (s c)
		       (unless (eq? c *the-eof-token*)
			 (unread-char c (hstream-port (hstream-of s))))
		       c))
     (native-method! 'READ type
		     (nlambda (s)
		       (let ((x (oak-read (hstream-port (hstream-of s)))))
			 (if (oak-eof? x) *the-eof-token* x))))
     (native-method! 'CLOSE type (nlambda (s) (close-port (hstream-port (hstream-of s))) s))
     (native-method! 'INTERACTIVE? type (nlambda (s) '())))
   (list *file-input-stream-type* *string-input-stream-type*))
  (native-method! 'INITIALIZE *string-output-stream-type*
		  (nlambda (s)
		    (set-oak-obj-aux! s (make-hstream (open-output-string) #t #t))
		    s))
  (native-method! 'INITIALIZE *string-input-stream-type*
		  (nlambda (s str)
		    (set-oak-obj-aux! s (make-hstream (open-input-string
						       (if (string? str) str (list->string str)))
						      #t #f))
		    s))
  (native-locked! 'OPEN-INPUT-FILE *operation*
		  (nlambda (name)
		    (make-stream-obj *file-input-stream-type* (open-input-file name) #f)))
  (define (open-out name append?)
    (let ((name (redirect-path name)))
      (make-stream-obj *file-output-stream-type*
		       (if append?
			   (open-file name "a")
			   (open-output-file name))
		       #f)))
  (native-locked! 'OPEN-OUTPUT-FILE *operation* (nlambda (name) (open-out name #f)))
  (native-locked! 'OPEN-OUTPUT-FILE-UGLY *operation* (nlambda (name) (open-out name #f)))
  (native-locked! 'OPEN-OUTPUT-FILE-APPEND *operation* (nlambda (name) (open-out name #t)))
  (native-locked! 'OPEN-OUTPUT-FILE-APPEND-UGLY *operation* (nlambda (name) (open-out name #t)))
  (native-locked! 'RENAME-FILE *operation*
		  (nlambda (old new)
		    (rename-file (redirect-path old) (redirect-path new))
		    #t))
  (native-locked! 'READ-UNTIL *operation*
		  (nlambda (closer dot? stream)
		    (unless (eq? closer *the-eof-token*)
		      (oak-host-error "READ-UNTIL is only supported up to end of file"))
		    (oak-read-all (hstream-port (hstream-of stream)))))
  (native-locked! 'SKIP-WHITESPACE *operation*
		  (nlambda (stream) (oak-skip-whitespace (hstream-port (hstream-of stream))) '())))

(define (install-stream-coercions!)
  (let ((co (type-co-op *string*)))
    (oak-add-method! co *string-output-stream-type*
		     (nlambda (s) (get-output-string (hstream-port (hstream-of s)))))))

;;;==========================================================================
;;; FORMAT
;;;==========================================================================

(define (stream-for-format stream)
  (cond ((eq? stream #t) (global-ref 'STANDARD-OUTPUT))
	(else stream)))

(define (oak-format stream control . args)
  (cond ((or (null? stream) (not stream))
	 (let ((s (make-stream-obj *string-output-stream-type* (open-output-string) #t)))
	   (apply oak-format s control args)
	   (get-output-string (hstream-port (hstream-of s)))))
	(else
	 (let ((stream (stream-for-format stream))
	       (len (string-length control)))
	   (let loop ((i 0) (args args))
	     (cond ((>= i len)
		    (unless (null? args)
		      (oak-host-error "FORMAT: ~A unconsumed arguments to ~S" (length args) control))
		    '())
		   ((char=? (string-ref control i) #\~)
		    ;; parameters
		    (let parse ((j (+ i 1)) (params '()))
		      (when (>= j len) (oak-host-error "FORMAT: control ends in directive: ~S" control))
		      (let ((c (string-ref control j)))
			(cond ((char=? c #\')
			       (parse (+ j 2) (cons (string-ref control (+ j 1)) params)))
			      ((char=? c #\,) (parse (+ j 1) (cons '() params)))
			      ((or (char-numeric? c) (char=? c #\-) (char=? c #\+))
			       (let num ((k (+ j 1)))
				 (if (and (< k len) (char-numeric? (string-ref control k)))
				     (num (+ k 1))
				     (parse k (cons (string->number (substring control j k)) params)))))
			      ((or (char=? c #\:) (char=? c #\@)) (parse (+ j 1) params))
			      (else
			       (let ((params (reverse params))
				     (dir (char-upcase c)))
				 (define (param n)
				   (if (< n (length params)) (list-ref params n) '()))
				 (define (pad-out text left?)
				   (let ((mincol (param 0)) (padchar (param 1)))
				     (if (integer? mincol)
					 (let ((padding (make-string (max 0 (- mincol (string-length text)))
								     (if (char? padchar) padchar #\space))))
					   (oak-write-string stream (if left? (string-append padding text)
									(string-append text padding))))
					 (oak-write-string stream text))))
				 (define (need-arg)
				   (when (null? args)
				     (oak-host-error "FORMAT: no argument for ~~~A in ~S" dir control)))
				 (define (printed arg escape? radix)
				   (let ((s (make-stream-obj *string-output-stream-type* (open-output-string) #t)))
				     (with-fluids* (list (cons 'PRINT-ESCAPE escape?) (cons 'PRINT-RADIX radix))
						   (lambda () (oak-print arg s)))
				     (get-output-string (hstream-port (hstream-of s)))))
				 (case dir
				   ((#\A) (need-arg) (pad-out (printed (car args) '() 10) #f)
				    (loop (+ j 1) (cdr args)))
				   ((#\S) (need-arg) (pad-out (printed (car args) #t 10) #f)
				    (loop (+ j 1) (cdr args)))
				   ((#\C) (need-arg) (pad-out (printed (car args) '() 10) #f)
				    (loop (+ j 1) (cdr args)))
				   ((#\D) (need-arg) (pad-out (printed (car args) #t 10) #t)
				    (loop (+ j 1) (cdr args)))
				   ((#\B) (need-arg) (pad-out (printed (car args) #t 2) #t)
				    (loop (+ j 1) (cdr args)))
				   ((#\O) (need-arg) (pad-out (printed (car args) #t 8) #t)
				    (loop (+ j 1) (cdr args)))
				   ((#\X) (need-arg) (pad-out (printed (car args) #t 16) #t)
				    (loop (+ j 1) (cdr args)))
				   ((#\R) (need-arg)
				    (oak-write-string stream (printed (car args) #t (param 0)))
				    (loop (+ j 1) (cdr args)))
				   ((#\P) (need-arg)
				    (unless (and (number? (car args)) (= (car args) 1))
				      (oak-write-char stream #\s))
				    (loop (+ j 1) (cdr args)))
				   ((#\!) (need-arg)
				    (oak-write-string stream (number->string (oak-object-hash (car args))))
				    (loop (+ j 1) (cdr args)))
				   ((#\%) (oak-write-char stream #\newline) (loop (+ j 1) args))
				   ((#\&) (oak-freshline stream) (loop (+ j 1) args))
				   ((#\~) (oak-write-char stream #\~) (loop (+ j 1) args))
				   ((#\newline) (loop (+ j 1) args))
				   (else (oak-host-error "FORMAT: unknown directive ~~~A in ~S" dir control)))))))))
		   (else
		    (oak-write-char stream (string-ref control i))
		    (loop (+ i 1) args))))))))

(define (install-format-natives!)
  (native-locked! 'FORMAT *operation* (nlambda (stream control . args)
					(apply oak-format stream control args))))

;;;==========================================================================
;;; PRINT
;;;==========================================================================

(define (oak-print obj stream)
  (oak-call (global-ref 'PRINT) obj stream))

(define (print-string s stream)
  (if (oak-true? (fluid-ref 'PRINT-ESCAPE))
      (begin
	(oak-write-char stream #\")
	(string-for-each (lambda (c)
			   (when (or (char=? c #\") (char=? c #\\))
			     (oak-write-char stream #\\))
			   (oak-write-char stream c))
			 s)
	(oak-write-char stream #\"))
      (oak-write-string stream s)))

(define (print-symbol s stream)
  (let ((name (symbol->string s)))
    (cond ((and (oak-true? (fluid-ref 'PRINT-ESCAPE))
		(symbol-requires-slashification? name))
	   (cond ((eq? (fluid-ref 'SYMBOL-SLASHIFICATION-STYLE) 'T-COMPATIBLE)
		  (if (string=? name "")
		      (oak-write-string stream "#[symbol \"\"]")
		      (string-for-each (lambda (c)
					 (oak-write-char stream #\\)
					 (oak-write-char stream c))
				       name)))
		 (else
		  (oak-write-char stream #\|)
		  (string-for-each (lambda (c)
				     (when (or (char=? c #\|) (char=? c #\\))
				       (oak-write-char stream #\\))
				     (oak-write-char stream c))
				   name)
		  (oak-write-char stream #\|))))
	  (else (oak-write-string stream name)))))

(define (print-char c stream)
  (cond ((oak-true? (fluid-ref 'PRINT-ESCAPE))
	 (oak-write-char stream #\#)
	 (oak-write-char stream #\\)
	 (let ((n (char->integer c)))
	   (cond ((and (or (< n 33) (> n 126)) (oak-char->name c))
		  => (lambda (name) (print-symbol name stream)))
		 (else (oak-write-char stream c)))))
	(else (oak-write-char stream c))))

(define (print-number n stream)
  (let ((radix (fluid-ref 'PRINT-RADIX)))
    (oak-write-string stream (string-upcase (number->string n (if (integer? radix) radix 10))))))

;;; Quotelike prefixes the printer abbreviates (reader-macros.oak).
(define (quotelike-prefix sym)
  (case sym
    ((QUOTE) #\')
    ((QUASIQUOTE) #\`)
    (else #f)))

(define *print-length-limit* 1000000)

(define (print-list l stream)
  (let ((level (fluid-ref 'PRINT-LEVEL)))
    (if (and (integer? level) (<= level 0))
	(oak-write-char stream #\#)
	(with-fluids* (list (cons 'PRINT-LEVEL (if (integer? level) (- level 1) '())))
	  (lambda ()
	    (let ((the-car (car l)))
	      (cond ((and (symbol? the-car) (quotelike-prefix the-car)
			  (pair? (cdr l)) (null? (cddr l)))
		     (oak-write-char stream (quotelike-prefix the-car))
		     (oak-print (cadr l) stream))
		    ((and (eq? the-car 'UNQUOTE-SPLICING) (pair? (cdr l)) (null? (cddr l)))
		     (oak-write-string stream ",@")
		     (oak-print (cadr l) stream))
		    ((and (eq? the-car 'UNQUOTE) (pair? (cdr l)) (null? (cddr l)))
		     (oak-write-char stream #\,)
		     (oak-print (cadr l) stream))
		    (else
		     (oak-write-char stream #\()
		     (let loop ((l l) (delimiter? #t) (len (fluid-ref 'PRINT-LENGTH))
				(budget *print-length-limit*))
		       (cond ((null? l) (oak-write-char stream #\)))
			     ((not (pair? l))
			      (oak-write-string stream " . ")
			      (oak-print l stream)
			      (oak-write-char stream #\)))
			     (else
			      (unless delimiter? (oak-write-char stream #\space))
			      (cond ((and (integer? len) (= len 0))
				     (oak-write-string stream "...)"))
				    ((zero? budget)
				     (oak-host-error "PRINT gave up; the list may be circular."))
				    (else
				     (oak-print (car l) stream)
				     (loop (cdr l) #f (if (integer? len) (- len 1) len)
					   (- budget 1)))))))))))))))

(define (print-vector v stream)
  (let ((level (fluid-ref 'PRINT-LEVEL)))
    (if (and (integer? level) (= level 0))
	(oak-write-char stream #\#)
	(begin
	  (oak-write-string stream "#(")
	  (with-fluids* (list (cons 'PRINT-LEVEL (if (integer? level) (- level 1) '())))
	    (lambda ()
	      (let ((n (vector-length v)) (len (fluid-ref 'PRINT-LENGTH)))
		(let loop ((i 0) (len len))
		  (cond ((and (integer? len) (= len 0)) (oak-write-string stream "..."))
			((< i n)
			 (oak-print (vector-ref v i) stream)
			 (when (< i (- n 1)) (oak-write-char stream #\space))
			 (loop (+ i 1) (if (integer? len) (- len 1) len))))))))
	  (oak-write-char stream #\))))))

(define (install-print-natives!)
  (native-op! 'PRINT *operation*)
  (native-op! 'PRINT-LIST-END *operation*)    ; print-list.oak's, for promise.oak
  (native-op! 'HASH-KEY *operation*)          ; hash-table.oak's, for bignum.oak
  (native-locked! 'PRINT-NOISE *operation* (nlambda (c) '()))
  (install-print-methods!))

;;; Re-run whenever the world re-initializes one of these types.
(define (install-print-methods!)
  (native-method! 'PRINT *object*
		  (nlambda (x stream) (oak-format stream "#<~A ~!>"
						  (or (hashq-ref *type-names* (oak-get-type x)) "Object") x) x))
  (native-method! 'PRINT *cons-pair* (nlambda (l stream) (print-list l stream) l))
  (native-method! 'PRINT *null-type* (nlambda (l stream) (oak-write-string stream "()") l))
  (native-method! 'PRINT *fixnum* (nlambda (n stream) (print-number n stream) n))
  (native-method! 'PRINT *truths* (nlambda (x stream) (oak-write-string stream "#T") x))
  (native-method! 'PRINT *locative* (nlambda (x stream) (oak-format stream "#<Loc ~!>" x) x))
  (native-method! 'PRINT *string* (nlambda (s stream) (print-string s stream) s)))

;;; When the world makes NUMBER, INTEGER etc. the fixnum type keeps its
;;; identity, so the native fixnum print method stays.  Fractions:
(define (install-number-print-methods!)
  (let ((rational (global-ref 'RATIONAL)))
    (when (oak-obj? rational)
      (native-method! 'PRINT rational (nlambda (n stream) (print-number n stream) n)))))
