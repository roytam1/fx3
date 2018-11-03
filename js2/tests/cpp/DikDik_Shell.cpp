// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is The JavaScript 2 Protoype.
 *
 * The Initial Developer of the Original Code is Netscape Communicatins Corp.
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

//
// JS2 shell.
//

#ifdef _WIN32
 // Turn off warnings about identifiers too long in browser information
 #pragma warning(disable: 4786)
#endif

#define EXITCODE_RUNTIME_ERROR 3

#include <algorithm>
#include <assert.h>

#include "world.h"
#include "reader.h"
#include "parser.h"
#include "js2runtime.h"
#include "bytecodegen.h"

#ifdef DEBUG
#include "tracer.h"
#include "collector.h"
#endif

#if defined(XP_MAC) && !defined(XP_MAC_MPW)
#include <SIOUX.h>
#include <MacTypes.h>

static char *mac_argv[] = {"js2", 0};

static void initConsole(StringPtr consoleName,
                        const char* startupMessage,
                        int &argc, char **&argv)
{
    SIOUXSettings.autocloseonquit = false;
    SIOUXSettings.asktosaveonclose = false;
    SIOUXSetTitle(consoleName);

    // Set up a buffer for stderr (otherwise it's a pig).
    static char buffer[BUFSIZ];
    setvbuf(stderr, buffer, _IOLBF, BUFSIZ);

    JavaScript::stdOut << startupMessage;

    argc = 1;
    argv = mac_argv;
}

#endif

using namespace JavaScript::JS2Runtime;


JavaScript::World world;
JavaScript::Arena a;

bool gTraceFlag = false;

namespace JavaScript {
namespace Shell {

// Interactively read a line from the input stream in and put it into
// s. Return false if reached the end of input before reading anything.
static bool promptLine(LineReader &inReader, string &s, const char *prompt)
{
    if (prompt) {
        stdOut << prompt;
      #ifdef XP_MAC_MPW
        // Print a CR after the prompt because MPW grabs the entire
        // line when entering an interactive command.
        stdOut << '\n';
      #endif
    }
    return inReader.readLine(s) != 0;
}


/* "filename" of the console */
const String ConsoleName = widenCString("<console>");
const bool showTokens = false;

#define INTERPRET_INPUT 1
//#define SHOW_ICODE 1


static js2val load(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 argc)
{
    if ((argc >= 1) && (JSValue::isString(argv[0]))) {    
        const String& fileName = *JSValue::string(argv[0]);
        cx->readEvalFile(fileName);
    }    
    return kUndefinedValue;
}

static js2val print(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 argc)
{
    for (uint32 i = 0; i < argc; i++) {
        JSValue::print(stdOut, argv[i]);
        stdOut << "\n";
    }
    return kUndefinedValue;
}

static js2val version(Context * /*cx*/, const js2val /*thisValue*/, js2val /*argv*/[], uint32 /*argc*/)
{
    return JSValue::newNumber(2.0);
}

static js2val debug(Context *cx, const js2val /*thisValue*/, js2val /*argv*/[], uint32 /*argc*/)
{
    cx->mDebugFlag = !cx->mDebugFlag;
    return kUndefinedValue;
}

static js2val trace(Context * /*cx*/, const js2val /*thisValue*/, js2val /*argv*/[], uint32 /*argc*/)
{
    gTraceFlag = true;
    stdOut << "Will report allocation stats\n";
    return kUndefinedValue;
}

static js2val dikdik(Context * /*cx*/, const js2val /*thisValue*/, js2val /*argv*/[], uint32 /*argc*/)
{
    extern void do_dikdik(Formatter &f);
    do_dikdik(stdOut);
    return kUndefinedValue;
}

static js2val quit(Context * /*cx*/, const js2val /*thisValue*/, js2val /*argv*/[], uint32 /*argc*/)
{
// XXX need correct call for other platforms
#ifdef XP_PC
    exit(0);
#endif
    return kUndefinedValue;
}

static int readEvalPrint(Context *cx, FILE *in)
{
    int result = 0;
    String buffer;
    string line;
    LineReader inReader(in);
    while (promptLine(inReader, line, buffer.empty() ? "dd> " : "> ")) {
        appendChars(buffer, line.data(), line.size());
        try {
            Parser p(world, a, cx->mFlags, buffer, ConsoleName);
            cx->setReader(&p.lexer.reader);
            if (showTokens) {
                Lexer &l = p.lexer;
                while (true) {
                    const Token &t = l.get(true);
                    if (t.hasKind(Token::end))
                        break;
                    stdOut << ' ';
                    t.print(stdOut, true);
                }
                stdOut << '\n';
            } else {
                StmtNode *parsedStatements = p.parseProgram();
                ASSERT(p.lexer.peek(true).hasKind(Token::end));
                if (cx->mDebugFlag)
                {
                    PrettyPrinter f(stdOut, 30);
                    {
                        PrettyPrinter::Block b(f, 2);
                        f << "Program =";
                        f.linearBreak(1);
                        StmtNode::printStatements(f, parsedStatements);
                    }
                    f.end();
                    stdOut << '\n';
                }
#ifdef INTERPRET_INPUT
                // Generate code for parsedStatements, which is a linked 
                // list of zero or more statements
                cx->buildRuntime(parsedStatements);
                JS2Runtime::ByteCodeModule* bcm = cx->genCode(parsedStatements, ConsoleName);
                if (bcm) {
#ifdef SHOW_ICODE
                    stdOut << *bcm;
#endif
                    bcm->setSource(buffer, ConsoleName);
                    cx->setReader(NULL);
                    js2val result = cx->interpret(bcm, 0, NULL, JSValue::newObject(cx->getGlobalObject()), NULL, 0);
                    if (!JSValue::isUndefined(result)) {
                        JSValue::print(stdOut, JSValue::toString(cx, result));
                        stdOut << "\n";
                    }
                    delete bcm;
                }
#endif
            }
            clear(buffer);
        } catch (Exception &e) {
            // If we got a syntax error on the end of input, then wait for a continuation
            // of input rather than printing the error message.
            if (!(e.hasKind(Exception::syntaxError) && e.lineNum && e.pos == buffer.size() &&
                  e.sourceFile == ConsoleName)) {
                stdOut << '\n' << e.fullMessage();
                clear(buffer);
                result = EXITCODE_RUNTIME_ERROR;
            }
        }
    }
    stdOut << '\n';
    return result;
}

static bool processArgs(Context *cx, int argc, char **argv, int *result)
{
    bool doInteractive = true;
    for (int i = 0; i < argc; i++)  {    
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            default:
                stdOut << "unrecognized command line switch\n";
                i = argc;
                break;
            case 'f':
                {
                    try {
                        cx->readEvalFile(JavaScript::widenCString(argv[++i]));
                    } catch (Exception &e) {
                        stdOut << '\n' << e.fullMessage();
                        *result = EXITCODE_RUNTIME_ERROR;
                        return false;
                    }
                    doInteractive = false;
                }
                break;
            }
        }
        else {
            if ((argv[i][0] == '/') && (argv[i][1] == '/')) {
                // skip rest of command line
                break;
            }
        }
    }
    return doInteractive;
}

} /* namespace Shell */
} /* namespace JavaScript */


int main(int argc, char **argv)
{
    using namespace JavaScript;
    using namespace Shell;
    
#if defined(XP_MAC) && !defined(XP_MAC_MPW)
    initConsole("\pJavaScript Shell", "Welcome to DikDik.\n", argc, argv);
#else
    stdOut << "Welcome to DikDik.\n";
#endif

#if DEBUG
    testCollector();
#endif

    try {
        JSObject *globalObject;
        Context cx(&globalObject, world, a, Pragma::js2);

        globalObject->defineVariable(&cx, widenCString("load"), (NamespaceList *)(NULL), Property::NoAttribute, NULL, JSValue::newFunction(new JSFunction(&cx, load, NULL)));
        globalObject->defineVariable(&cx, widenCString("print"), (NamespaceList *)(NULL), Property::NoAttribute, NULL, JSValue::newFunction(new JSFunction(&cx, print, NULL)));
        globalObject->defineVariable(&cx, widenCString("debug"), (NamespaceList *)(NULL), Property::NoAttribute, NULL, JSValue::newFunction(new JSFunction(&cx, debug, NULL)));
        globalObject->defineVariable(&cx, widenCString("trace"), (NamespaceList *)(NULL), Property::NoAttribute, NULL, JSValue::newFunction(new JSFunction(&cx, trace, NULL)));
        globalObject->defineVariable(&cx, widenCString("dikdik"), (NamespaceList *)(NULL), Property::NoAttribute, NULL, JSValue::newFunction(new JSFunction(&cx, dikdik, NULL)));
        globalObject->defineVariable(&cx, widenCString("version"), (NamespaceList *)(NULL), Property::NoAttribute, NULL, JSValue::newFunction(new JSFunction(&cx, version, NULL)));
        globalObject->defineVariable(&cx, widenCString("quit"), (NamespaceList *)(NULL), Property::NoAttribute, NULL, JSValue::newFunction(new JSFunction(&cx, quit, NULL)));

        bool doInteractive = true;
        int result = 0;
        if (argc > 1) {
            doInteractive = processArgs(&cx, argc - 1, argv + 1, &result);
        }
        if (doInteractive)
            result = readEvalPrint(&cx, stdin);

        if (gTraceFlag)
            trace_dump(JavaScript::stdOut);

        return result;
    }
    catch (Exception &e) {
        stdOut << '\n' << e.fullMessage();
        return EXITCODE_RUNTIME_ERROR;
    }
}
