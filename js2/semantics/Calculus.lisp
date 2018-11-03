;;; ***** BEGIN LICENSE BLOCK *****
;;; Version: MPL 1.1/GPL 2.0/LGPL 2.1
;;;
;;; The contents of this file are subject to the Mozilla Public License Version
;;; 1.1 (the "License"); you may not use this file except in compliance with
;;; the License. You may obtain a copy of the License at
;;; http://www.mozilla.org/MPL/
;;;
;;; Software distributed under the License is distributed on an "AS IS" basis,
;;; WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
;;; for the specific language governing rights and limitations under the
;;; License.
;;;
;;; The Original Code is the Language Design and Prototyping Environment.
;;;
;;; The Initial Developer of the Original Code is
;;; Netscape Communications Corporation.
;;; Portions created by the Initial Developer are Copyright (C) 1999-2002
;;; the Initial Developer. All Rights Reserved.
;;;
;;; Contributor(s):
;;;   Waldemar Horwat <waldemar@acm.org>
;;;
;;; Alternatively, the contents of this file may be used under the terms of
;;; either the GNU General Public License Version 2 or later (the "GPL"), or
;;; the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
;;; in which case the provisions of the GPL or the LGPL are applicable instead
;;; of those above. If you wish to allow use of your version of this file only
;;; under the terms of either the GPL or the LGPL, and not to allow others to
;;; use your version of this file under the terms of the MPL, indicate your
;;; decision by deleting the provisions above and replace them with the notice
;;; and other provisions required by the GPL or the LGPL. If you do not delete
;;; the provisions above, a recipient may use your version of this file under
;;; the terms of any one of the MPL, the GPL or the LGPL.
;;;
;;; ***** END LICENSE BLOCK *****


;;;
;;; ECMAScript semantic calculus
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;

(declaim (optimize (debug 3))) ;*****
(defvar *trace-variables* nil)


; Different Common Lisp implementations map their floating-point types to Common Lisp types differently.
; Change the code below to encode which ones correspond to IEEE single (float32) and double (float64) values.

(defconstant *float32-type* #+mcl 'short-float #-mcl 'single-float)
(deftype float32 ()
         '(or
           #+mcl (and short-float (not (eql 0.0s0)) (not (eql -0.0s0)))
           #-mcl (and single-float (not (eql 0.0f0)) (not (eql -0.0f0)))
           (member :+zero32 :-zero32 :+infinity32 :-infinity32 :nan32)))
; The exponent character emitted by (with-standard-io-syntax (format "~E" x)) when printing an IEEE single value
(defconstant *float32-exponent-char* #+mcl #\S #-mcl #\f)


(defconstant *float64-type* 'double-float)
(deftype float64 ()
         '(or
           (and double-float (not (eql 0.0)) (not (eql -0.0)))
           (member :+zero64 :-zero64 :+infinity64 :-infinity64 :nan64)))
; The exponent character emitted by (with-standard-io-syntax (format "~E" x)) when printing an IEEE double value
(defconstant *float64-exponent-char* #+mcl #\E #-mcl #\d)



#+mcl (dolist (indent-spec '((? . 1) (/*/ . 1) (lisp-call . 3) (throw-error . 1) (apply . 1) (funcall . 1) (declare-action . 5) (production . 3) (rule . 2) (function . 2)
                             (define . 2) (deftag . 1) (defrecord . 1) (deftype . 1) (tag . 1) (%text . 1)
                             (assert . 1) (var . 2) (const . 2) (rwhen . 1) (while . 1) (for-each . 2)
                             (new . 1) (set-field . 1) (list-set-of . 1) (%list-set-of . 1) (:narrow . 1) (:select . 1)))
        (pushnew indent-spec ccl:*fred-special-indent-alist* :test #'equal))


; Return x/y, ensuring that it is an integer.
(defun int/ (x y)
  (let ((q (/ x y)))
    (if (integerp q)
      q
      (error "int/ must produce an integer"))))


; Return the floor of log10 of rational value r
(defun floor-log10 (r)
  (cond
   ((or (not (rationalp r)) (<= r 0)) (error "Bad argument ~S to floor-log10" r))
   ((>= r 1)
    (do ((result 0 (1+ result)))
        ((< r 10) result)
      (setq r (floor r 10))))
   (t
    (do ((result 0 (1- result)))
        ((>= r 1) result)
      (setq r (* r 10))))))


; Return the boolean exclusive or of the arguments.
(defun xor (&rest as)
  (let ((result nil))
    (dolist (a as)
      (when a
        (setq result (not result))))
    result))


; A boolean version of = that works on any nil/non-nil values.
(declaim (inline boolean=))
(defun boolean= (a b)
  (eq (not a) (not b)))


; Complement of eq.
(declaim (inline not-eq))
(defun not-eq (a b)
  (not (eq a b)))


(defun digit-char-36 (char)
  (assert-non-null (digit-char-p char 36)))


; Call map on each element of the list l.  If map returns true, call filter on that element.  Gather the results
; of the calls to filter into a new list and return that list.
(defun filter-map-list (filter map l)
  (let ((results nil))
    (dolist (e l)
      (when (funcall filter e)
        (push (funcall map e) results)))
    (nreverse results)))

; Call map on each element of the sequence s.  If map returns true, call filter on that element.  Gather the results
; of the calls to filter into a new sequence of type result-type and return that sequence.
(defun filter-map (result-type filter map s)
  (let ((results nil))
    (map nil
         #'(lambda (e)
             (when (funcall filter e)
               (push (funcall map e) results)))
         s)
    (coerce result-type (nreverse results))))


; Return the same symbol in the keyword package.
(defun find-keyword (symbol)
  (assert-non-null (find-symbol (string symbol) (find-package :keyword))))


;;; ------------------------------------------------------------------------------------------------------
;;; DOUBLE-PRECISION FLOATING-POINT NUMBERS

(declaim (inline finite64?))
(defun finite64? (n)
  (and (typep n *float64-type*) (not (zerop n))))

(defun float64? (n)
  (or (finite64? n) (member n '(:+zero64 :-zero64 :+infinity64 :-infinity64 :nan64))))

; Evaluate expr.  If it evaluates successfully, return its value except if it evaluates to
; +0.0 or -0.0, in which case return :+zero64 (but not :-zero64).
; If evaluating expr overflows, evaluate sign; if it returns a positive value, return :+infinity64;
; otherwise return :-infinity64.  sign should not return zero.
(defmacro handle-overflow64 (expr &body sign)
  (let ((x (gensym)))
    `(handler-case (let ((,x ,expr))
                     (if (zerop ,x) :+zero64 ,x))
       (floating-point-overflow () (if (minusp (progn ,@sign)) :-infinity64 :+infinity64)))))


(defun rational-to-float64 (r)
  (let ((f (handle-overflow64 (coerce r *float64-type*)
             r)))
    (if (eq f :+zero64)
      (if (minusp r) :-zero64 :+zero64)
      f)))


(defun float32-to-float64 (x)
  (case x
    (:+zero32 :+zero64)
    (:-zero32 :-zero64)
    (:+infinity32 :+infinity64)
    (:-infinity32 :-infinity64)
    (:nan32 :nan64)
    (t (coerce x *float64-type*))))


; Return true if n is +0 or -0 and false otherwise.
(declaim (inline float64-is-zero))
(defun float64-is-zero (n)
  (or (eq n :+zero64) (eq n :-zero64)))


; Return true if n is NaN and false otherwise.
(declaim (inline float64-is-nan))
(defun float64-is-nan (n)
  (eq n :nan64))


; Return true if n is :+infinity64 or :-infinity64 and false otherwise.
(declaim (inline float64-is-infinite))
(defun float64-is-infinite (n)
  (or (eq n :+infinity64) (eq n :-infinity64)))


; Truncate n to the next lower integer.  Signal an error if n isn't finite.
(defun truncate-finite-float64 (n)
  (if (float64-is-zero n)
    0
    (truncate n)))


; Return:
;   :less if n<m;
;   :equal if n=m;
;   :greater if n>m.
(defun rational-compare (n m)
  (cond
   ((< n m) :less)
   ((> n m) :greater)
   (t :equal)))


; Return:
;   :less if n<m;
;   :equal if n=m;
;   :greater if n>m;
;   :unordered if either n or m is :nan64.
(defun float64-compare (n m)
  (when (float64-is-zero n)
    (setq n 0.0))
  (when (float64-is-zero m)
    (setq m 0.0))
  (cond
   ((or (float64-is-nan n) (float64-is-nan m)) :unordered)
   ((eql n m) :equal)
   ((or (eq n :+infinity64) (eq m :-infinity64)) :greater)
   ((or (eq m :+infinity64) (eq n :-infinity64)) :less)
   ((< n m) :less)
   ((> n m) :greater)
   (t :equal)))


; Return
;    1 if n is +0.0, :+infinity64, or any positive floating-point number;
;   -1 if n is -0.0, :-infinity64, or any positive floating-point number;
;    0 if n is :nan64.
(defun float64-sign (n)
  (case n
    ((:+zero64 :+infinity64) 1)
    ((:-zero64 :-infinity64) -1)
    (:nan64 0)
    (t (round (float-sign n)))))


; Return
;   0 if either n or m is :nan64;
;   1 if n and m have the same float64-sign;
;  -1 if n and m have different float64-signs.
(defun float64-sign-xor (n m)
  (* (float64-sign n) (float64-sign m)))


; Return d truncated towards zero into a 32-bit integer.  Overflows wrap around.
(defun float64-to-uint32 (d)
  (case d
    ((:+zero64 :-zero64 :+infinity64 :-infinity64 :nan64) 0)
    (t (mod (truncate d) #x100000000))))


; Return the absolute value of n.
(defun float64-abs (n)
  (case n
    ((:+zero64 :-zero64) :+zero64)
    ((:+infinity64 :-infinity64) :+infinity64)
    (:nan64 :nan64)
    (t (abs n))))


; Return -n.
(defun float64-neg (n)
  (case n
    (:+zero64 :-zero64)
    (:-zero64 :+zero64)
    (:+infinity64 :-infinity64)
    (:-infinity64 :+infinity64)
    (:nan64 :nan64)
    (t (- n))))


; Return n+m.
(defun float64-add (n m)
  (case n
    (:+zero64 (if (eq m :-zero64) :+zero64 m))
    (:-zero64 m)
    (:+infinity64 (case m
                    ((:-infinity64 :nan64) :nan64)
                    (t :+infinity64)))
    (:-infinity64 (case m
                    ((:+infinity64 :nan64) :nan64)
                    (t :-infinity64)))
    (:nan64 :nan64)
    (t (case m
         ((:+zero64 :-zero64) n)
         (:+infinity64 :+infinity64)
         (:-infinity64 :-infinity64)
         (:nan64 :nan64)
         (t (handle-overflow64 (+ n m)
              (let ((n-sign (float-sign n))
                    (m-sign (float-sign m)))
                (assert-true (= n-sign m-sign)) ;If the signs are opposite, we can't overflow.
                n-sign)))))))


; Return n-m.
(defun float64-subtract (n m)
  (float64-add n (float64-neg m)))


; Return n*m.
(defun float64-multiply (n m)
  (let ((sign (float64-sign-xor n m))
        (n (float64-abs n))
        (m (float64-abs m)))
    (let ((result (cond
                   ((zerop sign) :nan64)
                   ((eq n :+infinity64) (if (eq m :+zero64) :nan64 :+infinity64))
                   ((eq m :+infinity64) (if (eq n :+zero64) :nan64 :+infinity64))
                   ((or (eq n :+zero64) (eq m :+zero64)) :+zero64)
                   (t (handle-overflow64 (* n m) 1)))))
      (if (minusp sign)
        (float64-neg result)
        result))))


; Return n/m.
(defun float64-divide (n m)
  (let ((sign (float64-sign-xor n m))
        (n (float64-abs n))
        (m (float64-abs m)))
    (let ((result (cond
                   ((zerop sign) :nan64)
                   ((eq n :+infinity64) (if (eq m :+infinity64) :nan64 :+infinity64))
                   ((eq m :+infinity64) :+zero64)
                   ((eq m :+zero64) (if (eq n :+zero64) :nan64 :+infinity64))
                   ((eq n :+zero64) :+zero64)
                   (t (handle-overflow64 (/ n m) 1)))))
      (if (minusp sign)
        (float64-neg result)
        result))))


; Return n%m, using the ECMAScript definition of %.
(defun float64-remainder (n m)
  (cond
   ((or (float64-is-nan n) (float64-is-nan m) (float64-is-infinite n) (float64-is-zero m)) :nan64)
   ((or (float64-is-infinite m) (float64-is-zero n)) n)
   (t (let ((result (float (rem (rational n) (rational m)))))
        (if (zerop result)
          (if (minusp n) :-zero64 :+zero64)
          result)))))


; s should be a string of decimal digits optionally preceded by a plus or minus sign.  Return s's
; value as an integer.
(defun string-to-integer (s)
  (let ((p 0)
        (sign 1)
        (n 0)
        (length (length s)))
    (case (char s 0)
      (#\+ (setq p 1))
      (#\- (setq sign -1) (setq p 1)))
    (assert (< p length))
    (do ()
        ((= p length))
      (setq n (+ (* n 10) (digit-char-p (char s p))))
      (incf p))
    (* sign n)))


; The number x should be a positive floating-point number that uses the given exponent-char when
; printed in exponential notation.
; Return two values:
;   A significand s, expressed as a string of decimal digits, the last of which is nonzero;
;   An exponent e, such that s*10^(e+1-length(s)), when converted to x's type, is the original number x.
;
; ***** Assumes that Common Lisp implements proper rounding and round-tripping when formatting a floating-point number.
(defun decompose-positive-float (x exponent-char)
  (unless (> x 0)
    (error "decompose-positive-float can only be called on a positive number"))
  (cond
   ((eql x 5e-324) (values "5" -324))
   ((eql x #+mcl 1s-45 #-mcl 1f-45) (values "1" -45))
   (t
    (let* ((str (format nil "~E" x))
           (p (position exponent-char str)))
      (unless (and p (eql (char str 1) #\.))
        (error "Internal problem in decompose-float.  Check the settings of *float32-exponent-char* and *float64-exponent-char* for your platform."))
      (let ((s-first (subseq str 0 1))
            (s-rest (subseq str 2 p)))
        (values
         (if (string= s-rest "0") s-first (concatenate 'string s-first s-rest))
         (string-to-integer (subseq str (1+ p)))))))))


; The number x should be a positive floating-point number that uses the given exponent-char when
; printed in exponential notation.
; Return two values:
;   A significand s, expressed as a string of decimal digits possibly containing a decimal point;
;   An exponent e, such that s*10^e is the absolute value of the original number.  e is nil if it would be zero.
; The number is expressed with e being nil if its absolute value is between 1e-6 inclusive and 1e21 exclusive
; unless always-show-exponent is true.
; If always-show-point is true, then s always contains either an exponent or a decimal point with at least one digit after it.
(defun positive-float-to-string-components (x exponent-char always-show-point always-show-exponent)
  (multiple-value-bind (s e) (decompose-positive-float x exponent-char)
    (let ((k (length s)))
      (cond
       ((and (<= k (1+ e) 21) (not always-show-exponent))
        (setq s (concatenate 'string s (make-string (- (1+ e) k) :initial-element #\0)))
        (when always-show-point
          (setq s (concatenate 'string s ".0")))
        (setq e nil))
       ((and (<= 0 e 20) (not always-show-exponent))
        (setq s (concatenate 'string (subseq s 0 (1+ e)) "." (subseq s (1+ e))))
        (setq e nil))
       ((and (<= -6 e -1) (not always-show-exponent))
        (setq s (concatenate 'string "0." (make-string (- (1+ e)) :initial-element #\0) s))
        (setq e nil))
       ((= k 1))
       (t (setq s (concatenate 'string (subseq s 0 1) "." (subseq s 1)))))
      (values s e))))


; The number x should be a positive finite64.
; Return two values:
;   A significand s, expressed as a string of decimal digits, the last of which is nonzero;
;   An exponent e, such that s*10^(e+1-length(s)), when converted to a float64, is the original number x.
(defun decompose-positive-float64 (x)
  (decompose-positive-float x *float64-exponent-char*))



;;; ------------------------------------------------------------------------------------------------------
;;; SINGLE-PRECISION FLOATING-POINT NUMBERS

(declaim (inline finite32?))
(defun finite32? (n)
  (and (typep n *float32-type*) (not (zerop n))))

(defun float32? (n)
  (or (finite32? n) (member n '(:+zero32 :-zero32 :+infinity32 :-infinity32 :nan32))))

; Evaluate expr.  If it evaluates successfully, return its value except if it evaluates to
; +0.0 or -0.0, in which case return :+zero32 (but not :-zero32).
; If evaluating expr overflows, evaluate sign; if it returns a positive value, return :+infinity32;
; otherwise return :-infinity32.  sign should not return zero.
(defmacro handle-overflow32 (expr &body sign)
  (let ((x (gensym)))
    `(handler-case (let ((,x ,expr))
                     (if (zerop ,x) :+zero32 ,x))
       (floating-point-overflow () (if (minusp (progn ,@sign)) :-infinity32 :+infinity32)))))


(defun rational-to-float32 (r)
  (let ((f (handle-overflow32 (coerce r *float32-type*)
             r)))
    (if (eq f :+zero32)
      (if (minusp r) :-zero32 :+zero32)
      f)))


; Return true if n is +0 or -0 and false otherwise.
(declaim (inline float32-is-zero))
(defun float32-is-zero (n)
  (or (eq n :+zero32) (eq n :-zero32)))


; Return true if n is NaN and false otherwise.
(declaim (inline float32-is-nan))
(defun float32-is-nan (n)
  (eq n :nan32))


; Return true if n is :+infinity32 or :-infinity32 and false otherwise.
(declaim (inline float32-is-infinite))
(defun float32-is-infinite (n)
  (or (eq n :+infinity32) (eq n :-infinity32)))


; Truncate n to the next lower integer.  Signal an error if n isn't finite.
(defun truncate-finite-float32 (n)
  (if (float32-is-zero n)
    0
    (truncate n)))


; Return:
;   :less if n<m;
;   :equal if n=m;
;   :greater if n>m;
;   :unordered if either n or m is :nan32.
(defun float32-compare (n m)
  (when (float32-is-zero n)
    (setq n (coerce 0.0 *float32-type*)))
  (when (float32-is-zero m)
    (setq m (coerce 0.0 *float32-type*)))
  (cond
   ((or (float32-is-nan n) (float32-is-nan m)) :unordered)
   ((eql n m) :equal)
   ((or (eq n :+infinity32) (eq m :-infinity32)) :greater)
   ((or (eq m :+infinity32) (eq n :-infinity32)) :less)
   ((< n m) :less)
   ((> n m) :greater)
   (t :equal)))


; Return
;    1 if n is +0.0, :+infinity32, or any positive floating-point number;
;   -1 if n is -0.0, :-infinity32, or any positive floating-point number;
;    0 if n is :nan32.
(defun float32-sign (n)
  (case n
    ((:+zero32 :+infinity32) 1)
    ((:-zero32 :-infinity32) -1)
    (:nan32 0)
    (t (round (float-sign n)))))


; Return
;   0 if either n or m is :nan32;
;   1 if n and m have the same float32-sign;
;  -1 if n and m have different float32-signs.
(defun float32-sign-xor (n m)
  (* (float32-sign n) (float32-sign m)))


; Return the absolute value of n.
(defun float32-abs (n)
  (case n
    ((:+zero32 :-zero32) :+zero32)
    ((:+infinity32 :-infinity32) :+infinity32)
    (:nan32 :nan32)
    (t (abs n))))


; Return -n.
(defun float32-neg (n)
  (case n
    (:+zero32 :-zero32)
    (:-zero32 :+zero32)
    (:+infinity32 :-infinity32)
    (:-infinity32 :+infinity32)
    (:nan32 :nan32)
    (t (- n))))


; Return n+m.
(defun float32-add (n m)
  (case n
    (:+zero32 (if (eq m :-zero32) :+zero32 m))
    (:-zero32 m)
    (:+infinity32 (case m
                    ((:-infinity32 :nan32) :nan32)
                    (t :+infinity32)))
    (:-infinity32 (case m
                    ((:+infinity32 :nan32) :nan32)
                    (t :-infinity32)))
    (:nan32 :nan32)
    (t (case m
         ((:+zero32 :-zero32) n)
         (:+infinity32 :+infinity32)
         (:-infinity32 :-infinity32)
         (:nan32 :nan32)
         (t (handle-overflow32 (+ n m)
              (let ((n-sign (float-sign n))
                    (m-sign (float-sign m)))
                (assert-true (= n-sign m-sign)) ;If the signs are opposite, we can't overflow.
                n-sign)))))))


; Return n-m.
(defun float32-subtract (n m)
  (float32-add n (float32-neg m)))


; Return n*m.
(defun float32-multiply (n m)
  (let ((sign (float32-sign-xor n m))
        (n (float32-abs n))
        (m (float32-abs m)))
    (let ((result (cond
                   ((zerop sign) :nan32)
                   ((eq n :+infinity32) (if (eq m :+zero32) :nan32 :+infinity32))
                   ((eq m :+infinity32) (if (eq n :+zero32) :nan32 :+infinity32))
                   ((or (eq n :+zero32) (eq m :+zero32)) :+zero32)
                   (t (handle-overflow32 (* n m) 1)))))
      (if (minusp sign)
        (float32-neg result)
        result))))


; Return n/m.
(defun float32-divide (n m)
  (let ((sign (float32-sign-xor n m))
        (n (float32-abs n))
        (m (float32-abs m)))
    (let ((result (cond
                   ((zerop sign) :nan32)
                   ((eq n :+infinity32) (if (eq m :+infinity32) :nan32 :+infinity32))
                   ((eq m :+infinity32) :+zero32)
                   ((eq m :+zero32) (if (eq n :+zero32) :nan32 :+infinity32))
                   ((eq n :+zero32) :+zero32)
                   (t (handle-overflow32 (/ n m) 1)))))
      (if (minusp sign)
        (float32-neg result)
        result))))


; Return n%m, using the ECMAScript definition of %.
(defun float32-remainder (n m)
  (cond
   ((or (float32-is-nan n) (float32-is-nan m) (float32-is-infinite n) (float32-is-zero m)) :nan32)
   ((or (float32-is-infinite m) (float32-is-zero n)) n)
   (t (let ((result (float (rem (rational n) (rational m)))))
        (if (zerop result)
          (if (minusp n) :-zero32 :+zero32)
          result)))))


; s should be a string of decimal digits optionally preceded by a plus or minus sign.  Return s's
; value as an integer.
(defun string-to-integer (s)
  (let ((p 0)
        (sign 1)
        (n 0)
        (length (length s)))
    (case (char s 0)
      (#\+ (setq p 1))
      (#\- (setq sign -1) (setq p 1)))
    (assert (< p length))
    (do ()
        ((= p length))
      (setq n (+ (* n 10) (digit-char-p (char s p))))
      (incf p))
    (* sign n)))


; The number x should be a positive finite32.
; Return two values:
;   A significand s, expressed as a string of decimal digits, the last of which is nonzero;
;   An exponent e, such that s*10^(e+1-length(s)), when converted to a float32, is the original number x.
(defun decompose-positive-float32 (x)
  (decompose-positive-float x *float32-exponent-char*))


;;; ------------------------------------------------------------------------------------------------------
;;; CHARACTER UTILITIES

(defun integer-to-supplementary-char (code-point)
  (unless (<= #x10000 code-point #x10FFFF)
    (error "Bad Unicode supplementary-char code point ~S" code-point))
  (cons :supplementary-char code-point))

(defun integer-to-char21 (code-point)
  (unless (<= 0 code-point #x10FFFF)
    (error "Bad Unicode code point ~S" code-point))
  (if (<= code-point #xFFFF)
    (code-char code-point)
    (cons :supplementary-char code-point)))

(defun char21-to-integer (ch)
  (cond
   ((characterp ch) (char-code ch))
   ((eq (car ch) :supplementary-char) (cdr ch))))

(defun char21< (a b)
  (< (char21-to-integer a) (char21-to-integer b)))

(defun char21<= (a b)
  (<= (char21-to-integer a) (char21-to-integer b)))

(defun char21> (a b)
  (> (char21-to-integer a) (char21-to-integer b)))

(defun char21>= (a b)
  (>= (char21-to-integer a) (char21-to-integer b)))


;;; ------------------------------------------------------------------------------------------------------
;;; SET UTILITIES

(defun integer-set-min (intset)
  (or (intset-min intset)
      (error "min of empty integer-set")))

(defun char16-set-min (intset)
  (code-char (or (intset-min intset)
                 (error "min of empty char16-set"))))


(defun integer-set-max (intset)
  (or (intset-max intset)
      (error "max of empty integer-set")))

(defun char16-set-max (intset)
  (code-char (or (intset-max intset)
                 (error "max of empty char16-set"))))


;;; ------------------------------------------------------------------------------------------------------
;;; CODE GENERATION

#+mcl(defvar *deferred-functions*)

(defun quiet-compile (name definition)
  #-mcl(compile name definition)
  #+mcl(handler-bind ((ccl::undefined-function-reference
                       #'(lambda (condition)
                           (setq *deferred-functions* (append (slot-value condition 'ccl::args) *deferred-functions*))
                           (muffle-warning condition))))
         (compile name definition)))


(defmacro defer-mcl-warnings (&body body)
  #-mcl`(with-compilation-unit () ,@body)
  #+mcl`(let ((*deferred-functions* nil))
          (multiple-value-prog1
            (with-compilation-unit () ,@body)
            (let ((missing-functions (remove-if #'fboundp *deferred-functions*)))
              (when missing-functions
                (warn "Undefined functions: ~S" missing-functions))))))


; If args has no elements, return the value of empty.
; If args has one element, return that element.
; If args has two or more elements, return (op . args).
(defun gen-poly-op (op empty args)
  (cond
   ((endp args) empty)
   ((endp (cdr args)) (car args))
   (t (cons op args))))


; Return `(progn ,@statements), optimizing where possible.
(defun gen-progn (statements)
  (cond
   ((endp statements) nil)
   ((and (endp (cdr statements))
         (let ((first-statement (first statements)))
           (not (and (consp first-statement)
                     (eq (first first-statement) 'declare)))))
    (first statements))
   (t (cons 'progn statements))))


; Return (nth <n> <code>), optimizing if possible.
(defun gen-nth-code (n code)
  (let ((abbrev (assoc n '((0 . first) (1 . second) (2 . third) (3 . fourth) (4 . fifth) (5 . sixth) (6 . seventh) (7 . eighth) (8 . ninth) (9 . tenth)))))
    (if abbrev
      (list (cdr abbrev) code)
      (list 'nth n code))))


; Return code that tests whether the result of evaluating code is a member of the given
; list of symbols using the test eq.
(defun gen-member-test (code symbols)
  (assert-true symbols)
  (if (cdr symbols)
    (list 'member code (list 'quote symbols) :test '#'eq)
    (list 'eq code (let ((symbol (car symbols)))
                     (if (constantp symbol)
                       symbol
                       (list 'quote symbol))))))


; Return `(funcall ,function-value ,@arg-values), optimizing where possible.
(defun gen-apply (function-value &rest arg-values)
  (let ((stripped-function-value (simple-strip-function function-value)))
    (cond
     (stripped-function-value
      (if (and (consp stripped-function-value)
               (eq (first stripped-function-value) 'lambda)
               (listp (second stripped-function-value))
               (cddr stripped-function-value)
               (every #'(lambda (arg)
                          (and (identifier? arg)
                               (not (eql (first-symbol-char arg) #\&))))
                      (second stripped-function-value)))
        (let ((function-args (second stripped-function-value))
              (function-body (cddr stripped-function-value)))
          (assert-true (= (length function-args) (length arg-values)))
          (if function-args
            (list* 'let
                   (mapcar #'list function-args arg-values)
                   function-body)
            (gen-progn function-body)))
        (cons stripped-function-value arg-values)))
     ((and (consp function-value)
           (eq (first function-value) 'symbol-function)
           (null (cddr function-value))
           (consp (cadr function-value))
           (eq (caadr function-value) 'quote)
           (identifier? (cadadr function-value))
           (null (cddadr function-value)))
      (cons (cadadr function-value) arg-values))
     (t (list* 'funcall function-value arg-values)))))


; Return `#'(lambda ,args (declare (ignore-if-unused ,@args)) ,body-code), optimizing
; where possible.
(defun gen-lambda (args body-code)
  (if args
    `#'(lambda ,args (declare (ignore-if-unused . ,args)) ,body-code)
    `#'(lambda () ,body-code)))


; If expr is a lambda-expression, return an equivalent expression that has
; the given name (which may be a symbol or a string; if it's a string, it is interned
; in the given package).  Otherwise, return expr unchanged.
; Attaching a name to lambda-expressions helps in debugging code by identifying
; functions in debugger backtraces.
(defun name-lambda (expr name &optional package)
  (if (and (consp expr)
           (eq (first expr) 'function)
           (consp (rest expr))
           (consp (second expr))
           (eq (first (second expr)) 'lambda)
           (null (cddr expr)))
    (let ((name (if (symbolp name)
                  name
                  (intern name package))))
      ;Avoid trouble when name is a lisp special form like if or lambda.
      (when (special-form-p name)
        (setq name (gensym name)))
      `(flet ((,name ,@(rest (second expr))))
         #',name))
    expr))


; Intern n symbols in the current package with names <prefix>0, <prefix>1, ...,
; <prefix>n-1, where <prefix> is the value of the prefix string.
; Return a list of these n symbols concatenated to the front of rest.
(defun intern-n-vars-with-prefix (prefix n rest)
  (if (zerop n)
    rest
    (intern-n-vars-with-prefix prefix (1- n) (cons (intern (format nil "~A~D" prefix n)) rest))))


; Make a new function with the given name.  The function takes n-args arguments and applies them to the
; function whose source code is in expr.  Return the source code for the function.
(defun gen-defun (expr name n-args)
  (when (special-form-p name)
    (error "Can't call make-defun on ~S" name))
  (if (and (consp expr)
           (eq (first expr) 'function)
           (consp (rest expr))
           (second expr)
           (null (cddr expr))
           (let ((stripped-expr (second expr)))
             (and (consp stripped-expr)
                  (eq (first stripped-expr) 'lambda)
                  (listp (second stripped-expr))
                  (cddr stripped-expr)
                  (every #'(lambda (arg)
                             (and (identifier? arg)
                                  (not (eql (first-symbol-char arg) #\&))))
                         (second stripped-expr)))))
    (let* ((stripped-expr (second expr))
           (function-args (second stripped-expr))
           (function-body (cddr stripped-expr)))
      (assert-true (= (length function-args) n-args))
      (list* 'defun name function-args function-body))
    (let ((args (intern-n-vars-with-prefix "_" n-args nil)))
      (list 'defun name args (apply #'gen-apply expr args)))))


; If code has the form (function <expr>), return <expr>; otherwise, return nil.
(defun simple-strip-function (code)
  (when (and (consp code)
             (eq (first code) 'function)
             (consp (rest code))
             (second code)
             (null (cddr code)))
    (assert-non-null (second code))))


; Strip the (function ...) covering from expr, leaving only a plain lambda expression.
(defun strip-function (expr name n-args)
  (when (special-form-p name)
    (error "Can't call make-defun on ~S" name))
  (if (and (consp expr)
           (eq (first expr) 'function)
           (consp (rest expr))
           (second expr)
           (null (cddr expr))
           (let ((stripped-expr (second expr)))
             (and (consp stripped-expr)
                  (eq (first stripped-expr) 'lambda)
                  (listp (second stripped-expr))
                  (cddr stripped-expr))))
    (second expr)
    (let ((args (intern-n-vars-with-prefix "_" n-args nil)))
      (list 'lambda args (apply #'gen-apply expr args)))))


; Generate a local variable for holding the value of expr.  Optimize the case where expr
; is an identifier or a number.
(defun gen-local-var (expr)
  (if (or (symbolp expr) (numberp expr))
    expr
    (gensym "L")))


; var should have been obtained from calling gen-local-var on expr.  Return
; `(let ((,var ,expr)) ,body-code),
; optimizing the cases that gen-local-var optimizes.
(defmacro let-local-var (var expr &body body-code)
  (let ((body (gensym "BODY")))
    `(let ((,body (list ,@body-code)))
       (if (eql ,var ,expr)
         (gen-progn ,body)
         (list* 'let (list (list ,var ,expr)) ,body)))))
       


;;; ------------------------------------------------------------------------------------------------------
;;; LF TOKENS

;;; Each symbol in the LF package is a variant of a terminal that represents that terminal preceded by one
;;; or more line breaks.

(defvar *lf-package* (make-package "LF" :use nil))

(defun make-lf-terminal (terminal)
  (assert-true (not (lf-terminal? terminal)))
  (multiple-value-bind (lf-terminal present) (intern (symbol-name terminal) *lf-package*)
    (unless (eq present :external)
      (export lf-terminal *lf-package*)
      (setf (get lf-terminal :sort-key) (concatenate 'string (symbol-name terminal) " "))
      (setf (get lf-terminal :origin) terminal)
      (setf (get terminal :lf-terminal) lf-terminal))
    lf-terminal))

(defun lf-terminal? (terminal)
  (eq (symbol-package terminal) *lf-package*))


(declaim (inline terminal-lf-terminal lf-terminal-terminal))
(defun terminal-lf-terminal (terminal)
  (get terminal :lf-terminal))
(defun lf-terminal-terminal (lf-terminal)
  (get lf-terminal :origin))


; Ensure that for each transition on a LF: terminal in the grammar there exists a corresponding transition
; on a non-LF: terminal.
(defun ensure-lf-subset (grammar)
  (all-state-transitions
   #'(lambda (state transitions-hash)
       (dolist (transition-pair (state-transitions state))
         (let ((terminal (car transition-pair)))
           (when (lf-terminal? terminal)
             (unless (equal (cdr transition-pair) (gethash (lf-terminal-terminal terminal) transitions-hash))
               (format *error-output* "State ~S: transition on ~S differs from transition on ~S~%"
                       state terminal (lf-terminal-terminal terminal)))))))
   grammar))


; Print a list of transitions on non-LF: terminals that do not have corresponding LF: transitions.
; Return a list of non-LF: terminals which behave identically to the corresponding LF: terminals.
(defun show-non-lf-only-transitions (grammar)
  (let ((invariant-terminalset (make-full-terminalset grammar))
        (terminals-vector (grammar-terminals grammar)))
    (dotimes (n (length terminals-vector))
      (let ((terminal (svref terminals-vector n)))
        (when (lf-terminal? terminal)
          (terminalset-difference-f invariant-terminalset (make-terminalset grammar terminal)))))
    (all-state-transitions
     #'(lambda (state transitions-hash)
         (dolist (transition-pair (state-transitions state))
           (let ((terminal (car transition-pair)))
             (unless (lf-terminal? terminal)
               (let ((lf-terminal (terminal-lf-terminal terminal)))
                 (when lf-terminal
                   (let ((lf-terminal-transition (gethash lf-terminal transitions-hash)))
                     (cond
                      ((null lf-terminal-transition)
                       (terminalset-difference-f invariant-terminalset (make-terminalset grammar terminal))
                       (format *error-output* "State ~S has transition on ~S but not on ~S~%"
                               state terminal lf-terminal))
                      ((not (equal (cdr transition-pair) lf-terminal-transition))
                       (terminalset-difference-f invariant-terminalset (make-terminalset grammar terminal))
                       (format *error-output* "State ~S transition on ~S differs from transition on ~S~%"
                               state terminal lf-terminal))))))))))
     grammar)
    (terminalset-list grammar invariant-terminalset)))


;;; ------------------------------------------------------------------------------------------------------
;;; GRAMMAR-INFO

(defstruct (grammar-info (:constructor make-grammar-info (name grammar &optional lexer))
                         (:copier nil)
                         (:predicate grammar-info?))
  (name nil :type symbol :read-only t)               ;The name of this grammar
  (grammar nil :type grammar :read-only t)           ;This grammar
  (lexer nil :type (or null lexer) :read-only t))    ;This grammar's lexer if this is a lexer grammar; nil if not


; Return the charclass that defines the given lexer nonterminal or nil if none.
(defun grammar-info-charclass (grammar-info nonterminal)
  (let ((lexer (grammar-info-lexer grammar-info)))
    (and lexer (lexer-charclass lexer nonterminal))))


; Return the charclass or partition that defines the given lexer nonterminal or nil if none.
(defun grammar-info-charclass-or-partition (grammar-info nonterminal)
  (let ((lexer (grammar-info-lexer grammar-info)))
    (and lexer (or (lexer-charclass lexer nonterminal)
                   (gethash nonterminal (lexer-partitions lexer))))))


;;; ------------------------------------------------------------------------------------------------------
;;; WORLDS

(defstruct (world (:constructor allocate-world)
                  (:copier nil)
                  (:predicate world?))
  (conditionals nil :type list)                      ;Assoc list of (conditional . highlight), where highlight can be a style keyword, nil (no style), or 'delete
  (package nil :type (or null package))              ;The package in which this world's identifiers are interned
  (next-type-serial-number 0 :type integer)          ;Serial number to be used for the next type defined
  (types-reverse nil :type (or null hash-table))     ;Hash table of (kind tag parameters) -> type; nil if invalid
  (false-tag nil :type (or null tag))                ;Tag used for false
  (true-tag nil :type (or null tag))                 ;Tag used for true
  (finite32-tag nil :type (or null tag))             ;Pseudo-tag used for accessing the value field of a finite32
  (finite64-tag nil :type (or null tag))             ;Pseudo-tag used for accessing the value field of a finite64
  (bottom-type nil :type (or null type))             ;Subtype of all types used for nonterminating computations
  (void-type nil :type (or null type))               ;Type used for placeholders
  (false-type nil :type (or null type))              ;Type used for false
  (true-type nil :type (or null type))               ;Type used for true
  (boolean-type nil :type (or null type))            ;Type used for booleans
  (integer-type nil :type (or null type))            ;Type used for integers
  (rational-type nil :type (or null type))           ;Type used for rational numbers
  (finite32-type nil :type (or null type))           ;Type used for nonzero finite single-precision floating-point numbers
  (finite64-type nil :type (or null type))           ;Type used for nonzero finite double-precision floating-point numbers
  (char16-type nil :type (or null type))             ;Type used for Unicode code units and BMP code points
  (supplementary-char-type nil :type (or null type)) ;Type used for Unicode supplementary code points
  (char21-type nil :type (or null type))             ;Type used for Unicode code points
  (string-type nil :type (or null type))             ;Type used for strings (vectors of char16s)
  (denormalized-false-type nil :type (or null type)) ;Type (denormalized-tag false)
  (denormalized-true-type nil :type (or null type))  ;Type (denormalized-tag true)
  (boxed-boolean-type nil :type (or null type))      ;Union type (union (tag true) (tag false))
  (grammar-infos nil :type list)                     ;List of grammar-info
  (commands-source nil :type list))                  ;List of source code of all commands applied to this world


; Return the name of the world.
(defun world-name (world)
  (package-name (world-package world)))


; Return a symbol in the given package whose value is that package's world structure.
(defun world-access-symbol (package)
  (find-symbol "*WORLD*" package))


; Return the world that created the given package.
(declaim (inline package-world))
(defun package-world (package)
  (symbol-value (world-access-symbol package)))


; Return the world that contains the given symbol.
(defun symbol-world (symbol)
  (package-world (symbol-package symbol)))


; Delete the world and its package.
(defun delete-world (world)
  (let ((package (world-package world)))
    (when package
      (delete-package package)))
  (setf (world-package world) nil))


; Create a world using a package with the given name.
; If the package is already used for another world, its contents
; are erased and the other world deleted.
(defun make-world (name)
  (assert-type name string)
  (let ((p (find-package name)))
    (when p
      (let* ((access-symbol (world-access-symbol p))
             (p-world (and (boundp access-symbol) (symbol-value access-symbol))))
        (unless p-world
          (error "Package ~A already in use" name))
        (assert-true (eq (world-package p-world) p))
        (delete-world p-world))))
  (let* ((p (make-package name :use nil))
         (world (allocate-world
                 :package p
                 :types-reverse (make-hash-table :test #'equal)))
         (access-symbol (intern "*WORLD*" p)))
    (set access-symbol world)
    (export access-symbol p)
    world))


; Intern s (which should be a symbol or a string) in this world's
; package and return the resulting symbol.
(defun world-intern (world s)
  (intern (string s) (world-package world)))


; Same as world-intern except that return nil if s is not already interned.
(defun world-find-symbol (world s)
  (find-symbol (string s) (world-package world)))


; Export symbol in its package, which must belong to some world.
(defun export-symbol (symbol)
  (assert-true (symbol-in-any-world symbol))
  (export symbol (symbol-package symbol)))


; Call f on each external symbol defined in the world's package.
(declaim (inline each-world-external-symbol))
(defun each-world-external-symbol (world f)
  (each-package-external-symbol (world-package world) f))


; Call f on each external symbol defined in the world's package that has
; a property with the given name.
; f takes two arguments:
;   the symbol
;   the value of the property
(defun each-world-external-symbol-with-property (world property f)
  (each-world-external-symbol
   world
   #'(lambda (symbol)
       (let ((value (get symbol property *get2-nonce*)))
         (unless (eq value *get2-nonce*)
           (funcall f symbol value))))))


; Return a list of all external symbols defined in the world's package that have
; a property with the given name.
; The list is sorted by symbol names.
(defun all-world-external-symbols-with-property (world property)
  (let ((list nil))
    (each-world-external-symbol
     world
     #'(lambda (symbol)
         (let ((value (get symbol property *get2-nonce*)))
           (unless (eq value *get2-nonce*)
             (push symbol list)))))
    (sort list #'string<)))


; Return true if s is a symbol that is defined in this world's package.
(declaim (inline symbol-in-world))
(defun symbol-in-world (world s)
  (and (symbolp s) (eq (symbol-package s) (world-package world))))


; Return true if s is a symbol that is defined in any world's package.
(defun symbol-in-any-world (s)
  (and (symbolp s)
       (let* ((package (symbol-package s))
              (access-symbol (world-access-symbol package)))
         (and (boundp access-symbol) (typep (symbol-value access-symbol) 'world)))))


; Return a list of grammars in the world
(defun world-grammars (world)
  (mapcar #'grammar-info-grammar (world-grammar-infos world)))


; Return the grammar-info with the given name in the world
(defun world-grammar-info (world name)
  (find name (world-grammar-infos world) :key #'grammar-info-name))


; Return the grammar with the given name in the world
(defun world-grammar (world name)
  (let ((grammar-info (world-grammar-info world name)))
    (assert-non-null
     (and grammar-info (grammar-info-grammar grammar-info)))))


; Return the lexer with the given name in the world
(defun world-lexer (world name)
  (let ((grammar-info (world-grammar-info world name)))
    (assert-non-null
     (and grammar-info (grammar-info-lexer grammar-info)))))


; Return a list of highlights allowed in this world.
(defun world-highlights (world)
  (let ((highlights nil))
    (dolist (c (world-conditionals world))
      (let ((highlight (cdr c)))
        (unless (or (null highlight) (eq highlight 'delete))
          (pushnew highlight highlights))))
    (nreverse highlights)))


; Return the highlight to which the given conditional maps.
; Return 'delete if the conditional should be omitted.
(defun resolve-conditional (world conditional)
  (let ((h (assoc conditional (world-conditionals world))))
    (if h
      (cdr h)
      (error "Bad conditional ~S" conditional))))


;;; ------------------------------------------------------------------------------------------------------
;;; SYMBOLS

;;; The following properties are attached to exported symbols in the world:
;;;
;;;   :preprocess    preprocessor function ((preprocessor-state id . form-arg-list) -> form-list re-preprocess) if this identifier
;;;                  is a preprocessor command like 'grammar, 'lexer, or 'production
;;;
;;;   :command       expression code generation function ((world grammar-info-var . form-arg-list) -> void) if this identifier
;;;                  is a command like 'deftype or 'define
;;;   :statement     expression code generation function ((world type-env rest last id . form-arg-list) -> codes, live, annotated-stmts)
;;;                  if this identifier is a statement like 'if or 'catch.
;;;                     codes is a list of generated statements.
;;;                     live is :dead if the statement cannot complete or a list of the symbols of :uninitialized variables that are initialized
;;;                       if the statement can complete.
;;;                     annotated-stmts is a list of generated annotated statements.
;;;   :special-form  expression code generation function ((world type-env id . form-arg-list) -> code, type, annotated-expr)
;;;                  if this identifier is a special form like 'tag or 'in
;;;   :condition     boolean condition code generation function ((world type-env id . form-arg-list) -> code, annotated-expr, true-type-env, false-type-env)
;;;                  if this identifier is a condition form like 'and or 'in
;;;
;;;   :primitive     primitive structure if this identifier is a primitive
;;;
;;;   :type-constructor  expression code generation function ((world allow-forward-references . form-arg-list) -> type) if this
;;;                  identifier is a type constructor like '->, 'vector, 'range-set, 'tag, or 'union
;;;   :deftype       type if this identifier is a type; nil if this identifier is a forward-referenced type
;;;
;;;   :non-reserved  true if this symbol is usable as an identifier despite being a :special-form, :condition, :primitive, or :type-constructor
;;;
;;;   <value>        value of this identifier if it is a variable of type other than ->
;;;   <function>     value of this identifier if it is a variable of type ->
;;;   :value-expr    unparsed expression defining the value of this identifier if it is a variable
;;;   :lisp-value-expr unparsed lisp expression defining the value of this identifier; overrides :value-expr
;;;   :mutable       if present and non-nil, this identifier is a mutable variable
;;;   :type          type of this identifier if it is a variable
;;;   :type-expr     unparsed expression defining the type of this identifier if it is a variable
;;;   :tag           tag structure if this identifier is a tag
;;;   :tag-hidden    a flag that, if true, indicates that this tag's name should not be visible
;;;   :tag=          a two-argument function that takes two values with this tag and compares them
;;;
;;;   :action        list of (grammar-info . grammar-symbol) that declare this action if this identifier is an action name
;;;
;;;   :depict-command           depictor function ((markup-stream world depict-env . form-arg-list) -> void)
;;;   :depict-statement         depictor function ((markup-stream world semicolon last-paragraph-style . form-annotated-arg-list) -> void)
;;;   :depict-special-form      depictor function ((markup-stream world level . form-annotated-arg-list) -> void)
;;;   :depict-type-constructor  depictor function ((markup-stream world level . form-arg-list) -> void)
;;;


; Return the preprocessor action associated with the given symbol or nil if none.
; This macro is appropriate for use with setf.
(defmacro symbol-preprocessor-function (symbol)
  `(get ,symbol :preprocess))


; Return the primitive definition associated with the given symbol or nil if none.
; This macro is appropriate for use with setf.
(defmacro symbol-primitive (symbol)
  `(get ,symbol :primitive))


; Return the tag definition associated with the given symbol or nil if none.
; This macro is appropriate for use with setf.
(defmacro symbol-tag (symbol)
  `(get ,symbol :tag))


; Call f on each tag definition in the world.
; f takes two arguments:
;   the name
;   the tag structure
(defun each-tag-definition (world f)
  (each-world-external-symbol-with-property world :tag f))


; Return a sorted list of the names of all tag definitions in the world.
(defun world-tag-definitions (world)
  (all-world-external-symbols-with-property world :tag))


; Return the type definition associated with the given symbol.
; Return nil if the symbol is a forward-referenced type.
; If the symbol has no type definition at all, return default
; (or nil if not specified).
; This macro is appropriate for use with setf.
(defmacro symbol-type-definition (symbol &optional default)
  `(get ,symbol :deftype ,@(and default (list default))))


; Return true if this symbol's symbol-type-definition is user-defined.
; This macro is appropriate for use with setf.
(defmacro symbol-type-user-defined (symbol)
  `(get ,symbol 'type-user-defined))


; Call f on each type definition, including forward-referenced types, in the world.
; f takes two arguments:
;   the symbol
;   the type (nil if forward-referenced)
(defun each-type-definition (world f)
  (each-world-external-symbol-with-property world :deftype f))


; Return a sorted list of the names of all type definitions, including
; forward-referenced types, in the world.
(defun world-type-definitions (world)
  (all-world-external-symbols-with-property world :deftype))


; Return the type of the variable associated with the given symbol or nil if none.
; This macro is appropriate for use with setf.
(defmacro symbol-type (symbol)
  `(get ,symbol :type))


; Return a list of (grammar-info . grammar-symbol) pairs that each indicate
; a grammar and a grammar-symbol in that grammar that has an action named by the given symbol.
; This macro is appropriate for use with setf.
(defmacro symbol-action (symbol)
  `(get ,symbol :action))


; Return an unused name for a new function in the world.  The given string is a suggested name.
; The returned value is a symbol.
(defun unique-function-name (world string)
  (let ((f (world-intern world string)))
    (if (fboundp f)
      (gentemp string (world-package world))
      f)))


;;; ------------------------------------------------------------------------------------------------------
;;; TAGS

(defstruct (field (:type list) (:constructor make-field (label type mutable optional)))
  label                     ;This field's name (not interned in the world)
  type                      ;This field's type 
  mutable                   ;True if this field is mutable
  optional)                 ;True if this field can be in an uninitialized state


(defstruct (tag (:constructor make-tag (name keyword mutable fields =-name link base)) (:predicate tag?))
  (name nil :type symbol :read-only t)               ;This tag's world-interned name
  (keyword nil :type (or null keyword) :read-only t) ;This tag's keyword (non-null only when the tag is immutable and has no fields)
  (mutable nil :type bool :read-only t)              ;True if this tag's equality is based on identity, in which case the tag's values have a hidden serial-number field
  (fields nil :type list :read-only t)               ;List of fields after eval-tags-types or (field-name field-type-expression [:const|:var|:opt-const|:opt-var]) before eval-tags-types
  (=-name nil :type symbol)                          ;Lazily computed name of a function that compares two values of this tag for equality; nil if not known yet
  (link nil :type (or null keyword) :read-only t)    ;:reference if this is a local tag, :external if it's a predefined tag, or nil for no cross-references to this tag
  (base nil :type integer :read-only t)              ;Position of first field in the list; -1 if it's special
  (appearance nil))                                  ;One of the following:
;                                                    ;  nil to display the constructor normally
;                                                    ;  (:suffix . markup) to display the constructor as a suffix (the constructor must be unary)
;                                                    ;  (:infix . markup) to display the constructor as an infix (the constructor must be binary)


; Return four values:
;   the one-based position of the tag's field corresponding to the given label or nil if the label is not present;
;   the type the field; 
;   true if the field is mutable;
;   true if the field is optional.
(defun tag-find-field (tag label)
  (do ((fields (tag-fields tag) (cdr fields))
       (n (tag-base tag) (1+ n)))
      ((endp fields) (values nil nil nil nil))
    (let ((field (car fields)))
      (when (eq label (field-label field))
        (return (values n (field-type field) (field-mutable field) (field-optional field)))))))


; Define a new tag.  Signal an error if the name is already used.  Return the tag.
; Do not evaluate the field and type expressions yet; that will be done by eval-tags-types.
; If hidden is true, mark the tag as hidden so that its name cannot be used to access it.
(defun add-tag (world name mutable fields link hidden)
  (assert-true (member link '(nil :reference :external)))
  (let ((name (scan-name world name)))
    (when (symbol-tag name)
      (error "Attempt to redefine tag ~A" name))
    (let ((keyword nil)
          (=-name nil))
      (unless (or mutable fields)
        (setq keyword (intern (string name) :keyword)))
      (when (or mutable (null fields))
        (setq =-name 'eq)
        (setf (get name :tag=) #'eq))
      (let ((tag (make-tag name keyword mutable (copy-list fields) =-name link (if mutable 2 1))))
        (setf (symbol-tag name) tag)
        (when hidden
          (setf (get name :tag-hidden) t))
        (export-symbol name)
        tag))))


; Evaluate the type expressions in the tag's fields.
(defun eval-tag-types (world tag)
  (do ((fields (tag-fields tag) (cdr fields))
       (labels nil))
      ((endp fields))
    (let ((field (first fields)))
      (unless (and (consp field) (identifier? (first field))
                   (consp (cdr field)) (second field)
                   (member (third field) '(nil :const :var :opt-const :opt-var))
                   (null (cdddr field)))
        (error "Bad field ~S" field))
      (let ((label (first field))
            (mutable (member (third field) '(:var :opt-var)))
            (optional (member (third field) '(:opt-const :opt-var))))
        (when (member label labels)
          (error "Duplicate label ~S" label))
        (push label labels)
        (when (and mutable (not (tag-mutable tag)))
          (error "Tag ~S is immutable but contains a mutable field ~S" (tag-name tag) label))
        (setf (first fields) (make-field label (scan-type world (second field)) mutable optional))))))


; Evaluate the type expressions in all of the world's tag's fields.
(defun eval-tags-types (world)
  (each-tag-definition
   world
   #'(lambda (name tag)
       (declare (ignore name))
       (eval-tag-types world tag))))


; Return the tag with the given un-world-interned name.  Signal an error if one wasn't found.
(defun scan-tag (world tag-name)
  (let* ((name (world-find-symbol world tag-name))
         (tag (symbol-tag name))
         (hidden (get name :tag-hidden)))
    (unless tag
      (error "No tag ~A defined" tag-name))
    (if hidden nil tag)))


; Scan label to produce a label that is present in the given tag.
; Return:
;   the label's position;
;   its field type;
;   a flag indicating whether the field is mutable;
;   a flag indicating whether the field is optional.
(defun scan-label (tag label)
  (multiple-value-bind (position field-type mutable optional) (tag-find-field tag label)
    (unless position
      (error "Label ~S not present in ~A" label (tag-name tag)))
    (values position field-type mutable optional)))


; Print the tag nicely on the given stream.
(defun print-tag (tag &optional (stream t))
  (pprint-logical-block (stream (tag-fields tag) :prefix "(" :suffix ")")
    (pprint-exit-if-list-exhausted)
    (loop
      (let ((field (pprint-pop)))
        (pprint-logical-block (stream nil :prefix "(" :suffix ")")
          (write (field-label field) :stream stream)
          (format stream " ~@_")
          (print-type (field-type field) stream)
          (when (field-mutable field)
            (format stream " ~@_:var"))
          (when (field-optional field)
            (format stream " ~@_:opt")))
        (pprint-exit-if-list-exhausted)
        (format stream " ~:_")))))


;;; ------------------------------------------------------------------------------------------------------
;;; TYPES

(deftype typekind ()
         '(member            ;tag            ;parameters
           :bottom           ;nil            ;nil
           :void             ;nil            ;nil
           :boolean          ;nil            ;nil
           :integer          ;nil            ;nil
           :rational         ;nil            ;nil
           :finite32         ;nil            ;nil    ;All non-zero finite 32-bit single-precision floating-point numbers
           :finite64         ;nil            ;nil    ;All non-zero finite 64-bit double-precision floating-point numbers
           :char16           ;nil            ;nil
           :supplementary-char ;nil          ;nil
           :->               ;nil            ;(result-type arg1-type arg2-type ... argn-type)
           :string           ;nil            ;(char16)
           :vector           ;nil            ;(element-type)
           :list-set         ;nil            ;(element-type)
           :range-set        ;nil            ;(element-type)
           :bit-set          ;(tag ... tag)  ;(element-type)  ;element-type is the type of the union of the tags
           :restricted-set   ;(n ... n)      ;(bit-set-type)  ;n's are in ascending numerical order; use :bottom or :bit-set insetad for the trivial cases
           :tag              ;tag            ;nil
           :denormalized-tag ;tag            ;nil
           :union            ;nil            ;(type ... type) sorted by ascending serial numbers
           :writable-cell    ;nil            ;(element-type)
           :delay))          ;nil            ;(type)

;A denormalized-tag is a singleton tag type whose value carries no meaning.
;
;All types are normalized except for those with kind :denormalized-tag and the boxed-boolean union type of tags true and false.
;
;A union type must have:
;  at least two types
;  only types with kinds :integer, :rational, :finite32, :finite64, :char16, :supplementary-char, :->, :string, :vector, :list-set, or :tag
;  no type that is a duplicate or subtype of another type in the union
;  at most one type each with kind :->
;  at most one type each with kind :vector or :list-set; furthermore, if such a type is present, then only keyword :tag types may be present
;  types sorted by ascending type-serial-number, except that :-> is given the serial number -1 and :vector and :list-set -2.
;
;Note that types with the above kinds (not including :->, :vector, or :list-set) never change their serial-numbers during unite-types, so
;unite-types does not need to worry about unions differing only in the order of their parameters.


(defstruct (type (:constructor allocate-type (serial-number kind tag parameters =-name /=-name)) (:predicate type?))
  (name nil :type symbol)                          ;This type's name; nil if this type is anonymous
  (serial-number nil :type integer)                ;This type's unique serial number
  (kind nil :type typekind :read-only t)           ;This type's kind
  (tag nil :read-only t)                           ;This type's tag; ordered list of tags for bit-set;
  ;                                                ;  set of included subsets represented as a sorted list of integers for restricted-set
  (parameters nil :type list :read-only t)         ;List of parameter types (either types or symbols if forward-referenced) describing a compound type
  (=-name nil :type symbol)                        ;Lazily computed name of a function that compares two values of this type for equality; nil if not known yet
  (/=-name nil :type symbol)                       ;Name of a function that complements = or nil if none
  (order-alist nil)                                ;Either nil or an association list ((< . t<) (> . t>) (<= . t<=) (>= . t>=)) where the t's are order functions for this type
  (range-set-encode nil :type symbol)              ;Either nil or the name of a function that converts an instance of this type to an integer for storage in a range-set
  (range-set-decode nil :type symbol))             ;Either nil or the name of a function that reverses the range-set-encode conversion


(declaim (inline make-->-type))
(defun make-->-type (world argument-types result-type)
  (make-type world :-> nil (cons result-type argument-types) nil nil))

(declaim (inline ->-argument-types))
(defun ->-argument-types (type)
  (assert-true (eq (type-kind type) :->))
  (cdr (type-parameters type)))

(declaim (inline ->-result-type))
(defun ->-result-type (type)
  (assert-true (eq (type-kind type) :->))
  (car (type-parameters type)))


(declaim (inline make-vector-type))
(defun make-vector-type (world element-type)
  (if (eq element-type (world-char16-type world))
    (world-string-type world)
    (make-type world :vector nil (list element-type) nil nil)))

(declaim (inline vector-element-type))
(defun vector-element-type (type)
  (assert-true (member (type-kind type) '(:vector :string)))
  (car (type-parameters type)))


(declaim (inline make-list-set-type))
(defun make-list-set-type (world element-type)
  (make-type world :list-set nil (list element-type) nil nil))

(declaim (inline make-range-set-type))
(defun make-range-set-type (world element-type)
  (make-type world :range-set nil (list element-type) intset=-name nil))

(defun make-bit-set-type (world tags)
  (let ((element-type (make-union-type world (mapcar #'(lambda (tag) (make-tag-type world tag)) tags))))
    (make-type world :bit-set tags (list element-type) '= '/=)))

; values must be sorted in ascending numerical order.
(defun make-restricted-set-type (world bit-set-type values)
  (assert-true (bit-set-type? bit-set-type))
  (if (endp values)
    (world-bottom-type world)
    (progn
      (when *value-asserts*
        (let ((prev -1))
          (dolist (v values)
            (unless (and (integerp v) (> v prev))
              (error "Bad restricted-set set of values: ~S" values))
            (setq prev v))
          (unless (< prev (ash 1 (length (type-tag bit-set-type))))
            (error "Bad restricted-set set of values: ~S" values))))
      (if (= (length values) (ash 1 (length (type-tag bit-set-type))))
        bit-set-type
        (make-type world :restricted-set values (list bit-set-type) '= '/=)))))


; Return the bit-set type underlying a bit-set or restricted-set.
(defun underlying-bit-set-type (type)
  (ecase (type-kind type)
    (:bit-set type)
    (:restricted-set (first (type-parameters type)))))


; Return the ordered list of keywords in a bit-set or restricted-set type.
(defun set-type-keywords (type)
  (ecase (type-kind type)
    (:bit-set (mapcar #'tag-name (type-tag type)))
    (:restricted-set (set-type-keywords (first (type-parameters type))))))


(defun bit-set-type? (v)
  (and (type? v) (eq (type-kind v) :bit-set)))


(defun set-element-type (type)
  (ecase (type-kind type)
    ((:list-set :range-set :bit-set) (first (type-parameters type)))
    (:restricted-set (set-element-type (first (type-parameters type))))))


(defun collection-element-type (type)
  (ecase (type-kind type)
    ((:vector :string :list-set :range-set :bit-set) (first (type-parameters type)))
    (:restricted-set (set-element-type (first (type-parameters type))))))


(declaim (inline make-tag-type))
(defun make-tag-type (world tag)
  (make-type world :tag tag nil (tag-=-name tag) nil))


(declaim (inline always-true))
(defun always-true (a b)
  (declare (ignore a b))
  t)

(declaim (inline always-false))
(defun always-false (a b)
  (declare (ignore a b))
  nil)

(declaim (inline make-denormalized-tag-type))
(defun make-denormalized-tag-type (world tag)
  (assert-true (tag-keyword tag))
  (make-type world :denormalized-tag tag nil 'always-true 'always-false))


(declaim (inline make-writable-cell-type))
(defun make-writable-cell-type (world element-type)
  (make-type world :writable-cell nil (list element-type) nil nil))

(declaim (inline writable-cell-element-type))
(defun writable-cell-element-type (type)
  (assert-true (eq (type-kind type) :writable-cell))
  (car (type-parameters type)))


(declaim (inline make-delay-type))
(defun make-delay-type (world type)
  (make-type world :delay nil (list type) nil nil))

(declaim (inline delay-element-type))
(defun delay-element-type (type)
  (assert-true (eq (type-kind type) :delay))
  (car (type-parameters type)))


; Return the type's tag if it has one.
; The types float32 and float64 are considered to have fake tags that have one field, named "value", at position -1.
; Return nil if the type is not one of the above.
(defun type-pseudo-tag (world type)
  (case (type-kind type)
    (:tag (type-tag type))
    (:finite32 (world-finite32-tag world))
    (:finite64 (world-finite64-tag world))))

  
; Return true if the type is a tag type or a union of tag types all of which have a field with
; the given label.
(defun type-has-field (world type label)
  (flet ((test (type)
           (let ((tag (type-pseudo-tag world type)))
             (and tag (tag-find-field tag label)))))
    (case (type-kind type)
      ((:tag :finite32 :finite64) (test type))
      (:union (every #'test (type-parameters type))))))


; Equivalent types are guaranteed to be eq to each other.
(declaim (inline type=))
(defun type= (type1 type2)
  (eq type1 type2))


; code is a lisp expression that evaluates to either :true or :false.
; Return a lisp expression that evaluates code and returns either t or nil.
(defun bool-unboxing-code (code)
  (if (constantp code)
    (ecase code
      (:true t)
      (:false nil))
    (list 'eq code :true)))


; code is a lisp expression that evaluates to either non-nil or nil.
; Return a lisp expression that evaluates code and returns either :true or :false.
(defun bool-boxing-code (code)
  (if (constantp code)
    (ecase code
      ((t) :true)
      ((nil) :false))
    (list 'if code :true :false)))


; code is a lisp expression that evaluates to a value of type type.
; If type is the same or more specific (i.e. a subtype) than supertype, return code that evaluates code
; and returns its value coerced to supertype.
; Signal an error if type is not a subtype of supertype.  expr contains the source code that generated code
; and is used for error reporting only.
;
; Coercions from :denormalized-tag types are not implemented, but they should not be necessary in practice.
; Coercions from vectors to strings or from strings to vectors are not implemented either.
(defun widening-coercion-code (world supertype type code expr)
  (if (type= type supertype)
    code
    (flet ((type-mismatch ()
             (error "Expected type ~A for ~:W but got type ~A"
                    (print-type-to-string supertype)
                    expr
                    (print-type-to-string type))))
      (let ((kind (type-kind type)))
        (if (eq kind :bottom)
          code
          (case (type-kind supertype)
            (:boolean
             (if (or (type= type (world-false-type world))
                     (type= type (world-true-type world))
                     (type= type (world-boxed-boolean-type world)))
               (bool-unboxing-code code)
               (type-mismatch)))
            (:rational
             (if (eq kind :integer)
               code
               (type-mismatch)))
            (:union
             (let ((supertype-types (type-parameters supertype)))
               (case kind
                 (:boolean
                  (if (and (member (world-false-type world) supertype-types) (member (world-true-type world) supertype-types))
                    (bool-boxing-code code)
                    (type-mismatch)))
                 (:integer
                  (if (or (member type supertype-types) (member (world-rational-type world) supertype-types))
                    code
                    (type-mismatch)))
                 ((:rational :finite32 :finite64 :char16 :supplementary-char :-> :string :tag)
                  (if (member type supertype-types)
                    code
                    (type-mismatch)))
                 ((:vector :list-set)
                  (let ((super-collection-type (find kind supertype-types :key #'type-kind)))
                    (if super-collection-type
                      (widening-coercion-code world super-collection-type type code expr)
                      (type-mismatch))))
                 (:union
                  (dolist (type-type (type-parameters type))
                    (unless (case (type-kind type-type)
                              (:integer (or (member type-type supertype-types) (member (world-rational-type world) supertype-types)))
                              ((:rational :finite32 :finite64 :char16 :supplementary-char :-> :string :tag :vector :list-set) (member type-type supertype-types)))
                      (type-mismatch)))
                  code)
                 (t (type-mismatch)))))
            ((:vector :list-set)
             (unless (eq kind (type-kind supertype))
               (type-mismatch))
             (let* ((par (gensym "PAR"))
                    (element-coercion-code (widening-coercion-code world (collection-element-type supertype) (collection-element-type type) par expr)))
               (if (eq element-coercion-code par)
                 code
                 `(mapcar #'(lambda (,par) ,element-coercion-code) code))))
            (:->
             (unless (eq kind :->)
               (type-mismatch))
             (let ((supertype-arguments (->-argument-types supertype))
                   (type-arguments (->-argument-types type)))
               (unless (= (length supertype-arguments) (length type-arguments))
                 (type-mismatch))
               (mapc #'(lambda (supertype-argument type-argument)
                         (unless (eq (widening-coercion-code world type-argument supertype-argument 'test 'test) 'test)
                           (error "Nontrivial type coercions of -> arguments are not supported yet")))
                     supertype-arguments type-arguments)
               (unless (eq (widening-coercion-code world (->-result-type supertype) (->-result-type type) 'test 'test) 'test)
                 (error "Nontrivial type coercion of -> result is not supported yet")))
             code)
            (:delay
             (if (eq kind :delay)
               (let ((code2 (widening-coercion-code world (delay-element-type supertype) (delay-element-type type) code expr)))
                 (unless (equal code code2)
                   (error "Nontrivial type coercion of delay result is not supported yet"))
                 code2)
               (widening-coercion-code world (delay-element-type supertype) type code expr)))
            (t (type-mismatch))))))))


; Return the list of constituent types that the given type would have if it were a union.
; The result is sorted by ascending serial numbers and contains no duplicates.
(defun type-to-union (world type)
  (ecase (type-kind type)
    (:boolean (type-parameters (world-boxed-boolean-type world)))
    ((:integer :rational :finite32 :finite64 :char16 :supplementary-char :-> :string :vector :list-set :tag) (list type))
    (:denormalized-tag (make-tag-type world (type-tag type)))
    (:union (type-parameters type))))


; Return the type's serial number, except that types with kind :-> are given the serial number -1
; and :vector and :list-set -2.
(defun type-union-serial-number (type)
  (or (cdr (assoc (type-kind type) '((:-> . -1) (:vector . -2) (:list-set . -2))))
      (type-serial-number type)))


; Merge the two lists of types sorted by ascending serial numbers, except that types with kind :-> are given the serial number -1
; and :vector and :list-set -2.
; The result is also sorted by ascending serial numbers and contains no duplicates.
(defun merge-type-lists (types1 types2)
  (cond
   ((endp types1) types2)
   ((endp types2) types1)
   (t (let ((type1 (first types1))
            (type2 (first types2)))
        (if (type= type1 type2)
          (cons type1 (merge-type-lists (rest types1) (rest types2)))
          (let ((serial-number1 (type-union-serial-number type1))
                (serial-number2 (type-union-serial-number type2)))
            (when (= serial-number1 serial-number2)
              (error "Duplicate function, vector, or set subtype of union: ~S ~S" type1 type2))
            (if (< serial-number1 serial-number2)
              (cons type1 (merge-type-lists (rest types1) types2))
              (cons type2 (merge-type-lists types1 (rest types2))))))))))


; Intersect the two lists of types sorted by ascending serial numbers, except that types with kind :-> are given the serial number -1
; and :vector and :list-set -2.
; The result is also sorted by ascending serial numbers and contains no duplicates.
(defun intersect-type-lists (types1 types2)
  (remove-if-not #'(lambda (type1) (member type1 types2)) types1))


; Return true if the list of types is sorted by serial number, except that types with kind :-> are given the serial number -1
; and :vector and :list-set -2.
(defun type-list-sorted (types)
  (let ((n (type-union-serial-number (first types))))
    (dolist (type (rest types) t)
      (let ((n2 (type-union-serial-number type)))
        (unless (< n n2)
          (return nil))
        (setq n n2)))))


(defun coercable-to-union-kind (kind)
  (member kind '(:boolean :integer :rational :finite32 :finite64 :char16 :supplementary-char :-> :string :vector :list-set :tag :denormalized-tag :union)))


; types is a list of distinct, non-overlapping types appropriate for inclusion in a union and
; sorted by increasing serial numbers.  Return the union type for holding types, reducing it to
; a simpler type as necessary.  If normalize is nil, don't change the representation of the destination type.
(defun reduce-union-type (world types normalize)
  (cond
   ((endp types) (world-bottom-type world))
   ((endp (cdr types)) (car types))
   ((and (endp (cddr types)) (member (world-true-type world) types) (member (world-false-type world) types))
    (if normalize
      (world-boolean-type world)
      (world-boxed-boolean-type world)))
   ((every #'(lambda (type) (eq (type-=-name type) 'eq)) types)
    (make-type world :union nil types 'eq nil))
   ((every #'(lambda (type) (member (type-=-name type) '(eq eql = char=))) types)
    (make-type world :union nil types 'eql nil))
   (t (make-type world :union nil types nil nil))))


; Return the union U of type1 and type2.  Note that a value of type1 or type2 might need to be coerced to
; be treated as a member of type U.
(defun type-union (world type1 type2)
  (labels
    ((numeric-kind (kind)
       (member kind '(:integer :rational)))
     (numeric-type (type)
       (numeric-kind (type-kind type))))
    (if (type= type1 type2)
      type1
      (let ((kind1 (type-kind type1))
            (kind2 (type-kind type2)))
        (cond
         ((eq kind1 :bottom) type2)
         ((eq kind2 :bottom) type1)
         ((and (numeric-kind kind1) (numeric-kind kind2)) (world-rational-type world))
         ((and (eq kind1 :vector) (eq kind2 :vector))
          (make-vector-type world (type-union world (vector-element-type type1) (vector-element-type type2))))
         ((and (eq kind1 :list-set) (eq kind2 :list-set))
          (make-list-set-type world (type-union world (set-element-type type1) (set-element-type type2))))
         ((and (coercable-to-union-kind kind1) (coercable-to-union-kind kind2))
          (let ((types (merge-type-lists (type-to-union world type1) (type-to-union world type2))))
            (when (> (count-if #'numeric-type types) 1)
              ;Currently the union of any two or more different numeric types is always rational.
              (setq types (merge-type-lists (remove-if #'numeric-type types) (list (world-rational-type world)))))
            (assert-true (type-list-sorted types))
            (reduce-union-type world types t)))
         (t (error "No union of types ~A and ~A" (print-type-to-string type1) (print-type-to-string type2))))))))


; Return the most specific common supertype of the types.  Note that a value of one of the given types may need to be
; coerced to be treated as a member of type U.
; If any of the types is not a type structure, then return a nested list of two-element unions like '(union <type1> <type2>).
(defun make-union-type (world &rest types)
  (if types
    (reduce #'(lambda (type1 type2)
                (if (and (type? type1) (type? type2))
                  (type-union world type1 type2)
                  (list 'union type1 type2)))
            types)
    (world-bottom-type world)))


; Return the intersection I of type1 and type2.  Note that a value of type I might need to be coerced to
; be treated as a member of type1 or type2.
; Not all intersections have been implemented yet, and some are too conservative, returning a smaller type than the exact intersection.
(defun type-intersection (world type1 type2)
  (if (type= type1 type2)
    type1
    (let ((kind1 (type-kind type1))
          (kind2 (type-kind type2)))
      (cond
       ((eq kind1 :bottom) type1)
       ((eq kind2 :bottom) type2)
       ((and (or (eq kind1 :union) (eq kind2 :union))
             (coercable-to-union-kind kind1) (coercable-to-union-kind kind2))
        (reduce-union-type world (intersect-type-lists (type-to-union world type1) (type-to-union world type2)) t))
       (t (error "No intersection of types ~A and ~A" (print-type-to-string type1) (print-type-to-string type2)))))))


; Return the most specific common supertype of the types.  Note that a value of the intersection type may need to be
; coerced to be treated as a member of one of the given types.
(defun make-intersection-type (world &rest types)
  (assert-true types)
  (reduce #'(lambda (type1 type2) (type-intersection world type1 type2))
          types))


; Ensure that subtype is a subtype of type.  subtype must not be the bottom type.
; Return two values:
;    subtype1, a type that is equivalent to subtype but may be denormalized.
;    subtype2, the type containing the instances of type but not subtype.
; Any concrete value of type will have either subtype1 or subtype2 without needing coercion.
; subtype1 and subtype2 may be denormalized in the following cases:
;    type is boolean and subtype is (tag true) or (tag false);
;    type is a union and subtype is boolean.
; Signal an error if there is no subtype2.
(defun type-difference (world type subtype)
  (flet ((type-mismatch ()
           (error "Cannot subtract type ~A from type ~A" (print-type-to-string subtype) (print-type-to-string type))))
    (if (type= type subtype)
      (if (type= subtype (world-bottom-type world))
        (type-mismatch)
        (values type (world-bottom-type world)))
      (case (type-kind type)
        (:boolean
         (cond
          ((or (type= subtype (world-false-type world)) (type= subtype (world-denormalized-false-type world)))
           (values (world-denormalized-false-type world) (world-denormalized-true-type world)))
          ((or (type= subtype (world-true-type world)) (type= subtype (world-denormalized-true-type world)))
           (values (world-denormalized-true-type world) (world-denormalized-false-type world)))
          ((type= subtype (world-boxed-boolean-type world))
           (values type (world-bottom-type world)))
          (t (type-mismatch))))
        (:rational
         (if (type= subtype (world-integer-type world))
           (values subtype 'fractional)
           (type-mismatch)))
        (:tag
          (if (and (eq (type-kind subtype) :denormalized-tag) (eq (type-tag type) (type-tag subtype)))
            (values type (world-bottom-type world))
            (type-mismatch)))
        (:denormalized-tag
         (if (and (eq (type-kind subtype) :tag) (eq (type-tag type) (type-tag subtype)))
           (values type (world-bottom-type world))
           (type-mismatch)))
        (:union
         (let ((types (type-parameters type)))
           (flet
             ((remove-subtype (subtype)
                (unless (member subtype types)
                  (type-mismatch))
                (setq types (remove subtype types))))
             (case (type-kind subtype)
               (:boolean
                (remove-subtype (world-false-type world))
                (remove-subtype (world-true-type world))
                (setq subtype (world-boxed-boolean-type world)))
               (:union
                (mapc #'remove-subtype (type-parameters subtype)))
               (:denormalized-tag
                (remove-subtype (make-tag-type world (type-tag subtype))))
               (t (remove-subtype subtype)))
             (values subtype (reduce-union-type world types nil)))))
        (t (type-mismatch))))))



; types must be a list of types suitable for inclusion in a :union type's parameters.  Return the following values:
;    a list of integerp, rationalp, finite32?, finite64?, characterp, functionp, stringp, and/or listp depending on whether types include the
;       :integer, :rational, :finite32, :finite64, :char16, :->, :string and/or :vector or :list-set member kinds;
;    a list of keywords used by non-list tags in the types;
;    a list of tag names used by list tags in the types
(defun analyze-union-types (types)
  (let ((atom-tests nil)
        (keywords nil)
        (list-tag-names nil)
        (has-listp nil))
    (dolist (type types)
      (ecase (type-kind type)
        (:integer (push 'integerp atom-tests))
        (:rational (push 'rationalp atom-tests))
        (:finite32 (push 'finite32? atom-tests))
        (:finite64 (push 'finite64? atom-tests))
        (:char16 (push 'characterp atom-tests))
        (:-> (push 'functionp atom-tests))
        (:string (push 'stringp atom-tests))
        ((:vector :list-set)
         (when has-listp
           (error "Unable to discriminate among the constituents in the union ~S" types))
         (setq has-listp t)
         (push 'listp atom-tests))
        (:tag (let* ((tag (type-tag type))
                     (keyword (tag-keyword tag)))
                (if keyword
                  (push keyword keywords)
                  (push (tag-name tag) list-tag-names))))
        (:supplementary-char (push :supplementary-char list-tag-names))))
    (when (and has-listp list-tag-names)
      (error "Unable to discriminate among the constituents in the union ~S" types))
    (values
     (nreverse atom-tests)
     (nreverse keywords)
     (nreverse list-tag-names))))


; code is a lisp expression that evaluates to a value of type type.  subtype is a subtype of type, which
; has already been verified by calling type-difference.
; Return a lisp expression that may evaluate code and returns non-nil if the value is a member of the subtype.
; The expression may evaluate code more than once or not at all.
(defun type-member-test-code (world subtype type code)
  (if (type= type subtype)
    t
    (ecase (type-kind type)
      (:boolean
       (cond
        ((or (type= subtype (world-false-type world)) (type= subtype (world-denormalized-false-type world)))
         (list 'not code))
        ((or (type= subtype (world-true-type world)) (type= subtype (world-denormalized-true-type world)))
         code)
        (t (error "Bad type-member-test-code"))))
      (:rational
       (if (type= subtype (world-integer-type world))
         (list 'integerp code)
         (error "Bad type-member-test-code")))
      ((:tag :denormalized-tag) t)
      (:union
       (multiple-value-bind (type-atom-tests type-keywords type-list-tag-names) (analyze-union-types (type-parameters type))
         (multiple-value-bind (subtype-atom-tests subtype-keywords subtype-list-tag-names)
                              (case (type-kind subtype)
                                (:boolean (values nil (list :false :true) nil))
                                (:union (analyze-union-types (type-parameters subtype)))
                                (:denormalized-tag (analyze-union-types (list (make-tag-type world (type-tag subtype)))))
                                (t (analyze-union-types (list subtype))))
           (assert-true (and (subsetp subtype-atom-tests type-atom-tests)
                             (subsetp subtype-keywords type-keywords)
                             (subsetp subtype-list-tag-names type-list-tag-names)))
           (gen-poly-op 'or nil
                        (nconc 
                         (mapcar #'(lambda (atom-test) (list atom-test code)) subtype-atom-tests)
                         (and subtype-keywords (list (gen-member-test code subtype-keywords)))
                         (and subtype-list-tag-names
                              (list (gen-poly-op 'and t
                                                 (nconc
                                                  (and (or type-atom-tests type-keywords) (list (list 'consp code)))
                                                  (list (gen-member-test (list 'car code) subtype-list-tag-names))))))))))))))



; Print the type nicely on the given stream.  If expand1 is true then print
; the type's top level even if it has a name.  In all other cases expand
; anonymous types but abbreviate named types by their names.
(defun print-type (type &optional (stream t) expand1)
  (if (and (type-name type) (not expand1))
    (write-string (symbol-name (type-name type)) stream)
    (case (type-kind type)
      (:bottom (write-string "bottom" stream))
      (:void (write-string "void" stream))
      (:boolean (write-string "boolean" stream))
      (:integer (write-string "integer" stream))
      (:rational (write-string "rational" stream))
      (:finite32 (write-string "finite32" stream))
      (:finite64 (write-string "finite64" stream))
      (:char16 (write-string "char16" stream))
      (:supplementary-char (write-string "supplementary-char" stream))
      (:-> (pprint-logical-block (stream nil :prefix "(" :suffix ")")
             (format stream "-> ~@_")
             (pprint-indent :current 0 stream)
             (pprint-logical-block (stream (->-argument-types type) :prefix "(" :suffix ")")
               (pprint-exit-if-list-exhausted)
               (loop
                 (print-type (pprint-pop) stream)
                 (pprint-exit-if-list-exhausted)
                 (format stream " ~:_")))
             (format stream " ~_")
             (print-type (->-result-type type) stream)))
      (:string (write-string "string" stream))
      (:vector (pprint-logical-block (stream nil :prefix "(" :suffix ")")
                 (format stream "vector ~@_")
                 (print-type (vector-element-type type) stream)))
      (:list-set (pprint-logical-block (stream nil :prefix "(" :suffix ")")
                   (format stream "list-set ~@_")
                   (print-type (set-element-type type) stream)))
      (:range-set (pprint-logical-block (stream nil :prefix "(" :suffix ")")
                    (format stream "range-set ~@_")
                    (print-type (set-element-type type) stream)))
      (:bit-set (pprint-logical-block (stream nil :prefix "(" :suffix ")")
                  (format stream "bit-set")
                  (dolist (keyword (set-type-keywords type))
                    (format stream " ~:_~A" keyword))))
      (:restricted-set (pprint-logical-block (stream nil :prefix "(" :suffix ")")
                         (format stream "restricted-set")
                         (dolist (keyword (set-type-keywords type))
                           (format stream " ~:_~A" keyword))
                         (format stream " ~_")
                         (pprint-logical-block (stream (type-tag type) :prefix "{" :suffix "}")
                           (pprint-exit-if-list-exhausted)
                           (loop
                             (print-value (pprint-pop) type stream)
                             (pprint-exit-if-list-exhausted)
                             (format stream " ~:_")))))
      (:tag (let ((tag (type-tag type)))
              (pprint-logical-block (stream nil :prefix "(" :suffix ")")
                (format stream "tag ~@_~A" (tag-name tag)))))
      (:union (pprint-logical-block (stream (type-parameters type) :prefix "(" :suffix ")")
                (write-string "union" stream)
                (pprint-exit-if-list-exhausted)
                (format stream " ~@_")
                (pprint-indent :current 0 stream)
                (loop
                  (print-type (pprint-pop) stream)
                  (pprint-exit-if-list-exhausted)
                  (format stream " ~:_"))))
      (:writable-cell (pprint-logical-block (stream nil :prefix "(" :suffix ")")
                        (format stream "writable-cell ~@_")
                        (print-type (writable-cell-element-type type) stream)))
      (:delay (pprint-logical-block (stream nil :prefix "(" :suffix ")")
                (format stream "delay ~@_")
                (print-type (delay-element-type type) stream)))
      (t (error "Bad typekind ~S" (type-kind type))))))


; Same as print-type except that accumulates the output in a string
; and returns that string.
(defun print-type-to-string (type &optional expand1)
  (with-output-to-string (stream)
    (print-type type stream expand1)))


(defmethod print-object ((type type) stream)
  (print-unreadable-object (type stream)
    (format stream "type~D ~@_" (type-serial-number type))
    (let ((name (type-name type)))
      (when name
        (format stream "~A = ~@_" name)))
    (print-type type stream t)))


; Create or reuse a type with the given kind, tag, and parameters.
; A type is reused if one already exists with equal kind, tag, and parameters.
; Return the type.
(defun make-type (world kind tag parameters =-name /=-name)
  (let ((reverse-key (list kind tag parameters)))
    (or (gethash reverse-key (world-types-reverse world))
        (let ((type (allocate-type (world-next-type-serial-number world) kind tag parameters =-name /=-name)))
          (incf (world-next-type-serial-number world))
          (setf (gethash reverse-key (world-types-reverse world)) type)))))


; Provide a new symbol for the type.  A type can have zero or more names.
; If forward-referenced, type may be a symbol or a list of the form (union <type> <type>).
; Signal an error if the name is already used.
; user-defined is true if this is a user-defined type rather than a predefined type.
(defun add-type-name (world type symbol user-defined)
  (assert-true (symbol-in-world world symbol))
  (when (symbol-type-definition symbol)
    (error "Attempt to redefine type ~A" symbol))
  ;If the old type was anonymous, give it this name.
  (when (and (type? type) (not (type-name type)))
    (setf (type-name type) symbol))
  (setf (symbol-type-definition symbol) type)
  (when user-defined
    (setf (symbol-type-user-defined symbol) t))
  (export-symbol symbol))


; Return an existing type with the given symbol, which must be interned in a world's package.
; Signal an error if there isn't an existing type.  If allow-forward-references is true and
; symbol is an undefined type identifier, allow it, create a forward-referenced type, and return symbol.
(defun get-type (symbol allow-forward-references)
  (let ((type (symbol-type-definition symbol)))
    (cond
     ((type? type) type)
     ((not allow-forward-references) (error "Undefined type ~A with value ~S" symbol type))
     (t (unless type
          (setf (symbol-type-definition symbol) nil))
        symbol))))


; Scan a type-expr to produce a type.  Return that type.
; If allow-forward-references is true and type-expr is an undefined type identifier,
; allow it, create a forward-referenced type in the world, and return type-expr unchanged.
; If allow-forward-references is true, also allow undefined type identifiers deeper within type-expr.
; If type-expr is already a type, return it unchanged.
(defun scan-type (world type-expr &optional allow-forward-references)
  (cond
   ((identifier? type-expr)
    (get-type (world-intern world type-expr) allow-forward-references))
   ((type? type-expr)
    type-expr)
   (t (let ((type-constructor (and (consp type-expr)
                                   (symbolp (first type-expr))
                                   (get (world-find-symbol world (first type-expr)) :type-constructor))))
        (if type-constructor
          (apply type-constructor world allow-forward-references (rest type-expr))
          (error "Bad type ~S" type-expr))))))


; Same as scan-type except that ensure that the type has the expected kind.
; Return the type.
(defun scan-kinded-type (world type-expr expected-type-kind)
  (let ((type (scan-type world type-expr)))
    (unless (eq (type-kind type) expected-type-kind)
      (error "Expected ~(~A~) but got ~A" expected-type-kind (print-type-to-string type)))
    type))


; (integer-list <value> ... <value>)
; Each <value> must be a constant expression.
; ***** Currently the lists are not checked, so this type is equivalent to integer except for display purposes.
(defun scan-integer-list (world allow-forward-references &rest value-exprs)
  (declare (ignore allow-forward-references))
  (when (endp value-exprs)
    (error "Integer list type must have at least one element"))
  (let* ((integer-type (world-integer-type world))
         (values (mapcar #'(lambda (value-expr) (eval (scan-typed-value world (make-type-env nil nil) value-expr integer-type)))
                         value-exprs)))
    (unless (every #'integerp values)
      (error "Bad integer list ~S" value-exprs))
    integer-type))


; (integer-range <low-limit> <high-limit>)
; <low-limit> and <high-limit> must be constant expressions.
; ***** Currently the ranges are not checked, so this type is equivalent to integer except for display purposes.
(defun scan-integer-range (world allow-forward-references low-limit-expr high-limit-expr)
  (declare (ignore allow-forward-references))
  (let* ((integer-type (world-integer-type world))
         (low-limit (eval (scan-typed-value world (make-type-env nil nil) low-limit-expr integer-type)))
         (high-limit (eval (scan-typed-value world (make-type-env nil nil) high-limit-expr integer-type))))
    (unless (and (integerp low-limit) (integerp high-limit) (<= low-limit high-limit))
      (error "Bad integer range ~S .. ~S" low-limit-expr high-limit-expr))
    integer-type))


; (exclude-zero <type>)
; ***** Currently the exclusion is not checked, so this type is equivalent to <type> except for display purposes.
(defun scan-exclude-zero (world allow-forward-references type-expr)
  (scan-type world type-expr allow-forward-references))


; (-> (<arg-type1> ... <arg-typen>) <result-type>)
(defun scan--> (world allow-forward-references arg-type-exprs result-type-expr)
  (unless (listp arg-type-exprs)
    (error "Bad -> argument type list ~S" arg-type-exprs))
  (make-->-type world
                (mapcar #'(lambda (te) (scan-type world te allow-forward-references)) arg-type-exprs)
                (scan-type world result-type-expr allow-forward-references)))


; (vector <element-type>)
(defun scan-vector (world allow-forward-references element-type)
  (make-vector-type world (scan-type world element-type allow-forward-references)))


; (list-set <element-type>)
(defun scan-list-set (world allow-forward-references element-type)
  (make-list-set-type world (scan-type world element-type allow-forward-references)))


; (range-set <element-type>)
(defun scan-range-set (world allow-forward-references element-type)
  (make-range-set-type world (scan-type world element-type allow-forward-references)))


; (bit-set <tag> ... <tag>)
(defun scan-bit-set (world allow-forward-references &rest tag-names)
  (declare (ignore allow-forward-references))
  (make-bit-set-type world (mapcar #'(lambda (tag-name)
                                       (let ((tag (scan-tag world tag-name)))
                                         (unless (tag-keyword tag)
                                           (error "Only singleton tags may be part of a bit-set"))
                                         tag))
                                   tag-names)))


; (restricted-set <bit-set-type> <value-expr> ... <value-expr>)
(defun scan-restricted-set (world allow-forward-references bit-set-type-expr &rest value-exprs)
  (let ((bit-set-type (scan-type world bit-set-type-expr allow-forward-references)))
    (unless (bit-set-type? bit-set-type)
      (error "~S must be a bit-set" bit-set-type-expr))
    (let ((values (mapcar #'(lambda (value-expr)
                              (assert-type (eval-typed-value world value-expr bit-set-type) integer))
                          value-exprs)))
      (setq values (sort values #'<))
      (let ((length1 (length values)))
        (delete-adjacent-duplicates values :test #'=)
        (unless (= (length values) length1)
          (error "Duplicate restricted-set value in ~S" value-exprs)))
      (make-restricted-set-type world bit-set-type values))))


; (tag <tag> ... <tag>)
(defun scan-tag-type (world allow-forward-references tag-name &rest tag-names)
  (if tag-names
    (apply #'make-union-type world (mapcar #'(lambda (tag-name)
                                               (scan-tag-type world allow-forward-references tag-name))
                                           (cons tag-name tag-names)))
    (make-tag-type world (scan-tag world tag-name))))


; (union <type1> ... <typen>)
(defun scan-union (world allow-forward-references &rest type-exprs)
  (apply #'make-union-type world (mapcar #'(lambda (type-expr)
                                             (scan-type world type-expr allow-forward-references))
                                         type-exprs)))


; (type-diff <type1> <type2>)
; Does not allow forward references in either operand.
(defun scan-type-diff (world allow-forward-references type-expr1 type-expr2)
  (declare (ignore allow-forward-references))
  (let ((type1 (scan-type world type-expr1 nil))
        (type2 (scan-type world type-expr2 nil)))
    (multiple-value-bind (subtype1 subtype2) (type-difference world type1 type2)
      (declare (ignore subtype1))
      subtype2)))


; (writable-cell <element-type>)
(defun scan-writable-cell (world allow-forward-references element-type)
  (make-writable-cell-type world (scan-type world element-type allow-forward-references)))


; (delay <element-type>)
(defun scan-delay (world allow-forward-references type)
  (make-delay-type world (scan-type world type allow-forward-references)))


; Resolve all forward type references to refer to their target types.
; Signal an error if any unresolved type references remain.
; Only types reachable from some type name are affected.  It is the caller's
; responsibility to make sure that these are the only types that exist.
; Return a list of all type structures encountered.
(defun resolve-forward-types (world)
  (let ((visited-types (make-hash-table :test #'eq)))
    (labels
      ((resolve-type-symbol (symbol type symbol-stack)
         (cond
          ((type? type) type)
          ((null type) (error "Undefined type ~A" symbol))
          ((member symbol symbol-stack)
           (error "Recursive type forward reference ~S ~S" symbol symbol-stack))
          (t (let ((type (resolve-type-expr type (cons symbol symbol-stack))))
               (assert-true (type? type))
               ;If the old type was anonymous, give it this name.
               (unless (type-name type)
                 (setf (type-name type) symbol))
               (setf (symbol-type-definition symbol) type)
               type))))
       
       (resolve-type-expr (type symbol-stack)
         (cond
          ((type? type) type)
          ((symbolp type)
           (resolve-type-symbol type (symbol-type-definition type) symbol-stack))
          ((structured-type? type '(tuple (eql union) t t))
           (let ((type1 (resolve-type-expr (second type) symbol-stack))
                 (type2 (resolve-type-expr (third type) symbol-stack)))
             (type-union world type1 type2)))
          (t (error "Bad forward-referenced type ~S" type))))
       
       (resolve-type-parameters (type)
         (unless (gethash type visited-types)
           (setf (gethash type visited-types) t)
           (do ((parameter-types (type-parameters type) (cdr parameter-types)))
               ((endp parameter-types))
             (let ((parameter-type (car parameter-types)))
               (unless (type? parameter-type)
                 (setq parameter-type (resolve-type-expr parameter-type nil))
                 (setf (car parameter-types) parameter-type))
               (resolve-type-parameters parameter-type))))))
      
      (each-type-definition
       world
       #'(lambda (symbol type)
           (unless (type? type)
             (setq type (resolve-type-symbol symbol type nil)))
           (resolve-type-parameters type))))
    (setf (world-types-reverse world) nil)
    (hash-table-keys visited-types)))


; Recompute the types-reverse hash table from the types in the types hash table and their constituents.
(defun recompute-type-caches (world)
  (let ((types-reverse (make-hash-table :test #'equal)))
    (labels
      ((visit-type (type)
         (let ((reverse-key (list (type-kind type) (type-tag type) (type-parameters type))))
           (assert-true (eq (gethash reverse-key types-reverse type) type))
           (unless (gethash reverse-key types-reverse)
             (setf (gethash reverse-key types-reverse) type)
             (mapc #'visit-type (type-parameters type))))))
      (visit-type (world-denormalized-false-type world))
      (visit-type (world-denormalized-true-type world))
      (visit-type (world-boxed-boolean-type world))
      (each-type-definition
       world
       #'(lambda (symbol type)
           (declare (ignore symbol))
           (visit-type type))))
    (setf (world-types-reverse world) types-reverse)))



; Return true if type1's serial-number is less than type2's serial-number;
; however, unnamed types' serial numbers are considered to be positive infinity.
(defun type-named-serial-number-< (type1 type2)
  (let ((name1 (if (type-name type1) 0 1))
        (name2 (if (type-name type2) 0 1)))
    (or (< name1 name2)
        (and (= name1 name2)
             (< (type-serial-number type1) (type-serial-number type2))))))


; Make all equivalent types be eq.  Only types reachable from some type name
; are affected, and names may be redirected to different type structures than
; the ones to which they currently point.  It is the caller's responsibility
; to make sure that there are no current outstanding references to types other
; than via type names (except for types for which it can be guaranteed that
; their type structures are defined only once; this applies to types such as
; integer and char16 but not (vector integer)).
;
; This function calls resolve-forward-types before making equivalent types be eq
; and recompute-type-caches just before returning.
;
; This function works by initially assuming that all types with the same kind
; and tag are the same type and then iterately determining which ones must be
; different because they contain different parameter types.
(defun unite-types (world)
  (let* ((types (resolve-forward-types world))
         (n-types (length types)))
    (labels
      ((gen-cliques-1 (get-key)
         (let ((types-to-cliques (make-hash-table :test #'eq :size n-types))
               (keys-to-cliques (make-hash-table :test #'equal))
               (n-cliques 0))
           (dolist (type types)
             (let* ((key (funcall get-key type))
                    (clique (gethash key keys-to-cliques)))
               (unless clique
                 (setq clique n-cliques)
                 (incf n-cliques)
                 (setf (gethash key keys-to-cliques) clique))
               (setf (gethash type types-to-cliques) clique)))
           (values n-cliques types-to-cliques)))
       
       (gen-cliques (n-old-cliques types-to-old-cliques)
         (labels
           ((get-old-clique (type)
              (assert-non-null (gethash type types-to-old-cliques)))
            (get-type-key (type)
              (cons (get-old-clique type)
                    (mapcar #'get-old-clique (type-parameters type)))))
           (multiple-value-bind (n-new-cliques types-to-new-cliques) (gen-cliques-1 #'get-type-key)
             (assert-true (>= n-new-cliques n-old-cliques))
             (if (/= n-new-cliques n-old-cliques)
               (gen-cliques n-new-cliques types-to-new-cliques)
               (translate-types n-new-cliques types-to-new-cliques)))))
       
       (translate-types (n-cliques types-to-cliques)
         (let ((clique-representatives (make-array n-cliques :initial-element nil)))
           (maphash #'(lambda (type clique)
                        (let ((representative (svref clique-representatives clique)))
                          (when (or (null representative) (type-named-serial-number-< type representative))
                            (setf (svref clique-representatives clique) type))))
                    types-to-cliques)
           (assert-true (every #'identity clique-representatives))
           (labels
             ((map-type (type)
                (svref clique-representatives (gethash type types-to-cliques))))
             (dolist (type types)
               (do ((parameter-types (type-parameters type) (cdr parameter-types)))
                   ((endp parameter-types))
                 (setf (car parameter-types) (map-type (car parameter-types)))))
             (each-type-definition
              world
              #'(lambda (symbol type)
                  (setf (symbol-type-definition symbol) (map-type type))))))))
      
      (multiple-value-call
       #'gen-cliques
       (gen-cliques-1 #'(lambda (type) (cons (type-kind type) (type-tag type)))))
      (recompute-type-caches world))))


;;; ------------------------------------------------------------------------------------------------------
;;; COMPARISONS


; Return (:test <type-equality-function>), simplifying to nil if the equality function is eql.
(defun element-test (world type)
  (let ((test (get-type-=-name world type)))
    (if (eq test 'eql)
      nil
      `(:test #',test))))


; Return non-nil if the values are equal.  value1 and value2 must both belong to a union type.
(defun union= (value1 value2)
  (or (eql value1 value2)
      (and (consp value1) (consp value2)
           (let ((tag-name1 (car value1))
                 (tag-name2 (car value2)))
             (and (eq tag-name1 tag-name2)
                  (funcall (get tag-name1 :tag=) value1 value2))))))


; Create an equality comparison function for elements of the given :vector type.
; Return the name of the function and also set it in the type.
(defun compute-vector-type-=-name (world type)
  (let ((element-type (vector-element-type type)))
    (case (type-kind element-type)
      ((:integer :rational) (setf (type-=-name type) 'equal))
      (t (let ((=-name (gentemp (format nil "~A_VECTOR_=" (type-name element-type)) (world-package world))))
           (setf (type-=-name type) =-name) ;Must do this now to prevent runaway recursion.
           (quiet-compile =-name `(lambda (a b)
                                    (and (= (length a) (length b))
                                         (every #',(get-type-=-name world element-type) a b))))
           =-name)))))


; Create an equality comparison function for elements of the given :list-set type.
; Return the name of the function and also set it in the type.
(defun compute-list-set-type-=-name (world type)
  (let* ((element-type (set-element-type type))
         (=-name (gentemp (format nil "~A_LISTSET_=" (type-name element-type)) (world-package world))))
    (setf (type-=-name type) =-name) ;Must do this now to prevent runaway recursion.
    (quiet-compile =-name `(lambda (a b)
                             (and (= (length a) (length b))
                                  (subsetp a b ,@(element-test world element-type)))))
    =-name))


; Create an equality comparison function for elements of the given :tag type.
; Return the name of the function and also set it in the type, the tag, and the :tag= property of the tag-name.
(defun compute-tag-type-=-name (world type)
  (let ((tag (type-tag type)))
    (assert-true (null (tag-=-name tag)))
    (labels
      ((fields-=-code (fields)
         (assert-true fields)
         (let ((field-=-code (cons (get-type-=-name world (field-type (car fields))) '((car a) (car b)))))
           (if (cdr fields)
             `(and ,field-=-code
                   (let ((a (cdr a))
                         (b (cdr b)))
                     ,(fields-=-code (cdr fields))))
             field-=-code))))
      
      (let* ((name (tag-name tag))
             (=-name (world-intern world (concatenate 'string (string name) "_="))))
        (setf (type-=-name type) =-name) ;Must do this now to prevent runaway recursion.
        (let ((=-code `(lambda (a b)
                         (let ((a (cdr a))
                               (b (cdr b)))
                           ,(fields-=-code (tag-fields tag))))))
          (assert-true (not (fboundp =-name)))
          (quiet-compile =-name =-code)
          (setf (get name :tag=) (symbol-function =-name))
          (setf (tag-=-name tag) =-name))))))


; Return the name of a function that compares two instances of this type and returns non-nil if they are equal.
; Signal an error if there is no such function.
; If the type is a tag, also set the :tag= property of the tag.
(defun get-type-=-name (world type)
  (or (type-=-name type)
      (case (type-kind type)
        (:vector (compute-vector-type-=-name world type))
        (:list-set (compute-list-set-type-=-name world type))
        (:tag (compute-tag-type-=-name world type))
        (:union
         (setf (type-=-name type) 'union=) ;Must do this now to prevent runaway recursion.
         (dolist (subtype (type-parameters type))
           (get-type-=-name world subtype)) ;Set the :tag= symbol properties.
         'union=)
        (t (error "Can't apply = to instances of type ~S" (print-type-to-string type))))))


; Return the name of a function that compares two instances of this type and returns non-nil if they satisfy the given
; order, which should be one of the symbols =, /=, <, >, <=, >=.
; Signal an error if there is no such function except for /=, in which case return nil.
(defun get-type-order-name (world type order)
  (ecase order
    (= (get-type-=-name world type))
    (/= (type-/=-name type))
    ((< > <= >=)
     (or (cdr (assoc order (type-order-alist type)))
         (error "Can't apply ~A to instances of type ~A" order (print-type-to-string type))))))


; Return code to compare code expression a against b using the given order, which should be one of
; the symbols =, /=, <, >, <=, >=, set<=.
; Signal an error if this is not possible.
(defun get-type-order-code (world type order a b)
  (flet ((simple-constant? (code)
           (or (keywordp code) (numberp code) (characterp code))))
    (cond
     ((eq order 'set<=)
      (unless (eq (type-kind type) :list-set)
        (error "set<= not implemented on type ~S" type))
      (list* 'subsetp a b (element-test world (set-element-type type))))
     (t
      (let ((order-name (get-type-order-name world type order)))
        (cond
         ((null order-name)
          (assert-true (eq order '/=))
          (list 'not (get-type-order-code world type '= a b)))
         ((and (eq order-name 'union=) (or (simple-constant? a) (simple-constant? b)))
          ;Optimize union= comparisons against a non-list constant.
          (list 'eql a b))
         (t (list order-name a b))))))))



;;; ------------------------------------------------------------------------------------------------------
;;; SPECIALS


(defun checked-callable (f)
  (let ((fun (callable f)))
    (unless fun
      (warn "Undefined function ~S" f))
    fun))


; Add a command or special form definition.  symbol is a symbol that names the
; preprocessor directive, command, or special form.  When a semantic form
;   (id arg1 arg2 ... argn)
; is encountered and id is a symbol with the same name as symbol, the form is
; replaced by the result of calling one of:
;   (expander preprocessor-state id arg1 arg2 ... argn)           if property is :preprocess
;   (expander world grammar-info-var arg1 arg2 ... argn)          if property is :command
;   (expander world type-env rest last id arg1 arg2 ... argn)     if property is :statement
;   (expander world type-env id arg1 arg2 ... argn)               if property is :special-form or :condition
;   (expander world allow-forward-references arg1 arg2 ... argn)  if property is :type-constructor
; expander must be a function or a function symbol.
;
; In the case of the statement expander only, rest is a list of the remaining statements in the block;
; the statement expander should recursively expand the statements in rest.
; last is non-nil if this statement+rest's return value would pass through as the return value of the function;
; last allows optimization of lisp code to eliminate extraneous return-from statements.
;
; depictor is used instead of expander when emitting markup for the command or special form.
; depictor is called via:
;   (depictor markup-stream world depict-env arg1 arg2 ... argn)     if property is :command
;   (depictor markup-stream world arg1 arg2 ... argn)                if property is :statement
;   (depictor markup-stream world level arg1 arg2 ... argn)          if property is :special-form
;   (depictor markup-stream world level arg1 arg2 ... argn)          if property is :type-constructor
;
(defun add-special (property symbol expander &optional depictor)
  (let ((emit-property (cdr (assoc property '((:command . :depict-command)
                                              (:statement . :depict-statement)
                                              (:special-form . :depict-special-form)
                                              (:condition)
                                              (:type-constructor . :depict-type-constructor))))))
    (assert-true (or emit-property (not depictor)))
    (assert-type symbol identifier)
    (when *value-asserts*
      (checked-callable expander)
      (when depictor (checked-callable depictor)))
    (when (or (get symbol property) (and emit-property (get symbol emit-property)))
      (error "Attempt to redefine ~A ~A" property symbol))
    (setf (get symbol property) expander)
    (when emit-property
      (if depictor
        (setf (get symbol emit-property) depictor)
        (remprop symbol emit-property)))
    (export-symbol symbol)))


;;; ------------------------------------------------------------------------------------------------------
;;; PRIMITIVES

(defstruct (primitive (:constructor make-primitive (type-expr value-code appearance &key markup1 markup2 level level1 level2))
                      (:predicate primitive?))
  (type nil :type (or null type))          ;Type of this primitive; nil if not computed yet
  (type-expr nil :read-only t)             ;Source type expression that designates the type of this primitive
  (value-code nil :read-only t)            ;Lisp expression that computes the value of this primitive
  (appearance nil :read-only t)            ;One of the possible primitive appearances (see below)
  (markup1 nil :read-only t)               ;Markup (item or list) for this primitive
  (markup2 nil :read-only t)               ;:global primitives:  name to use for an external reference
  ;                                        ;:unary primitives:   markup (item or list) for this primitive's closer
  ;                                        ;:infix primitives:   true if spaces should be put around primitive
  (level nil :read-only t)                 ;Precedence level of markup for this primitive
  (level1 nil :read-only t)                ;Precedence level required for first argument of this primitive
  (level2 nil :read-only t))               ;Precedence level required for second argument of this primitive

;appearance is one of the following:
; :global      The primitive appears as a regular, global function or constant; its markup is in markup1.
;                If this primitive should generate an external reference, markup2 contains the name to use for the reference
; :infix       The primitive is an infix binary primitive; its markup is in markup1; if markup2 is true, put spaces around markup1
; :unary       The primitive is a prefix and/or suffix unary primitive; the prefix is in markup1 and suffix in markup2
; :phantom     The primitive disappears when emitting markup for it


; Call this to declare all primitives when initially constructing a world,
; before types have been constructed.
(defun declare-primitive (symbol type-expr value-code appearance &rest key-args)
  (when (symbol-primitive symbol)
    (error "Attempt to redefine primitive ~A" symbol))
  (setf (symbol-primitive symbol) (apply #'make-primitive type-expr value-code appearance key-args))
  (export-symbol symbol))


; Call this to compute the primitive's type from its type-expr.
(defun define-primitive (world primitive)
  (setf (primitive-type primitive) (scan-type world (primitive-type-expr primitive))))


; If name is an identifier not already used by a special form, command, or primitive,
; return it interened into the world's package.  If not, generate an error.
(defun scan-name (world name)
  (unless (identifier? name)
    (error "~S should be an identifier" name))
  (let ((symbol (world-intern world name)))
    (when (and (get-properties (symbol-plist symbol) '(:special-form :condition :primitive :type-constructor))
               (not (get symbol :non-reserved)))
      (error "~A is reserved" symbol))
    symbol))


;;; ------------------------------------------------------------------------------------------------------
;;; TYPE ENVIRONMENTS

;;; A type environment is an alist that associates bound variables with their types.
;;; A variable may be bound multiple times; the first binding in the environment list
;;; shadows ones further in the list.
;;; The following kinds of bindings are allowed in a type environment:
;;;
;;;   <type-env-local> (see below)
;;;   Normal local variable
;;;
;;;   <type-env-action> (see below)
;;;   Action variable
;;;
;;;   (:return . type)
;;;   The function's return type
;;;
;;;   (:return-block-name . symbol-or-nil)
;;;   The name of the lisp return-from block to be used for returning from this function or nil if not needed yet.
;;;   This binding's symbol-or-nil is mutated in place as needed.
;;;
;;;   (:lhs-symbol . symbol)
;;;   The lhs nonterminal's symbol if this is a type environment for an action function.
;;;

(defstruct (type-env-local (:type list) (:constructor make-type-env-local (name type mode)))
  name      ;World-interned name of the local variable
  type      ;That variable's type
  mode)     ;:const if the variable is read-only;
;           ;:var if it's writable;
;           ;:uninitialized if it's writable but not initialized unless the name also appears in the type-env's live list;
;           ;:function if it's bound by flet;
;           ;:reserved if it's bound by reserve;
;           ;:unused if it's defined but shouldn't be used

(defstruct (type-env-action (:type list) (:constructor make-type-env-action (key local-symbol type general-grammar-symbol)))
  key                     ;(action symbol . index)
  ;                       ;   action is a world-interned symbol denoting the action function being called
  ;                       ;   symbol is a terminal or nonterminal's symbol on which the action is called
  ;                       ;   index is the one-based index used to distinguish among identical
  ;                       ;     symbols in the rhs of a production.  The first occurrence of this
  ;                       ;     symbol has index 1, the second has index 2, and so on.
  ;                       ;     The occurrence of symbol on the left side of the production has index 0.
  local-symbol            ;A unique local variable name used to represent the action function's value in the generated lisp code
  type                    ;Type of the action function's value
  general-grammar-symbol) ;The general-grammar-symbol corresponding to the index-th instance of symbol in the production's rhs

(defstruct (type-env (:constructor make-type-env (bindings live)))
  (bindings nil :type list)   ;List of bindings
  (live nil :type list))      ;List of symbols of :uninitialized variables that have been initialized


(defparameter *null-type-env* (make-type-env nil nil))
(defconstant *type-env-flags* '(:return :return-block-name :lhs-symbol))


; If symbol is a local variable, return its binding; if not, return nil.
; symbol must already be world-interned.
(defun type-env-get-local (type-env symbol)
  (assoc symbol (type-env-bindings type-env) :test #'eq))


; name must be the name of an :uninitialized variable in this type-env.  Return true if this variable
; has been initialized.
(defun type-env-initialized (type-env name)
  (member name (type-env-live type-env) :test #'eq))


; If the currently generated function is an action for a rule with at least index
; instances of the given grammar-symbol's symbol on the right-hand side, and if action is
; a legal action for that symbol, return the type-env-action; otherwise, return nil.
; action must already be world-interned.
(defun type-env-get-action (type-env action symbol index)
  (assoc (list* action symbol index) (type-env-bindings type-env) :test #'equal))


; Nondestructively append the binding to the front of the type-env and return the new type-env.
; If shadow is true, the binding may shadow an existing local variable with the same name.
(defun type-env-add-binding (type-env name type mode &optional shadow)
  (assert-true (and
                (symbolp name)
                (type? type)
                (member mode '(:const :var :uninitialized :function :reserved :unused))))
  (unless shadow
    (let ((binding (type-env-get-local type-env name)))
      (when binding
        (error "Local variable ~A:~A shadows an existing local variable ~A:~A"
               name (print-type-to-string type)
               (type-env-local-name binding) (print-type-to-string (type-env-local-type binding))))))
  (make-type-env
   (cons (make-type-env-local name type mode) (type-env-bindings type-env))
   (type-env-live type-env)))


; Define the reserved name as a :const binding.
(defun type-env-unreserve-binding (type-env name type)
  (let ((binding (type-env-get-local type-env name)))
    (unless (and binding (eq (type-env-local-mode binding) :reserved))
      (error "Local variable ~A:~A needs to be reserved first" name (print-type-to-string type)))
    (type-env-add-binding type-env name type :const t)))


; Nondestructively shadow the type of the binding of name in type-env and return the new type-env.
(defun type-env-narrow-binding (type-env name type)
  (let ((binding (assert-non-null (type-env-get-local type-env name))))
    (type-env-add-binding type-env name type (type-env-local-mode binding) t)))


; Nondestructively unshadow the type of the binding of name in type-env and return two values:
;   the previous binding of name;
;   the new type-env.
(defun type-env-unnarrow-binding (type-env name)
  (let* ((bindings (type-env-bindings type-env))
         (shadow-tail (assert-non-null (member name bindings :test #'eq :key #'car)))
         (tail (cdr shadow-tail))
         (old-binding (assoc name tail :test #'eq)))
    (unless old-binding
      (error "Can't unshadow ~S" name))
    (let ((unshadowed-bindings (nconc (ldiff bindings shadow-tail) tail)))
      (values
       old-binding
       (make-type-env unshadowed-bindings (type-env-live type-env))))))


; Mark name as an initialized variable.  It should have been declared as :uninitialized.
(defun type-env-initialize-var (type-env name)
  (if (type-env-initialized type-env name)
    type-env
    (make-type-env
     (type-env-bindings type-env)
     (cons name (type-env-live type-env)))))


; Create new bindings for the function's return type and return block name and return the new type-env.
(defun type-env-init-function (type-env return-type)
  (set-type-env-flag
   (set-type-env-flag type-env :return return-type)
   :return-block-name
   nil))


; Either reuse or generate a name for return-from statements exiting this function.
(defun gen-type-env-return-block-name (type-env)
  (let ((return-block-binding (assert-non-null (assoc :return-block-name (type-env-bindings type-env)))))
    (or (cdr return-block-binding)
        (setf (cdr return-block-binding) (gensym "RETURN")))))


; Return an environment obtained from the type-env by adding a binding of flag to value.
(defun set-type-env-flag (type-env flag value)
  (assert-true (member flag *type-env-flags*))
  (make-type-env
   (acons flag value (type-env-bindings type-env))
   (type-env-live type-env)))


; Return the value bound to the given flag.
(defun get-type-env-flag (type-env flag)
  (assert-true (member flag *type-env-flags*))
  (cdr (assoc flag (type-env-bindings type-env))))


; Ensure that sub-type-env is derived from base-type-env.
(defun ensure-narrowed-type-env (base-type-env sub-type-env)
  (unless (and (tailp (type-env-bindings base-type-env) (type-env-bindings sub-type-env))
               (equal (type-env-live base-type-env) (type-env-live sub-type-env)))
    (error "The type environment ~S isn't narrower than ~S" sub-type-env base-type-env)))


; live1 and live2 are either :dead or lists of :uninitialized variables that have been initialized.
; Return :dead if both live1 and live2 are dead or a list of initialized variables that would be valid
; on a merge point between code paths resulting in live1 and live2.
(defun merge-live-lists (live1 live2)
  (cond
   ((eq live1 :dead) live2)
   ((eq live2 :dead) live1)
   (t (intersection live1 live2 :test #'eq))))


; If live is :dead, return nil; otherwise, return type-env with live substituted for type-env's old live list.
(defun substitute-live (type-env live)
  (cond
   ((eq live :dead) nil)
   ((equal live (type-env-live type-env)) type-env)
   (t (make-type-env (type-env-bindings type-env) live))))


;;; ------------------------------------------------------------------------------------------------------
;;; VALUES

;;; A value is one of the following:
;;;   A void value (represented by nil)
;;;   A boolean (nil for false; non-nil for true)
;;;   An integer
;;;   A rational number
;;;   A *float32-type* (or :+zero32, :-zero32, :+infinity32, :-infinity32, or :nan32)
;;;   A *float64-type* (or :+zero64, :-zero64, :+infinity64, :-infinity64, or :nan64)
;;;   A character
;;;   A function (represented by a lisp function)
;;;   A string
;;;   A vector (represented by a list)
;;;   A list-set (represented by an unordered list of its elements)
;;;   A range-set of integers or characters (represented by an intset of its elements converted to integers)
;;;   A bit-set (represented by an integer with 1's in bits corresponding to present tags)  ***** Not implemented yet *****
;;;   A restricted-set (represented by an integer with 1's in bits corresponding to present tags)  ***** Not implemented yet *****
;;;   A tag (represented by either a keyword or a list (keyword [serial-num] field-value1 ... field-value n));
;;;     serial-num is a unique integer present only on mutable tag instances.
;;;   A writable-cell (represented by a cons whose car is a flag that is true if the cell is initialized
;;;     and cdr is nil or the value)
;;;   A delayed-value structure


(defstruct (delayed-value (:constructor make-delayed-value (symbol)) (:predicate delayed-value?))
  (symbol nil :type symbol :read-only t))  ;Global variable name


; Return the bit-set value as a list of tag keywords.
(defun bit-set-to-list (value bit-set-type)
  (assert-true (and (bit-set-type? bit-set-type) (integerp value) (>= value 0) (< value (ash 1 (length (type-tag bit-set-type))))))
  (let ((tags-present nil))
    (dolist (tag (type-tag bit-set-type))
      (when (oddp value)
        (push (assert-non-null (tag-keyword tag)) tags-present))
      (setq value (ash value -1)))
    (nreverse tags-present)))
  

; Return true if the value appears to have the given tag.  This function
; may return false positives (return true when the value doesn't actually
; have the given type) but never false negatives.
; If shallow is true, only test at the top level.
(defun value-has-tag (value tag &optional shallow)
  (labels
    ((check-fields (fields values)
       (if (endp fields)
         (null values)
         (and (consp values)
              (or shallow (value-has-type (car values) (field-type (car fields))))
              (check-fields (cdr fields) (cdr values))))))
    (let ((keyword (tag-keyword tag)))
      (if keyword
        (eq value keyword)
        (and (consp value)
             (eq (car value) (tag-name tag))
             (let ((values (cdr value))
                   (fields (tag-fields tag)))
               (if (tag-mutable tag)
                 (and (consp values) (integerp (car values)) (check-fields fields (cdr values)))
                 (check-fields fields values))))))))


; Return true if the value appears to have the given type.  This function
; may return false positives (return true when the value doesn't actually
; have the given type) but never false negatives.
; If shallow is true, only test at the top level.
(defun value-has-type (value type &optional shallow)
  (case (type-kind type)
    (:bottom nil)
    (:void t)
    (:boolean t)
    (:integer (integerp value))
    (:rational (rationalp value))
    (:finite32 (and (finite32? value) (not (zerop value))))
    (:finite64 (and (finite64? value) (not (zerop value))))
    (:char16 (characterp value))
    (:supplementary-char (and (consp value) (eq (car value) :supplementary-char) (integerp (cdr value)) (<= #x10000 (cdr value) #x10FFFF)))
    (:-> (functionp value))
    (:string (stringp value))
    (:vector (value-list-has-type value (vector-element-type type) shallow))
    (:list-set (value-list-has-type value (set-element-type type) shallow))
    (:range-set (valid-intset? value))
    (:bit-set (and (integerp value) (<= 0 value) (< value (ash 1 (length (type-tag type))))))
    (:restricted-set (member value (type-tag type)))
    (:tag (value-has-tag value (type-tag type) shallow))
    (:union (some #'(lambda (subtype) (value-has-type value subtype shallow))
                  (type-parameters type)))
    (:writable-cell (and (consp value)
                         (if (car value)
                           (or shallow (value-has-type (cdr value) (writable-cell-element-type type)))
                           (null (cdr value)))))
    (:delay (or (delayed-value? value) (value-has-type value (delay-element-type type))))
    (t (error "Bad typekind ~S" (type-kind type)))))


; Return true if the value is a list of elements that appear to have the given type.  This function
; may return false positives (return true when the value doesn't actually
; have the given type) but never false negatives.
; If shallow is true, only check the list structure -- don't test that the elements have the given type.
(defun value-list-has-type (values type shallow)
  (or (null values)
      (and (consp values)
           (or shallow (value-has-type (car values) type))
           (value-list-has-type (cdr values) type shallow))))


; Print the values list using set notation.
(defun print-set-of-values (values element-type stream)
  (pprint-logical-block (stream values :prefix "{" :suffix "}")
    (pprint-exit-if-list-exhausted)
    (loop
      (print-value (pprint-pop) element-type stream)
      (pprint-exit-if-list-exhausted)
      (format stream " ~:_"))))


; Print the value nicely on the given stream.  type is the value's type.
(defun print-value (value type &optional (stream t))
  (assert-true (value-has-type value type t))
  (case (type-kind type)
    (:void (assert-true (null value))
           (write-string "empty" stream))
    (:boolean (write-string (if value "true" "false") stream))
    ((:integer :rational :char16 :supplementary-char :->) (write value :stream stream))
    ((:finite32 :finite64) (write value :stream stream))
    (:string (prin1 value stream))
    (:vector (let ((element-type (vector-element-type type)))
               (pprint-logical-block (stream value :prefix "(" :suffix ")")
                 (pprint-exit-if-list-exhausted)
                 (loop
                   (print-value (pprint-pop) element-type stream)
                   (pprint-exit-if-list-exhausted)
                   (format stream " ~:_")))))
    (:list-set (print-set-of-values value (set-element-type type) stream))
    (:range-set (let ((converter (range-set-decode-function (set-element-type type))))
                  (pprint-logical-block (stream value :prefix "{" :suffix "}")
                    (pprint-exit-if-list-exhausted)
                    (loop
                      (let* ((values (pprint-pop))
                             (value1 (car values))
                             (value2 (cdr values)))
                        (if (= value1 value2)
                          (write (funcall converter value1) :stream stream)
                          (write (list (funcall converter value1) (funcall converter value2)) :stream stream))))
                    (pprint-exit-if-list-exhausted)
                    (format stream " ~:_"))))
    ((:bit-set :restricted-set) (print-set-of-values (bit-set-to-list value (underlying-bit-set-type type)) (set-element-type type) stream))
    (:tag (let ((tag (type-tag type)))
            (if (tag-keyword tag)
              (write value :stream stream)
              (pprint-logical-block (stream (tag-fields tag) :prefix "[" :suffix "]")
                (write (pop value) :stream stream)
                (when (tag-mutable tag)
                  (format stream " ~:_~D" (pop value)))
                (loop
                  (pprint-exit-if-list-exhausted)
                  (format stream " ~:_")
                  (print-value (pop value) (field-type (pprint-pop)) stream))))))
    (:union (dolist (subtype (type-parameters type)
                             (error "~S is not an instance of ~A" value (print-type-to-string type)))
              (when (value-has-type value subtype t)
                (print-value value subtype stream)
                (return))))
    (:writable-cell (if (car value)
                      (print-value (cdr value) (writable-cell-element-type type) stream)
                      (write-string "uninitialized" stream)))
    (:delay (if (delayed-value? value)
              (write value :stream stream)
              (print-value value (delay-element-type type) stream)))
    (t (error "Bad typekind ~S" (type-kind type)))))


; Print a list of values nicely on the given stream.  types is the list of the
; values' types (and should have the same length as the list of values).
; If prefix and/or suffix are non-null, use them as beginning and ending
; delimiters of the printed list.
(defun print-values (values types &optional (stream t) &key prefix suffix)
  (assert-true (= (length values) (length types)))
  (pprint-logical-block (stream values :prefix prefix :suffix suffix)
    (pprint-exit-if-list-exhausted)
    (dolist (type types)
      (print-value (pprint-pop) type stream)
      (pprint-exit-if-list-exhausted)
      (format stream " ~:_"))))


;;; ------------------------------------------------------------------------------------------------------
;;; VALUE EXPRESSIONS

;;; Expressions are annotated to avoid having to duplicate the expression scanning logic when
;;; emitting markup for expressions.  Expression forms are prefixed with an expr-annotation symbol
;;; to indicate their kinds.  These symbols are in their own package to avoid potential confusion
;;; with keywords, variable names, terminals, etc.
;;;
;;; Some special forms are extended to include parsed type information for the benefit of markup logic.
(eval-when (:compile-toplevel :load-toplevel :execute)
  (defpackage "EXPR-ANNOTATION"
    (:use)
    (:export "CONSTANT"        ;(expr-annotation:constant <constant>)
             "PRIMITIVE"       ;(expr-annotation:primitive <interned-id>)
             "TAG"             ;(expr-annotation:tag <tag>)
             "LOCAL"           ;(expr-annotation:local <interned-id>)      ;Local or lexically scoped variable
             "GLOBAL"          ;(expr-annotation:global <interned-id>)     ;Global variable
             "CALL"            ;(expr-annotation:call <function-expr> <arg-expr> ... <arg-expr>)
             "ACTION"          ;(expr-annotation:action <action> <general-grammar-symbol> <optional-index>)
             "BEGIN"           ;(expr-annotation:begin <statement> ... <statement>)
             "SPECIAL-FORM"))) ;(expr-annotation:special-form <interned-form> ...)


; Return true if the annotated-stmt is a statement with the given special-form, which must be a symbol
; but does not have to be interned in the world's package.
(defun special-form-annotated-stmt? (world special-form annotated-stmt)
  (eq (first annotated-stmt) (world-find-symbol world special-form)))


; Return true if the annotated-expr is a special form annotated expression with
; the given special-form.  special-form must be a symbol but does not have to be interned
; in the world's package.
(defun special-form-annotated-expr? (world special-form annotated-expr)
  (and (eq (first annotated-expr) 'expr-annotation:special-form)
       (eq (second annotated-expr) (world-find-symbol world special-form))))


; Return the value of the global variable with the given symbol.
; Compute the value if the variable was unbound.
; Use the *busy-variables* list to prevent infinite recursion while computing variable values.
(defmacro fetch-value (symbol)
  `(if (boundp ',symbol)
     (symbol-value ',symbol)
     (compute-variable-value ',symbol)))


; Store the value into the global variable with the given symbol.
(defmacro store-global-value (symbol value)
  `(if (boundp ',symbol)
     (setf (symbol-value ',symbol) ,value)
     (error "Unbound variable ~S" ',symbol)))


; Generate a lisp expression that will call the given action on the grammar symbol.
; type-env is the type environment.
; Return three values:
;   The expression's value (a lisp expression)
;   The expression's type
;   The annotated value-expr
(defun scan-action-call (type-env action symbol &optional (index 1 index-supplied))
  (unless (integerp index)
    (error "Production rhs grammar symbol index ~S must be an integer" index))
  (let ((symbol-action (type-env-get-action type-env action symbol index)))
    (unless symbol-action
      (error "Action ~S not found" (list action symbol index)))
    (let ((multiple-symbols (type-env-get-action type-env action symbol (if (= index 0) 1 2))))
      (when (and (not index-supplied) multiple-symbols)
        (error "Ambiguous index in action ~S" (list action symbol)))
      (when (and (= index 1)
                 (not multiple-symbols)
                 (grammar-symbol-= symbol (assert-non-null (get-type-env-flag type-env :lhs-symbol))))
        (setq multiple-symbols t))
      (values (type-env-action-local-symbol symbol-action)
              (type-env-action-type symbol-action)
              (list* 'expr-annotation:action action (type-env-action-general-grammar-symbol symbol-action)
                     (and multiple-symbols (list index)))))))


; Generate a lisp expression that will compute the value of value-expr.
; type-env is the type environment.  The expression may refer to free variables
; present in the type-env.
; Return three values:
;   The expression's value (a lisp expression)
;   The expression's type
;   The annotated value-expr
(defun scan-value (world type-env value-expr)
  (labels
    ((syntax-error ()
       (error "Syntax error: ~S" value-expr))
     
     ;Scan a function call.  The function has already been scanned into its value and type,
     ;but the arguments are still unprocessed.
     (scan-call (function-value function-type function-annotated-expr arg-exprs)
       (let ((arg-values nil)
             (arg-types nil)
             (arg-annotated-exprs nil))
         (dolist (arg-expr arg-exprs)
           (multiple-value-bind (arg-value arg-type arg-annotated-expr) (scan-value world type-env arg-expr)
             (push arg-value arg-values)
             (push arg-type arg-types)
             (push arg-annotated-expr arg-annotated-exprs)))
         (let ((arg-values (nreverse arg-values))
               (arg-types (nreverse arg-types))
               (arg-annotated-exprs (nreverse arg-annotated-exprs)))
           (handler-bind (((or error warning)
                           #'(lambda (condition)
                               (declare (ignore condition))
                               (format *error-output*
                                       "~@<In ~S: ~_Function of type ~A called with arguments of types~:_~{ ~A~}~:>~%"
                                       value-expr
                                       (print-type-to-string function-type)
                                       (mapcar #'print-type-to-string arg-types)))))
             (unless (eq (type-kind function-type) :->)
               (error "Non-function called"))
             (let ((parameter-types (->-argument-types function-type)))
               (unless (= (length arg-types) (length parameter-types))
                 (error "Argument count mismatch"))
               (let ((arg-values (mapcar #'(lambda (arg-expr arg-value arg-type parameter-type)
                                             (widening-coercion-code world parameter-type arg-type arg-value arg-expr))
                                         arg-exprs arg-values arg-types parameter-types)))
                 (values (apply #'gen-apply function-value arg-values)
                         (->-result-type function-type)
                         (list* 'expr-annotation:call function-annotated-expr arg-annotated-exprs))))))))
     
     ;Scan an interned identifier
     (scan-identifier (symbol)
       (let ((symbol-binding (type-env-get-local type-env symbol)))
         (if symbol-binding
           (ecase (type-env-local-mode symbol-binding)
             ((:const :var)
              (values (type-env-local-name symbol-binding)
                      (type-env-local-type symbol-binding)
                      (list 'expr-annotation:local symbol)))
             (:uninitialized
              (if (type-env-initialized type-env symbol)
                (values (type-env-local-name symbol-binding)
                        (type-env-local-type symbol-binding)
                        (list 'expr-annotation:local symbol))
                (error "Uninitialized variable ~A referenced" symbol)))
             (:function
               (values (list 'function (type-env-local-name symbol-binding))
                       (type-env-local-type symbol-binding)
                       (list 'expr-annotation:local symbol)))
             ((:reserved :unused) (error "Unused variable ~A referenced" symbol)))
           (let ((primitive (symbol-primitive symbol)))
             (if primitive
               (values (primitive-value-code primitive) (primitive-type primitive) (list 'expr-annotation:primitive symbol))
               (let ((tag (symbol-tag symbol)))
                 (if (and tag (tag-keyword tag))
                   (values (tag-keyword tag)
                           (make-tag-type world tag)
                           (list 'expr-annotation:tag tag))
                   (let ((type (symbol-type symbol)))
                     (if type
                       (values (if (eq (type-kind type) :->)
                                 (list 'symbol-function (list 'quote symbol))
                                 (list 'fetch-value symbol))
                               type
                               (list 'expr-annotation:global symbol))
                       (syntax-error))))))))))
     
     ;Scan a call or special form
     (scan-cons (first rest)
       (if (identifier? first)
         (let ((symbol (world-intern world first)))
           (let ((handler (get symbol :special-form)))
             (if handler
               (apply handler world type-env symbol rest)
               (if (and (symbol-action symbol)
                        (let ((local (type-env-get-local type-env symbol)))
                          (not (and local (eq (type-kind (type-env-local-type local)) :->)))))
                 (multiple-value-bind (action-value action-type action-annotated-expr) (apply #'scan-action-call type-env symbol rest)
                   (if (eq (type-kind action-type) :writable-cell)
                     (progn
                       (assert-true (symbolp action-value))
                       (values
                        `(if (car ,action-value)
                           (cdr ,action-value)
                           (error "Uninitialized writable-cell"))
                        (writable-cell-element-type action-type)
                        action-annotated-expr))
                     (values action-value action-type action-annotated-expr)))
                 (multiple-value-call #'scan-call (scan-identifier symbol) rest)))))
         (multiple-value-call #'scan-call (scan-value world type-env first) rest)))
     
     (scan-constant (value-expr type)
       (values value-expr type (list 'expr-annotation:constant value-expr))))
    
    (assert-three-values
     (cond
      ((consp value-expr) (scan-cons (first value-expr) (rest value-expr)))
      ((identifier? value-expr) (scan-identifier (world-intern world value-expr)))
      ((integerp value-expr) (scan-constant value-expr (world-integer-type world)))
      ((typep value-expr *float64-type*)
       (if (zerop value-expr)
         (error "Use +zero64 or -zero64 instead of 0.0")
         (scan-constant value-expr (world-finite64-type world))))
      ((characterp value-expr) (scan-constant value-expr (world-char16-type world)))
      ((stringp value-expr) (scan-constant value-expr (world-string-type world)))
      (t (syntax-error))))))


; Same as scan-value except that ensure that the value has the expected type.
; Return two values:
;   The expression's value (a lisp expression)
;   The annotated value-expr
(defun scan-typed-value (world type-env value-expr expected-type)
  (multiple-value-bind (value type annotated-expr) (scan-value world type-env value-expr)
    (values (widening-coercion-code world expected-type type value value-expr) annotated-expr)))


(defun eval-typed-value (world value-expr expected-type)
  (eval (scan-typed-value world *null-type-env* value-expr expected-type)))


; Same as scan-value except that ensure that the value has type bottom or void.
; Return three values:
;   The expression's value (a lisp expression)
;   True if value has type void
;   The annotated value-expr
(defun scan-void-value (world type-env value-expr)
  (multiple-value-bind (value type annotated-expr) (scan-value world type-env value-expr)
    (values
     value
     (case (type-kind type)
       (:bottom nil)
       (:void t)
       (t (error "Value ~S:~A should be void" value-expr (print-type-to-string type))))
     annotated-expr)))


; Same as scan-value except that ensure that the value is a vector type.
; Return three values:
;   The expression's value (a lisp expression)
;   The expression's type
;   The annotated value-expr
(defun scan-vector-value (world type-env value-expr)
  (multiple-value-bind (value type annotated-expr) (scan-value world type-env value-expr)
    (unless (member (type-kind type) '(:string :vector))
      (error "Value ~S:~A should be a vector" value-expr (print-type-to-string type)))
    (values value type annotated-expr)))


; Same as scan-value except that ensure that the value is a set type.
; Return three values:
;   The expression's value (a lisp expression)
;   The expression's type
;   The annotated value-expr
(defun scan-set-value (world type-env value-expr)
  (multiple-value-bind (value type annotated-expr) (scan-value world type-env value-expr)
    (unless (member (type-kind type) '(:list-set :range-set :bit-set :restricted-set))
      (error "Value ~S:~A should be a set" value-expr (print-type-to-string type)))
    (values value type annotated-expr)))


; Same as scan-value except that ensure that the value is a vector or set type.
; Return three values:
;   The expression's value (a lisp expression)
;   The expression's type kind
;   The expression's element type
;   The annotated value-expr
(defun scan-collection-value (world type-env value-expr)
  (multiple-value-bind (value type annotated-expr) (scan-value world type-env value-expr)
    (let ((kind (type-kind type)))
      (unless (member kind '(:string :vector :list-set :range-set :bit-set :restricted-set))
        (error "Value ~S:~A should be a vector or a set" value-expr (print-type-to-string type)))
      (values value kind (collection-element-type type) annotated-expr))))


; Same as scan-value except that ensure that the value is a tag type, float32, float64, or a union of these types.
; The types float32 and float64 are converted into fake tags that have one field, named "value", at position -1.
; Return four values:
;   The expression's value (a lisp expression)
;   The expression's type
;   A list of tags in the expression's type (includes pseudo-tags with a value field at offset -1 for :finite32 and :finite64 if these types are present)
;   The annotated value-expr
(defun scan-union-tag-value (world type-env value-expr)
  (multiple-value-bind (value type annotated-expr) (scan-value world type-env value-expr)
    (flet ((bad-type ()
             (error "Value ~S:~A should be a tag or union of tags" value-expr (print-type-to-string type))))
      (values
       value
       type
       (case (type-kind type)
         ((:tag :finite32 :finite64) (list (type-pseudo-tag world type)))
         (:union (mapcar #'(lambda (type2)
                             (or (type-pseudo-tag world type2)
                                 (bad-type)))
                         (type-parameters type)))
         (t (bad-type)))
       annotated-expr))))


; Generate a lisp expression that will compute the boolean condition expression in condition-expr.
; type-env is the type environment.  The expression may refer to free variables present in the type-env.
; Return four values:
;   The code for the condition;
;   The annotated code for the condition;
;   A type-env to use if the condition is true;
;   A type-env to use if the condition is false.
(defun scan-condition (world type-env condition-expr)
  (when (consp condition-expr)
    (let ((first (first condition-expr)))
      (when (identifier? first)
        (let* ((symbol (world-intern world first))
               (handler (get symbol :condition)))
          (when handler
            (return-from scan-condition (assert-four-values (apply handler world type-env symbol (rest condition-expr)))))))))
  (multiple-value-bind (condition-code condition-annotated-expr)
                       (scan-typed-value world type-env condition-expr (world-boolean-type world))
    (values condition-code condition-annotated-expr type-env type-env)))


; Return the code for computing value-expr, which will be assigned to the symbol.  Check that the
; value has the given type.
(defun scan-global-value (symbol value-expr type)
  (scan-typed-value (symbol-world symbol) *null-type-env* value-expr type))


; Same as scan-typed-value except that also allow the form (begin . <statements>); in this case
; return can be used to return the expression's value.
; Return two values:
;   The expression's value (a lisp expression)
;   The annotated value-expr
(defun scan-typed-value-or-begin (world type-env value-expr expected-type)
  (if (and (consp value-expr) (eq (first value-expr) 'begin))
    (let* ((result-type (scan-type world expected-type))
           (local-type-env (type-env-init-function type-env result-type)))
      (multiple-value-bind (body-codes body-annotated-stmts) (finish-function-code world local-type-env result-type (cdr value-expr))
        (values (gen-progn body-codes)
                (cons 'expr-annotation:begin body-annotated-stmts))))
    (scan-typed-value world type-env value-expr expected-type)))



; Generate the defun code for the world's variable named by symbol.
; The variable's type must be ->.
(defun compute-variable-function (symbol value-expr type)
  (handler-bind (((or error warning)
                  #'(lambda (condition)
                      (declare (ignore condition))
                      (format *error-output* "~&~@<~2IWhile computing ~A: ~_~:W~:>~%" symbol value-expr))))
    (assert-true (not (or (boundp symbol) (fboundp symbol))))
    (let ((code (strip-function (scan-global-value symbol value-expr type) symbol (length (->-argument-types type))))
          (code2 (get symbol :lisp-value-expr)))
      (when code2
        (setq code code2))
      (when *trace-variables*
        (format *trace-output* "~&~S ::= ~:W~%" symbol code))
      (quiet-compile symbol code))))


(defvar *busy-variables* nil)


; Compute the value of a world's variable named by symbol.  Return the variable's value.
; If the variable already has a computed value, return it unchanged.  The variable's type must not be ->.
; If computing the value requires the values of other variables, compute them as well.
; Use the *busy-variables* list to prevent infinite recursion while computing variable values.
(defun compute-variable-value (symbol)
  (cond
   ((member symbol *busy-variables*) (error "Definition of ~A refers to itself" symbol))
   ((boundp symbol) (symbol-value symbol))
   ((fboundp symbol) (error "compute-variable-value should be called only once on a function"))
   (t (let* ((*busy-variables* (cons symbol *busy-variables*))
             (value-expr (get symbol :value-expr))
             (type (symbol-type symbol)))
        (when (get symbol :lisp-value-expr)
          (error "Can't use defprimitive on non-function ~S" symbol))
        (handler-bind (((or error warning)
                        #'(lambda (condition)
                            (declare (ignore condition))
                            (format *error-output* "~&~@<~2IWhile computing ~A: ~_~:W~:>~%"
                                    symbol value-expr))))
          (assert-true (not (eq (type-kind type) :->)))
          (let ((value-code (scan-global-value symbol value-expr type)))
            (when *trace-variables*
              (format *trace-output* "~&~S := ~:W~%" symbol value-code))
            (set symbol (eval value-code))))))))


;;; ------------------------------------------------------------------------------------------------------
;;; SPECIAL FORMS

;;; Constants

(defun eval-todo ()
  (error "Reached a TODO expression"))

; (todo)
; Raises an error.
(defun scan-todo (world type-env special-form)
  (declare (ignore type-env))
  (values
   '(eval-todo)
   (world-bottom-type world)
   (list 'expr-annotation:special-form special-form)))


; (bottom)
; Raises an error.  Same as todo except that it doesn't carry the connotation of something that
; should be filled in in the future.
(defun scan-bottom-expr (world type-env special-form)
  (declare (ignore type-env))
  (values
   '(eval-bottom)
   (world-bottom-type world)
   (list 'expr-annotation:special-form special-form)))


; (hex <integer> [<length>])
; Alternative way of writing the integer in hexadecimal.  length is the minimum number of digits to write.
(defun scan-hex (world type-env special-form n &optional (length 1))
  (declare (ignore type-env))
  (unless (and (integerp n) (integerp length) (>= length 0))
    (error "Bad hex constant ~S [~S]" n length))
  (values
   n
   (world-integer-type world)
   (list 'expr-annotation:special-form special-form n length)))


; (float32 <value>)
; Alternative way of writing a finite, nonzero float32 constant.
(defun scan-float32 (world type-env special-form value)
  (declare (ignore type-env special-form))
  (unless (typep value *float64-type*)
    (error "Bad float32 constant ~S" value))
  (let ((f32 (coerce value *float32-type*)))
    (when (zerop f32)
      (error "Use +zero32 or -zero32 instead of (float32 0.0)"))
    (values
     f32
     (world-finite32-type world)
     (list 'expr-annotation:constant f32))))


; (supplementary-char <integer>)
; <integer> must be between #x10000 and #x10FFFF.
(defun scan-supplementary-char (world type-env special-form code-point)
  (declare (ignore type-env))
  (unless (and (integerp code-point) (<= #x10000 code-point #x10FFFF))
    (error "Bad supplementary-char constant ~S" code-point))
  (values
   (list 'quote (cons :supplementary-char code-point))
   (world-supplementary-char-type world)
   (list 'expr-annotation:special-form special-form code-point)))


;;; Expressions


; (/*/ <value-expr> . <styled-text>)
; Evaluate <value-expr>, but depict <styled-text>.
(defun scan-/*/ (world type-env special-form value-expr &rest text)
  (multiple-value-bind (code type annotated-expr) (scan-value world type-env value-expr)
    (declare (ignore annotated-expr))
    (when (endp text)
      (error "/*/ needs a text comment"))
    (let ((text2 (scan-expressions-in-comment world type-env text)))
      (values
       code
       type
       (list* 'expr-annotation:special-form special-form text2)))))


; (/*/ <value-expr> . <styled-text>)
; Evaluate <value-expr>, but depict <styled-text>.
(defun scan-/*/-condition (world type-env special-form value-expr &rest text)
  (multiple-value-bind (code annotated-expr true-type-env false-type-env)
                       (scan-condition world type-env value-expr)
    (declare (ignore annotated-expr))
    (when (endp text)
      (error "/*/ needs a text comment"))
    (let ((text2 (scan-expressions-in-comment world type-env text)))
      (values
       code
       (list* 'expr-annotation:special-form special-form text2)
       true-type-env
       false-type-env))))


; (lisp-call <lisp-function> <arg-exprs> <result-type-expr> . <styled-text>)
; Evaluate <lisp-function> applied to the results of evaluating <arg-exprs>, but depict <styled-text>.
; <styled-text> can contain the entry (:operand <n>) to depict the nth operand, with n starting from 0.
(defun scan-lisp-call (world type-env special-form lisp-function arg-exprs result-type-expr &rest text)
  (let ((result-type (scan-type world result-type-expr))
        (arg-values nil)
        (arg-annotated-exprs nil))
    (dolist (arg-expr arg-exprs)
      (multiple-value-bind (arg-value arg-type arg-annotated-expr) (scan-value world type-env arg-expr)
        (declare (ignore arg-type))
        (push arg-value arg-values)
        (push arg-annotated-expr arg-annotated-exprs)))
    (let ((arg-values (nreverse arg-values))
          (arg-annotated-exprs (nreverse arg-annotated-exprs)))
      (let ((text2 (scan-expressions-in-comment world type-env text)))
        (values
         (cons lisp-function arg-values)
         result-type
         (list* 'expr-annotation:special-form special-form arg-annotated-exprs text2))))))


(defun semantic-expt (base exponent)
  (assert-true (and (rationalp base) (integerp exponent)))
  (when (and (zerop base) (not (plusp exponent)))
    (error "0 raised to a nonpositive exponent"))
  (expt base exponent))


; (expt <base> <exponent>)
; The result is rational unless both base and exponent are integer constants and the result is an integer.
(defun scan-expt (world type-env special-form base-expr exponent-expr)
  (multiple-value-bind (base-code base-annotated-expr) (scan-typed-value world type-env base-expr (world-rational-type world))
    (multiple-value-bind (exponent-code exponent-annotated-expr) (scan-typed-value world type-env exponent-expr (world-integer-type world))
      (let ((code (list 'semantic-expt base-code exponent-code))
            (type (world-rational-type world)))
        (when (and (constantp base-code) (constantp exponent-code))
          (setq code (semantic-expt base-code exponent-code))
          (when (integerp code)
            (setq type (world-integer-type world))))
        (values
         code
         type
         (list 'expr-annotation:special-form special-form base-annotated-expr exponent-annotated-expr))))))


; Return the depict name for one of the comparison symbols =, /=, <, >, <=, >=, set<=.
(defun comparison-name (order)
  (cdr (assoc order '((= . "=") (/= . :not-equal) (< . "<") (> . ">") (<= . :less-or-equal) (>= . :greater-or-equal) (set<= . :subset-eq-10)))))


; Both expr1 and expr2 are coerced to the given type and then compared using the given order.
; The result is a boolean.  order-name should be suitable for depict.
(defun scan-comparison (world type-env special-form order expr1 expr2 type-expr)
  (let ((type (scan-type world type-expr)))
    (multiple-value-bind (code1 annotated-expr1) (scan-typed-value world type-env expr1 type)
      (multiple-value-bind (code2 annotated-expr2) (scan-typed-value world type-env expr2 type)
        (values
         (get-type-order-code world type order code1 code2)
         (world-boolean-type world)
         (list 'expr-annotation:special-form special-form (comparison-name order) annotated-expr1 annotated-expr2))))))


; (= <expr1> <expr2> [<type>])
(defun scan-= (world type-env special-form expr1 expr2 &optional (type-expr 'integer))
  (scan-comparison world type-env special-form '= expr1 expr2 type-expr))

; (/= <expr1> <expr2> [<type>])
(defun scan-/= (world type-env special-form expr1 expr2 &optional (type-expr 'integer))
  (scan-comparison world type-env special-form '/= expr1 expr2 type-expr))

; (< <expr1> <expr2> [<type>])
(defun scan-< (world type-env special-form expr1 expr2 &optional (type-expr 'integer))
  (scan-comparison world type-env special-form '< expr1 expr2 type-expr))

; (> <expr1> <expr2> [<type>])
(defun scan-> (world type-env special-form expr1 expr2 &optional (type-expr 'integer))
  (scan-comparison world type-env special-form '> expr1 expr2 type-expr))

; (<= <expr1> <expr2> [<type>])
(defun scan-<= (world type-env special-form expr1 expr2 &optional (type-expr 'integer))
  (scan-comparison world type-env special-form '<= expr1 expr2 type-expr))

; (>= <expr1> <expr2> [<type>])
(defun scan->= (world type-env special-form expr1 expr2 &optional (type-expr 'integer))
  (scan-comparison world type-env special-form '>= expr1 expr2 type-expr))

; (set<= <expr1> <expr2> <type>)
(defun scan-set<= (world type-env special-form expr1 expr2 type-expr)
  (scan-comparison world type-env special-form 'set<= expr1 expr2 type-expr))


; (cascade <type> <expr1> <order1> <expr2> <order2> ... <ordern-1> <exprn>)
; Shorthand for (and (<order1> <expr1> <expr2> <type>) (<order1> <expr2> <expr3> <type>) ... (<ordern-1> <exprn-1> <exprn> <type>)),
; where each order must be one of the symbols =, /=, <, >, <=, >=, set<=.
; The intermediate expressions are evaluated at most once.
(defun scan-cascade (world type-env special-form type-expr expr1 &rest orders-and-exprs)
  (let ((type (scan-type world type-expr)))
    (labels
      ((cascade (v1 orders-and-exprs)
         (unless (and (consp orders-and-exprs) (consp (cdr orders-and-exprs))
                      (member (first orders-and-exprs) '(= /= < > <= >= set<=)))
           (error "Bad cascade tail: ~S" orders-and-exprs))
         (let* ((order (first orders-and-exprs))
                (order-name (comparison-name order))
                (expr2 (second orders-and-exprs))
                (orders-and-exprs (cddr orders-and-exprs)))
           (multiple-value-bind (code2 annotated-expr2) (scan-typed-value world type-env expr2 type)
             (if orders-and-exprs
               (let ((v2 (gen-local-var code2)))
                 (multiple-value-bind (codes annotations) (cascade v2 orders-and-exprs)
                   (values
                    (let-local-var v2 code2
                      `(and ,(get-type-order-code world type order v1 v2) ,codes))
                    (list* order-name annotated-expr2 annotations))))
               (values
                (get-type-order-code world type order v1 code2)
                (list order-name annotated-expr2)))))))
      
      (multiple-value-bind (code1 annotated-expr1) (scan-typed-value world type-env expr1 type)
        (let ((v1 (gen-local-var code1)))
          (multiple-value-bind (codes annotations) (cascade v1 orders-and-exprs)
            (values
             (let-local-var v1 code1 codes)
             (world-boolean-type world)
             (list* 'expr-annotation:special-form special-form annotated-expr1 annotations))))))))


; (and <expr> ... <expr>)
; Short-circuiting logical AND.
(defun scan-and (world type-env special-form expr &rest exprs)
  (multiple-value-bind (code annotated-expr true-type-env false-type-env)
                       (apply #'scan-and-condition world type-env special-form expr exprs)
    (declare (ignore true-type-env false-type-env))
    (values
     code
     (world-boolean-type world)
     annotated-expr)))

; (or <expr> ... <expr>)
; Short-circuiting logical OR.
(defun scan-or (world type-env special-form expr &rest exprs)
  (multiple-value-bind (code annotated-expr true-type-env false-type-env)
                       (apply #'scan-or-condition world type-env special-form expr exprs)
    (declare (ignore true-type-env false-type-env))
    (values
     code
     (world-boolean-type world)
     annotated-expr)))

; (xor <expr> ... <expr>)
; Logical XOR.
(defun scan-xor (world type-env special-form expr &rest exprs)
  (multiple-value-map-bind (codes annotated-exprs)
                           #'(lambda (expr)
                               (scan-typed-value world type-env expr (world-boolean-type world)))
                           ((cons expr exprs))
    (values
     (gen-poly-op 'xor nil codes)
     (world-boolean-type world)
     (list* 'expr-annotation:special-form special-form 'xor annotated-exprs))))


; (not <expr>)
(defun scan-not-condition (world type-env special-form expr)
  (multiple-value-bind (expr-code expr-annotated-expr expr-true-type-env expr-false-type-env)
                       (scan-condition world type-env expr)
    (values
     (list 'not expr-code)
     (list 'expr-annotation:call (list 'expr-annotation:primitive special-form) expr-annotated-expr)
     expr-false-type-env
     expr-true-type-env)))


; (and <expr> ... <expr>)
; Short-circuiting logical AND.
(defun scan-and-condition (world type-env special-form expr &rest exprs)
  (multiple-value-bind (code1 annotated-expr1 true-type-env false-type-env)
                       (scan-condition world type-env expr)
    (let ((codes (list code1))
          (annotated-exprs (list annotated-expr1)))
      (dolist (expr2 exprs)
        (multiple-value-bind (code2 annotated-expr2 true-type-env2 false-type-env2)
                             (scan-condition world true-type-env expr2)
          (push code2 codes)
          (push annotated-expr2 annotated-exprs)
          (setq true-type-env true-type-env2)
          (ensure-narrowed-type-env false-type-env false-type-env2)))
      (values
       (gen-poly-op 'and t (nreverse codes))
       (list* 'expr-annotation:special-form special-form 'and (nreverse annotated-exprs))
       true-type-env
       false-type-env))))


; (or <expr> ... <expr>)
; Short-circuiting logical OR.
(defun scan-or-condition (world type-env special-form expr &rest exprs)
  (multiple-value-bind (code1 annotated-expr1 true-type-env false-type-env)
                       (scan-condition world type-env expr)
    (let ((codes (list code1))
          (annotated-exprs (list annotated-expr1)))
      (dolist (expr2 exprs)
        (multiple-value-bind (code2 annotated-expr2 true-type-env2 false-type-env2)
                             (scan-condition world false-type-env expr2)
          (push code2 codes)
          (push annotated-expr2 annotated-exprs)
          (setq false-type-env false-type-env2)
          (ensure-narrowed-type-env true-type-env true-type-env2)))
      (values
       (gen-poly-op 'or nil (nreverse codes))
       (list* 'expr-annotation:special-form special-form 'or (nreverse annotated-exprs))
       true-type-env
       false-type-env))))


; (begin . <statements>)
; Only allowed at the top level of an action.


(defun finish-function-code (world type-env result-type body-statements)
  (multiple-value-bind (body-codes body-live body-annotated-stmts) (scan-statements world type-env body-statements t)
    (assert-true (or (listp body-live) (eq body-live :dead)))
    (when (and (listp body-live) (not (or (type= result-type (world-void-type world))
                                          (type= result-type (world-bottom-type world)))))
      (error "Execution falls off the end of a function with result type ~A" (print-type-to-string result-type)))
    (let ((return-block-name (get-type-env-flag type-env :return-block-name)))
      (values
       (if return-block-name
         (list (list* 'block return-block-name body-codes))
         body-codes)
       body-annotated-stmts))))


; Scan a local function.
;   arg-binding-exprs should have the form ((<var1> <type1> [:var | :unused]) ... (<varn> <typen> [:var | :unused])).
;   result-type-expr should be a type expression.
;   body-statements contains the function's body statements.
; Return three values:
;   A list of lisp function bindings followed by the code (i.e. '((a b c) (declare (ignore c)) (* a b)));
;   The function's complete type;
;   The annotated body statements.
(defun scan-function-or-lambda (world type-env arg-binding-exprs result-type-expr body-statements)
  (handler-bind (((or error warning)
                  #'(lambda (condition)
                      (declare (ignore condition))
                      (format *error-output* "~&~@<~2IWhile processing lambda ~_~S ~_~S ~_~S:~:>~%"
                              arg-binding-exprs result-type-expr body-statements))))
    (let* ((result-type (scan-type world result-type-expr))
           (local-type-env (type-env-init-function type-env result-type))
           (args nil)
           (arg-types nil)
           (unused-args nil))
      (unless (listp arg-binding-exprs)
        (error "Bad function bindings ~S" arg-binding-exprs))
      (dolist (arg-binding-expr arg-binding-exprs)
        (unless (and (consp arg-binding-expr)
                     (consp (cdr arg-binding-expr))
                     (member (cddr arg-binding-expr) '(nil (:var) (:unused)) :test #'equal))
          (error "Bad function binding ~S" arg-binding-expr))
        (let ((arg-symbol (scan-name world (first arg-binding-expr)))
              (arg-type (scan-type world (second arg-binding-expr)))
              (arg-mode (or (third arg-binding-expr) :const)))
          (setq local-type-env (type-env-add-binding local-type-env arg-symbol arg-type arg-mode))
          (push arg-symbol args)
          (push arg-type arg-types)
          (when (eq arg-mode :unused)
            (push arg-symbol unused-args))))
      (setq args (nreverse args))
      (setq arg-types (nreverse arg-types))
      (setq unused-args (nreverse unused-args))
      (multiple-value-bind (body-codes body-annotated-stmts) (finish-function-code world local-type-env result-type body-statements)
        (when unused-args
          (push (list 'declare (cons 'ignore unused-args)) body-codes))
        (values (cons args body-codes)
                (make-->-type world arg-types result-type)
                body-annotated-stmts)))))


; (lambda ((<var1> <type1> [:var | :unused]) ... (<varn> <typen> [:var | :unused])) <result-type> . <statements>)
(defun scan-lambda (world type-env special-form arg-binding-exprs result-type-expr &rest body-statements)
  (multiple-value-bind (args-and-body-codes type body-annotated-stmts)
                       (scan-function-or-lambda world type-env arg-binding-exprs result-type-expr body-statements)
    (values
     (list 'function (cons 'lambda args-and-body-codes))
     type
     (list* 'expr-annotation:special-form special-form arg-binding-exprs result-type-expr body-annotated-stmts))))


; (coerce-parameters (<type1> ... <typen>) <function-expr>)
; Coerces the function <function-expr> to a function with the same number of parameters but with types
; <type1> through <typen>, which may be more general than <function-expr>'s parameter types.  A dynamic check
; ensures that the run-time values belong to <function-expr>'s parameter types.
;*****


; (if <condition-expr> <true-expr> <false-expr>)
(defun scan-if-expr (world type-env special-form condition-expr true-expr false-expr)
  (multiple-value-bind (condition-code condition-annotated-expr true-type-env false-type-env)
                       (scan-condition world type-env condition-expr)
    (multiple-value-bind (true-code true-type true-annotated-expr) (scan-value world true-type-env true-expr)
      (multiple-value-bind (false-code false-type false-annotated-expr) (scan-value world false-type-env false-expr)
        (handler-bind (((or error warning)
                        #'(lambda (condition)
                            (declare (ignore condition))
                            (format *error-output* "~&~@<~2IWhile processing if with alternatives~_ ~S: ~A and~_ ~S: ~A:~:>~%"
                                    true-expr (print-type-to-string true-type)
                                    false-expr (print-type-to-string false-type)))))
          (let ((type (type-union world true-type false-type)))
            (values
             (list 'if condition-code
                   (widening-coercion-code world type true-type true-code condition-expr)
                   (widening-coercion-code world type false-type false-code condition-expr))
             type
             (list 'expr-annotation:special-form special-form condition-annotated-expr true-annotated-expr false-annotated-expr))))))))


;;; Vectors

(defmacro non-empty-vector (v operation-name)
  `(or ,v (error ,(concatenate 'string operation-name " called on empty vector"))))

(defun make-vector-expr (world special-form element-type element-codes element-annotated-exprs)
  (values
   (if element-codes
     (let ((elements-code (cons 'list element-codes)))
       (if (eq element-type (world-char16-type world))
         (if (cdr element-codes)
           (list 'coerce elements-code ''string)
           (list 'string (car element-codes)))
         elements-code))
     (if (eq element-type (world-char16-type world))
       ""
       nil))
   (make-vector-type world element-type)
   (list* 'expr-annotation:special-form special-form element-annotated-exprs)))

; (vector <element-expr> ... <element-expr>)
; Makes a vector of one or more elements.
(defun scan-vector-expr (world type-env special-form element-expr &rest element-exprs)
  (multiple-value-bind (element-code element-type element-annotated-expr) (scan-value world type-env element-expr)
    (multiple-value-map-bind (rest-codes rest-annotated-exprs)
                             #'(lambda (element-expr)
                                 (scan-typed-value world type-env element-expr element-type))
                             (element-exprs)
      (make-vector-expr world special-form element-type (cons element-code rest-codes) (cons element-annotated-expr rest-annotated-exprs)))))


; (vector-of <element-type> <element-expr> ... <element-expr>)
; Makes a vector of zero or more elements of the given type.
(defun scan-vector-of (world type-env special-form element-type-expr &rest element-exprs)
  (let ((element-type (scan-type world element-type-expr)))
    (multiple-value-map-bind (element-codes element-annotated-exprs)
                             #'(lambda (element-expr)
                                 (scan-typed-value world type-env element-expr element-type))
                             (element-exprs)
      (make-vector-expr world special-form element-type element-codes element-annotated-exprs))))


; (repeat <element-type> <element-expr> <count-expr>)
; Makes a vector of count-expr copies of element-expr coerced to the given type.
(defun scan-repeat (world type-env special-form element-type-expr element-expr count-expr)
  (let ((element-type (scan-type world element-type-expr)))
    (multiple-value-bind (element-code element-annotated-expr) (scan-typed-value world type-env element-expr element-type)
      (multiple-value-bind (count-code count-annotated-expr) (scan-typed-value world type-env count-expr (world-integer-type world))
        (let ((vector-type (make-vector-type world element-type)))
          (values
           (if (eq vector-type (world-string-type world))
             `(make-string ,count-code :initial-element ,element-code)
             `(make-list ,count-code :initial-element ,element-code))
           vector-type
           (list 'expr-annotation:special-form special-form element-annotated-expr count-annotated-expr)))))))


; Same as nth, except that ensures that the element is actually present.
(defun checked-nth (list n)
  (car (non-empty-vector (nthcdr n list) "nth")))

; (nth <vector-expr> <n-expr>)
; Returns the nth element of the vector.  Throws an error if the vector's length is less than n.
(defun scan-nth (world type-env special-form vector-expr n-expr)
  (multiple-value-bind (vector-code vector-type vector-annotated-expr) (scan-vector-value world type-env vector-expr)
    (multiple-value-bind (n-code n-annotated-expr) (scan-typed-value world type-env n-expr (world-integer-type world))
      (values
       (cond
        ((eq vector-type (world-string-type world))
         `(char ,vector-code ,n-code))
        ((eql n-code 0)
         `(car (non-empty-vector ,vector-code "first")))
        (t `(checked-nth ,vector-code ,n-code)))
       (vector-element-type vector-type)
       (list 'expr-annotation:special-form special-form vector-annotated-expr n-annotated-expr)))))


; (subseq <vector-expr> <low-expr> [<high-expr>])
; Returns a vector containing elements of the given vector from low-expr to high-expr inclusive.
; high-expr defaults to length-1.
; It is required that 0 <= low-expr <= high-expr+1 <= length.
(defun scan-subseq (world type-env special-form vector-expr low-expr &optional high-expr)
  (let ((integer-type (world-integer-type world)))
    (multiple-value-bind (vector-code vector-type vector-annotated-expr) (scan-vector-value world type-env vector-expr)
      (multiple-value-bind (low-code low-annotated-expr) (scan-typed-value world type-env low-expr integer-type)
        (if high-expr
          (multiple-value-bind (high-code high-annotated-expr) (scan-typed-value world type-env high-expr integer-type)
            (values
             `(subseq ,vector-code ,low-code (1+ ,high-code))
             vector-type
             (list 'expr-annotation:special-form special-form vector-annotated-expr low-annotated-expr high-annotated-expr)))
          (values
           (case low-code
             (0 vector-code)
             (1 (if (eq vector-type (world-string-type world))
                  `(subseq ,vector-code 1)
                  `(cdr (non-empty-vector ,vector-code "rest"))))
             (t `(subseq ,vector-code ,low-code)))
           vector-type
           (list 'expr-annotation:special-form special-form vector-annotated-expr low-annotated-expr nil)))))))


; (cons <value-expr> <vector-expr>)
; Returns a vector consisting of <value-expr> followed by all values in <vector-expr>.
(defun scan-cons (world type-env special-form value-expr vector-expr)
  (multiple-value-bind (vector-code vector-type vector-annotated-expr) (scan-vector-value world type-env vector-expr)
    (multiple-value-bind (value-code value-annotated-expr) (scan-typed-value world type-env value-expr (vector-element-type vector-type))
      (values
       (if (eq vector-type (world-string-type world))
         `(concatenate 'string (list ,value-code) ,vector-code)
         (list 'cons value-code vector-code))
       vector-type
       (list 'expr-annotation:special-form special-form value-annotated-expr vector-annotated-expr)))))


; (append <vector-expr> <vector-expr> ... <vector-expr>)
; Returns a vector contatenating the given vectors, which must have the same element type.
(defun scan-append (world type-env special-form vector1-expr &rest vector-exprs)
  (unless vector-exprs
    (error "append requires at least two lists"))
  (multiple-value-bind (vector1-code vector-type vector1-annotated-expr) (scan-vector-value world type-env vector1-expr)
    (multiple-value-map-bind (vector-codes vector-annotated-exprs)
                             #'(lambda (vector-expr) (scan-typed-value world type-env vector-expr vector-type))
                             (vector-exprs)
      (values
       (if (eq vector-type (world-string-type world))
         `(concatenate 'string ,vector1-code ,@vector-codes)
         (list* 'append vector1-code vector-codes))
       vector-type
       (list* 'expr-annotation:special-form special-form vector1-annotated-expr vector-annotated-exprs)))))


; (set-nth <vector-expr> <n-expr> <value-expr>)
; Returns a vector containing the same elements of the given vector except that the nth has been replaced
; with value-expr.  n must be between 0 and length-1, inclusive.
(defun scan-set-nth (world type-env special-form vector-expr n-expr value-expr)
  (multiple-value-bind (vector-code vector-type vector-annotated-expr) (scan-vector-value world type-env vector-expr)
    (multiple-value-bind (n-code n-annotated-expr) (scan-typed-value world type-env n-expr (world-integer-type world))
      (multiple-value-bind (value-code value-annotated-expr) (scan-typed-value world type-env value-expr (vector-element-type vector-type))
        (values
         (let ((vector (gensym "V"))
               (n (gensym "N")))
           `(let ((,vector ,vector-code)
                  (,n ,n-code))
              (if (or (< ,n 0) (>= ,n (length ,vector)))
                (error "Range error")
                ,(if (eq vector-type (world-string-type world))
                   `(progn
                      (setq ,vector (copy-seq ,vector))
                      (setf (char ,vector ,n) ,value-code)
                      ,vector)
                   (let ((l (gensym "L")))
                     `(let ((,l (nthcdr ,n ,vector)))
                        (append (ldiff ,vector ,l)
                                (cons ,value-code (cdr ,l)))))))))
         vector-type
         (list 'expr-annotation:special-form special-form vector-annotated-expr n-annotated-expr value-annotated-expr))))))


;;; Sets

(defun make-list-set-expr (world special-form element-type element-codes element-annotated-exprs)
  (values
   (cond
    ((endp element-codes) nil)
    ((endp (cdr element-codes)) (cons 'list element-codes))
    ((every #'(lambda (element) (or (numberp element) (stringp element))) element-codes)
     (list 'quote (remove-duplicates element-codes :test #'equal)))
    (t `(delete-duplicates (list ,@element-codes) ,@(element-test world element-type))))
   (make-list-set-type world element-type)
   (list* 'expr-annotation:special-form special-form element-annotated-exprs)))

; (list-set <element-expr> ... <element-expr>)
; (%list-set <element-expr> ... <element-expr>)
; Makes a set of one or more elements.
(defun scan-list-set-expr (world type-env special-form element-expr &rest element-exprs)
  (multiple-value-bind (element-code element-type element-annotated-expr) (scan-value world type-env element-expr)
    (multiple-value-map-bind (rest-codes rest-annotated-exprs)
                             #'(lambda (element-expr)
                                 (scan-typed-value world type-env element-expr element-type))
                             (element-exprs)
      (make-list-set-expr world special-form element-type (cons element-code rest-codes) (cons element-annotated-expr rest-annotated-exprs)))))

; (list-set-of <element-type> <element-expr> ... <element-expr>)
; (%list-set-of <element-type> <element-expr> ... <element-expr>)
; Makes a set of zero or more elements of the given type.
(defun scan-list-set-of (world type-env special-form element-type-expr &rest element-exprs)
  (let ((element-type (scan-type world element-type-expr)))
    (multiple-value-map-bind (element-codes element-annotated-exprs)
                             #'(lambda (element-expr)
                                 (scan-typed-value world type-env element-expr element-type))
                             (element-exprs)
      (make-list-set-expr world special-form element-type element-codes element-annotated-exprs))))


; expr is the source code of an expression that generates a value of the given element-type.  Return
; the source code of an expression that generates the corresponding integer for storage in a range-set of
; the given element-type.
(defun range-set-encode-expr (element-type expr)
  (let ((encode (type-range-set-encode element-type)))
    (cond
     ((null encode) (error "Values of type ~S cannot be stored in range-sets" element-type))
     ((eq encode 'identity) expr)
     (t (list encode expr)))))


; expr is the source code of an expression that generates an integer.  Return the source code that undoes
; the transformation done by range-set-encode-expr.
(defun range-set-decode-expr (element-type expr)
  (let ((decode (type-range-set-decode element-type)))
    (cond
     ((null decode) (error "Values of type ~S cannot be stored in range-sets" element-type))
     ((eq decode 'identity) expr)
     (t (list decode expr)))))


; Return a function that converts integers to values of the given element-type for retrieval from a range-set.
(defun range-set-decode-function (element-type)
  (symbol-function (type-range-set-decode element-type)))


; (range-set-of <element-type> <element-expr> ... <element-expr>)  ==>
; (range-set-of-ranges <element-type> <element-expr> nil ... <element-expr> nil)
(defun scan-range-set-of (world type-env special-form element-type-expr &rest element-exprs)
  (apply #'scan-range-set-of-ranges
    world type-env special-form element-type-expr
    (mapcan #'(lambda (element-expr)
                (list element-expr nil))
            element-exprs)))


; (range-set-of-ranges <element-type> <low-expr> <high-expr> ... <low-expr> <high-expr>)
; Makes a set of zero or more elements or element ranges.  Each <high-expr> can be null to indicate a
; one-element range.
(defun scan-range-set-of-ranges (world type-env special-form element-type-expr &rest element-exprs)
  (let* ((element-type (scan-type world element-type-expr))
         (high t))
    (multiple-value-map-bind (element-codes element-annotated-exprs)
                             #'(lambda (element-expr)
                                 (setq high (not high))
                                 (if (and high (null element-expr))
                                   (values nil nil)
                                   (multiple-value-bind (element-code element-annotated-expr)
                                                        (scan-typed-value world type-env element-expr element-type)
                                     (values (range-set-encode-expr element-type element-code)
                                             element-annotated-expr))))
                             (element-exprs)
      (unless high
        (error "Odd number of range-set-of-ranges elements: ~S" element-exprs))
      (values
       (cons 'intset-from-ranges element-codes)
       (make-range-set-type world element-type)
       (list* 'expr-annotation:special-form special-form element-annotated-exprs)))))


; (set* <set-expr> <set-expr>)
; Returns the intersection of the two sets, which must have the same kind.
(defun scan-set* (world type-env special-form set1-expr set2-expr)
  (multiple-value-bind (set1-code set-type set1-annotated-expr) (scan-set-value world type-env set1-expr)
    (multiple-value-bind (set2-code set2-annotated-expr) (scan-typed-value world type-env set2-expr set-type)
      (values
       (ecase (type-kind set-type)
         (:list-set (list* 'intersection set1-code set2-code (element-test world (set-element-type set-type))))
         (:range-set (list 'intset-intersection set1-code set2-code)))
       set-type
       (list 'expr-annotation:special-form special-form set1-annotated-expr set2-annotated-expr)))))


; (set+ <set-expr> <set-expr>)
; Returns the union of the two sets, which must have the same kind.
(defun scan-set+ (world type-env special-form set1-expr set2-expr)
  (multiple-value-bind (set1-code set1-type set1-annotated-expr) (scan-set-value world type-env set1-expr)
    (multiple-value-bind (set2-code set2-type set2-annotated-expr) (scan-set-value world type-env set2-expr)
      (let* ((set-type (type-union world set1-type set2-type))
             (set1-coerced-code (widening-coercion-code world set-type set1-type set1-code set1-expr))
             (set2-coerced-code (widening-coercion-code world set-type set2-type set2-code set2-expr)))
        (values
         (ecase (type-kind set-type)
           (:list-set (list* 'union set1-coerced-code set2-coerced-code (element-test world (set-element-type set-type))))
           (:range-set (list 'intset-union set1-coerced-code set2-coerced-code)))
         set-type
         (list 'expr-annotation:special-form special-form set1-annotated-expr set2-annotated-expr))))))


; (set- <set-expr> <set-expr>)
; Returns the difference of the two sets, which must have the same kind.
(defun scan-set- (world type-env special-form set1-expr set2-expr)
  (multiple-value-bind (set1-code set-type set1-annotated-expr) (scan-set-value world type-env set1-expr)
    (multiple-value-bind (set2-code set2-annotated-expr) (scan-typed-value world type-env set2-expr set-type)
      (values
       (ecase (type-kind set-type)
         (:list-set (list* 'set-difference set1-code set2-code (element-test world (set-element-type set-type))))
         (:range-set (list 'intset-difference set1-code set2-code)))
       set-type
       (list 'expr-annotation:special-form special-form set1-annotated-expr set2-annotated-expr)))))


(defun bit-set-index-code (type elt-code)
  (let ((keywords (set-type-keywords type)))
    (if (keywordp elt-code)
      (position elt-code keywords)
      (list 'position elt-code (list 'quote keywords)))))


; (set-in <elt-expr> <set-expr>)
; Returns true if <elt-expr> is a member of the set <set-expr>.
(defun scan-set-in (world type-env special-form elt-expr set-expr)
  (multiple-value-bind (set-code set-type set-annotated-expr) (scan-set-value world type-env set-expr)
    (let ((elt-type (set-element-type set-type)))
      (multiple-value-bind (elt-code elt-annotated-expr) (scan-typed-value world type-env elt-expr elt-type)
        (values
         (ecase (type-kind set-type)
           (:list-set (list* 'member elt-code set-code (element-test world elt-type)))
           (:range-set (list 'intset-member? (range-set-encode-expr elt-type elt-code) set-code))
           ((:bit-set :restricted-set) (list 'logbitp (bit-set-index-code set-type elt-code) set-code)))
         (world-boolean-type world)
         (list 'expr-annotation:special-form special-form :member-10 elt-annotated-expr set-annotated-expr))))))


; (set-not-in <elt-expr> <set-expr>)
; Returns true if <elt-expr> is not a member of the set <set-expr>.
(defun scan-set-not-in (world type-env special-form elt-expr set-expr)
  (multiple-value-bind (set-code set-type set-annotated-expr) (scan-set-value world type-env set-expr)
    (let ((elt-type (set-element-type set-type)))
      (multiple-value-bind (elt-code elt-annotated-expr) (scan-typed-value world type-env elt-expr elt-type)
        (values
         (ecase (type-kind set-type)
           (:list-set (list 'not (list* 'member elt-code set-code (element-test world elt-type))))
           (:range-set (list 'not (list 'intset-member? (range-set-encode-expr elt-type elt-code) set-code)))
           ((:bit-set :restricted-set) (list 'not (list 'logbitp (bit-set-index-code set-type elt-code) set-code))))
         (world-boolean-type world)
         (list 'expr-annotation:special-form special-form :not-member-10 elt-annotated-expr set-annotated-expr))))))


(defun elt-of (set)
  (if set
    (car set)
    (error "elt-of called on empty set")))

(defun range-set-elt-of (set)
  (or (intset-min set)
      (error "elt-of called on empty set")))

(defun bit-set-elt-of (set keywords)
  (dolist (keyword keywords)
    (when (oddp set)
      (return-from bit-set-elt-of keyword))
    (setq set (ash set -1)))
  (error "elt-of called on empty set"))

; (elt-of <elt-expr>)
; Returns any element of <set-expr>, which must be a nonempty set.
(defun scan-elt-of (world type-env special-form set-expr)
  (multiple-value-bind (set-code set-type set-annotated-expr) (scan-set-value world type-env set-expr)
    (let ((elt-type (set-element-type set-type)))
      (values
       (ecase (type-kind set-type)
         (:list-set (list 'elt-of set-code))
         (:range-set (range-set-decode-expr elt-type (list 'range-set-elt-of set-code)))
         ((:bit-set :restricted-set) (list 'bit-set-elt-of set-code (list 'quote (set-type-keywords set-type)))))
       elt-type
       (list 'expr-annotation:special-form special-form set-annotated-expr)))))


(defun unique-elt-of (set)
  (if (and set (endp (cdr set)))
    (car set)
    (error "unique-elt-of called on a set with other than one element")))

(defun range-set-unique-elt-of (set)
  (unless (= (intset-length set) 1)
    (error "unique-elt-of called on a set with other than one element"))
  (intset-min set))

(defun bit-set-unique-elt-of (set keywords)
  (unless (= (logcount set) 1)
    (error "unique-elt-of called on a set with other than one element"))
  (assert-non-null (nth (integer-length set) keywords)))

; (unique-elt-of <elt-expr> [<var> <condition-expr>])
; Returns the one element of <set-expr>, which must have exactly one element.  If <var> and <condition-expr> are given,
; then return the one element of <set-expr> that satisfies <condition-expr>; there must be exactly one such element.
; <var> may shadow an existing local variable.
(defun scan-unique-elt-of (world type-env special-form set-expr &optional var-source condition-expr)
  (multiple-value-bind (set-code set-type set-annotated-expr) (scan-set-value world type-env set-expr)
    (let ((elt-type (set-element-type set-type)))
      (if var-source
        (let* ((var (scan-name world var-source))
               (local-type-env (type-env-add-binding type-env var elt-type :const t)))
          (multiple-value-bind (condition-code condition-annotated-expr) (scan-typed-value world local-type-env condition-expr (world-boolean-type world))
            (unless (eq (type-kind set-type) :list-set)
              (error "Not implemented"))
            (values
             `(unique-elt-of (remove-if-not #'(lambda (,var) ,condition-code) ,set-code))
             elt-type
             (list 'expr-annotation:special-form special-form set-annotated-expr var condition-annotated-expr))))
        (values
         (ecase (type-kind set-type)
           (:list-set (list 'unique-elt-of set-code))
           (:range-set (range-set-decode-expr elt-type (list 'range-set-unique-elt-of set-code)))
           ((:bit-set :restricted-set) (list 'bit-set-unique-elt-of set-code (list 'quote (set-type-keywords set-type)))))
         elt-type
         (list 'expr-annotation:special-form special-form set-annotated-expr))))))


;;; Vectors or Sets

; (empty <vector-or-set-expr>)
; Returns true if the vector or set has zero elements.
; This is equivalent to (= (length <vector-or-set-expr>) 0) but is implemented more efficiently.
(defun scan-empty (world type-env special-form collection-expr)
  (multiple-value-bind (collection-code collection-kind element-type collection-annotated-expr) (scan-collection-value world type-env collection-expr)
    (declare (ignore element-type))
    (values
     (ecase collection-kind
       (:string `(zerop (length ,collection-code)))
       ((:vector :list-set) (list 'endp collection-code))
       (:range-set (list 'intset-empty collection-code))
       ((:bit-set :restricted-set) (list '= collection-code 0)))
     (world-boolean-type world)
     (list 'expr-annotation:special-form special-form collection-kind collection-annotated-expr))))


; (nonempty <vector-or-set-expr>)
; Returns true if the vector or set does not have zero elements.
; This is equivalent to (/= (length <vector-or-set-expr>) 0) but is implemented more efficiently.
(defun scan-nonempty (world type-env special-form collection-expr)
  (multiple-value-bind (collection-code collection-kind element-type collection-annotated-expr) (scan-collection-value world type-env collection-expr)
    (declare (ignore element-type))
    (values
     (ecase collection-kind
       (:string `(/= (length ,collection-code) 0))
       ((:vector :list-set) collection-code)
       (:range-set `(not (intset-empty ,collection-code)))
       ((:bit-set :restricted-set) (list '/= collection-code 0)))
     (world-boolean-type world)
     (list 'expr-annotation:special-form special-form collection-kind collection-annotated-expr))))


; (length <vector-or-set-expr>)
; Returns the number of elements in the vector or set.
(defun scan-length (world type-env special-form collection-expr)
  (multiple-value-bind (collection-code collection-kind element-type collection-annotated-expr) (scan-collection-value world type-env collection-expr)
    (declare (ignore element-type))
    (values
     (ecase collection-kind
       ((:string :vector :list-set) (list 'length collection-code))
       (:range-set (list 'intset-length collection-code))
       ((:bit-set :restricted-set) (list 'logcount collection-code)))
     (world-integer-type world)
     (list 'expr-annotation:special-form special-form collection-annotated-expr))))


; (some <vector-or-set-expr> <var> <condition-expr>)
; Return true if there exists an element <var> of <vector-or-set-expr> on which <condition-expr> is true.
; Not implemented on range-sets.
(defun scan-some (world type-env special-form collection-expr var-source condition-expr)
  (multiple-value-bind (code annotated-expr true-type-env false-type-env)
                       (scan-some-condition world type-env special-form collection-expr var-source condition-expr)
    (declare (ignore true-type-env false-type-env))
    (values code (world-boolean-type world) annotated-expr)))


; (some <vector-or-set-expr> <var> <condition-expr> [:define-true])
; Return true if there exists an element <var> of <vector-or-set-expr> on which <condition-expr> is true.
; If :define-true is given, set <var> to be any such element (the first if in a vector) in the true branch; <var> must have been reserved.
; Not implemented on range-sets.
(defun scan-some-condition (world type-env special-form collection-expr var-source condition-expr &optional define-true)
  (unless (member define-true '(nil :define-true))
    (error "~S must be :define-true"))
  (multiple-value-bind (collection-code collection-kind element-type collection-annotated-expr) (scan-collection-value world type-env collection-expr)
    (unless (member collection-kind '(:vector :string :list-set))
      (error "Not implemented"))
    (let* ((var (scan-name world var-source))
           (local-type-env (if define-true
                             (type-env-unreserve-binding type-env var element-type)
                             (type-env-add-binding type-env var element-type :const))))
      (multiple-value-bind (condition-code condition-annotated-expr true-type-env false-type-env) (scan-condition world local-type-env condition-expr)
        (declare (ignore false-type-env))
        (let ((result-annotated-expr (list 'expr-annotation:special-form special-form 'some collection-annotated-expr var condition-annotated-expr))
              (coerced-collection-code (if (eq collection-kind :string) `(coerce ,collection-code 'list) collection-code)))
          (if define-true
            (values
             (let ((v (gensym "V")))
               `(dolist (,v ,coerced-collection-code)
                  (when (let ((,var ,v)) ,condition-code)
                    (setq ,var ,v)
                    (return t))))
             result-annotated-expr
             true-type-env
             type-env)
            (values
             `(some #'(lambda (,var) ,condition-code) ,coerced-collection-code)
             result-annotated-expr
             type-env
             type-env)))))))


; (every <vector-or-set-expr> <var> <condition-expr>)
; Return true if every element <var> in <vector-or-set-expr> satisfies <condition-expr>.
; Not implemented on range-sets.
(defun scan-every (world type-env special-form collection-expr var-source condition-expr)
  (multiple-value-bind (collection-code collection-kind element-type collection-annotated-expr) (scan-collection-value world type-env collection-expr)
    (unless (member collection-kind '(:vector :string :list-set))
      (error "Not implemented"))
    (let* ((var (scan-name world var-source))
           (local-type-env (type-env-add-binding type-env var element-type :const)))
      (multiple-value-bind (condition-code condition-annotated-expr) (scan-typed-value world local-type-env condition-expr (world-boolean-type world))
        (let ((coerced-collection-code (if (eq collection-kind :string) `(coerce ,collection-code 'list) collection-code)))
          (values
           `(every #'(lambda (,var) ,condition-code) ,coerced-collection-code)
           (world-boolean-type world)
           (list 'expr-annotation:special-form special-form 'every collection-annotated-expr var condition-annotated-expr)))))))


; (map <vector-or-set-expr> <var> <value-expr> [<condition-expr>])
; Return a vector or set of <value-expr> applied to all elements <var> of <vector-or-set-expr> on which <condition-expr> is true.
; The map produces a vector if given a vector or a list-set if given a list-set.
; Not implemented on range-sets.
(defun scan-map (world type-env special-form collection-expr var-source value-expr &optional (condition-expr 'true))
  (multiple-value-bind (collection-code collection-kind element-type collection-annotated-expr) (scan-collection-value world type-env collection-expr)
    (let* ((var (scan-name world var-source))
           (local-type-env (type-env-add-binding type-env var element-type :const)))
      (multiple-value-bind (condition-code condition-annotated-expr true-type-env false-type-env)
                           (scan-condition world local-type-env condition-expr)
        (declare (ignore false-type-env))
        (multiple-value-bind (value-code value-type value-annotated-expr) (scan-value world true-type-env value-expr)
          (let* ((source-is-vector (member collection-kind '(:string :vector)))
                 (source-is-string (eq collection-kind :string))
                 (destination-is-string (and source-is-vector (eq value-type (world-char16-type world))))
                 (result-type (case collection-kind
                                ((:string :vector) (make-vector-type world value-type))
                                (:list-set (make-list-set-type world value-type))
                                (t (error "Map not implemented on this kind of a set"))))
                 (destination-sequence-type (if destination-is-string 'string 'list))
                 (result-annotated-expr (list 'expr-annotation:special-form special-form collection-kind collection-annotated-expr var value-annotated-expr condition-annotated-expr)))
            (cond
             ((eq condition-code 't)
              (values
               (let ((mapcar-code `(mapcar #'(lambda (,var) ,value-code) ,collection-code)))
                 (cond
                  ((or source-is-string destination-is-string) `(map ',destination-sequence-type ,@(cdr mapcar-code)))
                  (source-is-vector mapcar-code)
                  (t (list* 'delete-duplicates mapcar-code (element-test world value-type)))))
               result-type
               (nbutlast result-annotated-expr)))
             ((eq value-expr var-source)
              (values
               `(remove-if-not #'(lambda (,var) ,condition-code) ,collection-code)
               result-type
               result-annotated-expr))
             (t 
              (values
               (let ((filter-map-list-code `(filter-map-list #'(lambda (,var) ,condition-code) #'(lambda (,var) ,value-code) ,collection-code)))
                 (cond
                  ((or source-is-string destination-is-string) `(filter-map ',destination-sequence-type ,@(cdr filter-map-list-code)))
                  (source-is-vector filter-map-list-code)
                  (t (list* 'delete-duplicates filter-map-list-code (element-test world value-type)))))
               result-type
               result-annotated-expr)))))))))


;;; Tuples and Records

(defparameter *record-counter* 0)

; (new <type> <field-expr1> ... <field-exprn>)
; Used to create both tuples and records.
; A <field-expr> should be one of the following:
;   an expression
;   :uninit to indicate an uninitialized field, which must have kind :opt-const or :opt-var
;   (:delay <global-var>) to indicate a field (which must have kind :opt-const or :opt-var) initialized the first time it's read to a global variable
(defun scan-new (world type-env special-form type-name &rest value-exprs)
  (let* ((type (scan-kinded-type world type-name :tag))
         (tag (type-tag type))
         (fields (tag-fields tag)))
    (unless (= (length value-exprs) (length fields))
      (error "Wrong number of ~A fields given in constructor: ~S" type-name value-exprs))
    (when (tag-keyword tag)
      (error "Don't use new to create tag ~A; refer to the tag directly instead" type-name))
    (multiple-value-map-bind (value-codes value-annotated-exprs)
                             #'(lambda (field value-expr)
                                 (cond
                                  ((eq value-expr :uninit)
                                   (if (field-optional field)
                                     (values :%uninit% value-expr)
                                     (error "Can't leave non-optional field ~S uninitialized" (field-label field))))
                                  ((field-optional field)
                                   (scan-typed-value world type-env value-expr (make-delay-type world (field-type field))))
                                  (t (scan-typed-value world type-env value-expr (field-type field)))))
                             (fields value-exprs)
      (values
       (let ((name (tag-name tag)))
         (if (tag-mutable tag)
           (list* 'list (list 'quote name) '(incf *record-counter*) value-codes)
           (list* 'list (list 'quote name) value-codes)))
       type
       (list* 'expr-annotation:special-form special-form type type-name value-annotated-exprs)))))


(defun check-optional-value (value)
  (cond
   ((eq value :%uninit%) (error "Uninitialized field read"))
   ((delayed-value? value)
    (let ((s (delayed-value-symbol value)))
      (if (boundp s)
        (symbol-value s)
        (compute-variable-value s))))
   (t value)))

; Return the tuple or record field's value.
(defun scan-&-maybe-opt (world type-env special-form label record-expr opt)
  (multiple-value-bind (record-code record-type tags record-annotated-expr) (scan-union-tag-value world type-env record-expr)
    (let ((position-alist nil)
          (field-types nil)
          (any-opt nil))
      (dolist (tag tags)
        (multiple-value-bind (position field-type mutable optional) (scan-label tag label)
          (declare (ignore mutable))
          (when optional
            (setq any-opt t))
          (let ((entry (assoc position position-alist)))
            (unless entry
              (setq entry (cons position nil))
              (push entry position-alist))
            (assert-true (null (tag-keyword tag)))
            (push (tag-name tag) (cdr entry))
            (push field-type field-types))))
      (unless (eq opt any-opt)
        (if any-opt
          (error "The field ~S may be uninitialized; use &opt instead" label)
          (error "The field ~S is always initialized; use & instead" label)))
      (assert-true position-alist)
      (setq position-alist (sort position-alist #'< :key #'car))
      (let ((result-type (apply #'make-union-type world field-types)))
        (dolist (field-type field-types)
          (unless (eq (widening-coercion-code world result-type field-type 'test 'test) 'test)
            (error "Nontrivial type coercions in & are not implemented yet")))
        (let ((code (let ((n (caar position-alist)))
                      (if (endp (cdr position-alist))
                        (if (= n -1)
                          (list 'rational record-code)
                          (gen-nth-code n record-code))
                        (let ((var (gen-local-var record-code)))
                          (let-local-var var record-code
                            (if (/= n -1)
                              `(case (car ,var)
                                 ,@(mapcar #'(lambda (entry) (list (cdr entry) (gen-nth-code (car entry) var)))
                                           position-alist))
                              `(if (floatp ,var)
                                 (rational ,var)
                                 ,(if (endp (cddr position-alist))
                                    (gen-nth-code (caadr position-alist) record-code)
                                    `(case (car ,var)
                                       ,@(mapcar #'(lambda (entry) (list (cdr entry) (gen-nth-code (car entry) var)))
                                                 (cdr position-alist))))))))))))
          (values
           (if any-opt
             (list 'check-optional-value code)
             code)
           result-type
           (list 'expr-annotation:special-form special-form record-type label record-annotated-expr)))))))

; (& <label> <record-expr>)
; Return the tuple or record field's value.
(defun scan-& (world type-env special-form label record-expr)
  (scan-&-maybe-opt world type-env special-form label record-expr nil))

; (&opt <label> <record-expr>)
; Return the tuple or record field's value.  Assert that the value is present.
(defun scan-&opt (world type-env special-form label record-expr)
  (scan-&-maybe-opt world type-env special-form label record-expr t))


; (set-field <expr> <label> <field-expr> ... <label> <field-expr>)
; Return a new tuple or record with its fields the same as in <expr> except for the specified ones.
(defun scan-set-field (world type-env special-form record-expr &rest labels-and-exprs)
  (multiple-value-bind (record-code record-type record-annotated-expr) (scan-value world type-env record-expr)
    (let ((n-replaced-fields (length labels-and-exprs)))
      (when (oddp n-replaced-fields)
        (error "Label without a field value in set-field"))
      (setq n-replaced-fields (/ n-replaced-fields 2))
      (unless (eq (type-kind record-type) :tag)
        (error "Value ~S:~A should be a tuple or a record" record-expr (print-type-to-string record-type)))
      (let* ((tag (type-tag record-type))
             (mutable (tag-mutable tag))
             (fields (tag-fields tag))
             (record-var (gen-local-var record-code))
             (n-fields (length fields))
             (replacements nil)
             (annotated-fields nil))
        (unless (> n-fields n-replaced-fields)
          (error "set-field replaces all fields in the tuple or record"))
        (do ((i n-fields (1- i)))
            ((zerop i))
          (push (gen-nth-code (+ i (if mutable 1 0)) record-var) replacements))
        (when mutable
          (push '(incf *record-counter*) replacements))
        (push (list 'quote (tag-name tag)) replacements)
        (do ((replacement-mask 0))
            ((endp labels-and-exprs))
          (let ((label (pop labels-and-exprs))
                (field-expr (pop labels-and-exprs)))
            (multiple-value-bind (position field-type mutable optional) (scan-label tag label)
              (declare (ignore mutable optional))
              (when (logbitp position replacement-mask)
                (error "Duplicate set-field label ~S" label))
              (setq replacement-mask (dpb 1 (byte 1 position) replacement-mask))
              (multiple-value-bind (field-code field-annotated-expr)
                                   (scan-typed-value world type-env field-expr field-type)
                (setf (nth position replacements) field-code)
                (push (list label field-annotated-expr) annotated-fields)))))
        (values
         (cons 'list replacements)
         record-type
         (list* 'expr-annotation:special-form special-form record-type record-annotated-expr (nreverse annotated-fields)))))))



;;; Unions

; (in <expr> <type>)
(defun scan-in (world type-env special-form value-expr type-expr)
  (let ((type (scan-type world type-expr)))
    (multiple-value-bind (value-code value-type value-annotated-expr) (scan-value world type-env value-expr)
      (type-difference world value-type type)
      (values
       (let ((var (gen-local-var value-code)))
         (let-local-var var value-code
           (type-member-test-code world type value-type var)))
       (world-boolean-type world)
       (list 'expr-annotation:special-form special-form value-annotated-expr type type-expr)))))


; (in <var> <type> <criteria>)
; <criteria> is one of:
;    nil            Don't constrain the type of <var>, which can also be an expression in this case only
;    :narrow-true   Constrain the type of <var> in the true branch
;    :narrow-false  Constrain the type of <var> in the false branch
;    :narrow-both   Constrain the type of <var> in both branches
(defun scan-in-condition (world type-env special-form var-expr type-expr &optional criteria)
  (cond
   ((null criteria)
    (multiple-value-bind (code type annotated-expr) (scan-in world type-env special-form var-expr type-expr)
      (declare (ignore type))
      (values code annotated-expr type-env type-env)))
   ((not (identifier? var-expr))
    (error "~S must be a variable" var-expr))
   ((not (member criteria '(:narrow-true :narrow-false :narrow-both)))
    (error "Bad criteria ~S" criteria))
   (t (let ((type (scan-type world type-expr)))
        (multiple-value-bind (var var-type var-annotated-expr) (scan-value world type-env var-expr)
          (multiple-value-bind (true-type false-type) (type-difference world var-type (scan-type world type-expr))
            (assert-true (symbolp var))
            (values
             (type-member-test-code world type var-type var)
             (list 'expr-annotation:special-form special-form var-annotated-expr type type-expr)
             (if (member criteria '(:narrow-true :narrow-both))
               (type-env-narrow-binding type-env var true-type)
               type-env)
             (if (member criteria '(:narrow-false :narrow-both))
               (type-env-narrow-binding type-env var false-type)
               type-env))))))))


; (not-in <expr> <type>)
(defun scan-not-in (world type-env special-form value-expr type-expr)
  (multiple-value-bind (code type annotated-expr) (scan-in world type-env special-form value-expr type-expr)
    (values
     (list 'not code)
     type
     annotated-expr)))


; (not-in <var> <type> <criteria>)
; <criteria> is one of:
;    nil            Don't constrain the type of <var>, which can also be an expression in this case only
;    :narrow-true   Constrain the type of <var> in the true branch
;    :narrow-false  Constrain the type of <var> in the false branch
;    :narrow-both   Constrain the type of <var> in both branches
(defun scan-not-in-condition (world type-env special-form var-expr type-expr &optional criteria)
  (let ((reverse-criteria (assoc criteria '((nil . nil) (:narrow-true . :narrow-false) (:narrow-false . :narrow-true) (:narrow-both . :narrow-both)))))
    (unless reverse-criteria
      (error "Bad criteria ~S" criteria))
    (multiple-value-bind (code annotated-expr true-type-env false-type-env)
                         (scan-in-condition world type-env special-form var-expr type-expr (cdr reverse-criteria))
      (values
       (list 'not code)
       annotated-expr
       false-type-env
       true-type-env))))


; (assert-in <expr> <type>)
; Returns the value of <expr>.
(defun scan-assert-in (world type-env special-form value-expr type-expr)
  (let ((type (scan-type world type-expr)))
    (multiple-value-bind (value-code value-type value-annotated-expr) (scan-value world type-env value-expr)
      (multiple-value-bind (true-type false-type) (type-difference world value-type type)
        (declare (ignore false-type))
        (values
         (let ((var (gen-local-var value-code)))
           (let-local-var var value-code
             (list 'assert (type-member-test-code world type value-type var))
             var))
         true-type
         (list 'expr-annotation:special-form special-form value-annotated-expr type type-expr))))))


; (assert-not-in <expr> <type>)
; Returns the value of <expr>.
(defun scan-assert-not-in (world type-env special-form value-expr type-expr)
  (let ((type (scan-type world type-expr)))
    (multiple-value-bind (value-code value-type value-annotated-expr) (scan-value world type-env value-expr)
      (multiple-value-bind (true-type false-type) (type-difference world value-type type)
        (declare (ignore true-type))
        (values
         (let ((var (gen-local-var value-code)))
           (let-local-var var value-code
             (list 'assert (list 'not (type-member-test-code world type value-type var)))
             var))
         false-type
         (list 'expr-annotation:special-form special-form value-annotated-expr type type-expr))))))



;;; Writable Cells

; (writable-cell-of <element-type>)
; Makes an uninitialized writable cell of the given type.
(defun scan-writable-cell-of (world type-env special-form element-type-expr)
  (declare (ignore type-env))
  (let ((element-type (scan-type world element-type-expr)))
    (values
     '(cons nil nil)
     (make-writable-cell-type world element-type)
     (list* 'expr-annotation:special-form special-form))))


;;; Delayed Values

(defun scan-delay-or-delay-of (world value-expr value-code element-type value-annotated-expr)
  (unless (and (consp value-code) (eq (first value-code) 'fetch-value) (= (length value-code) 2) (symbolp (second value-code)))
    (error "delay expression ~S must refer to a global variable" value-expr))
  (values
   (list 'make-delayed-value (list 'quote (second value-code)))
   (make-delay-type world element-type)
   value-annotated-expr))

; (delay <global>)
; Makes a delayed-global-read object for accessing the given global.  Such an object can be accessed only by assigning it to
; an :opt-const or :opt-var record field and then reading it.
(defun scan-delay-expr (world type-env special-form value-expr)
  (declare (ignore special-form))
  (multiple-value-bind (value-code element-type value-annotated-expr) (scan-value world type-env value-expr)
    (scan-delay-or-delay-of world value-expr value-code element-type value-annotated-expr)))


; (delay-of <element-type> <global>)
; Makes a delayed-global-read object for accessing the given global.  Such an object can be accessed only by assigning it to
; an :opt-const or :opt-var record field and then reading it.
(defun scan-delay-of-expr (world type-env special-form element-type-expr value-expr)
  (declare (ignore special-form))
  (let ((element-type (scan-type world element-type-expr)))
    (multiple-value-bind (value-code value-annotated-expr) (scan-typed-value world type-env value-expr element-type)
      (scan-delay-or-delay-of world value-expr value-code element-type value-annotated-expr))))




;;; ------------------------------------------------------------------------------------------------------
;;; STATEMENT EXPRESSIONS


; If source is a list that starts with a statement keyword, return that interned keyword;
; otherwise return nil.
(defun statement? (world source)
  (and (consp source)
       (let ((id (first source)))
         (and (identifier? id)
              (let ((symbol (world-find-symbol world id)))
                (when (get symbol :statement)
                  symbol))))))


; Generate a list of lisp expressions that will execute the given statements.
; type-env is the type environment or nil if these statements are not reachable.
; last is true if these statements' lisp return value becomes the return value of the function if the function falls through.
;
; Return three values:
;   A list of codes (a list of lisp expressions)
;   :dead if the statement cannot complete or a list of the symbols of :uninitialized variables that are initialized if the statement can complete.
;   A list of annotated statements
(defun scan-statements (world type-env statements last)
  (if statements
    (if type-env
      (let* ((statement (first statements))
             (rest-statements (rest statements))
             (symbol (statement? world statement)))
        (if symbol
          (apply (get symbol :statement) world type-env rest-statements last symbol (rest statement))
          (multiple-value-bind (statement-code live statement-annotated-expr)
                               (scan-void-value world type-env statement)
            (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                                 (scan-statements world (and live type-env) rest-statements last)
              (values (cons statement-code rest-codes)
                      rest-live
                      (cons (list (world-intern world 'exec) statement-annotated-expr) rest-annotated-stmts))))))
      (error "Unreachable statements: ~S" statements))
    (values nil
            (if type-env (type-env-live type-env) :dead)
            nil)))


; Compute the initial type-env to use for the given general-production's action code.
; The first cell of the type-env gives the production's lhs nonterminal's symbol;
; the remaining cells give the action arguments in order.
; If include-lhs is true, include the lhs's actions with index 0 at the beginning of the
; environment.
(defun general-production-action-env (grammar general-production include-lhs)
  (let* ((index-override nil)
         (current-indices nil)
         (lhs-general-nonterminal (general-production-lhs general-production))
         (bound-arguments-alist (nonterminal-sample-bound-argument-alist grammar lhs-general-nonterminal)))
    (flet ((general-symbol-action-env (general-grammar-symbol)
             (let* ((symbol (general-grammar-symbol-symbol general-grammar-symbol))
                    (index (or index-override (incf (getf current-indices symbol 0))))
                    (grammar-symbol (instantiate-general-grammar-symbol bound-arguments-alist general-grammar-symbol)))
               (mapcar
                #'(lambda (declaration)
                    (let* ((action-symbol (car declaration))
                           (action-type (cdr declaration))
                           (local-symbol (gensym (symbol-name action-symbol))))
                      (make-type-env-action
                       (list* action-symbol symbol index)
                       local-symbol
                       action-type
                       general-grammar-symbol)))
                (grammar-symbol-signature grammar grammar-symbol)))))
      (let ((env (set-type-env-flag 
                  (make-type-env (mapcan #'general-symbol-action-env (general-production-rhs general-production)) nil)
                  :lhs-symbol (general-grammar-symbol-symbol lhs-general-nonterminal))))
        (when include-lhs
          (setq index-override 0)
          (setq env (make-type-env (nconc (general-symbol-action-env lhs-general-nonterminal) (type-env-bindings env))
                                   (type-env-live env))))
        env))))


; Return the number of arguments that a function returned by compute-action-code
; would expect.
(defun n-action-args (grammar production)
  (let ((n-args 0))
    (dolist (grammar-symbol (production-rhs production))
      (incf n-args (length (grammar-symbol-signature grammar grammar-symbol))))
    n-args))


; Compute the code for evaluating body-expr to obtain the value of one of the
; production's actions.  Verify that the result has the given type and that the
; type is the same as type-expr.
(defun compute-action-code (world production action-symbol type-expr body-expr type initial-env)
  (handler-bind (((or error warning)
                  #'(lambda (condition)
                      (declare (ignore condition))
                      (format *error-output* "~&~@<~2IWhile processing action ~A on ~S: ~_~:W~:>~%"
                              action-symbol (production-name production) body-expr))))
    (let ((type2 (scan-type world type-expr)))
      (unless (type= type type2)
        (error "Action declared using type ~A but defined using ~A"
               (print-type-to-string type) (print-type-to-string type2))))
    (let ((body-code (scan-typed-value-or-begin world initial-env body-expr type)))
      (name-lambda body-code
                   (concatenate 'string (symbol-name (production-name production))
                                "~" (symbol-name action-symbol))
                   (world-package world)))))


; Compute the body of all grammar actions for this production.
(defun compute-production-code (world grammar production)
  (let* ((lhs (production-lhs production))
         (n-action-args (n-action-args grammar production))
         (initial-env (general-production-action-env grammar production nil))
         (args (mapcar #'cadr (cdr (type-env-bindings initial-env)))))
    (assert-true (= (length args) n-action-args))
    (let* ((result-vars nil)
           (code-bindings
            (mapcar
             #'(lambda (action-binding)
                 (let ((action-symbol (car action-binding))
                       (action (cdr action-binding)))
                   (unless action
                     (error "Missing action ~S for production ~S" (car action-binding) (production-name production)))
                   (multiple-value-bind (has-type type) (action-declaration grammar (production-lhs production) action-symbol)
                     (declare (ignore has-type))
                     (let ((code (compute-action-code world production action-symbol (action-type action) (action-expr action) type initial-env))
                           (result-var (gensym (symbol-name action-symbol))))
                       (when *trace-variables*
                         (format *trace-output* "~&~@<~S[~S] := ~2I~_~:W~:>~%" action-symbol (production-name production) code))
                       (push result-var result-vars)
                       (setq initial-env
                             (make-type-env (cons (make-type-env-action
                                                   (list* action-symbol (general-grammar-symbol-symbol lhs) 0)
                                                   result-var
                                                   type
                                                   lhs)
                                                  (type-env-bindings initial-env))
                                            (type-env-live initial-env)))
                       (list result-var code)))))
             (production-actions production)))
           (filtered-args (mapcar #'(lambda (arg)
                                      (and (tree-member arg code-bindings) arg))
                                  args))
           (production-code
            (if code-bindings
              `(lambda (stack)
                 (list*-bind ,(nreconc filtered-args '(stack-rest)) stack
                   (let* ,code-bindings
                     (list* ,@result-vars stack-rest))))
              `(lambda (stack)
                 (nthcdr ,n-action-args stack))))
           (production-code-name (unique-function-name world (string (production-name production)))))
      (setf (production-n-action-args production) n-action-args)
      (when *trace-variables*
        (format *trace-output* "~&~@<all[~S] := ~2I~_~:W~:>~%" (production-name production) production-code))
      (handler-bind (((or error warning)
                      #'(lambda (condition)
                          (declare (ignore condition))
                          (format *error-output* "~&While computing production ~S:~%" (production-name production)))))
        (quiet-compile production-code-name production-code)
        (setf (production-evaluator production) (symbol-function production-code-name))))))


; Return a list of all grammar symbols' symbols that are present in at least one expr-annotation:action
; in the annotated expression.  The symbols are returned in no particular order.
(defun annotated-expr-grammar-symbols (annotated-expr)
  (let ((symbols nil))
    (labels
      ((scan (annotated-expr)
         (when (consp annotated-expr)
           (if (eq (first annotated-expr) 'expr-annotation:action)
             (pushnew (general-grammar-symbol-symbol (third annotated-expr)) symbols :test *grammar-symbol-=*)
             (mapc #'scan annotated-expr)))))
      (scan annotated-expr)
      symbols)))


; text is a list of strings and forms intended for a comment.  Interpret a few special forms as follows:
;   (:expr <result-type> <expr>)
;     becomes converted to (:annotated-expr <annotated-expr>)
;   (:def-const <name> <type>)
;     augments the environment for the rest of the comment with a local variable named <name> with type <type>.
;   (:initialize <name>)
;     augments the environment for the rest of the comment by initializing the local variable named <name>.
(defun scan-expressions-in-comment (world type-env text)
  (mapcan #'(lambda (item)
              (if (consp item)
                (let ((key (first item)))
                  (case key
                    (:expr
                     (unless (= (length item) 3)
                       (error "Bad :expr ~S" item))
                     (let ((result-type (scan-type world (second item))))
                       (multiple-value-bind (value-code value-annotated-expr) (scan-typed-value world type-env (third item) result-type)
                         (declare (ignore value-code))
                         (list (list :annotated-expr value-annotated-expr)))))
                    (:def-const
                      (unless (= (length item) 3)
                        (error "Bad :expr ~S" item))
                      (let* ((symbol (scan-name world (second item)))
                             (type (scan-type world (third item))))
                        (setq type-env (type-env-add-binding type-env symbol type :const)))
                      nil)
                    (:initialize
                     (unless (= (length item) 2)
                       (error "Bad :expr ~S" item))
                     (let ((symbol (scan-name world (second item))))
                       (setq type-env (type-env-initialize-var type-env symbol)))
                     nil)
                    (t (list item))))
                (list item)))
          text))


;;; ------------------------------------------------------------------------------------------------------
;;; STATEMENTS


; (// . <styled-text>)
; (note . <styled-text>)
; A one-paragraph comment using the given <styled-text>.  The note form precedes the text with the keyword 'note'.
(defun scan-// (world type-env rest-statements last special-form &rest text)
  (unless text
    (error "// or note should have non-empty text"))
  (let ((text2 (scan-expressions-in-comment world type-env text)))
    (multiple-value-bind (rest-codes rest-live rest-annotated-stmts) (scan-statements world type-env rest-statements last)
      (values rest-codes
              rest-live
              (cons (cons special-form text2) rest-annotated-stmts)))))


; (/* . <styled-text>)
; A one-paragraph comment using the given <styled-text>.  The subsequent statements are hidden until the next (*/) statement
; or until the end of the subsequent statements if they cannot fall through.
; These comments cannot nest.
(defun scan-/* (world type-env rest-statements last special-form &rest text)
  (unless text
    (error "/* should have non-empty text"))
  (let ((text2 (scan-expressions-in-comment world type-env text)))
    (multiple-value-bind (rest-codes rest-live rest-annotated-stmts) (scan-statements world type-env rest-statements last)
      (let ((end-special-form (assert-non-null (world-find-symbol world '*/))))
        (loop
          (when (endp rest-annotated-stmts)
            (if (eq rest-live :dead)
              (return)
              (error "Missing */")))
          (let* ((annotated-stmt (pop rest-annotated-stmts))
                 (stmt-keyword (first annotated-stmt)))
            (cond
             ((eq stmt-keyword special-form)
              (error "/* comments can't nest"))
             ((eq stmt-keyword end-special-form)
              (return))))))
      (values rest-codes
              rest-live
              (cons (cons special-form text2) rest-annotated-stmts)))))


; (*/)
; Terminates a /* comment.
(defun scan-*/ (world type-env rest-statements last special-form)
  (multiple-value-bind (rest-codes rest-live rest-annotated-stmts) (scan-statements world type-env rest-statements last)
    (values rest-codes
            rest-live
            (cons (list special-form) rest-annotated-stmts))))


(defun eval-bottom ()
  (error "Reached a BOTTOM statement"))

; (bottom . <styled-text>)
; Raises an error.
(defun scan-bottom (world type-env rest-statements last special-form &rest text)
  (let ((text2 (scan-expressions-in-comment world type-env text)))
    (scan-statements world nil rest-statements last)
    (values
     (list '(eval-bottom))
     :dead
     (list (cons special-form text2)))))


; (assert <condition-expr> . <styled-text>)
; Used to declare conditions that are known to be true if the semantics function correctly.  Don't use this to
; verify user input.
; <styled-text> can contain the entry (:assertion) to depict <condition-expr>.
(defun scan-assert (world type-env rest-statements last special-form condition-expr &rest text)
  (unless text
    (setq text '((:assertion) ";")))
  (let ((text2 (scan-expressions-in-comment world type-env text)))
    (multiple-value-bind (condition-code condition-annotated-expr true-type-env false-type-env)
                         (scan-condition world type-env condition-expr)
      (declare (ignore false-type-env))
      (multiple-value-bind (rest-codes rest-live rest-annotated-stmts) (scan-statements world true-type-env rest-statements last)
        (values (cons (list 'assert condition-code) rest-codes)
                rest-live
                (cons (list* special-form condition-annotated-expr text2) rest-annotated-stmts))))))


; (quiet-assert <condition-expr>)
; Used to declare conditions that are known to be true if the semantics function correctly.  Don't use this to
; verify user input.
; A quiet-assert does not appear in the depicted statements.
(defun scan-quiet-assert (world type-env rest-statements last special-form condition-expr)
  (declare (ignore special-form))
  (multiple-value-bind (condition-code condition-annotated-expr true-type-env false-type-env)
                       (scan-condition world type-env condition-expr)
    (declare (ignore condition-annotated-expr false-type-env))
    (multiple-value-bind (rest-codes rest-live rest-annotated-stmts) (scan-statements world true-type-env rest-statements last)
      (values (cons (list 'assert condition-code) rest-codes)
              rest-live
              rest-annotated-stmts))))


; (exec <expr>)
(defun scan-exec (world type-env rest-statements last special-form expr)
  (multiple-value-bind (statement-code statement-type statement-annotated-expr)
                       (scan-value world type-env expr)
    (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                         (scan-statements world (and (not (eq (type-kind statement-type) :bottom)) type-env) rest-statements last)
      (values (cons statement-code rest-codes)
              rest-live
              (cons (list special-form statement-annotated-expr) rest-annotated-stmts)))))


; (const <name> <type> <value>)
; (var <name> <type> <value>)
(defun scan-const (world type-env rest-statements last special-form name type-expr value-expr)
  (let* ((symbol (scan-name world name))
         (type (scan-type world type-expr))
         (placeholder-type-env (type-env-add-binding type-env symbol type :unused)))
    (multiple-value-bind (value-code value-annotated-expr) (scan-typed-value world placeholder-type-env value-expr type)
      (let ((local-type-env (type-env-add-binding type-env symbol type (find-keyword special-form))))
        (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                             (scan-statements world local-type-env rest-statements last)
          (values
           (list `(let ((,symbol ,value-code))
                    ,@rest-codes))
           rest-live
           (cons (list special-form name type-expr value-annotated-expr) rest-annotated-stmts)))))))


; (var <name> <type> [<value>])
(defun scan-var (world type-env rest-statements last special-form name type-expr &optional value-expr)
  (if value-expr
    (scan-const world type-env rest-statements last special-form name type-expr value-expr)
    (let* ((symbol (scan-name world name))
           (type (scan-type world type-expr)))
      (let ((local-type-env (type-env-add-binding type-env symbol type :uninitialized)))
        (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                             (scan-statements world local-type-env rest-statements last)
          (unless (eq rest-live :dead)
            (setq rest-live (remove symbol rest-live :test #'eq)))
          (values
           (list `(let (,symbol) ,@rest-codes))
           rest-live
           (cons (list special-form name type-expr) rest-annotated-stmts)))))))


; (multiple-value-bind ((<name> <type>) ...) <lisp-function> <arg-exprs>)
; Evaluate <lisp-function> applied to the results of evaluating <arg-exprs>.  The function should return multiple values,
; which are assigned to new variables with the given names and types.
(defun scan-multiple-value-bind (world type-env rest-statements last special-form names-and-types lisp-function arg-exprs)
  (unless (structured-type? names-and-types '(list (tuple t t)))
    (error "Bad definitions for scan-multiple-value-bind"))
  (let ((arg-values nil))
    (dolist (arg-expr arg-exprs)
      (multiple-value-bind (arg-value arg-type arg-annotated-expr) (scan-value world type-env arg-expr)
        (declare (ignore arg-type arg-annotated-expr))
        (push arg-value arg-values)))
    (let ((arg-values (nreverse arg-values))
          (symbols nil))
      (dolist (name-and-type names-and-types)
        (let* ((symbol (scan-name world (first name-and-type)))
               (type (scan-type world (second name-and-type))))
          (setq type-env (type-env-add-binding type-env symbol type :var))
          (push symbol symbols)))
      (setq symbols (nreverse symbols))
      (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                           (scan-statements world type-env rest-statements last)
        (unless (eq rest-live :dead)
          (dolist (symbol symbols)
            (setq rest-live (remove symbol rest-live :test #'eq))))
        (values
         (list `(multiple-value-bind ,symbols (,lisp-function ,@arg-values) ,@rest-codes))
         rest-live
         (cons (list special-form names-and-types lisp-function arg-exprs) rest-annotated-stmts))))))


; (reserve <name>)
; Used to reserve <name> as a variable that can be later defined by a (some <name> ... :define-true) expression.
(defun scan-reserve (world type-env rest-statements last special-form name)
  (declare (ignore special-form))
  (let* ((symbol (scan-name world name))
         (local-type-env (type-env-add-binding type-env symbol (world-void-type world) :reserved)))
    (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                         (scan-statements world local-type-env rest-statements last)
      (values
       (list `(let (,symbol) ,@rest-codes))
       rest-live
       rest-annotated-stmts))))


; (function (<name> (<var1> <type1> [:var | :unused]) ... (<varn> <typen> [:var | :unused])) <result-type> . <statements>)
(defun scan-function (world type-env rest-statements last special-form name-and-arg-binding-exprs result-type-expr &rest body-statements)
  (unless (consp name-and-arg-binding-exprs)
    (error "Bad function name and bindings: ~S" name-and-arg-binding-exprs))
  (let* ((symbol (scan-name world (first name-and-arg-binding-exprs)))
         (placeholder-type-env (type-env-add-binding type-env symbol (world-void-type world) :unused)))
    (multiple-value-bind (args-and-body-codes type body-annotated-stmts)
                         (scan-function-or-lambda world placeholder-type-env (rest name-and-arg-binding-exprs) result-type-expr body-statements)
      (let ((local-type-env (type-env-add-binding type-env symbol type :function)))
        (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                             (scan-statements world local-type-env rest-statements last)
          (values
           (list `(flet ((,symbol ,@args-and-body-codes))
                    ,@rest-codes))
           rest-live
           (cons (list* special-form name-and-arg-binding-exprs result-type-expr body-annotated-stmts) rest-annotated-stmts)))))))


; (<- <name> <value> [:end-narrow])
; Mutate the local or global variable.
(defun scan-<- (world type-env rest-statements last special-form name value-expr &optional end-narrow)
  (unless (member end-narrow '(nil :end-narrow))
    (error "Bad flag ~S given to <-" end-narrow))
  (let* ((symbol (scan-name world name))
         (symbol-binding (type-env-get-local type-env symbol))
         (type-env2 type-env)
         type)
    (if symbol-binding
      (case (type-env-local-mode symbol-binding)
        (:var
          (when end-narrow
            (multiple-value-setq (symbol-binding type-env2) (type-env-unnarrow-binding type-env symbol)))
          (setq type (type-env-local-type symbol-binding)))
        (:uninitialized
         (when end-narrow
           (error ":end-narrow not applicable to uninitialized variables"))
         (setq type (type-env-local-type symbol-binding))
         (setq type-env2 (type-env-initialize-var type-env2 symbol)))
        (t (error "Local variable ~A not writable" name)))
      (progn
        (setq type (symbol-type symbol))
        (unless type
          (error "Unknown local or global variable ~A" name))
        (unless (get symbol :mutable)
          (error "Global variable ~A not writable" name))
        (when end-narrow
          (error ":end-narrow not applicable to global variables"))))
    (multiple-value-bind (value-code value-annotated-expr) (scan-typed-value world type-env value-expr type)
      (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                           (scan-statements world type-env2 rest-statements last)
        (values
         (cons (if symbol-binding
                 (list 'setq (type-env-local-name symbol-binding) value-code)
                 (list 'store-global-value symbol value-code))
               rest-codes)
         rest-live
         (cons (list special-form name value-annotated-expr (not symbol-binding)) rest-annotated-stmts))))))


; (&= <label> <record-expr> <value-expr>)
; Writes the value of the field.
(defun scan-&= (world type-env rest-statements last special-form label record-expr value-expr)
  (multiple-value-bind (record-code record-type tags record-annotated-expr) (scan-union-tag-value world type-env record-expr)
    (let ((position-alist nil)
          (field-types nil))
      (dolist (tag tags)
        (multiple-value-bind (position field-type mutable optional) (scan-label tag label)
          (declare (ignore optional))
          (unless mutable
            (error "Attempt to write to immutable field ~S of ~S" label (tag-name tag)))
          (let ((entry (assoc position position-alist)))
            (unless entry
              (setq entry (cons position nil))
              (push entry position-alist))
            (assert-true (null (tag-keyword tag)))
            (push (tag-name tag) (cdr entry))
            (push field-type field-types))))
      (assert-true position-alist)
      (setq position-alist (sort position-alist #'< :key #'car))
      (let ((destination-type (apply #'make-intersection-type world field-types)))
        (dolist (field-type field-types)
          (unless (eq (widening-coercion-code world field-type destination-type 'test 'test) 'test)
            (error "Type coercions in &= are not implemented yet")))
        (multiple-value-bind (value-code value-annotated-expr) (scan-typed-value world type-env value-expr destination-type)
          (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                               (scan-statements world type-env rest-statements last)
            (values
             (cons
              (if (endp (cdr position-alist))
                (list 'setf (gen-nth-code (caar position-alist) record-code) value-code)
                (let ((var (gen-local-var record-code))
                      (val (gen-local-var value-code)))
                  (let-local-var var record-code
                    (let-local-var val value-code
                      `(case (car ,var)
                         ,@(mapcar #'(lambda (entry) (list (cdr entry) (list 'setf (gen-nth-code (car entry) var) val)))
                                   position-alist))))))
              rest-codes)
             rest-live
             (cons (list special-form record-type label record-annotated-expr value-annotated-expr) rest-annotated-stmts))))))))


; (&const= <label> <record-expr> <value-expr>)
; Initializes the value of an optional constant field.
(defun scan-&const= (world type-env rest-statements last special-form label record-expr value-expr)
  (multiple-value-bind (record-code record-type tags record-annotated-expr) (scan-union-tag-value world type-env record-expr)
    (let ((position-alist nil)
          (field-types nil))
      (dolist (tag tags)
        (multiple-value-bind (position field-type mutable optional) (scan-label tag label)
          (declare (ignore mutable))
          (unless optional
            (error "Attempt to initialize a non-optional field ~S of ~S" label (tag-name tag)))
          (let ((entry (assoc position position-alist)))
            (unless entry
              (setq entry (cons position nil))
              (push entry position-alist))
            (assert-true (null (tag-keyword tag)))
            (push (tag-name tag) (cdr entry))
            (push field-type field-types))))
      (assert-true position-alist)
      (setq position-alist (sort position-alist #'< :key #'car))
      (let ((destination-type (apply #'make-intersection-type world field-types)))
        (dolist (field-type field-types)
          (unless (eq (widening-coercion-code world field-type destination-type 'test 'test) 'test)
            (error "Type coercions in &const= are not implemented yet")))
        (multiple-value-bind (value-code value-annotated-expr) (scan-typed-value world type-env value-expr destination-type)
          (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                               (scan-statements world type-env rest-statements last)
            (values
             (append
              (if (endp (cdr position-alist))
                (list
                 (list 'assert (list 'eq (gen-nth-code (caar position-alist) record-code) :%uninit%))
                 (list 'setf (gen-nth-code (caar position-alist) record-code) value-code))
                (let ((var (gen-local-var record-code))
                      (val (gen-local-var value-code)))
                  (let-local-var var record-code
                    (let-local-var val value-code
                      `(case (car ,var)
                         ,@(mapcar #'(lambda (entry) (list (cdr entry)
                                                           (list 'assert (list 'eq (gen-nth-code (car entry) var) :%uninit%))
                                                           (list 'setf (gen-nth-code (car entry) var) val)))
                                   position-alist))))))
              rest-codes)
             rest-live
             (cons (list special-form record-type label record-annotated-expr value-annotated-expr) rest-annotated-stmts))))))))


; (action<- <action> <value>)
; Mutate the writable action.  This can be done only once per action.
(defun scan-action<- (world type-env rest-statements last special-form action value-expr)
  (unless (and (consp action) (identifier? (first action)))
    (error "Bad action: ~S" action))
  (let ((symbol (world-intern world (first action))))
    (unless (symbol-action symbol)
      (error "~S is not an action name" (first action)))
    (multiple-value-bind (action-value action-type action-annotated-expr) (apply #'scan-action-call type-env symbol (rest action))
      (unless (eq (type-kind action-type) :writable-cell)
        (error "action<- type ~S must be a writable-cell" action-type))
      (assert-true (symbolp action-value))
      (multiple-value-bind (value-code value-annotated-expr) (scan-typed-value world type-env value-expr (writable-cell-element-type action-type))
        (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                             (scan-statements world type-env rest-statements last)
          (values
           (if (or (symbolp value-code) (numberp value-code))
             (list* `(when (car ,action-value)
                       (error "Attempt to write ~S to an already initialized writable-cell ~S" ,value-code ,action-value))
                    `(setf (car ,action-value) t)
                    `(setf (cdr ,action-value) ,value-code)
                    rest-codes)
             (let ((v (gensym "V")))
               (cons `(let ((,v ,value-code))
                        (when (car ,action-value)
                          (error "Attempt to write ~S to an already initialized writable-cell ~S" ,v ,action-value))
                        (setf (car ,action-value) t)
                        (setf (cdr ,action-value) ,v))
                     rest-codes)))
           rest-live
           (cons (list special-form action-annotated-expr value-annotated-expr) rest-annotated-stmts)))))))


; (return [<value-expr>])
(defun scan-return (world type-env rest-statements last special-form &optional value-expr)
  (let ((value-code nil)
        (value-annotated-expr nil)
        (type (get-type-env-flag type-env :return)))
    (cond
     (value-expr
      (multiple-value-setq (value-code value-annotated-expr)
        (scan-typed-value world type-env value-expr type)))
     ((not (type= type (world-void-type world)))
      (error "Return statement needs a value")))
    (scan-statements world nil rest-statements last)
    (values
     (list (if last
             value-code
             (list* 'return-from
                    (gen-type-env-return-block-name type-env)
                    (and value-code (list value-code)))))
     :dead
     (list (list special-form value-annotated-expr)))))


; (rwhen <condition-expr> . <true-statements>)
; Same as when except that checks that true-statements cannot fall through and generates more efficient code.
(defun scan-rwhen (world type-env rest-statements last special-form condition-expr &rest true-statements)
  (multiple-value-bind (condition-code condition-annotated-expr true-type-env false-type-env)
                       (scan-condition world type-env condition-expr)
    (multiple-value-bind (true-codes true-live true-annotated-stmts) (scan-statements world true-type-env true-statements last)
      (unless (eq true-live :dead)
        (error "rwhen statements ~S must not fall through" true-statements))
      (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                           (scan-statements world false-type-env rest-statements last)
        (values (list (list 'if condition-code (gen-progn true-codes) (gen-progn rest-codes)))
                rest-live
                (cons (list special-form (cons condition-annotated-expr true-annotated-stmts)) rest-annotated-stmts))))))


; (when <condition-expr> . <true-statements>)
(defun scan-when (world type-env rest-statements last special-form condition-expr &rest true-statements)
  (scan-cond world type-env rest-statements last special-form (cons condition-expr true-statements)))


; (if <condition-expr> <true-statement> <false-statement>)
(defun scan-if-stmt (world type-env rest-statements last special-form condition-expr true-statement false-statement)
  (scan-cond world type-env rest-statements last special-form (list condition-expr true-statement) (list nil false-statement)))


; Generate and optimize a cond statement with the given cases.
(defun gen-cond-code (cases)
  (cond
   ((endp cases) nil)
   ((endp (cdr cases))
    (cons 'when (car cases)))
   ((and (endp (cddr cases)) (eq (car (second cases)) t) (endp (cddr (first cases))) (endp (cddr (second cases))))
    (list 'if (first (first cases)) (second (first cases)) (second (second cases))))
   (t (cons 'cond cases))))


; (cond (<condition-expr> . <statements>) ... (<condition-expr> . <statements>) [(nil . <statements>)])
; <condition-expr> can be one of the following:
;  nil            Always true; used for an "else" clause
;  true           Same as nil
;  <expr>         Condition expression <expr>
(defun scan-cond (world type-env rest-statements last special-form &rest cases)
  (unless cases
    (error "Empty cond statement"))
  (let ((local-type-env type-env)
        (nested-last (and last (null rest-statements)))
        (case-codes nil)
        (annotated-cases nil)
        (joint-live :dead)
        (found-default-case nil))
    (dolist (case cases)
      (unless (consp case)
        (error "Bad cond case: ~S" case))
      (when found-default-case
        (error "Cond case follows default case: ~S" cases))
      (let ((condition-expr (first case)))
        (multiple-value-bind (condition-code condition-annotated-expr true-type-env false-type-env)
                             (if (member condition-expr '(nil true))
                               (values t nil local-type-env local-type-env)
                               (scan-condition world local-type-env condition-expr))
          (when (eq condition-code t)
            (if (cdr cases)
              (setq found-default-case t)
              (error "Cond statement consisting only of an else case: ~S" cases)))
          (multiple-value-bind (codes live annotated-stmts) (scan-statements world true-type-env (rest case) nested-last)
            (push (cons condition-code codes) case-codes)
            (push (cons condition-annotated-expr annotated-stmts) annotated-cases)
            (setq joint-live (merge-live-lists joint-live live)))
          (setq local-type-env false-type-env))))
    (unless found-default-case
      (setq joint-live (merge-live-lists joint-live (type-env-live type-env))))
    (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                         (scan-statements world (substitute-live type-env joint-live) rest-statements last)
      (values (cons (gen-cond-code (nreverse case-codes)) rest-codes)
              rest-live
              (cons (cons special-form (nreverse annotated-cases)) rest-annotated-stmts)))))


; (while <condition-expr> . <statements>)
(defun scan-while (world type-env rest-statements last special-form condition-expr &rest loop-statements)
  (multiple-value-bind (condition-code condition-annotated-expr true-type-env false-type-env)
                       (scan-condition world type-env condition-expr)
    (multiple-value-bind (loop-codes loop-live loop-annotated-stmts) (scan-statements world true-type-env loop-statements nil)
      (unless (listp loop-live)
        (warn "While loop can execute at most once: ~S ~S" condition-expr loop-statements))
      (let ((infinite (and (constantp condition-code) (symbolp condition-code) condition-code)))
        (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                             (scan-statements world (and (not infinite) false-type-env) rest-statements last)
          (values
           (cons (if infinite
                   (cons 'loop loop-codes)
                   `(do ()
                        ((not ,condition-code))
                      ,@loop-codes))
                 rest-codes)
           rest-live
           (cons (list* special-form condition-annotated-expr loop-annotated-stmts) rest-annotated-stmts)))))))


; (for-each <vector-or-set-expr> <var> . <statements>)
; Not implemented on range-sets.
(defun scan-for-each (world type-env rest-statements last special-form collection-expr var-source &rest loop-statements)
  (multiple-value-bind (collection-code collection-kind element-type collection-annotated-expr) (scan-collection-value world type-env collection-expr)
    (case collection-kind
      ((:vector :list-set))
      (:string (setq collection-code (list 'coerce collection-code ''list)))
      (t (error "Not implemented")))
    (let* ((var (scan-name world var-source))
           (local-type-env (type-env-add-binding type-env var element-type :const)))
      (multiple-value-bind (loop-codes loop-live loop-annotated-stmts) (scan-statements world local-type-env loop-statements nil)
        (unless (listp loop-live)
          (warn "For-each loop can execute at most once: ~S ~S" collection-expr var-source loop-statements))
        (multiple-value-bind (rest-codes rest-live rest-annotated-stmts) (scan-statements world type-env rest-statements last)
          (values
           (cons `(dolist (,var ,collection-code)
                    ,@loop-codes)
                 rest-codes)
           rest-live
           (cons (list* special-form collection-annotated-expr var loop-annotated-stmts) rest-annotated-stmts)))))))


(defconstant *semantic-exception-type-name* 'semantic-exception)

; (throw <value-expr> . <styled-text>)
; <value-expr> must have type *semantic-exception-type-name*, which must be the name of some user-defined type in the environment.
; If present, <styled-text> is depicted after the throw statement, separated by an em-dash.
(defun scan-throw (world type-env rest-statements last special-form value-expr &rest text)
  (multiple-value-bind (value-code value-annotated-expr)
                       (scan-typed-value world type-env value-expr (scan-type world *semantic-exception-type-name*))
    (scan-statements world type-env rest-statements last)
    (let ((text2 (scan-expressions-in-comment world type-env text)))
      (values
       (list (list 'throw :semantic-exception value-code))
       :dead
       (list (list* special-form value-annotated-expr text2))))))


(defparameter *an-error-list* '(-argument-error -attribute-error -eval-error -uninitialized-error))

; (throw-error <error> . <styled-text>)
; Syntactic sugar for:
; (throw (/*/ (system-error <error> <message>) "a[n] " <error> " exception " :m-dash " " . <styled-text>))
(defun scan-throw-error (world type-env rest-statements last special-form error-name &rest text)
  (declare (ignore special-form))
  (scan-statements
   world
   type-env
   (cons `(throw (/*/ (system-error ,error-name ,(simple-text-to-string text))
                   ,(if (member error-name *an-error-list*) "an" "a")
                   :nbsp
                   (:global ,error-name)
                   :nbsp
                   "exception"
                   ,@(and text (list* " " :m-dash " " text))))
         rest-statements)
   last))


(defun simple-text-to-string (text)
  (if text
    (apply #'concatenate 'string
           (mapcar #'(lambda (text-item)
                       (cond
                        ((stringp text-item) text-item)
                        ((eq text-item :apostrophe) "'")
                        ((characterp text-item) (string text-item))
                        ((and (consp text-item) (= (length text-item) 2) (eq (first text-item) :character-literal) (stringp (second text-item)))
                         (second text-item))
                        (t "*")))
                   text))
    'undefined))


; (catch <body-statements> (<var> [:unused]) . <handler-statements>)
(defun scan-catch (world type-env rest-statements last special-form body-statements arg-binding-expr &rest handler-statements)
  (multiple-value-bind (body-codes body-live body-annotated-stmts) (scan-statements world type-env body-statements nil)
    (unless (and (consp arg-binding-expr)
                 (member (cdr arg-binding-expr) '(nil (:unused)) :test #'equal))
      (error "Bad catch binding ~S" arg-binding-expr))
    (let* ((nested-last (and last (null rest-statements)))
           (arg-symbol (scan-name world (first arg-binding-expr)))
           (arg-type (scan-type world *semantic-exception-type-name*))
           (local-type-env (type-env-add-binding type-env arg-symbol arg-type :const)))
      (multiple-value-bind (handler-codes handler-live handler-annotated-stmts) (scan-statements world local-type-env handler-statements nested-last)
        (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                             (scan-statements world (substitute-live type-env (merge-live-lists body-live handler-live)) rest-statements last)
          (let ((code
                 `(block nil
                    (let ((,arg-symbol (catch :semantic-exception ,@body-codes ,@(when (listp body-live) '((return))))))
                      ,@(and (eq (second arg-binding-expr) :unused) `((declare (ignore ,arg-symbol))))
                      ,@handler-codes))))
            (values (cons code rest-codes)
                    rest-live
                    (cons (list* special-form body-annotated-stmts arg-binding-expr handler-annotated-stmts) rest-annotated-stmts))))))))


(defun case-error ()
  (error "No case chosen"))

; (case <value-expr> (key <type> . <statements>) ... (keyword <type> . <statements>))
; where each key is one of:
;    :select    No special action
;    :narrow    Narrow the type of <value-expr>, which must be a variable, to this case's <type>
;    :otherwise Catch-all else case; <type> should be either nil or the remaining catch-all type
(defun scan-case (world type-env rest-statements last special-form value-expr &rest cases)
  (multiple-value-bind (value-code value-type value-annotated-expr) (scan-value world type-env value-expr)
    (handler-bind (((or error warning)
                    #'(lambda (condition)
                        (declare (ignore condition))
                        (format *error-output* "~@<In case ~S: ~A ~_~S~:>" value-expr (print-type-to-string value-type) cases))))
      (let ((var (if (symbolp value-code) value-code (gensym "CASE")))
            (nested-last (and last (null rest-statements))))
        (labels
          ((process-remaining-cases (cases remaining-type)
             (if cases
               (let ((case (car cases))
                     (cases (cdr cases)))
                 (unless (and (consp case) (consp (cdr case)) (member (car case) '(:select :narrow :otherwise)))
                   (error "Bad case ~S" case))
                 (let ((key (first case))
                       (type-expr (second case))
                       (statements (cddr case)))
                   (if (eq key :otherwise)
                     (progn
                       (when cases
                         (error "Otherwise case must be the last one"))
                       (when type-expr
                         (let ((type (scan-type world type-expr)))
                           (unless (type= type remaining-type)
                             (error "Otherwise case type ~A given but ~A expected"
                                    (print-type-to-string type) (print-type-to-string remaining-type)))))
                       (when (type= remaining-type (world-bottom-type world))
                         (error "Otherwise case not reached"))
                       (multiple-value-bind (statements-codes statements-live statements-annotated-stmts)
                                            (scan-statements world type-env statements nested-last)
                         (values (list (cons t statements-codes))
                                 statements-live
                                 (list (list* key type-expr statements-annotated-stmts)))))
                     (multiple-value-bind (type remaining-type) (type-difference world remaining-type (scan-type world type-expr))
                       (let ((condition-code (type-member-test-code world type value-type var)))
                         (multiple-value-bind (remaining-code remaining-live remaining-annotated-stmts)
                                              (process-remaining-cases cases remaining-type)
                           (ecase key
                             (:select
                              (multiple-value-bind (statements-codes statements-live statements-annotated-stmts)
                                                   (scan-statements world type-env statements nested-last)
                                (values (cons (cons condition-code statements-codes) remaining-code)
                                        (merge-live-lists statements-live remaining-live)
                                        (cons (list* key type-expr statements-annotated-stmts) remaining-annotated-stmts))))
                             (:narrow
                               (unless (equal var value-code)
                                 (error "const and var cases can only be used when dispatching on a variable"))
                               (multiple-value-bind (statements-codes statements-live statements-annotated-stmts)
                                                    (scan-statements world (type-env-narrow-binding type-env var type) statements nested-last)
                                 (values (cons (cons condition-code statements-codes) remaining-code)
                                         (merge-live-lists statements-live remaining-live)
                                         (cons (list* key type-expr statements-annotated-stmts) remaining-annotated-stmts)))))))))))
               (if (type= remaining-type (world-bottom-type world))
                 (values '((t (case-error))) :dead nil)
                 (error "Type ~A not considered in case" remaining-type)))))
          
          (multiple-value-bind (cases-code cases-live cases-annotated-stmts) (process-remaining-cases cases value-type)
            (multiple-value-bind (rest-codes rest-live rest-annotated-stmts)
                                 (scan-statements world (substitute-live type-env cases-live) rest-statements last)
              (values
               (cons (if (equal var value-code)
                       (cons 'cond cases-code)
                       `(let ((,var ,value-code))
                          (cond ,@cases-code)))
                     rest-codes)
               rest-live
               (cons (list* special-form value-annotated-expr cases-annotated-stmts) rest-annotated-stmts)))))))))


;;; ------------------------------------------------------------------------------------------------------
;;; COMMANDS

; (%highlight <highlight> <command> ... <command>)
; Evaluate the given commands.  <highlight> is a hint for printing.
; If <highlight> is :hide, then the commands are evaluated but not printed.
(defun scan-%highlight (world grammar-info-var highlight &rest commands)
  (declare (ignore highlight))
  (scan-commands world grammar-info-var commands))


; (%... ...)
; Ignore any command that starts with a %.  These commands are hints for printing.
(defun scan-% (world grammar-info-var &rest rest)
  (declare (ignore world grammar-info-var rest)))


; (deftag <name>)
; Create the immutable tag in the world and set its contents.
; Do not evaluate the field and type expressions yet; that will be done by eval-tags-types.
(defun scan-deftag (world grammar-info-var name)
  (declare (ignore grammar-info-var))
  (add-tag world name nil nil :reference nil))


; Create the tuple or record.  Return the type.
(defun scan-deftuple-or-defrecord (world record name fields user-defined)
  (let* ((tag (add-tag world name record fields :reference t))
         (symbol (tag-name tag))
         (type (make-tag-type world tag)))
    (add-type-name world type symbol user-defined)
    type))


; (deftuple <name> (<name1> <type1>) ... (<namen> <typen>))
; Create the immutable tuple and tag in the world and set its contents.
; Do not evaluate the field and type expressions yet; that will be done by eval-tags-types.
(defun scan-deftuple (world grammar-info-var name &rest fields)
  (declare (ignore grammar-info-var))
  (unless fields
    (error "A tuple must have at least one field; use a tag instead"))
  (scan-deftuple-or-defrecord world nil name fields t))


; (defrecord <name> (<name1> <type1> [:const | :var | :opt-const | :opt-var]) ... (<namen> <typen> [:const | :var | :opt-const | :opt-var]))
; Create the mutable record and tag in the world and set its contents.
; Do not evaluate the field and type expressions yet; that will be done by eval-tags-types.
; :const fields are immutable;
; :var fields are mutable;
; :opt-const fields can be left uninitialized but can only be initialized once;
; :opt-var fields are mutable and can be left uninitialized.
(defun scan-defrecord (world grammar-info-var name &rest fields)
  (declare (ignore grammar-info-var))
  (scan-deftuple-or-defrecord world t name fields t))


; (deftype <name> <type>)
; Create the type in the world and set its contents.
(defun scan-deftype (world grammar-info-var name type-expr)
  (declare (ignore grammar-info-var))
  (let* ((symbol (scan-name world name))
         (type (scan-type world type-expr t)))
    (add-type-name world type symbol t)))


; (define <name> <type> <value>)
; (defun <name> (-> (<type1> ... <typen>) <result-type>) (lambda ((<arg1> <type1>) ... (<argn> <typen>)) <result-type> . <statements>))
; Create the constant in the world but do not evaluate its type or value yet.
(defun scan-define (world grammar-info-var name type-expr value-expr)
  (declare (ignore grammar-info-var))
  (let ((symbol (scan-name world name)))
    (unless (eq (get symbol :value-expr *get2-nonce*) *get2-nonce*)
      (error "Attempt to redefine variable ~A" symbol))
    (setf (get symbol :value-expr) value-expr)
    (setf (get symbol :type-expr) type-expr)
    (export-symbol symbol)))


; (defprimitive <name> <lisp-lambda-expr>)
; Overrides a defun of <name> with the result of compiling <lisp-lambda-expr>.
(defun scan-defprimitive (world grammar-info-var name lisp-lambda-expr)
  (declare (ignore grammar-info-var))
  (let ((symbol (scan-name world name)))
    (unless (get symbol :value-expr)
      (error "Need to define ~A before using defprimitive on it" symbol))
    (setf (get symbol :lisp-value-expr) lisp-lambda-expr)))


; (defvar <name> <type> <value>)
; Create the variable in the world but do not evaluate its type or value yet.
(defun scan-defvar (world grammar-info-var name type-expr value-expr)
  (declare (ignore grammar-info-var))
  (let ((symbol (scan-name world name)))
    (unless (eq (get symbol :value-expr *get2-nonce*) *get2-nonce*)
      (error "Attempt to redefine variable ~A" symbol))
    (setf (get symbol :value-expr) value-expr)
    (setf (get symbol :type-expr) type-expr)
    (setf (get symbol :mutable) t)
    (export-symbol symbol)))


; (definfix <type> <markup> <param1> <param2>)
; <type> must be a tuple or record tag.  Define the syntax for depicting its constructor to be the infix operator
; depicted by <markup>.  <param1> and <param2> are used as parameter names for depicting the definfix definition itself.
(defun scan-definfix (world grammar-info-var type-name markup param1 param2)
  (declare (ignore grammar-info-var param1 param2))
  (let* ((symbol (scan-name world type-name))
         (type (get-type symbol nil)))
    (unless (eq (type-kind type) :tag)
      (error "~A should be a tag type" (print-type-to-string type)))
    (let ((tag (type-tag type)))
      (when (tag-appearance tag)
        (error "Duplicate appearance on tag ~S" tag))
      (setf (tag-appearance tag) (cons :infix markup)))))


; (set-grammar <name>)
; Set the current grammar to the grammar or lexer with the given name.
(defun scan-set-grammar (world grammar-info-var name)
  (let ((grammar-info (world-grammar-info world name)))
    (unless grammar-info
      (error "Unknown grammar ~A" name))
    (setf (car grammar-info-var) grammar-info)))


; (clear-grammar)
; Clear the current grammar.
(defun scan-clear-grammar (world grammar-info-var)
  (declare (ignore world))
  (setf (car grammar-info-var) nil))


; Get the grammar-info-var's grammar.  Signal an error if there isn't one.
(defun checked-grammar (grammar-info-var)
  (let ((grammar-info (car grammar-info-var)))
    (if grammar-info
      (grammar-info-grammar grammar-info)
      (error "Grammar needed"))))


; (declare-action <action-name> <general-grammar-symbol> <type> <mode> <parameter-list> <command> ... <command>)
; <mode> is one of:
;    :hide      Don't depict this action declaration because it's for a hidden production;
;    :forward   Depict this action declaration; it forwards to calls to the same action in all nonterminals on the rhs;
;    :singleton Don't depict this action declaration because it contains a singleton production;
;    :action    Depict this action declaration; all corresponding actions will be depicted by depict-action;
;    :actfun    Depict this action declaration; all corresponding actions will be depicted by depict-actfun;
;    :writable  Depict this action declaration but not actions.
; <parameter-list> contains the names of the action parameters when <mode> is :actfun.
(defun scan-declare-action (world grammar-info-var action-name general-grammar-symbol-source type-expr mode parameter-list &rest commands)
  (declare (ignore parameter-list))
  (unless (member mode '(:hide :forward :singleton :action :actfun :writable))
    (error "Bad declare-action mode ~S" mode))
  (let* ((grammar (checked-grammar grammar-info-var))
         (action-symbol (scan-name world action-name))
         (general-grammar-symbol (grammar-parametrization-intern grammar general-grammar-symbol-source)))
    (declare-action grammar general-grammar-symbol action-symbol type-expr)
    (dolist (grammar-symbol (general-grammar-symbol-instances grammar general-grammar-symbol))
      (push (cons (car grammar-info-var) grammar-symbol) (symbol-action action-symbol)))
    (export-symbol action-symbol))
  (scan-commands world grammar-info-var commands))


; (action <action-name> <production-name> <type> <mode> <value>)
; (actfun <action-name> <production-name> <type> <mode> <value>)
; <mode> is one of:
;    :hide      Don't depict this action;
;    :singleton Depict this action along with its declaration;
;    :first     Depict this action, which is the first in the rule;
;    :middle    Depict this action, which is neither the first nor the last in the rule;
;    :last      Depict this action, which is the last in the rule.
(defun scan-action (world grammar-info-var action-name production-name type-expr mode value-expr)
  (unless (member mode '(:hide :singleton :first :middle :last))
    (error "Bad action mode ~S" mode))
  (let ((grammar (checked-grammar grammar-info-var))
        (action-symbol (world-intern world action-name)))
    (define-action grammar production-name action-symbol type-expr value-expr)))


; (terminal-action <action-name> <terminal> <lisp-function>)
(defun scan-terminal-action (world grammar-info-var action-name terminal function)
  (let ((grammar (checked-grammar grammar-info-var))
        (action-symbol (world-intern world action-name)))
    (define-terminal-action grammar terminal action-symbol (symbol-function function))))


;;; ------------------------------------------------------------------------------------------------------
;;; INITIALIZATION

(defparameter *default-specials*
  '((:preprocess
     (? preprocess-?)
     (declare-action preprocess-declare-action)
     (define preprocess-define)
     (action preprocess-action)
     (grammar preprocess-grammar)
     (line-grammar preprocess-line-grammar)
     (lexer preprocess-lexer)
     (grammar-argument preprocess-grammar-argument)
     (production preprocess-production)
     (rule preprocess-rule)
     (exclude preprocess-exclude))
    
    (:command
     (%highlight scan-%highlight depict-%highlight) ;For internal use only; use ? instead.
     (%heading scan-% depict-%heading)
     (%text scan-% depict-%text)
     (grammar-argument scan-% depict-grammar-argument)
     (%rule scan-% depict-%rule)
     (%charclass scan-% depict-%charclass)
     (%print-actions scan-% depict-%print-actions)
     (deftag scan-deftag depict-deftag)
     (deftuple scan-deftuple depict-deftuple)
     (defrecord scan-defrecord depict-deftuple)
     (deftype scan-deftype depict-deftype)
     (define scan-define depict-define)
     (defun scan-define depict-defun)    ;Occurs from desugaring a function define
     (defprimitive scan-defprimitive depict-defprimitive)
     (defvar scan-defvar depict-defvar)
     (definfix scan-definfix depict-definfix)
     (set-grammar scan-set-grammar depict-set-grammar)
     (clear-grammar scan-clear-grammar depict-clear-grammar)
     (declare-action scan-declare-action depict-declare-action)
     (action scan-action depict-action)
     (actfun scan-action depict-actfun)
     (terminal-action scan-terminal-action depict-terminal-action))
    
    (:statement
     (// scan-// depict-//)
     (note scan-// depict-note)
     (/* scan-/* depict-//)
     (*/ scan-*/ depict-*/)
     (bottom scan-bottom depict-bottom)
     (assert scan-assert depict-assert)
     (quiet-assert scan-quiet-assert nil)
     (exec scan-exec depict-exec)
     (const scan-const depict-var)
     (var scan-var depict-var)
     (multiple-value-bind scan-multiple-value-bind depict-multiple-value-bind)
     (reserve scan-reserve nil)
     (function scan-function depict-function)
     (<- scan-<- depict-<-)
     (&= scan-&= depict-&=)
     (&const= scan-&const= depict-&=)
     (action<- scan-action<- depict-action<-)
     (return scan-return depict-return)
     (rwhen scan-rwhen depict-cond)
     (when scan-when depict-cond)
     (if scan-if-stmt depict-cond)
     (cond scan-cond depict-cond)
     (while scan-while depict-while)
     (for-each scan-for-each depict-for-each)
     (throw scan-throw depict-throw)
     (throw-error scan-throw-error nil)
     (catch scan-catch depict-catch)
     (case scan-case depict-case))
    
    (:special-form
     ;;Constants
     (todo scan-todo depict-todo)
     (bottom scan-bottom-expr depict-bottom-expr)
     (hex scan-hex depict-hex)
     (float32 scan-float32 nil)
     (supplementary-char scan-supplementary-char depict-supplementary-char)
     
     ;;Expressions
     (/*/ scan-/*/ depict-/*/)
     (lisp-call scan-lisp-call depict-lisp-call)
     (expt scan-expt depict-expt)
     (= scan-= depict-comparison)
     (/= scan-/= depict-comparison)
     (< scan-< depict-comparison)
     (> scan-> depict-comparison)
     (<= scan-<= depict-comparison)
     (>= scan->= depict-comparison)
     (set<= scan-set<= depict-comparison)
     (cascade scan-cascade depict-cascade)
     (and scan-and depict-and-or-xor)
     (or scan-or depict-and-or-xor)
     (xor scan-xor depict-and-or-xor)
     (lambda scan-lambda depict-lambda)
     (if scan-if-expr depict-if-expr)
     
     ;;Vectors
     (vector scan-vector-expr depict-vector-expr)
     (vector-of scan-vector-of depict-vector-expr)
     (repeat scan-repeat depict-repeat)
     (nth scan-nth depict-nth)
     (subseq scan-subseq depict-subseq)
     (cons scan-cons depict-cons)
     (append scan-append depict-append)
     (set-nth scan-set-nth depict-set-nth)
     
     ;;Sets
     (list-set scan-list-set-expr depict-list-set-expr)
     (%list-set scan-list-set-expr depict-%list-set-expr)
     (list-set-of scan-list-set-of depict-list-set-expr)
     (%list-set-of scan-list-set-of depict-%list-set-expr)
     (range-set-of scan-range-set-of depict-range-set-of-ranges)
     (range-set-of-ranges scan-range-set-of-ranges depict-range-set-of-ranges)
     (set* scan-set* depict-set*)
     (set+ scan-set+ depict-set+)
     (set- scan-set- depict-set-)
     (set-in scan-set-in depict-set-in)
     (set-not-in scan-set-not-in depict-set-in)
     (elt-of scan-elt-of depict-elt-of)
     (unique-elt-of scan-unique-elt-of depict-unique-elt-of)
     
     ;;Vectors or Sets
     (empty scan-empty depict-empty)
     (nonempty scan-nonempty depict-nonempty)
     (length scan-length depict-length)
     (some scan-some depict-some)
     (every scan-every depict-some)
     (map scan-map depict-map)
     
     ;;Tuples and Records
     (new scan-new depict-new)
     (& scan-& depict-&)
     (&opt scan-&opt depict-&)
     (set-field scan-set-field depict-set-field)
     
     ;;Unions
     (in scan-in depict-in)
     (not-in scan-not-in depict-not-in)
     (assert-in scan-assert-in depict-assert-in)
     (assert-not-in scan-assert-not-in depict-assert-in)
     
     ;;Writable Cells
     (writable-cell-of scan-writable-cell-of depict-writable-cell-of) ;For internal use only
     
     ;;Delayed Values
     (delay scan-delay-expr nil)
     (delay-of scan-delay-of-expr nil))
    
    (:condition
     (/*/ scan-/*/-condition)
     (not scan-not-condition)
     (and scan-and-condition)
     (or scan-or-condition)
     (some scan-some-condition)
     (in scan-in-condition)
     (not-in scan-not-in-condition))
    
    (:type-constructor
     (integer-list scan-integer-list depict-integer-list)
     (integer-range scan-integer-range depict-integer-range)
     (exclude-zero scan-exclude-zero depict-exclude-zero)
     (-> scan--> depict-->)
     (vector scan-vector depict-vector)
     (list-set scan-list-set depict-set)
     (range-set scan-range-set depict-set)
     (tag scan-tag-type depict-tag-type)
     (union scan-union depict-union)
     (type-diff scan-type-diff depict-type-diff)
     (writable-cell scan-writable-cell depict-writable-cell)
     (delay scan-delay depict-delay))))


(defparameter *default-non-reserved* '(length))


(defparameter *default-primitives*
  '((neg (-> (integer) integer) #'- :unary :minus nil %prefix% %prefix%)
    (abs (-> (integer) integer) #'abs :unary "|" "|" %primary% %expr%)
    (* (-> (integer integer) integer) #'* :infix :cartesian-product-10 nil %factor% %factor% %factor%)
    (int/ (-> (integer integer) integer) #'int/ :infix "/" nil %factor% %factor% %prefix%)
    (mod (-> (integer integer) integer) #'mod :infix ((:semantic-keyword "mod")) t %factor% %factor% %prefix%)
    (+ (-> (integer integer) integer) #'+ :infix "+" t %term% %term% %term%)
    (- (-> (integer integer) integer) #'- :infix :minus t %term% %term% %factor%)
    
    ;(rational-compare (-> (rational rational) order) #'rational-compare)
    (rat-neg (-> (rational) rational) #'- :unary :minus nil %prefix% %prefix%)
    (rat-abs (-> (rational) rational) #'abs :unary "|" "|" %primary% %expr%)
    (rat* (-> (rational rational) rational) #'* :infix :cartesian-product-10 nil %factor% %factor% %factor%)
    (rat/ (-> (rational rational) rational) #'/ :infix "/" nil %factor% %factor% %prefix%)
    (rat+ (-> (rational rational) rational) #'+ :infix "+" t %term% %term% %term%)
    (rat- (-> (rational rational) rational) #'- :infix :minus t %term% %term% %factor%)
    (floor (-> (rational) integer) #'floor :unary :left-floor-10 :right-floor-10 %primary% %expr%)
    (ceiling (-> (rational) integer) #'ceiling :unary :left-ceiling-10 :right-ceiling-10 %primary% %expr%)
    (floor-log10 (-> (rational) integer) #'floor-log10 :unary (:left-floor-10 "log" (:subscript "10") "(") (")" :right-floor-10) %primary% %expr%)
    
    (not (-> (boolean) boolean) #'not :unary ((:semantic-keyword "not") " ") nil %not% %not%)
    
    (bitwise-and (-> (integer integer) integer) #'logand)
    (bitwise-or (-> (integer integer) integer) #'logior)
    (bitwise-xor (-> (integer integer) integer) #'logxor)
    (bitwise-shift (-> (integer integer) integer) #'ash)
    
    (real-to-float32 (-> (rational) float32) #'rational-to-float32 :unary nil ((:subscript "f32")) %term% %primary%)
    (truncate-finite-float32 (-> (finite-float32) integer) #'truncate-finite-float32)
    
    ;(float32-compare (-> (float32 float32) order) #'float32-compare)
    (float32-abs (-> (float32 float32) float32) #'float32-abs)
    (float32-negate (-> (float32) float32) #'float32-neg)
    (float32-add (-> (float32 float32) float32) #'float32-add)
    (float32-subtract (-> (float32 float32) float32) #'float32-subtract)
    (float32-multiply (-> (float32 float32) float32) #'float32-multiply)
    (float32-divide (-> (float32 float32) float32) #'float32-divide)
    (float32-remainder (-> (float32 float32) float32) #'float32-remainder)
    
    (real-to-float64 (-> (rational) float64) #'rational-to-float64 :unary nil ((:subscript "f64")) %term% %primary%)
    (float32-to-float64 (-> (float32) float64) #'float32-to-float64)
    (truncate-finite-float64 (-> (finite-float64) integer) #'truncate-finite-float64)
    
    ;(float64-compare (-> (float64 float64) order) #'float64-compare)
    (float64-abs (-> (float64 float64) float64) #'float64-abs)
    (float64-negate (-> (float64) float64) #'float64-neg)
    (float64-add (-> (float64 float64) float64) #'float64-add)
    (float64-subtract (-> (float64 float64) float64) #'float64-subtract)
    (float64-multiply (-> (float64 float64) float64) #'float64-multiply)
    (float64-divide (-> (float64 float64) float64) #'float64-divide)
    (float64-remainder (-> (float64 float64) float64) #'float64-remainder)
    
    (integer-to-char16 (-> (integer) char16) #'code-char)
    (char16-to-integer (-> (char16) integer) #'char-code)
    (integer-to-supplementary-char (-> (integer) supplementary-char) #'integer-to-supplementary-char)
    (integer-to-char21 (-> (integer) char21) #'integer-to-char21)
    (char21-to-integer (-> (char21) integer) #'char21-to-integer)
    
    ;(integer-set-min (-> (integer-set) integer) #'integer-set-min :unary ((:semantic-keyword "min") " ") nil %min-max% %prefix%)
    ;(integer-set-max (-> (integer-set) integer) #'integer-set-max :unary ((:semantic-keyword "max") " ") nil %min-max% %prefix%)
    ;(char16-set-min (-> (char16-set) char16) #'char16-set-min :unary ((:semantic-keyword "min") " ") nil %min-max% %prefix%)
    ;(char16-set-max (-> (char16-set) char16) #'char16-set-max :unary ((:semantic-keyword "max") " ") nil %min-max% %prefix%)
    
    (digit-value (-> (char16) integer) #'digit-char-36)))


; \#boolean is a boxed version of boolean.  Use it as a return type of a function that returns boolean if the entire function type
; will be coerced to the type of a function that returns a union including boolean.


;;; Partial order of primitives for deciding when to depict parentheses.
(defparameter *primitive-level* (make-partial-order))
(def-partial-order-element *primitive-level* %primary%)                                          ;id, constant, (e), tag<...>, |e|
(def-partial-order-element *primitive-level* %suffix% %primary%)                                 ;f(...), a[i], a[i...j], a[i<-v], a.l, action
(def-partial-order-element *primitive-level* %prefix% %suffix%)                                  ;-e, new tag<...>, a^b
(def-partial-order-element *primitive-level* %min-max% %prefix%)                                 ;min, max
(def-partial-order-element *primitive-level* %not% %prefix%)                                     ;not
(def-partial-order-element *primitive-level* %factor% %prefix%)                                  ;/, *, intersection, tuple-infix
(def-partial-order-element *primitive-level* %term% %factor%)                                    ;+, -, append, union, set difference
(def-partial-order-element *primitive-level* %relational% %term% %min-max% %not%)                ;<, <=, >, >=, =, /=, is, member, ...
(def-partial-order-element *primitive-level* %logical% %relational%)                             ;and, or, xor
(def-partial-order-element *primitive-level* %expr% %logical%)                                   ;?:, some, every, elt-of, unique-elt-of


; Return the tail end of the lambda list for make-primitive.  The returned list always starts with
; an appearance constant and is followed by additional keywords as appropriate for that appearance.
(defun process-primitive-spec-appearance (name primitive-spec-appearance)
  (if primitive-spec-appearance
    (let ((appearance (first primitive-spec-appearance))
          (args (rest primitive-spec-appearance)))
      (cons
       appearance
       (ecase appearance
         (:global
          (assert-type args (tuple t symbol))
          (list :markup1 (first args) :level (symbol-value (second args))))
         (:infix
          (assert-type args (tuple t bool symbol symbol symbol))
          (list :markup1 (first args) :markup2 (second args) :level (symbol-value (third args))
                :level1 (symbol-value (fourth args)) :level2 (symbol-value (fifth args))))
         (:unary
          (assert-type args (tuple t t symbol symbol))
          (list :markup1 (first args) :markup2 (second args) :level (symbol-value (third args))
                :level1 (symbol-value (fourth args))))
         (:phantom
          (assert-true (null args))
          (list :level %primary%)))))
    (let ((name (symbol-lower-mixed-case-name name)))
      `(:global :markup1 ((:global-variable ,name)) :markup2 ,name :level ,%suffix%))))


; Create a world with the given name and set up the built-in properties of its symbols.
; conditionals is an association list of (conditional . highlight), where conditional is a symbol
; and highlight is either:
;   a style keyword:   Use that style to highlight the contents of any (? conditional ...) commands
;   nil:               Include the contents of any (? conditional ...) commands without highlighting them
;   delete:            Don't include the contents of (? conditional ...) commands
(defun init-world (name conditionals)
  (assert-type conditionals (list (cons symbol (or null keyword (eql delete)))))
  (let ((world (make-world name)))
    (setf (world-conditionals world) conditionals)
    (dolist (specials-list *default-specials*)
      (let ((property (car specials-list)))
        (dolist (special-spec (cdr specials-list))
          (apply #'add-special
            property
            (world-intern world (first special-spec))
            (rest special-spec)))))
    (dolist (non-reserved *default-non-reserved*)
      (let ((symbol (world-intern world non-reserved)))
        (assert (get-properties (symbol-plist symbol) '(:special-form :condition :primitive :type-constructor)))
        (setf (get symbol :non-reserved) t)
        (export-symbol symbol)))
    (dolist (primitive-spec *default-primitives*)
      (let ((name (world-intern world (first primitive-spec))))
        (apply #'declare-primitive
          name
          (second primitive-spec)
          (third primitive-spec)
          (process-primitive-spec-appearance name (cdddr primitive-spec)))))
    
    ;Define simple types
    (add-type-name world
                   (setf (world-false-type world) (make-tag-type world (setf (world-false-tag world) (add-tag world 'false nil nil nil nil))))
                   (world-intern world 'false-type)
                   nil)
    (add-type-name world
                   (setf (world-true-type world) (make-tag-type world (setf (world-true-tag world) (add-tag world 'true nil nil nil nil))))
                   (world-intern world 'true-type)
                   nil)
    (setf (world-denormalized-false-type world) (make-denormalized-tag-type world (world-false-tag world)))
    (setf (world-denormalized-true-type world) (make-denormalized-tag-type world (world-true-tag world)))
    (assert-true (< (type-serial-number (world-false-type world)) (type-serial-number (world-true-type world))))
    (setf (world-boxed-boolean-type world)
          (make-type world :union nil (list (world-false-type world) (world-true-type world)) 'eq nil))
    (add-type-name world (world-boxed-boolean-type world) (world-intern world '\#boolean) nil)
    (flet ((make-simple-type (name kind =-name /=-name)
             (let ((type (make-type world kind nil nil =-name /=-name)))
               (add-type-name world type (world-intern world name) nil)
               type)))
      (setf (world-bottom-type world) (make-simple-type 'bottom-type :bottom nil nil))
      (setf (world-void-type world) (make-simple-type 'void :void nil nil))
      (setf (world-boolean-type world) (make-simple-type 'boolean :boolean 'boolean= nil))
      
      (let ((integer-type (make-simple-type 'integer :integer '= '/=)))
        (setf (world-integer-type world) integer-type)
        (setf (type-order-alist integer-type) '((< . <) (> . >) (<= . <=) (>= . >=)))
        (setf (type-range-set-encode integer-type) 'identity)
        (setf (type-range-set-decode integer-type) 'identity))
      (let ((rational-type (make-simple-type 'rational :rational '= '/=)))
        (setf (world-rational-type world) rational-type)
        (setf (type-order-alist rational-type) '((< . <) (> . >) (<= . <=) (>= . >=))))
      (setf (world-finite32-type world) (make-simple-type 'nonzero-finite-float32 :finite32 '= '/=))
      (setf (world-finite64-type world) (make-simple-type 'nonzero-finite-float64 :finite64 '= '/=))
      (setf (world-finite32-tag world) (make-tag :finite32 nil nil (list (make-field 'value (world-rational-type world) nil nil)) '= nil -1))
      (setf (world-finite64-tag world) (make-tag :finite64 nil nil (list (make-field 'value (world-rational-type world) nil nil)) '= nil -1))
      
      (let* ((char16-type (make-simple-type 'char16 :char16 'char= 'char/=))
             (supplementary-char-type (make-simple-type 'supplementary-char :supplementary-char 'equal nil))
             (char21-type (make-union-type world char16-type supplementary-char-type)))
        (setf (type-order-alist char16-type) '((< . char<) (> . char>) (<= . char<=) (>= . char>=)))
        (setf (type-range-set-encode char16-type) 'char-code)
        (setf (type-range-set-decode char16-type) 'code-char)
        (setf (type-order-alist supplementary-char-type) '((< . char21<) (> . char21>) (<= . char21<=) (>= . char21>=)))
        (setf (type-range-set-encode supplementary-char-type) 'char21-to-integer)
        (setf (type-range-set-decode supplementary-char-type) 'integer-to-char21)
        (setf (type-order-alist char21-type) '((< . char21<) (> . char21>) (<= . char21<=) (>= . char21>=)))
        (setf (type-=-name char21-type) 'equal)
        (setf (type-range-set-encode char21-type) 'char21-to-integer)
        (setf (type-range-set-decode char21-type) 'integer-to-char21)
        (add-type-name world char21-type (world-intern world 'char21) nil)
        (setf (world-char16-type world) char16-type)
        (setf (world-supplementary-char-type world) supplementary-char-type)
        (setf (world-char21-type world) char21-type))
      (let ((string-type (make-type world :string nil (list (world-char16-type world)) 'string= 'string/=)))
        (setf (type-order-alist string-type) '((< . string<) (> . string>) (<= . string<=) (>= . string>=)))
        (add-type-name world string-type (world-intern world 'string) nil)
        (setf (world-string-type world) string-type)))
    
    (add-type-name world (make-range-set-type world (world-integer-type world)) (world-intern world 'integer-set) nil)
    (add-type-name world (make-range-set-type world (world-char16-type world)) (world-intern world 'char16-set) nil)
    (add-type-name world (make-range-set-type world (world-char21-type world)) (world-intern world 'char21-set) nil)
    
    ;Define order, floating-point, and long integer types
    (let (;(order-types (mapcar
          ;              #'(lambda (tag-name)
          ;                  (make-tag-type world (add-tag world tag-name nil nil nil nil)))
          ;              '(less equal greater unordered)))
          (float32-tag-types (mapcar
                              #'(lambda (tag-name)
                                  (make-tag-type world (add-tag world tag-name nil nil nil nil)))
                              '(+zero32 -zero32 +infinity32 -infinity32 nan32)))
          (float64-tag-types (mapcar
                              #'(lambda (tag-name)
                                  (make-tag-type world (add-tag world tag-name nil nil nil nil)))
                              '(+zero64 -zero64 +infinity64 -infinity64 nan64))))
      ;(add-type-name world (apply #'make-union-type world order-types) (world-intern world 'order) nil)
      (let ((float32-type (apply #'make-union-type world (world-finite32-type world) float32-tag-types))
            (float64-type (apply #'make-union-type world (world-finite64-type world) float64-tag-types))
            (finite-float32-type (make-union-type world (world-finite32-type world) (first float32-tag-types) (second float32-tag-types)))
            (finite-float64-type (make-union-type world (world-finite64-type world) (first float64-tag-types) (second float64-tag-types))))
        (add-type-name world float32-type (world-intern world 'float32) nil)
        (add-type-name world float64-type (world-intern world 'float64) nil)
        (add-type-name world finite-float32-type (world-intern world 'finite-float32) nil)
        (add-type-name world finite-float64-type (world-intern world 'finite-float64) nil)
        (let ((long-type (scan-deftuple-or-defrecord world nil 'long '((value (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)))) nil))
              (u-long-type (scan-deftuple-or-defrecord world nil 'u-long '((value (integer-range 0 (- (expt 2 64) 1)))) nil)))
          (setf (tag-appearance (type-tag long-type)) '(:suffix (:subscript (:tag-name "long"))))
          (setf (tag-appearance (type-tag u-long-type)) '(:suffix (:subscript (:tag-name "ulong"))))
          (add-type-name world (make-union-type world float32-type float64-type long-type u-long-type) (world-intern world 'general-number) nil)
          (add-type-name world (make-union-type world finite-float32-type finite-float64-type long-type u-long-type)
                         (world-intern world 'finite-general-number) nil))))
    world))


(defun print-world (world &optional (stream t) (all t))
  (pprint-logical-block (stream nil)
    (labels
      ((default-print-contents (symbol value stream)
         (declare (ignore symbol))
         (write value :stream stream))
       
       (print-symbols-and-contents (property title separator print-contents)
         (let ((symbols (all-world-external-symbols-with-property world property)))
           (when symbols
             (pprint-logical-block (stream symbols)
               (write-string title stream)
               (pprint-indent :block 2 stream)
               (pprint-newline :mandatory stream)
               (loop
                 (let ((symbol (pprint-pop)))
                   (pprint-logical-block (stream nil)
                     (if separator
                       (format stream "~A ~@_~:I~A " symbol separator)
                       (format stream "~A " symbol))
                     (funcall print-contents symbol (get symbol property) stream)))
                 (pprint-exit-if-list-exhausted)
                 (pprint-newline :mandatory stream)))
             (pprint-newline :mandatory stream)
             (pprint-newline :mandatory stream)))))
      
      (when all
        (print-symbols-and-contents
         :preprocess "Preprocessor actions:" "::" #'default-print-contents)
        (print-symbols-and-contents
         :command "Commands:" "::" #'default-print-contents)
        (print-symbols-and-contents
         :statement "Special Forms:" "::" #'default-print-contents)
        (print-symbols-and-contents
         :special-form "Special Forms:" "::" #'default-print-contents)
        (print-symbols-and-contents
         :condition "Conditions:" "::" #'default-print-contents)
        (print-symbols-and-contents
         :primitive "Primitives:" ":"
         #'(lambda (symbol primitive stream)
             (declare (ignore symbol))
             (let ((type (primitive-type primitive)))
               (if type
                 (print-type type stream)
                 (format stream "~@<<<~;~W~;>>~:>" (primitive-type-expr primitive))))
             (format stream " ~_= ~@<<~;~W~;>~:>" (primitive-value-code primitive))))
        (print-symbols-and-contents
         :type-constructor "Type Constructors:" "::" #'default-print-contents))
      
      (print-symbols-and-contents
       :tag "Tags:" "=="
       #'(lambda (symbol tag stream)
           (declare (ignore symbol))
           (print-tag tag stream)))
      (print-symbols-and-contents
       :deftype "Types:" "=="
       #'(lambda (symbol type stream)
           (if type
             (print-type type stream (eq symbol (type-name type)))
             (format stream "<forward-referenced>"))))
      (print-symbols-and-contents
       :value-expr "Values:" ":"
       #'(lambda (symbol value-expr stream)
           (let ((type (symbol-type symbol)))
             (if type
               (print-type type stream)
               (format stream "~@<<<~;~W~;>>~:>" (get symbol :type-expr)))
             (format stream " ~_= ")
             (if (boundp symbol)
               (print-value (symbol-value symbol) type stream)
               (format stream "~@<<<~;~W~;>>~:>" value-expr)))))
      (print-symbols-and-contents
       :action "Actions:" nil
       #'(lambda (action-symbol grammar-info-and-symbols stream)
           (pprint-newline :miser stream)
           (pprint-logical-block (stream (reverse grammar-info-and-symbols))
             (pprint-exit-if-list-exhausted)
             (loop
               (let* ((grammar-info-and-symbol (pprint-pop))
                      (grammar-info (car grammar-info-and-symbol))
                      (grammar (grammar-info-grammar grammar-info))
                      (grammar-symbol (cdr grammar-info-and-symbol)))
                 (write-string ": " stream)
                 (multiple-value-bind (has-type type) (action-declaration grammar grammar-symbol action-symbol)
                   (declare (ignore has-type))
                   (pprint-logical-block (stream nil)
                     (print-type type stream)
                     (format stream " ~_{~S ~S}" (grammar-info-name grammar-info) grammar-symbol))))
               (pprint-exit-if-list-exhausted)
               (pprint-newline :mandatory stream))))))))


(defmethod print-object ((world world) stream)
  (print-unreadable-object (world stream)
    (format stream "world ~A" (world-name world))))


;;; ------------------------------------------------------------------------------------------------------
;;; EVALUATION

; Scan a command.  Create types and variables in the world but do not evaluate variables' types or values yet.
; grammar-info-var is a cons cell whose car is either nil or a grammar-info for the grammar currently being defined.
(defun scan-command (world grammar-info-var command)
  (handler-bind (((or error warning)
                  #'(lambda (condition)
                      (declare (ignore condition))
                      (format *error-output* "~&~@<~2IWhile processing: ~_~:W~:>~%" command))))
    (let ((handler (and (consp command)
                        (identifier? (first command))
                        (get (world-intern world (first command)) :command))))
      (if handler
        (apply handler world grammar-info-var (rest command))
        (error "Bad command")))))


; Scan a list of commands.  See scan-command above.
(defun scan-commands (world grammar-info-var commands)
  (dolist (command commands)
    (scan-command world grammar-info-var command)))


; Compute the primitives' types from their type-exprs.
(defun define-primitives (world)
  (each-world-external-symbol-with-property
   world
   :primitive
   #'(lambda (symbol primitive)
       (declare (ignore symbol))
       (define-primitive world primitive))))


; Compute the types and values of all variables accumulated by scan-command.
(defun eval-variables (world)
  ;Compute the variables' types first.
  (each-world-external-symbol-with-property
   world
   :type-expr
   #'(lambda (symbol type-expr)
       (when (symbol-tag symbol)
         (error "~S is both a tag and a variable" symbol))
       (setf (get symbol :type) (scan-type world type-expr))))
  
  ;Then compute the variables' values.
  (let ((vars nil))
    (each-world-external-symbol-with-property
     world
     :value-expr
     #'(lambda (symbol value-expr)
         (let ((type (symbol-type symbol)))
           (if (eq (type-kind type) :->)
             (compute-variable-function symbol value-expr type)
             (push symbol vars)))))
    (mapc #'compute-variable-value vars)))


; Compute the types of all grammar declarations accumulated by scan-declare-action.
(defun eval-action-declarations (world)
  (dolist (grammar (world-grammars world))
    (each-action-declaration
     grammar
     #'(lambda (grammar-symbol action-declaration)
         (declare (ignore grammar-symbol))
         (setf (cdr action-declaration) (scan-type world (cdr action-declaration)))))))


; Compute the bodies of all grammar actions accumulated by scan-action.
(defun eval-action-definitions (world)
  (dolist (grammar (world-grammars world))
    (maphash
     #'(lambda (terminal action-bindings)
         (dolist (action-binding action-bindings)
           (unless (cdr action-binding)
             (error "Missing action ~S for terminal ~S" (car action-binding) terminal))))
     (grammar-terminal-actions grammar))
    (each-grammar-production
     grammar
     #'(lambda (production)
         (compute-production-code world grammar production)))))


; Evaluate the given commands in the world.
; This method can only be called once.
(defun eval-commands (world commands)
  (defer-mcl-warnings
    (define-primitives world)
    (ensure-proper-form commands)
    (assert-true (null (world-commands-source world)))
    (setf (world-commands-source world) commands)
    (let ((grammar-info-var (list nil)))
      (scan-commands world grammar-info-var commands))
    (unite-types world)
    (eval-tags-types world)
    (eval-action-declarations world)
    (eval-variables world)
    (eval-action-definitions world)))


;;; ------------------------------------------------------------------------------------------------------
;;; PREPROCESSING

(defstruct (preprocessor-state (:constructor make-preprocessor-state (world)))
  (world nil :type world :read-only t)                ;The world into which preprocessed symbols are interned
  (highlight nil :type symbol)                        ;The current highlight style or nil if none
  (kind nil :type (member nil :grammar :lexer))       ;The kind of grammar being accumulated or nil if none
  (kind2 nil :type (member nil :lalr-1 :lr-1 :canonical-lr-1)) ;The kind of parser
  (name nil :type symbol)                             ;Name of the grammar being accumulated or nil if none
  (parametrization nil :type (or null grammar-parametrization)) ;Parametrization of the grammar being accumulated or nil if none
  (start-symbol nil :type symbol)                     ;Start symbol of the grammar being accumulated or nil if none
  (grammar-source-reverse nil :type list)             ;List of productions in the grammar being accumulated (in reverse order)
  (excluded-nonterminals-source nil :type list)       ;List of nonterminals to be excluded from the grammar
  (grammar-options nil :type list)                    ;List of other options for make-grammar
  (charclasses-source nil)                            ;List of charclasses in the lexical grammar being accumulated
  (lexer-actions-source nil)                          ;List of lexer actions in the lexical grammar being accumulated
  (grammar-infos-reverse nil :type list))             ;List of grammar-infos already completed (in reverse order)


; Ensure that the preprocessor-state is accumulating a grammar or a lexer.
(defun preprocess-ensure-grammar (preprocessor-state)
  (unless (preprocessor-state-kind preprocessor-state)
    (error "No active grammar at this point")))


; Finish generating the current grammar-info if one is in progress.
; Return any extra commands needed for this grammar-info.
; The result list can be mutated using nconc.
(defun preprocessor-state-finish-grammar (preprocessor-state)
  (let ((kind (preprocessor-state-kind preprocessor-state)))
    (and kind
         (let ((parametrization (preprocessor-state-parametrization preprocessor-state))
               (start-symbol (preprocessor-state-start-symbol preprocessor-state))
               (grammar-source (nreverse (preprocessor-state-grammar-source-reverse preprocessor-state)))
               (excluded-nonterminals-source (preprocessor-state-excluded-nonterminals-source preprocessor-state))
               (grammar-options (preprocessor-state-grammar-options preprocessor-state))
               (highlights (world-highlights (preprocessor-state-world preprocessor-state))))
           (multiple-value-bind (grammar lexer extra-commands)
                                (ecase kind
                                  (:grammar
                                   (values (apply #'make-and-compile-grammar
                                             (preprocessor-state-kind2 preprocessor-state)
                                             parametrization
                                             start-symbol
                                             grammar-source
                                             :excluded-nonterminals excluded-nonterminals-source
                                             :highlights highlights
                                             grammar-options)
                                           nil
                                           nil))
                                  (:lexer 
                                   (multiple-value-bind (lexer extra-commands)
                                                        (apply #'make-lexer-and-grammar
                                                          (preprocessor-state-kind2 preprocessor-state)
                                                          (preprocessor-state-charclasses-source preprocessor-state)
                                                          (preprocessor-state-lexer-actions-source preprocessor-state)
                                                          parametrization
                                                          start-symbol
                                                          grammar-source
                                                          :excluded-nonterminals excluded-nonterminals-source
                                                          :highlights highlights
                                                          grammar-options)
                                     (values (lexer-grammar lexer) lexer extra-commands))))
             (let ((grammar-info (make-grammar-info (preprocessor-state-name preprocessor-state) grammar lexer)))
               (setf (preprocessor-state-kind preprocessor-state) nil)
               (setf (preprocessor-state-kind2 preprocessor-state) nil)
               (setf (preprocessor-state-name preprocessor-state) nil)
               (setf (preprocessor-state-parametrization preprocessor-state) nil)
               (setf (preprocessor-state-start-symbol preprocessor-state) nil)
               (setf (preprocessor-state-grammar-source-reverse preprocessor-state) nil)
               (setf (preprocessor-state-excluded-nonterminals-source preprocessor-state) nil)
               (setf (preprocessor-state-grammar-options preprocessor-state) nil)
               (setf (preprocessor-state-charclasses-source preprocessor-state) nil)
               (setf (preprocessor-state-lexer-actions-source preprocessor-state) nil)
               (push grammar-info (preprocessor-state-grammar-infos-reverse preprocessor-state))
               (append extra-commands (list '(clear-grammar)))))))))


; Helper function for preprocess-source.
; source is a list of preprocessor directives and commands.  Preprocess these commands
; using the given preprocessor-state and return the resulting list of commands.
(defun preprocess-list (preprocessor-state source)
  (let ((world (preprocessor-state-world preprocessor-state)))
    (flet
      ((preprocess-one (form)
         (when (consp form)
           (let ((first (car form)))
             (when (identifier? first)
               (let ((action (symbol-preprocessor-function (world-intern world first))))
                 (when action
                   (handler-bind (((or error warning)
                                   #'(lambda (condition)
                                       (declare (ignore condition))
                                       (format *error-output* "~&~@<~2IWhile preprocessing: ~_~:W~:>~%" form))))
                     (multiple-value-bind (preprocessed-form re-preprocess) (apply action preprocessor-state form)
                       (return-from preprocess-one
                         (if re-preprocess
                           (preprocess-list preprocessor-state preprocessed-form)
                           preprocessed-form)))))))))
         (list form)))
      
      (mapcan #'preprocess-one source))))


; source is a list of preprocessor directives and commands.  Preprocess these commands
; and return the following results:
;   a list of preprocessed commands;
;   a list of grammar-infos extracted from preprocessor directives.
(defun preprocess-source (world source)
  (let* ((preprocessor-state (make-preprocessor-state world))
         (commands (preprocess-list preprocessor-state source))
         (commands (nconc commands (preprocessor-state-finish-grammar preprocessor-state))))
    (values commands (nreverse (preprocessor-state-grammar-infos-reverse preprocessor-state)))))


; Create a new world with the given name and preprocess and evaluate the given
; source commands in it.
; conditionals is an association list of (conditional . highlight), where conditional is a symbol
; and highlight is either:
;   a style keyword:   Use that style to highlight the contents of any (? conditional ...) commands
;   nil:               Include the contents of any (? conditional ...) commands without highlighting them
;   delete:            Don't include the contents of (? conditional ...) commands
(defun generate-world (name source &optional conditionals)
  (let ((world (init-world name conditionals)))
    (multiple-value-bind (commands grammar-infos) (preprocess-source world source)
      (dolist (grammar-info grammar-infos)
        (clear-actions (grammar-info-grammar grammar-info)))
      (setf (world-grammar-infos world) grammar-infos)
      (eval-commands world commands)
      world)))


;;; ------------------------------------------------------------------------------------------------------
;;; PREPROCESSOR ACTIONS


; (? <conditional> <command> ... <command>)
;   ==>
; (%highlight <highlight> <command> ... <command>)
;   or
; <empty>
(defun preprocess-? (preprocessor-state command conditional &rest commands)
  (declare (ignore command))
  (let ((highlight (resolve-conditional (preprocessor-state-world preprocessor-state) conditional))
        (saved-highlight (preprocessor-state-highlight preprocessor-state)))
    (cond
     ((eq highlight 'delete) (values nil nil))
     ((eq highlight saved-highlight) (values commands t))
     (t (values
         (unwind-protect
           (progn
             (setf (preprocessor-state-highlight preprocessor-state) highlight)
             (list (list* '%highlight highlight (preprocess-list preprocessor-state commands))))
           (setf (preprocessor-state-highlight preprocessor-state) saved-highlight))
         nil)))))


; (declare-action <action-name> <general-grammar-symbol> <type> <mode> <parameter-list> <command> ... <command>)
;   ==>
; (declare-action <action-name> <general-grammar-symbol> <type> <mode> <parameter-list> <command> ... <command>)
(defun preprocess-declare-action (preprocessor-state command action-name general-grammar-symbol-source type-expr mode parameter-list &rest commands)
  (declare (ignore command))
  (values
   (list (list* 'declare-action action-name general-grammar-symbol-source type-expr mode parameter-list
                (preprocess-list preprocessor-state commands)))
   nil))


; commands is a list of commands and/or (? <conditional> ...), where the ... is a list of commands.
; Call f on each non-deleted command, passing it that command and the current value of highlight.
; f returns a list of preprocessed commands; return the destructive concatenation of these lists.
(defun each-preprocessed-command (f preprocessor-state commands highlight)
  (mapcan
   #'(lambda (command)
       (if (and (consp command) (eq (car command) '?))
         (progn
           (assert-type command (cons t cons t (list t)))
           (let* ((commands (cddr command))
                  (new-highlight (resolve-conditional (preprocessor-state-world preprocessor-state) (second command))))
             (cond
              ((eq new-highlight 'delete) nil)
              ((eq new-highlight highlight) (each-preprocessed-command f preprocessor-state commands new-highlight))
              (t (list (list* '? (second command) (each-preprocessed-command f preprocessor-state commands new-highlight)))))))
         (funcall f command highlight)))
   commands))


; (define <name> <type> <value>)
;   ==>
; (define <name> <type> <value>)
;
; (define (<name> (<arg1> <type1>) ... (<argn> <typen>)) <result-type> . <statements>)
;   ==>
; (defun <name> (-> (<type1> ... <typen>) <result-type>)
;    (lambda ((<arg1> <type1>) ... (<argn> <typen>)) <result-type> . <statements>))
(defun preprocess-define (preprocessor-state command name type &rest value-or-statements)
  (declare (ignore command preprocessor-state))
  (values
   (list
    (if (consp name)
      (let ((bindings (rest name)))
        (list 'defun
              (first name)
              (list '-> (mapcar #'second bindings) type)
              (list* 'lambda bindings type value-or-statements)))
      (list* 'define name type value-or-statements)))
   nil))


; (action <action-name> <production-name> <type> <mode> <value>)
;   ==>
; (action <action-name> <production-name> <type> <mode> <value>)
;
; (action (<action-name> (<arg1>) ... (<argn>)) <production-name> (-> (<type1> ... <typen>) <result-type>) <mode> . <statements>)
;   ==>
; (actfun <action-name> <production-name> (-> (<type1> ... <typen>) <result-type>) <mode>
;    (lambda ((<arg1> <type1>) ... (<argn> <typen>)) <result-type> . <statements>))
(defun preprocess-action (preprocessor-state command action-name production-name type mode &rest value-or-statements)
  (declare (ignore command preprocessor-state))
  (values
   (list
    (if (consp action-name)
      (let ((action-name (first action-name))
            (abbreviated-bindings (rest action-name)))
        (unless (and (consp type) (eq (first type) '->))
          (error "Destructuring requires ~S to be a -> type" type))
        (let ((->-parameters (second type))
              (->-result (third type)))
          (unless (= (length ->-parameters) (length abbreviated-bindings))
            (error "Parameter count mistmatch: ~S and ~S" ->-parameters abbreviated-bindings))
          (let ((bindings (mapcar #'(lambda (binding type)
                                      (if (consp binding)
                                        (list* (first binding) type (rest binding))
                                        (list binding type)))
                                  abbreviated-bindings
                                  ->-parameters)))
            (list 'actfun action-name production-name type mode (list* 'lambda bindings ->-result value-or-statements)))))
      (list* 'action action-name production-name type mode value-or-statements)))
   nil))


(defun preprocess-grammar-or-lexer (preprocessor-state kind kind2 name start-symbol &rest grammar-options)
  (assert-type name identifier)
  (let ((commands (preprocessor-state-finish-grammar preprocessor-state)))
    (when (find name (preprocessor-state-grammar-infos-reverse preprocessor-state) :key #'grammar-info-name)
      (error "Duplicate grammar ~S" name))
    (setf (preprocessor-state-kind preprocessor-state) kind)
    (setf (preprocessor-state-kind2 preprocessor-state) kind2)
    (setf (preprocessor-state-name preprocessor-state) name)
    (setf (preprocessor-state-parametrization preprocessor-state) (make-grammar-parametrization))
    (setf (preprocessor-state-start-symbol preprocessor-state) start-symbol)
    (setf (preprocessor-state-grammar-options preprocessor-state) grammar-options)
    (values
     (nconc commands (list (list 'set-grammar name)))
     nil)))


; (grammar <name> <kind> <start-symbol>)
;   ==>
; grammar:
;   Begin accumulating a grammar with the given name and start symbol;
; commands:
;   (set-grammar <name>)
(defun preprocess-grammar (preprocessor-state command name kind2 start-symbol)
  (declare (ignore command))
  (preprocess-grammar-or-lexer preprocessor-state :grammar kind2 name start-symbol))


(defun generate-line-break-constraints (terminal)
  (assert-type terminal user-terminal)
  (list 
   (list terminal :line-break)
   (list (make-lf-terminal terminal) :no-line-break)))


; (line-grammar <name> <kind> <start-symbol>)
;   ==>
; grammar:
;   Begin accumulating a grammar with the given name and start symbol.
;   Allow :no-line-break constraints.
; commands:
;   (set-grammar <name>)
(defun preprocess-line-grammar (preprocessor-state command name kind2 start-symbol)
  (declare (ignore command))
  (preprocess-grammar-or-lexer preprocessor-state :grammar kind2 name start-symbol
                               :variant-constraint-names '(:line-break :no-line-break)
                               :variant-generator #'generate-line-break-constraints))


; (lexer <name> <kind> <start-symbol> <charclasses-source> <lexer-actions-source>)
;   ==>
; grammar:
;   Begin accumulating a lexer with the given name, start symbol, charclasses, and lexer actions;
; commands:
;   (set-grammar <name>)
(defun preprocess-lexer (preprocessor-state command name kind2 start-symbol charclasses-source lexer-actions-source)
  (declare (ignore command))
  (multiple-value-prog1
    (preprocess-grammar-or-lexer preprocessor-state :lexer kind2 name start-symbol)
    (setf (preprocessor-state-charclasses-source preprocessor-state) charclasses-source)
    (setf (preprocessor-state-lexer-actions-source preprocessor-state) lexer-actions-source)))


; (grammar-argument <argument> <attribute> <attribute> ... <attribute>)
;   ==>
; grammar parametrization:
;   (<argument> <attribute> <attribute> ... <attribute>)
; commands:
;   (grammar-argument <argument> <attribute> <attribute> ... <attribute>)
(defun preprocess-grammar-argument (preprocessor-state command argument &rest attributes)
  (preprocess-ensure-grammar preprocessor-state)
  (grammar-parametrization-declare-argument (preprocessor-state-parametrization preprocessor-state) argument attributes)
  (values (list (list* command argument attributes))
          nil))


; (production <lhs> <rhs> <name>)
;   ==>
; grammar:
;   (<lhs> <rhs> <name> <current-highlight>)
; commands:
;   (%rule <lhs>)
(defun preprocess-production (preprocessor-state command lhs rhs name)
  (declare (ignore command))
  (preprocess-ensure-grammar preprocessor-state)
  (push (list lhs rhs name (preprocessor-state-highlight preprocessor-state))
        (preprocessor-state-grammar-source-reverse preprocessor-state))
  (values (list (list '%rule lhs))
          t))


; (rule <general-grammar-symbol>
;       ((<action-name-1> <type-1>) ... (<action-name-n> <type-n>))
;   (production <lhs-1> <rhs-1> <name-1> (<action-spec-1-1> . <body-1-1>) ... (<action-spec-1-n> . <body-1-n>))
;   ...
;   (production <lhs-m> <rhs-m> <name-m> (<action-spec-m-1> . <body-m-1>) ... (<action-spec-m-n> . <body-m-n>)))
;   ==>
; grammar:
;   (<lhs-1> <rhs-1> <name-1> <current-highlight>)
;   ...
;   (<lhs-m> <rhs-m> <name-m> <current-highlight>)
; commands:
;   (%rule <lhs-1>)
;   ...
;   (%rule <lhs-m>)
;   (declare-action <action-name-1> <general-grammar-symbol> <type-1> <mode> <parameter-list>)
;      (action <action-spec-1-1> <name-1> <type-1> <mode> . <body-1-1>)
;      ...
;      (action <action-spec-m-1> <name-m> <type-1> <mode> . <body-m-1>)
;   ...
;   (declare-action <action-name-n> <general-grammar-symbol> <type-n> <mode> <parameter-list>)
;      (action <action-spec-1-n> <name-1> <type-n> <mode> . <body-1-n>)
;      ...
;      (action <action-spec-m-n> <name-m> <type-n> <mode> . <body-m-n>)
;
; The productions may be enclosed by (? <conditional> ...) preprocessor actions.
;
; If one of the <body-x-y> is :forward, then the action must be a function action and the corresponding action's
; <body-z-y> must also be :forward in every other production.  This action expands into a function that calls
; actions with the same name in every nonterminal on the right side of the grammar production, passing them the
; same parameters as the action received.
;
; If one of the <body-x-y> is :forward-result, then the action must be a function action and the corresponding action's
; <body-z-y> must also be :forward-result in every other production.  This action expands into a function that calls
; actions with the same name in every nonterminal on the right side of the grammar production, passing them the
; same parameters as the action received and returns the result.  Each production must have exactly one nonterminal.
(defun preprocess-rule (preprocessor-state command general-grammar-symbol action-declarations &rest productions)
  (declare (ignore command))
  (assert-type action-declarations (list (tuple symbol t)))
  (preprocess-ensure-grammar preprocessor-state)
  (labels
    ((writable-action (action-declaration)
       (let ((type (second action-declaration)))
         (and (consp type)
              (eq (first type) 'writable-cell))))
     
     (actions-match (action-declarations parameter-lists actions)
       (or (and (endp action-declarations) (endp parameter-lists) (endp actions))
           (let ((action-declaration (first action-declarations)))
             (if (writable-action action-declaration)
               (progn
                 (when (eq (first parameter-lists) t)
                   (setf (first parameter-lists) :value))
                 (actions-match (rest action-declarations) (rest parameter-lists) actions))
               (let* ((declared-action-name (first action-declaration))
                      (action (first actions))
                      (action-name (first action))
                      (action-body (rest action))
                      (parameter-list :value))
                 (when (consp action-name)
                   (setq parameter-list (mapcar #'(lambda (arg)
                                                    (if (consp arg)
                                                      (first arg)
                                                      arg))
                                                (rest action-name)))
                   (setq action-name (first action-name))
                   (cond
                    ((equal action-body '(:forward))
                     (setq parameter-list (cons :forward parameter-list)))
                    ((equal action-body '(:forward-result))
                     (setq parameter-list (cons :forward-result parameter-list)))))
                 (when (eq (first parameter-lists) t)
                   (setf (first parameter-lists) parameter-list))
                 (and (eq declared-action-name action-name)
                      (equal (first parameter-lists) parameter-list)
                      (actions-match (rest action-declarations) (rest parameter-lists) (rest actions)))))))))
    
    (let* ((n-productions 0)
           (parameter-lists (make-list (length action-declarations) :initial-element t))
           (commands-reverse
            (nreverse
             (each-preprocessed-command
              #'(lambda (production highlight)
                  (assert-true (eq (first production) 'production))
                  (let ((lhs (second production))
                        (rhs (third production))
                        (name (assert-type (fourth production) symbol))
                        (actions (assert-type (cddddr production) (list (cons t t)))))
                    (unless (actions-match action-declarations parameter-lists actions)
                      (error "Action name or parameter list mismatch: ~S vs. ~S" action-declarations actions))
                    (push (list lhs rhs name highlight) (preprocessor-state-grammar-source-reverse preprocessor-state))
                    (incf n-productions)
                    (list (list '%rule lhs))))
              preprocessor-state
              productions
              (preprocessor-state-highlight preprocessor-state)))))
      (when (= n-productions 0)
        (error "Empty rule"))
      (let ((i 4))
        (dolist (action-declaration action-declarations)
          (let* ((action-name (first action-declaration))
                 (parameter-list (pop parameter-lists))
                 (writable (writable-action action-declaration))
                 (forward-mode (if (and (consp parameter-list) (member (first parameter-list) '(:forward :forward-result)))
                                 (first parameter-list)
                                 nil))
                 (declare-mode (cond
                                (writable :writable)
                                (forward-mode :forward)
                                ((= n-productions 1) :singleton)
                                ((eq parameter-list :value) :action)
                                (t (assert-true (listp parameter-list)) :actfun)))
                 (j 0))
            (when forward-mode
              (setq parameter-list (cdr parameter-list)))
            (push (list*
                   'declare-action action-name general-grammar-symbol (second action-declaration) declare-mode parameter-list
                   (each-preprocessed-command
                    #'(lambda (production highlight)
                        (declare (ignore highlight))
                        (let* ((name (fourth production))
                               (action (cond
                                        (writable
                                         (list action-name (list 'writable-cell-of (second (second action-declaration)))))
                                        ((eq forward-mode :forward)
                                         (let ((forwarded-calls (generate-forwarded-calls action-name (third production) parameter-list)))
                                           (if forwarded-calls
                                             (cons (cons action-name parameter-list) forwarded-calls)
                                             (list (cons action-name (mapcar #'(lambda (parameter) (list parameter :unused)) parameter-list))))))
                                        ((eq forward-mode :forward-result)
                                         (let ((forwarded-calls (generate-forwarded-calls action-name (third production) parameter-list)))
                                           (unless (= (length forwarded-calls) 1)
                                             (error ":forward-result productions must have exactly one nonterminal"))
                                           (list (cons action-name parameter-list) (cons 'return forwarded-calls))))
                                        (t (nth i production))))
                               (mode (cond
                                      ((= n-productions 1) :singleton)
                                      ((= j 0) :first)
                                      ((= j (1- n-productions)) :last)
                                      (t :middle))))
                          (incf j)
                          (list (list* 'action (first action) name (second action-declaration) mode (rest action)))))
                    preprocessor-state
                    productions
                    (preprocessor-state-highlight preprocessor-state)))
                  commands-reverse)
            (assert-true (= j n-productions))
            (unless writable
              (incf i))))
        (values (nreverse commands-reverse) t)))))


(defun generate-forwarded-calls (action-name rhs arguments)
  (let ((counts nil))
    (labels
      ((process-grammar-symbol (general-grammar-symbol)
         (cond
          ((and (keywordp general-grammar-symbol) (not (member general-grammar-symbol '(:- :line-break :no-line-break))))
           (let ((count (incf (getf counts general-grammar-symbol 0))))
             (list (cons (list action-name general-grammar-symbol count) arguments))))
          ((consp general-grammar-symbol)
           (process-grammar-symbol (first general-grammar-symbol)))
          (t nil))))
      (mapcan #'process-grammar-symbol rhs))))


; (exclude <lhs> ... <lhs>)
;   ==>
; grammar excluded nonterminals:
;   <lhs> ... <lhs>;
(defun preprocess-exclude (preprocessor-state command &rest excluded-nonterminals-source)
  (declare (ignore command))
  (preprocess-ensure-grammar preprocessor-state)
  (setf (preprocessor-state-excluded-nonterminals-source preprocessor-state)
        (append excluded-nonterminals-source (preprocessor-state-excluded-nonterminals-source preprocessor-state)))
  (values nil nil))


;;; ------------------------------------------------------------------------------------------------------
;;; DEBUGGING

(defmacro fsource (name)
  `(function-lambda-expression #',name))

(defmacro =source (name)
  `(function-lambda-expression (get ',name :tag=)))


#|
(defun test ()
  (handler-bind ((ccl::undefined-function-reference
                  #'(lambda (condition)
                      (break)
                      (muffle-warning condition))))
    (let ((s1 (gentemp "TEMP"))
          (s2 (gentemp "TEMP")))
      (compile s1 `(lambda (x) (,s2 x y)))
      (compile s2 `(lambda (x) (,s1 x))))))
|#