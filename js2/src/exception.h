/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef exception_h___
#define exception_h___

namespace JavaScript
{

//
// Exceptions
//

    // A JavaScript exception (other than out-of-memory, for which we use the
    // standard C++ exception bad_alloc).
    struct Exception {
        enum Kind {
            syntaxError,
            stackOverflow,
            internalError,
            runtimeError,
            referenceError,
            rangeError,
            typeError,
            uncaughtError,
            semanticError,
            userException,
            definitionError,
            badValueError,
            compileExpressionError,
            propertyAccessError,
            uninitializedError,
            argumentMismatchError,
            attributeError,
            constantError,
            argumentsError
        };
        
        Kind kind;              // The exception's kind
        String message;         // The detailed message
        String sourceFile;      // A description of the source code that caused the error
        uint32 lineNum;         // Number of line that caused the error
        size_t charNum;         // Character offset within the line that caused the error
        size_t pos;             // Offset within the input of the error
        String sourceLine;      // The text of the source line
        js2val value;           // The value for a user exception

        Exception (Kind kind, const char *message):
                kind(kind), message(widenCString(message)), lineNum(0), charNum(0) {}
        
        Exception (Kind kind, const String &message):
                kind(kind), message(message), lineNum(0), charNum(0) {}
        
        Exception(Kind kind, const String &message, const String &sourceFile, uint32 lineNum, size_t charNum,
                  size_t pos, const String &sourceLine):
                kind(kind), message(message), sourceFile(sourceFile), lineNum(lineNum), charNum(charNum), pos(pos),
                sourceLine(sourceLine) {}
        
        Exception(Kind kind, const String &message, const String &sourceFile, uint32 lineNum, size_t charNum,
                  size_t pos, const char16 *sourceLineBegin, const char16 *sourceLineEnd):
                kind(kind), message(message), sourceFile(sourceFile), lineNum(lineNum), charNum(charNum), pos(pos),
                sourceLine(sourceLineBegin, sourceLineEnd) {}

        Exception(js2val v) : kind(userException), lineNum(0), charNum(0), value(v) {}

        bool hasKind(Kind k) const {return kind == k;}
        const char *kindString() const;
        String fullMessage() const;
    };

    struct JS2UserException : public Exception {

        JS2UserException(js2val x) : Exception(userException, "User Exception"), userValue(x) { }
        js2val userValue;
    };


    // Throw a stackOverflow exception if the execution stack has gotten too large.
    inline void checkStackSize() {}
}

#endif /* exception_h___ */
