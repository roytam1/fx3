/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
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

package com.netscape.jsdebugging.apitests;

import com.netscape.jsdebugging.apitests.xml.*;
import java.io.*;
import java.util.*;
import java.lang.*;

/**
 * Runs tests and analyzer.
 *
 * @author Alex Rakhlin
 */
  
public class Harness {
    
    public static String start_vm = "cmd /c jre -mx10000000000 -nojit -cp c:/src/ns/js/rhino/;c:/src/ns/js/jsdj/dist/classes/;c:/src/ns/js/jsdj/dist/classes/ifc11.jar;c:/src/ns/js/jsdj/classes/;c:/src/ns/js/jsdj/dist/bin/Debug/;%CLASSPATH% ";

    public static void main(String args[]) {
        
        Tags.init();

        _processArguments (args);

        String whichTests = "com.netscape.jsdebugging.apitests.testing.tests.TestScriptLoading "+
                            "com.netscape.jsdebugging.apitests.testing.tests.TestStepping "+
                            "com.netscape.jsdebugging.apitests.testing.tests.TestEvalInStackFrame "+
                            "com.netscape.jsdebugging.apitests.testing.tests.TestErrorReporter ";
                            
        runTest ("out_local.xml", whichTests, "local", _filenames, "temp_local.xml");
        runTest ("out_rhino.xml", whichTests, "rhino", _filenames, "temp_rhino.xml");
        
        Date d = new Date ();
        String output_dir = d.getMonth()+"."+d.getDay()+"."+d.getYear()+"_"+d.getHours()+"."+d.getMinutes();
        runAnalyzer ("out_local.xml", "out_rhino.xml", "index.html", output_dir);
    }
    
    public static void runAnalyzer (String file1, String file2, String output, String base_dir){
        new File (base_dir).mkdir();
        String command = start_vm+"com.netscape.jsdebugging.apitests.Analyzer ";
        String arguments = " -f1 "+file1+" -f2 "+file2+" -out "+output+" -outdir "+base_dir;
        runProgram (command + arguments);
    }
    
    public static void runTest (String fileout, String whichTests, String engine, String jsfiles, String tempfile){
        String time_stamp = new Date().toString();
        String command = start_vm+"com.netscape.jsdebugging.apitests.Main ";
        String arguments = " -f "+jsfiles+" -o "+tempfile+" -t "+whichTests+" -e "+engine;
        runProgram (command + arguments);
        
        _makeXMLDocument (tempfile, fileout, time_stamp);
    }
    
    /**
     * run a program and wait for it to finish execution.
     */
    public static void runProgram (String command){
        try {
            Runtime rm = java.lang.Runtime.getRuntime();
            Process p = rm.exec (command);            
            p.waitFor ();
        }
        catch (IOException e) { System.out.println ("ERROR EXECUTING PROGRAM "+e.getMessage()); }
        catch (InterruptedException e){ System.out.println ("Interrupted exception"); }   
    }
    
    private static void _makeXMLDocument (String filein, String fileout, String time_stamp){
        _xmlw = new XMLWriter (fileout, 0);
        /* write header */
        _xmlw.prDTDInit();
        _xmlw.startTag (Tags.doc_tag);
        _xmlw.startTag (Tags.header_tag);
        _xmlw.tag (Tags.date_start_tag, time_stamp);
        _xmlw.tag (Tags.date_finish_tag, (new Date()).toString());
        _xmlw.tag (Tags.version_tag, "1.0");
        _xmlw.endTag (Tags.header_tag);
        _xmlw.startTag (Tags.main_tag);
        /* end write header */
        // copy stuff from the generated temp file
        try {
            DataInputStream d = new DataInputStream (new FileInputStream (filein));
            byte[] bytes = new byte[d.available()];
            d.readFully (bytes);
            _xmlw.write (new String (bytes));
        } catch (FileNotFoundException e) { System.out.println ("File not found"); }
          catch (IOException e) { System.out.println ("oops"); }
        /* write footer */
        _xmlw.endTag (Tags.main_tag);
        _xmlw.endTag(Tags.doc_tag);
        _xmlw.close();
        /* end write footer */
    }
    
    
    private static void _processArguments (String args[]){
        _filenames = "";
        for (int i=0; i < args.length; i++) {
            String arg = args[i];
            if (arg.equals("-f")) {
                for (int j=i+1; j < args.length; j++)
                   if (!args[j].startsWith ("-")) _filenames=_filenames+args[j]+" ";
                   else break;
            }
        }
    }
    
    
    private static XMLWriter _xmlw = null;
    private static String _filenames;
}