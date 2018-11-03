;;;
;;; JavaScript 2.0 lexer
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


(progn
  (defparameter *lw*
    (generate-world
     "L"
     '((lexer code-lexer
              :lalr-1
              :$next-input-element
              ((:unicode-character (% every (:text "Any Unicode character")) () t)
               (:unicode-initial-alphabetic
                (% initial-alpha (:text "Any character in category" :space
                                        (:external-name "Lu") " (uppercase letter)," :space
                                        (:external-name "Ll") " (lowercase letter)," :space
                                        (:external-name "Lt") " (titlecase letter)," :space
                                        (:external-name "Lm") " (modifier letter)," :space
                                        (:external-name "Lo") " (other letter), or" :space
                                        (:external-name "Nl") " (letter number)" :space "in the Unicode Character Database"))
                () t)
               (:unicode-alphanumeric
                (% alphanumeric (:text "Any character in category" :space
                                       (:external-name "Lu") " (uppercase letter)," :space
                                       (:external-name "Ll") " (lowercase letter)," :space
                                       (:external-name "Lt") " (titlecase letter)," :space
                                       (:external-name "Lm") " (modifier letter)," :space
                                       (:external-name "Lo") " (other letter)," :space
                                       (:external-name "Nd") " (decimal number)," :space
                                       (:external-name "Nl") " (letter number)," :space
                                       (:external-name "Mn") " (non-spacing mark)," :space
                                       (:external-name "Mc") " (combining spacing mark), or" :space
                                       (:external-name "Pc") " (connector punctuation)" :space "in the Unicode Character Database"))
                () t)
               (:white-space-character (++ (#?0009 #?000B #?000C #\space #?00A0)
                                           (#?2000 #?2001 #?2002 #?2003 #?2004 #?2005 #?2006 #?2007)
                                           (#?2008 #?2009 #?200A #?200B)
                                           (#?3000)) ())
               (:line-terminator (#?000A #?000D #?0085 #?2028 #?2029) ())
               (:non-terminator (- :unicode-character :line-terminator)
                                (($default-action $default-action)))
               (:non-terminator-or-slash (- :non-terminator (#\/)) ())
               (:non-terminator-or-asterisk-or-slash (- :non-terminator (#\* #\/)) ())
               (:white-space-or-line-terminator-char (+ :white-space-character :line-terminator) ())
               (:initial-identifier-character (+ :unicode-initial-alphabetic (#\$ #\_))
                                              (($default-action $default-action)))
               (:continuing-identifier-character (+ :unicode-alphanumeric (#\$ #\_))
                                                 (($default-action $default-action)))
               (:a-s-c-i-i-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                                 (($default-action $default-action)
                                  (decimal-value $digit-value)))
               (:non-zero-digit (#\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                                ((decimal-value $digit-value)))
               (:hex-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9 #\A #\B #\C #\D #\E #\F #\a #\b #\c #\d #\e #\f)
                           ((hex-value $digit-value)))
               (:letter-e (#\E #\e) (($default-action $default-action)))
               (:letter-x (#\X #\x) (($default-action $default-action)))
               (:letter-f (#\F #\f) (($default-action $default-action)))
               (:letter-l (#\L #\l) (($default-action $default-action)))
               (:letter-u (#\U #\u) (($default-action $default-action)))
               ((:literal-string-char single) (- :unicode-character (+ (#\' #\\) :line-terminator))
                (($default-action $default-action)))
               ((:literal-string-char double) (- :unicode-character (+ (#\" #\\) :line-terminator))
                (($default-action $default-action)))
               (:identity-escape (- :non-terminator (+ (#\_) :unicode-alphanumeric))
                                 (($default-action $default-action)))
               (:ordinary-reg-exp-char (- :non-terminator (#\\ #\/))
                                       (($default-action $default-action))))
              (($default-action char16 nil identity)
               ($digit-value integer digit-value digit-char-36)))
       
       (rule :$next-input-element
             ((lex (union input-element extended-rational)))
         (production :$next-input-element ($num (:next-input-element num)) $next-input-element-num
           (lex (lex :next-input-element)))
         (production :$next-input-element ($re (:next-input-element re)) $next-input-element-re
           (lex (lex :next-input-element)))
         (production :$next-input-element ($div (:next-input-element div)) $next-input-element-div
           (lex (lex :next-input-element)))
         (production :$next-input-element ($string-to-number :string-numeric-literal) $next-input-element-string-to-number
           (lex (lex :string-numeric-literal)))
         (production :$next-input-element ($parse-float :string-decimal-literal) $next-input-element-parse-float
           (lex (lex :string-decimal-literal))))
       
       (%text nil "The lexer" :apostrophe "s start symbols are: "
              (:grammar-symbol (:next-input-element num)) " if the previous input element was a number; "
              (:grammar-symbol (:next-input-element re)) " if the previous input element was not a number and a "
              (:character-literal #\/) " should be interpreted as a regular expression; and "
              (:grammar-symbol (:next-input-element div)) " if the previous input element was not a number and a "
              (:character-literal #\/) " should be interpreted as a division or division-assignment operator.")
       (%text nil "In addition to the above, the start symbol " (:grammar-symbol :string-numeric-literal)
              " is used by the syntactic semantics for string-to-number conversions and the start symbol " (:grammar-symbol :string-decimal-literal)
              " is used by the syntactic semantics for implementing the " (:character-literal "parseFloat") " function.")
       
       (deftag line-break)
       (deftag end-of-input)
       
       (deftuple keyword (name string))
       (deftuple punctuator (name string))
       (deftuple identifier (name string))
       (deftuple number-token (value general-number))
       (deftag negated-min-long)
       (deftuple string-token (value string))
       (deftuple regular-expression (body string) (flags string))
       
       (deftype token (union keyword punctuator identifier number-token (tag negated-min-long) string-token regular-expression))
       (deftype input-element (union (tag line-break end-of-input) token))
       
       
       (deftag syntax-error)
       (deftag range-error)
       (deftype semantic-exception (tag syntax-error range-error))
       
       (%heading 2 "Unicode Character Classes")
       (%charclass :unicode-character)
       (%charclass :unicode-initial-alphabetic)
       (%charclass :unicode-alphanumeric)
       (%charclass :white-space-character)
       (%charclass :line-terminator)
       (%charclass :a-s-c-i-i-digit)
       (%print-actions)
       
       (%heading 2 "Comments")
       (production :line-comment (#\/ #\/ :line-comment-characters) line-comment)
       
       (production :line-comment-characters () line-comment-characters-empty)
       (production :line-comment-characters (:line-comment-characters :non-terminator) line-comment-characters-chars)
       
       (%charclass :non-terminator)
       
       (production :single-line-block-comment (#\/ #\* :block-comment-characters #\* #\/) single-line-block-comment)
       
       (production :block-comment-characters () block-comment-characters-empty)
       (production :block-comment-characters (:block-comment-characters :non-terminator-or-slash) block-comment-characters-chars)
       (production :block-comment-characters (:pre-slash-characters #\/) block-comment-characters-slash)
       
       (production :pre-slash-characters () pre-slash-characters-empty)
       (production :pre-slash-characters (:block-comment-characters :non-terminator-or-asterisk-or-slash) pre-slash-characters-chars)
       (production :pre-slash-characters (:pre-slash-characters #\/) pre-slash-characters-slash)
       
       (%charclass :non-terminator-or-slash)
       (%charclass :non-terminator-or-asterisk-or-slash)
       
       (production :multi-line-block-comment (#\/ #\* :multi-line-block-comment-characters :block-comment-characters #\* #\/) multi-line-block-comment)
       
       (production :multi-line-block-comment-characters (:block-comment-characters :line-terminator) multi-line-block-comment-characters-first)
       (production :multi-line-block-comment-characters (:multi-line-block-comment-characters :block-comment-characters :line-terminator)
                   multi-line-block-comment-characters-rest)
       (%print-actions)
       
       (%heading 2 "White Space")
       
       (production :white-space () white-space-empty)
       (production :white-space (:white-space :white-space-character) white-space-character)
       (production :white-space (:white-space :single-line-block-comment) white-space-single-line-block-comment)
       
       (%heading 2 "Line Breaks")
       
       (production :line-break (:line-terminator) line-break-line-terminator)
       (production :line-break (:line-comment :line-terminator) line-break-line-comment)
       (production :line-break (:multi-line-block-comment) line-break-multi-line-block-comment)
       
       (production :line-breaks (:line-break) line-breaks-first)
       (production :line-breaks (:line-breaks :white-space :line-break) line-breaks-rest)
       
       (%heading 2 "Input Elements")
       
       (grammar-argument :nu re div num)
       (grammar-argument :nu_2 re div)
       
       (rule (:next-input-element :nu)
             ((lex input-element))
         (production (:next-input-element re) (:white-space (:input-element re)) next-input-element-re
           (lex (lex :input-element)))
         (production (:next-input-element div) (:white-space (:input-element div)) next-input-element-div
           (lex (lex :input-element)))
         (production (:next-input-element num) ((:- :continuing-identifier-character #\\) :white-space (:input-element div)) next-input-element-num
           (lex (lex :input-element))))
       
       (%print-actions)
       
       (rule (:input-element :nu_2)
             ((lex input-element))
         (production (:input-element :nu_2) (:line-breaks) input-element-line-breaks
           (lex line-break))
         (production (:input-element :nu_2) (:identifier-or-keyword) input-element-identifier-or-keyword
           (lex (lex :identifier-or-keyword)))
         (production (:input-element :nu_2) (:punctuator) input-element-punctuator
           (lex (lex :punctuator)))
         (production (:input-element div) (:division-punctuator) input-element-division-punctuator
           (lex (lex :division-punctuator)))
         (production (:input-element :nu_2) (:numeric-literal) input-element-numeric-literal
           (lex (lex :numeric-literal)))
         (production (:input-element :nu_2) (:string-literal) input-element-string-literal
           (lex (lex :string-literal)))
         (production (:input-element re) (:reg-exp-literal) input-element-reg-exp-literal
           (lex (lex :reg-exp-literal)))
         (production (:input-element :nu_2) (:end-of-input) input-element-end
           (lex end-of-input)))
       
       (production :end-of-input ($end) end-of-input-end)
       (production :end-of-input (:line-comment $end) end-of-input-line-comment)
       (%print-actions)
       
       (%heading 2 "Keywords and Identifiers")
       
       (rule :identifier-or-keyword
             ((lex input-element))
         (production :identifier-or-keyword (:identifier-name) identifier-or-keyword-identifier-name
           (lex (begin
                 (const id string (lex :identifier-name))
                 (if (and (set-in id (list-set "abstract" "as" "break" "case" "catch" "class" "const" "continue" "debugger" "default" "delete" "do" "else" "enum"
                                               "export" "extends" "false" "finally" "for" "function" "get" "goto" "if" "implements" "import" "in"
                                               "instanceof" "interface" "is" "namespace" "native" "new" "null" "package" "private" "protected" "public" "return"
                                               "set" "super" "switch" "synchronized" "this" "throw" "throws" "transient" "true" "try" "typeof" "use"
                                               "var" "volatile" "while" "with"))
                          (not (contains-escapes :identifier-name)))
                   (return (new keyword id))
                   (return (new identifier id)))))))
       (%print-actions)
       
       (rule :identifier-name
             ((lex string) (contains-escapes boolean))
         (production :identifier-name (:initial-identifier-character-or-escape) identifier-name-initial
           (lex (vector (lex :initial-identifier-character-or-escape)))
           (contains-escapes (contains-escapes :initial-identifier-character-or-escape)))
         (production :identifier-name (:null-escapes :initial-identifier-character-or-escape) identifier-name-initial-null-escapes
           (lex (vector (lex :initial-identifier-character-or-escape)))
           (contains-escapes true))
         (production :identifier-name (:identifier-name :continuing-identifier-character-or-escape) identifier-name-continuing
           (lex (append (lex :identifier-name) (vector (lex :continuing-identifier-character-or-escape))))
           (contains-escapes (or (contains-escapes :identifier-name)
                                 (contains-escapes :continuing-identifier-character-or-escape))))
         (production :identifier-name (:identifier-name :null-escape) identifier-name-null-escape
           (lex (lex :identifier-name))
           (contains-escapes true)))
       
       (production :null-escapes (:null-escape) null-escapes-one)
       (production :null-escapes (:null-escapes :null-escape) null-escapes-more)
       
       (production :null-escape (#\\ #\_) null-escape-underscore)
       
       (rule :initial-identifier-character-or-escape
             ((lex char16) (contains-escapes boolean))
         (production :initial-identifier-character-or-escape (:initial-identifier-character) initial-identifier-character-or-escape-ordinary
           (lex ($default-action :initial-identifier-character))
           (contains-escapes false))
         (production :initial-identifier-character-or-escape (#\\ :hex-escape) initial-identifier-character-or-escape-escape
           (lex (begin 
                 (const ch char16 (lex :hex-escape))
                 (if (lisp-call initial-identifier-character? (ch)
                                boolean
                       "the nonterminal " (:grammar-symbol :initial-identifier-character) " can expand into " (:expr string (vector ch)))
                   (return ch)
                   (throw syntax-error))))
           (contains-escapes true)))
       
       (%charclass :initial-identifier-character)
       
       (rule :continuing-identifier-character-or-escape
             ((lex char16) (contains-escapes boolean))
         (production :continuing-identifier-character-or-escape (:continuing-identifier-character) continuing-identifier-character-or-escape-ordinary
           (lex ($default-action :continuing-identifier-character))
           (contains-escapes false))
         (production :continuing-identifier-character-or-escape (#\\ :hex-escape) continuing-identifier-character-or-escape-escape
           (lex (begin
                 (const ch char16 (lex :hex-escape))
                 (if (lisp-call continuing-identifier-character? (ch)
                                boolean
                       "the nonterminal " (:grammar-symbol :continuing-identifier-character) " can expand into " (:expr string (vector ch)))
                   (return ch)
                   (throw syntax-error))))
           (contains-escapes true)))
       
       (%charclass :continuing-identifier-character)
       (%print-actions)
       
       
       (%heading 2 "Punctuators")
       
       (rule :punctuator ((lex token))
         (production :punctuator (#\!) punctuator-not (lex (new punctuator "!")))
         (production :punctuator (#\! #\=) punctuator-not-equal (lex (new punctuator "!=")))
         (production :punctuator (#\! #\= #\=) punctuator-not-identical (lex (new punctuator "!==")))
         (production :punctuator (#\%) punctuator-modulo (lex (new punctuator "%")))
         (production :punctuator (#\% #\=) punctuator-modulo-equals (lex (new punctuator "%=")))
         (production :punctuator (#\&) punctuator-and (lex (new punctuator "&")))
         (production :punctuator (#\& #\&) punctuator-logical-and (lex (new punctuator "&&")))
         (production :punctuator (#\& #\& #\=) punctuator-logical-and-equals (lex (new punctuator "&&=")))
         (production :punctuator (#\& #\=) punctuator-and-equals (lex (new punctuator "&=")))
         (production :punctuator (#\() punctuator-open-parenthesis (lex (new punctuator "(")))
         (production :punctuator (#\)) punctuator-close-parenthesis (lex (new punctuator ")")))
         (production :punctuator (#\*) punctuator-times (lex (new punctuator "*")))
         (production :punctuator (#\* #\=) punctuator-times-equals (lex (new punctuator "*=")))
         (production :punctuator (#\+) punctuator-plus (lex (new punctuator "+")))
         (production :punctuator (#\+ #\+) punctuator-increment (lex (new punctuator "++")))
         (production :punctuator (#\+ #\=) punctuator-plus-equals (lex (new punctuator "+=")))
         (production :punctuator (#\,) punctuator-comma (lex (new punctuator ",")))
         (production :punctuator (#\-) punctuator-minus (lex (new punctuator "-")))
         (production :punctuator (#\- #\-) punctuator-decrement (lex (new punctuator "--")))
         (production :punctuator (#\- #\=) punctuator-minus-equals (lex (new punctuator "-=")))
         (production :punctuator (#\.) punctuator-dot (lex (new punctuator ".")))
         (production :punctuator (#\. #\. #\.) punctuator-triple-dot (lex (new punctuator "...")))
         (production :punctuator (#\:) punctuator-colon (lex (new punctuator ":")))
         (production :punctuator (#\: #\:) punctuator-namespace (lex (new punctuator "::")))
         (production :punctuator (#\;) punctuator-semicolon (lex (new punctuator ";")))
         (production :punctuator (#\<) punctuator-less-than (lex (new punctuator "<")))
         (production :punctuator (#\< #\<) punctuator-left-shift (lex (new punctuator "<<")))
         (production :punctuator (#\< #\< #\=) punctuator-left-shift-equals (lex (new punctuator "<<=")))
         (production :punctuator (#\< #\=) punctuator-less-than-or-equal (lex (new punctuator "<=")))
         (production :punctuator (#\=) punctuator-assignment (lex (new punctuator "=")))
         (production :punctuator (#\= #\=) punctuator-equal (lex (new punctuator "==")))
         (production :punctuator (#\= #\= #\=) punctuator-identical (lex (new punctuator "===")))
         (production :punctuator (#\>) punctuator-greater-than (lex (new punctuator ">")))
         (production :punctuator (#\> #\=) punctuator-greater-than-or-equal (lex (new punctuator ">=")))
         (production :punctuator (#\> #\>) punctuator-right-shift (lex (new punctuator ">>")))
         (production :punctuator (#\> #\> #\=) punctuator-right-shift-equals (lex (new punctuator ">>=")))
         (production :punctuator (#\> #\> #\>) punctuator-logical-right-shift (lex (new punctuator ">>>")))
         (production :punctuator (#\> #\> #\> #\=) punctuator-logical-right-shift-equals (lex (new punctuator ">>>=")))
         (production :punctuator (#\?) punctuator-question (lex (new punctuator "?")))
         (production :punctuator (#\[) punctuator-open-bracket (lex (new punctuator "[")))
         (production :punctuator (#\]) punctuator-close-bracket (lex (new punctuator "]")))
         (production :punctuator (#\^) punctuator-xor (lex (new punctuator "^")))
         (production :punctuator (#\^ #\=) punctuator-xor-equals (lex (new punctuator "^=")))
         (production :punctuator (#\^ #\^) punctuator-logical-xor (lex (new punctuator "^^")))
         (production :punctuator (#\^ #\^ #\=) punctuator-logical-xor-equals (lex (new punctuator "^^=")))
         (production :punctuator (#\{) punctuator-open-brace (lex (new punctuator "{")))
         (production :punctuator (#\|) punctuator-or (lex (new punctuator "|")))
         (production :punctuator (#\| #\=) punctuator-or-equals (lex (new punctuator "|=")))
         (production :punctuator (#\| #\|) punctuator-logical-or (lex (new punctuator "||")))
         (production :punctuator (#\| #\| #\=) punctuator-logical-or-equals (lex (new punctuator "||=")))
         (production :punctuator (#\}) punctuator-close-brace (lex (new punctuator "}")))
         (production :punctuator (#\~) punctuator-complement (lex (new punctuator "~"))))
       
       (rule :division-punctuator ((lex token))
         (production :division-punctuator (#\/ (:- #\/ #\*)) punctuator-divide (lex (new punctuator "/")))
         (production :division-punctuator (#\/ #\=) punctuator-divide-equals (lex (new punctuator "/="))))
       (%print-actions)
       
       (%heading 2 "Numeric Literals")
       
       (rule :numeric-literal ((lex token))
         (production :numeric-literal ((:decimal-literal no-leading-zeros)) numeric-literal-decimal
           (lex (new number-token (real-to-float64 (lex :decimal-literal)))))
         (production :numeric-literal (:hex-integer-literal) numeric-literal-hex
           (lex (new number-token (real-to-float64 (lex :hex-integer-literal)))))
         (production :numeric-literal ((:decimal-literal no-leading-zeros) :letter-f) numeric-literal-single
           (lex (new number-token (real-to-float32 (lex :decimal-literal)))))
         (production :numeric-literal (:integer-literal :letter-l) numeric-literal-long
           (lex (begin
                 (const i integer (lex :integer-literal))
                 (cond
                  ((<= i (- (expt 2 63) 1)) (return (new number-token (new long i))))
                  ((= i (expt 2 63)) (return negated-min-long))
                  (nil (throw range-error))))))
         (production :numeric-literal (:integer-literal :letter-u :letter-l) numeric-literal-unsigned-long
           (lex (begin
                 (const i integer (lex :integer-literal))
                 (if (<= i (- (expt 2 64) 1))
                   (return (new number-token (new u-long i)))
                   (throw range-error))))))
       
       (rule :integer-literal ((lex integer))
         (production :integer-literal ((:decimal-integer-literal no-leading-zeros)) integer-literal-decimal
           (lex (lex :decimal-integer-literal)))
         (production :integer-literal (:hex-integer-literal) integer-literal-hex
           (lex (lex :hex-integer-literal))))
       (%charclass :letter-f)
       (%charclass :letter-l)
       (%charclass :letter-u)
       (%print-actions)
       
       (grammar-argument :zeta no-leading-zeros allow-leading-zeros)
       
       (rule (:decimal-literal :zeta) ((lex rational))
         (production (:decimal-literal :zeta) ((:mantissa :zeta)) decimal-literal
           (lex (lex :mantissa)))
         (production (:decimal-literal :zeta) ((:mantissa :zeta) :letter-e :signed-integer) decimal-literal-exponent
           (lex (rat* (lex :mantissa) (expt 10 (lex :signed-integer))))))
       
       (%charclass :letter-e)
       
       (rule (:mantissa :zeta) ((lex rational))
         (production (:mantissa :zeta) ((:decimal-integer-literal :zeta)) mantissa-integer
           (lex (lex :decimal-integer-literal)))
         (production (:mantissa :zeta) ((:decimal-integer-literal :zeta) #\.) mantissa-integer-dot
           (lex (lex :decimal-integer-literal)))
         (production (:mantissa :zeta) ((:decimal-integer-literal :zeta) #\. :fraction) mantissa-integer-dot-fraction
           (lex (rat+ (lex :decimal-integer-literal) (lex :fraction))))
         (production (:mantissa :zeta) (#\. :fraction) mantissa-dot-fraction
           (lex (lex :fraction))))
       
       (rule (:decimal-integer-literal :zeta) ((lex integer))
         (production (:decimal-integer-literal no-leading-zeros) (#\0) decimal-integer-literal-0
           (lex 0))
         (production (:decimal-integer-literal no-leading-zeros) (:non-zero-decimal-digits) decimal-integer-literal-nonzero
           (lex (lex :non-zero-decimal-digits)))
         (production (:decimal-integer-literal allow-leading-zeros) (:decimal-digits) decimal-integer-literal-decimal-digits
           (lex (lex :decimal-digits))))
       
       (rule :non-zero-decimal-digits ((lex integer))
         (production :non-zero-decimal-digits (:non-zero-digit) non-zero-decimal-digits-first
           (lex (decimal-value :non-zero-digit)))
         (production :non-zero-decimal-digits (:non-zero-decimal-digits :a-s-c-i-i-digit) non-zero-decimal-digits-rest
           (lex (+ (* 10 (lex :non-zero-decimal-digits)) (decimal-value :a-s-c-i-i-digit)))))
       
       (%charclass :non-zero-digit)
       
       (rule :fraction ((lex rational))
         (production :fraction (:decimal-digits) fraction-decimal-digits
           (lex (rat/ (lex :decimal-digits)
                      (expt 10 (n-digits :decimal-digits))))))
       (%print-actions)
       
       (rule :signed-integer ((lex integer))
         (production :signed-integer (:optional-sign :decimal-digits) signed-integer-sign-and-digits
           (lex (* (lex :optional-sign) (lex :decimal-digits)))))
       
       (rule :optional-sign ((lex (integer-list -1 1)))
         (production :optional-sign () optional-sign-none
           (lex 1))
         (production :optional-sign (#\+) optional-sign-plus
           (lex 1))
         (production :optional-sign (#\-) optional-sign-minus
           (lex -1)))
       (%print-actions)
       
       (rule :decimal-digits
             ((lex integer) (n-digits integer))
         (production :decimal-digits (:a-s-c-i-i-digit) decimal-digits-first
           (lex (decimal-value :a-s-c-i-i-digit))
           (n-digits 1))
         (production :decimal-digits (:decimal-digits :a-s-c-i-i-digit) decimal-digits-rest
           (lex (+ (* 10 (lex :decimal-digits)) (decimal-value :a-s-c-i-i-digit)))
           (n-digits (+ (n-digits :decimal-digits) 1))))
       (%print-actions)
       
       (rule :hex-integer-literal ((lex integer))
         (production :hex-integer-literal (#\0 :letter-x :hex-digit) hex-integer-literal-first
           (lex (hex-value :hex-digit)))
         (production :hex-integer-literal (:hex-integer-literal :hex-digit) hex-integer-literal-rest
           (lex (+ (* 16 (lex :hex-integer-literal)) (hex-value :hex-digit)))))
       (%charclass :letter-x)
       (%charclass :hex-digit)
       (%print-actions)
       
       (%heading 2 "String Literals")
       
       (grammar-argument :theta single double)
       (rule :string-literal ((lex token))
         (production :string-literal (#\' (:string-chars single) #\') string-literal-single
           (lex (new string-token (lex :string-chars))))
         (production :string-literal (#\" (:string-chars double) #\") string-literal-double
           (lex (new string-token (lex :string-chars)))))
       (%print-actions)
       
       (rule (:string-chars :theta) ((lex string))
         (production (:string-chars :theta) () string-chars-none
           (lex ""))
         (production (:string-chars :theta) ((:string-chars :theta) (:string-char :theta)) string-chars-some
           (lex (append (lex :string-chars)
                        (vector (lex :string-char)))))
         (production (:string-chars :theta) ((:string-chars :theta) :null-escape) string-chars-null-escape
           (lex (lex :string-chars))))
       
       (rule (:string-char :theta) ((lex char16))
         (production (:string-char :theta) ((:literal-string-char :theta)) string-char-literal
           (lex ($default-action :literal-string-char)))
         (production (:string-char :theta) (#\\ :string-escape) string-char-escape
           (lex (lex :string-escape))))
       
       (%charclass (:literal-string-char single))
       (%charclass (:literal-string-char double))
       (%print-actions)
       
       (rule :string-escape ((lex char16))
         (production :string-escape (:control-escape) string-escape-control
           (lex (lex :control-escape)))
         (production :string-escape (:zero-escape) string-escape-zero
           (lex (lex :zero-escape)))
         (production :string-escape (:hex-escape) string-escape-hex
           (lex (lex :hex-escape)))
         (production :string-escape (:identity-escape) string-escape-non-escape
           (lex ($default-action :identity-escape))))
       (%charclass :identity-escape)
       (%print-actions)
       
       (rule :control-escape ((lex char16))
         (production :control-escape (#\b) control-escape-backspace (lex #?0008))
         (production :control-escape (#\f) control-escape-form-feed (lex #?000C))
         (production :control-escape (#\n) control-escape-new-line (lex #?000A))
         (production :control-escape (#\r) control-escape-return (lex #?000D))
         (production :control-escape (#\t) control-escape-tab (lex #?0009))
         (production :control-escape (#\v) control-escape-vertical-tab (lex #?000B)))
       (%print-actions)
       
       (rule :zero-escape ((lex char16))
         (production :zero-escape (#\0 (:- :a-s-c-i-i-digit)) zero-escape-zero
           (lex #?0000)))
       (%print-actions)
       
       (rule :hex-escape ((lex char16))
         (production :hex-escape (#\x :hex-digit :hex-digit) hex-escape-2
           (lex (integer-to-char16 (+ (* 16 (hex-value :hex-digit 1))
                                      (hex-value :hex-digit 2)))))
         (production :hex-escape (#\u :hex-digit :hex-digit :hex-digit :hex-digit) hex-escape-4
           (lex (integer-to-char16 (+ (+ (+ (* 4096 (hex-value :hex-digit 1))
                                            (* 256 (hex-value :hex-digit 2)))
                                         (* 16 (hex-value :hex-digit 3)))
                                      (hex-value :hex-digit 4)))))
         (production :hex-escape (#\U :hex-digit :hex-digit :hex-digit :hex-digit :hex-digit :hex-digit :hex-digit :hex-digit) hex-escape-8
           (lex (todo))))
       
       (%print-actions)
       
       (%heading 2 "Regular Expression Literals")
       
       (rule :reg-exp-literal ((lex token))
         (production :reg-exp-literal (:reg-exp-body :reg-exp-flags) reg-exp-literal
           (lex (new regular-expression (lex :reg-exp-body) (lex :reg-exp-flags)))))
       
       (rule :reg-exp-flags ((lex string))
         (production :reg-exp-flags () reg-exp-flags-none
           (lex ""))
         (production :reg-exp-flags (:reg-exp-flags :continuing-identifier-character-or-escape) reg-exp-flags-more
           (lex (append (lex :reg-exp-flags) (vector (lex :continuing-identifier-character-or-escape)))))
         (production :reg-exp-flags (:reg-exp-flags :null-escape) reg-exp-flags-null-escape
           (lex (lex :reg-exp-flags))))
       
       (rule :reg-exp-body ((lex string))
         (production :reg-exp-body (#\/ (:- #\*) :reg-exp-chars #\/) reg-exp-body
           (lex (lex :reg-exp-chars))))
       
       (rule :reg-exp-chars ((lex string))
         (production :reg-exp-chars (:reg-exp-char) reg-exp-chars-one
           (lex (lex :reg-exp-char)))
         (production :reg-exp-chars (:reg-exp-chars :reg-exp-char) reg-exp-chars-more
           (lex (append (lex :reg-exp-chars) (lex :reg-exp-char)))))
       
       (rule :reg-exp-char ((lex string))
         (production :reg-exp-char (:ordinary-reg-exp-char) reg-exp-char-ordinary
           (lex (vector ($default-action :ordinary-reg-exp-char))))
         (production :reg-exp-char (#\\ :non-terminator) reg-exp-char-escape
           (lex (vector #\\ ($default-action :non-terminator)))))
       
       (%charclass :ordinary-reg-exp-char)
       (%print-actions)
       
       (%heading 1 "String-to-Number Conversion")
       
       (rule :string-numeric-literal ((lex extended-rational))
         (production :string-numeric-literal (:string-white-space) string-numeric-literal-white-space
           (lex +zero))
         (production :string-numeric-literal (:string-white-space :signed-decimal-literal :string-white-space) string-numeric-literal-signed-decimal-literal
           (lex (lex :signed-decimal-literal)))
         (production :string-numeric-literal (:string-white-space :optional-sign :hex-integer-literal :string-white-space) string-numeric-literal-hex-integer-literal
           (lex (combine-with-sign (lex :optional-sign) (lex :hex-integer-literal)))))
       
       (rule :signed-decimal-literal ((lex extended-rational))
         (production :signed-decimal-literal (:optional-sign (:decimal-literal allow-leading-zeros)) signed-decimal-literal-decimal-literal
           (lex (combine-with-sign (lex :optional-sign) (lex :decimal-literal))))
         (production :signed-decimal-literal (:optional-sign #\I #\n #\f #\i #\n #\i #\t #\y) signed-decimal-literal-infinity
           (lex (if (> (lex :optional-sign) 0) +infinity -infinity)))
         (production :signed-decimal-literal (#\N #\a #\N) signed-decimal-literal-nan
           (lex nan)))
       
       (production :string-white-space () string-white-space-empty)
       (production :string-white-space (:string-white-space :white-space-or-line-terminator-char) string-white-space-character)
       (%charclass :white-space-or-line-terminator-char)
       
       (deftag +zero)
       (deftag -zero)
       (deftag +infinity)
       (deftag -infinity)
       (deftag nan)
       (deftype extended-rational (union rational (tag +zero -zero +infinity -infinity nan)))
       (%print-actions)
       
       (define (combine-with-sign (sign (integer-list -1 1)) (q rational)) extended-rational
         (cond
          ((/= q 0 rational) (return (rat* sign q)))
          ((> sign 0) (return +zero))
          (nil (return -zero))))
       
       (%heading 2 "parseFloat Conversion")
       
       (rule :string-decimal-literal ((lex extended-rational))
         (production :string-decimal-literal (:string-white-space :signed-decimal-literal) string-decimal-literal-signed-decimal-literal
           (lex (lex :signed-decimal-literal))))
       (%print-actions)
       )))
  
  (defparameter *ll* (world-lexer *lw* 'code-lexer))
  (defparameter *lg* (lexer-grammar *ll*))
  (set-up-lexer-metagrammar *ll*)
  (defparameter *lm* (lexer-metagrammar *ll*)))


; Convert the string to an extended-rational if the entire string is an expansion of the :string-numeric-literal nonterminal.
(defun string-to-extended-rational (string)
  (handler-case
    (first (action-parse *lg* (lexer-classifier *ll*) (cons '$string-to-number (coerce string 'list))))
    (syntax-error () :syntax-error)))


; Convert the string to an extended-rational if a prefix of the string is an expansion of the :string-decimal-literal nonterminal.
; Pick the longest prefix that works.  No indication is given of any ignored trailing characters.
(defun string-prefix-to-float (string)
  (handler-case
    (first (action-metaparse *lm* (lexer-classifier *ll*) (cons '$parse-float (coerce string 'list))))
    (syntax-error () :syntax-error)))


(defun dump-lexer ()
  (values
   (depict-rtf-to-local-file
    "JS20/LexerGrammar.rtf"
    "JavaScript 2 Lexical Grammar"
    #'(lambda (rtf-stream)
        (depict-world-commands rtf-stream *lw* :heading-offset 1 :visible-semantics nil)))
   (depict-rtf-to-local-file
    "JS20/LexerSemantics.rtf"
    "JavaScript 2 Lexical Semantics"
    #'(lambda (rtf-stream)
        (depict-world-commands rtf-stream *lw* :heading-offset 1)))
   (depict-html-to-local-file
    "JS20/LexerGrammar.html"
    "JavaScript 2 Lexical Grammar"
    t
    #'(lambda (rtf-stream)
        (depict-world-commands rtf-stream *lw* :heading-offset 1 :visible-semantics nil))
    :external-link-base "notation.html")
   (depict-html-to-local-file
    "JS20/LexerSemantics.html"
    "JavaScript 2 Lexical Semantics"
    t
    #'(lambda (rtf-stream)
        (depict-world-commands rtf-stream *lw* :heading-offset 1))
    :external-link-base "notation.html")))


#|
(dump-lexer)

(depict-rtf-to-local-file
 "JS20/LexerCharClasses.rtf"
 "JavaScript 2 Lexical Character Classes"
 #'(lambda (rtf-stream)
     (depict-paragraph (rtf-stream :grammar-header)
       (depict rtf-stream "Character Classes"))
     (dolist (charclass (lexer-charclasses *ll*))
       (depict-charclass rtf-stream charclass))
     (depict-paragraph (rtf-stream :grammar-header)
       (depict rtf-stream "Grammar"))
     (depict-grammar rtf-stream *lg*)))

(with-local-output (s "JS20/LexerGrammar.txt") (print-lexer *ll* s) (print-grammar *lg* s))

(print-illegal-strings m)
|#


#+allegro (clean-grammar *lg*) ;Remove this line if you wish to print the grammar's state tables.
(length (grammar-states *lg*))
