/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "ZipFileReader.h"
#include "ErrorHandling.h"

/* Little test program to test ZipFileReader
 * argv[1] - zip archive
 * argv[2] - filename
 * Prints whether the file was found in the archive or not
 */
int main(int, char **argv)
{
  bool status;
  Pool pool;

  ZipArchive archive(argv[1], pool, status);

  if (!status) {
    printf("Could not open archive\n");
    return 1;
  }

  const DirectoryEntry *entry = archive.lookup(argv[2]);

  if (!entry) {
    printf("File Name %s not found in file\n", argv[2]);
    return 1;
  }
  
  try {
    ZipFileReader reader(pool, archive, entry);
  } catch (VerifyError err) {
    printf("Could not open ZipFileReader for file %s: error %d\n", 
	   argv[2], err.cause);
    return 1;
  }

  printf("File %s was found; reader successfully created.\n", argv[2]);
}


#ifdef DEBUG_LAURENT 
#ifdef __GNUC__
// for gcc with -fhandle-exceptions
void terminate() {}
#endif
#endif
