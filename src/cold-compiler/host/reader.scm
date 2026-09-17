;;; This file is part of Oaklisp.
;;;
;;; reader.scm -- an Oaklisp reader for the Guile-hosted bootstrap.
;;;
;;; Reads Oaklisp source syntax (src/world/reader.oak, read-token.oak,
;;; reader-macros.oak, hash-reader.oak, read-char.oak) into host data:
;;; symbols are upcased, #F and () both read as the empty list, #T is
;;; #t, strings are fresh mutable strings.  Only the syntax the world
;;; sources actually use is supported; anything else is an error.
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

;;; The dot token and the "nothing was read" token.

(define oak-dot-token (list 'oak-dot-token))
(define oak-unread-object (list 'oak-unread-object))

(define oak-eof (list 'oak-eof-token))
(define (oak-eof? x) (eq? x oak-eof))

;;; Character syntax classes, as in reader.oak's STANDARD-READ-TABLE.

(define (oak-char-syntax c)
  (cond ((eof-object? c) 'eof)
	((char=? c #\\) 'single-escape)
	((memv c '(#\( #\) #\[ #\] #\{ #\} #\" #\| #\; #\, #\' #\`))
	 'terminating-macro)
	((char=? c #\#) 'nonterminating-macro)
	((<= (char->integer c) 32) 'whitespace)
	((<= (char->integer c) 126) 'constituent)
	(else 'illegal)))

(define (oak-constituent-ish? c)
  ;; What READ-TOKEN keeps accumulating into a token: constituents and
  ;; non-terminating macro characters.
  (let ((s (oak-char-syntax c)))
    (or (eq? s 'constituent) (eq? s 'nonterminating-macro))))

;;; Named characters, in the order read-char.oak defines them; later
;;; entries take precedence when printing, which is why the printer in
;;; printer.scm walks this list from the end.

(define oak-named-chars
  '((NULL . 0) (NUL . 0) (SOH . 1) (STX . 2) (ETX . 3) (EOT . 4)
    (ENQ . 5) (ACK . 6) (BEL . 7) (BS . 8) (HT . 9) (NL . 10) (LF . 10)
    (VT . 11) (NP . 12) (FF . 12) (CR . 13) (SO . 14) (SI . 15)
    (DLE . 16) (DC1 . 17) (DC2 . 18) (DC3 . 19) (DC4 . 20) (NAK . 21)
    (SYN . 22) (ETB . 23) (CAN . 24) (EM . 25) (SUB . 26)
    (ALTMODE . 27) (ESCAPE . 27) (ESC . 27) (FS . 28) (GS . 29)
    (RS . 30) (US . 31) (DELETE . 127) (RUBOUT . 127) (DEL . 127)
    (RETURN . 13) (NEWLINE . 10) (FORM . 12) (PAGE . 12) (TAB . 9)
    (SPACE . 32) (BACKSPACE . 8) (BELL . 7) (FLUID . 22) (COERCER . 25)))

(define (oak-char-name->char sym)
  (let ((p (assq sym oak-named-chars)))
    (and p (integer->char (cdr p)))))

(define (oak-char->name c)
  ;; The most recently defined name for a character code, or #f.
  (let loop ((l (reverse oak-named-chars)))
    (cond ((null? l) #f)
	  ((= (cdar l) (char->integer c)) (caar l))
	  (else (loop (cdr l))))))

;;; The reader proper.  A reader state is just a Guile input port; we
;;; use peek-char/read-char on it.  INPUT-BASE is a parameter so #x and
;;; friends can rebind it.

(define oak-input-base (make-parameter 10))
(define oak-read-suppress (make-parameter #f))
(define oak-features '(OAKLISP SCHEME))

(define (oak-read-error port msg . args)
  (error (apply format #f (string-append "oak reader: " msg) args)))

(define (oak-skip-whitespace port)
  (let ((c (peek-char port)))
    (cond ((eof-object? c) c)
	  ((eq? (oak-char-syntax c) 'whitespace)
	   (read-char port)
	   (oak-skip-whitespace port))
	  (else c))))

(define (oak-digit-value c base)
  ;; Value of C as a digit in BASE, or #f.
  (let* ((n (char->integer c))
	 (v (cond ((and (>= n 48) (<= n 57)) (- n 48))
		  ((and (>= n 65) (<= n 90)) (- n 55))
		  ((and (>= n 97) (<= n 122)) (- n 87))
		  (else #f))))
    (and v (< v base) v)))

;;; Read a token: a number or a symbol.  Follows read-token.oak.
;;; Returns a number, a symbol, or OAK-DOT-TOKEN.

(define (oak-read-token port)
  (let ((base (oak-input-base)))
    (let loop ((chars '()) (escaped? #f))
      (let ((c (peek-char port)))
	(cond ((or (eof-object? c)
		   (memq (oak-char-syntax c) '(whitespace terminating-macro)))
	       (oak-finish-token (reverse chars) escaped? base port))
	      ((eq? (oak-char-syntax c) 'single-escape)
	       (read-char port)
	       (let ((c2 (read-char port)))
		 (if (eof-object? c2)
		     (oak-read-error port "EOF after backslash")
		     (loop (cons c2 chars) #t))))
	      ((eq? (oak-char-syntax c) 'illegal)
	       (oak-read-error port "illegal character ~S in token" c))
	      (else
	       (read-char port)
	       (loop (cons (char-upcase c) chars) escaped?)))))))

(define (oak-finish-token chars escaped? base port)
  (let ((str (list->string chars)))
    (cond ((and (not escaped?) (string=? str "."))
	   oak-dot-token)
	  ((and (not escaped?) (string=? str "..."))
	   (oak-read-error port "the token ... cannot be read unescaped"))
	  ((and (not escaped?) (oak-parse-number str base)) => (lambda (n) n))
	  (else (string->symbol str)))))

(define (oak-parse-number str base)
  ;; ['-']<digit>+['/'<digit>+]  |  ['-']<digit>+'.'<digit>*  |  ['-']'.'<digit>+
  (let* ((len (string-length str))
	 (neg? (and (> len 0) (char=? (string-ref str 0) #\-)))
	 (start (if neg? 1 0)))
    (define (digits from to)
      ;; value of digits str[from,to) or #f if any isn't a digit or empty
      (and (< from to)
	   (let loop ((i from) (n 0))
	     (if (= i to) n
		 (let ((v (oak-digit-value (string-ref str i) base)))
		   (and v (loop (+ i 1) (+ (* n base) v))))))))
    (define (sign x) (if neg? (- x) x))
    (and (< start len)
	 (let ((slash (string-index str #\/))
	       (dot (string-index str #\.)))
	   (cond (slash
		  (let ((n (digits start slash))
			(d (digits (+ slash 1) len)))
		    (and n d (not (zero? d)) (sign (/ n d)))))
		 (dot
		  (let ((whole (if (= dot start) 0 (digits start dot)))
			(frac-len (- len dot 1)))
		    (and whole
			 (or (> dot start) (> frac-len 0))
			 (let ((frac (if (zero? frac-len) 0
					 (digits (+ dot 1) len))))
			   (and frac
				(sign (+ whole (/ frac (expt base frac-len)))))))))
		 (else
		  (let ((n (digits start len)))
		    (and n (sign n)))))))))

;;; Read characters up to (not including) the delimiter, honouring
;;; backslash escapes, as READ-CHARLIST-UNTIL does.

(define (oak-read-string-until port delim)
  (let loop ((chars '()))
    (let ((c (read-char port)))
      (cond ((eof-object? c)
	     (oak-read-error port "EOF while reading until ~S" delim))
	    ((char=? c delim) (list->string (reverse chars)))
	    ((char=? c #\\)
	     (let ((c2 (read-char port)))
	       (if (eof-object? c2)
		   (oak-read-error port "EOF after backslash in string")
		   (loop (cons c2 chars)))))
	    (else (loop (cons c chars)))))))

;;; Read a list up to CLOSE.  Handles dotted tails.

(define (oak-read-list port close)
  (let loop ((items '()))
    (oak-skip-whitespace port)
    (let ((c (peek-char port)))
      (cond ((eof-object? c) (oak-read-error port "EOF inside list"))
	    ((char=? c close)
	     (read-char port)
	     (reverse items))
	    (else
	     (let ((x (oak-read-1 port)))
	       (cond ((eq? x oak-unread-object) (loop items))
		     ((eq? x oak-dot-token)
		      ;; ( . x) reads as x, the way READ-UNTIL takes it.
		      (let ((tail (oak-read port)))
			(when (oak-eof? tail)
			  (oak-read-error port "EOF after dot"))
			(oak-skip-whitespace port)
			(let ((c2 (read-char port)))
			  (unless (and (char? c2) (char=? c2 close))
			    (oak-read-error port "bad dotted list")))
			(append (reverse items) tail)))
		     (else (loop (cons x items))))))))))

(define (oak-read-vector port)
  (list->vector (oak-read-list port #\))))

;;; Read one object; may return OAK-UNREAD-OBJECT for comments etc.

(define (oak-read-1 port)
  (let ((c (oak-skip-whitespace port)))
    (cond
     ((eof-object? c) (read-char port) oak-eof)
     ((char=? c #\() (read-char port) (oak-read-list port #\)))
     ((char=? c #\[) (read-char port) (oak-read-list port #\]))
     ((or (char=? c #\)) (char=? c #\]) (char=? c #\{) (char=? c #\}))
      (read-char port)
      (oak-read-error port "stray ~S" c))
     ((char=? c #\") (read-char port) (oak-read-string-until port #\"))
     ((char=? c #\|) (read-char port)
      (string->symbol (oak-read-string-until port #\|)))
     ((char=? c #\;)
      (let skip ()
	(let ((c (read-char port)))
	  (if (or (eof-object? c) (char=? c #\newline) (char=? c #\return))
	      oak-unread-object
	      (skip)))))
     ((char=? c #\') (read-char port) (list 'QUOTE (oak-read port)))
     ((char=? c #\`) (read-char port) (list 'QUASIQUOTE (oak-read port)))
     ((char=? c #\,) (read-char port)
      (if (eqv? (peek-char port) #\@)
	  (begin (read-char port) (list 'UNQUOTE-SPLICING (oak-read port)))
	  (list 'UNQUOTE (oak-read port))))
     ((char=? c #\#) (read-char port) (oak-read-hash port))
     (else (oak-read-token port)))))

(define (oak-read-hash port)
  ;; After the #: optional decimal argument, then dispatch character.
  (let loop ((arg #f))
    (let ((c (read-char port)))
      (cond ((eof-object? c) (oak-read-error port "EOF after #"))
	    ((oak-digit-value c 10)
	     => (lambda (v) (loop (+ (* (or arg 0) 10) v))))
	    (else
	     (let ((c (char-upcase c)))
	       (define (no-arg)
		 (when arg (oak-read-error port "#~A~A takes no argument" arg c)))
	       (case c
		 ((#\\) (no-arg) (oak-read-char-literal port))
		 ((#\T) (no-arg) #t)
		 ((#\F) (no-arg) '())
		 ((#\*) (no-arg) (list 'FLUID (oak-read port)))
		 ((#\^) (no-arg) (list 'COERCER (oak-read port)))
		 ((#\() (oak-read-hash-vector port arg))
		 ((#\|) (no-arg) (oak-skip-block-comment port) oak-unread-object)
		 ((#\;) (no-arg)
		  (parameterize ((oak-read-suppress #t)) (oak-read port))
		  oak-unread-object)
		 ((#\B) (no-arg) (parameterize ((oak-input-base 2)) (oak-read port)))
		 ((#\O) (no-arg) (parameterize ((oak-input-base 8)) (oak-read port)))
		 ((#\D) (no-arg) (parameterize ((oak-input-base 10)) (oak-read port)))
		 ((#\X) (no-arg) (parameterize ((oak-input-base 16)) (oak-read port)))
		 ((#\R) (unless arg (oak-read-error port "#R needs an argument"))
		  (parameterize ((oak-input-base arg)) (oak-read port)))
		 ((#\[) (no-arg) (oak-read-hash-bracket port))
		 ((#\+) (no-arg)
		  (let ((feat (oak-read port)))
		    (if (oak-feature? feat)
			(oak-read port)
			(begin (parameterize ((oak-read-suppress #t)) (oak-read port))
			       oak-unread-object))))
		 ((#\-) (no-arg)
		  (let ((feat (oak-read port)))
		    (if (oak-feature? feat)
			(begin (parameterize ((oak-read-suppress #t)) (oak-read port))
			       oak-unread-object)
			(oak-read port))))
		 ((#\E) (no-arg) (oak-read port))
		 (else (oak-read-error port "unknown # syntax #~A" c)))))))))

(define (oak-feature? f)
  (cond ((symbol? f) (and (memq f oak-features) #t))
	((and (pair? f) (eq? (car f) 'AND)) (every oak-feature? (cdr f)))
	((and (pair? f) (eq? (car f) 'OR)) (any oak-feature? (cdr f)))
	((and (pair? f) (eq? (car f) 'NOT)) (not (oak-feature? (cadr f))))
	(else (error "bad feature specifier" f))))

(define (oak-read-hash-vector port arg)
  (let* ((items (oak-read-list port #\)))
	 (len (length items))
	 (n (or arg len)))
    (when (< n len) (oak-read-error port "#~A( has too many elements" n))
    (let ((v (make-vector n '())))
      (let loop ((i 0) (l items) (last '()))
	(when (< i n)
	  (let ((it (if (pair? l) (car l) last)))
	    (vector-set! v i it)
	    (loop (+ i 1) (if (pair? l) (cdr l) '()) it))))
      v)))

(define (oak-skip-block-comment port)
  ;; #| ... |# with nesting.
  (let loop ((level 1))
    (let ((c (read-char port)))
      (cond ((eof-object? c) (oak-read-error port "EOF in #| comment"))
	    ((char=? c #\#)
	     (if (eqv? (peek-char port) #\|)
		 (begin (read-char port) (loop (+ level 1)))
		 (loop level)))
	    ((char=? c #\|)
	     (if (eqv? (peek-char port) #\#)
		 (begin (read-char port)
			(if (= level 1) #t (loop (- level 1))))
		 (loop level)))
	    (else (loop level))))))

(define (oak-read-hash-bracket port)
  ;; #[symbol "name"]
  (let* ((selector (oak-read port))
	 (items (oak-read-list port #\])))
    (cond ((eq? selector 'SYMBOL)
	   (unless (and (= (length items) 1) (string? (car items)))
	     (oak-read-error port "malformed #[symbol ...]"))
	   (string->symbol (car items)))
	  (else (oak-read-error port "unknown #[~A ...] construct" selector)))))

;;; #\c: the character after the backslash is taken literally, then any
;;; further constituent characters extend the token; a one character
;;; token is that character, otherwise it names a character.

(define (oak-read-char-literal port)
  (let ((first (read-char port)))
    (when (eof-object? first) (oak-read-error port "EOF after #\\"))
    (let loop ((chars (list first)))
      (let ((c (peek-char port)))
	(if (and (char? c) (oak-constituent-ish? c))
	    (begin (read-char port) (loop (cons (char-upcase c) chars)))
	    (let ((tok (reverse chars)))
	      (if (null? (cdr tok))
		  (car tok)
		  (let* ((name (string->symbol
				(list->string (cons (char-upcase (car tok))
						    (cdr tok)))))
			 (ch (oak-char-name->char name)))
		    (or ch
			(oak-read-error port "unknown character name ~A" name))))))))))

;;; The entry point: read one object, or OAK-EOF at end of input.

(define (oak-read port)
  (let loop ()
    (let ((x (oak-read-1 port)))
      (if (eq? x oak-unread-object)
	  (loop)
	  x))))

(define (oak-read-all port)
  (let loop ((forms '()))
    (let ((x (oak-read port)))
      (if (oak-eof? x)
	  (reverse forms)
	  (loop (cons x forms))))))

(define (oak-read-file-forms filename)
  (call-with-input-file filename oak-read-all))
