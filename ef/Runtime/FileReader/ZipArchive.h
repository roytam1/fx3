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
#ifndef _ZIP_ARCHIVE_H_
#define _ZIP_ARCHIVE_H_

#include "Fundamentals.h"
#include "prio.h"
#include "plstr.h"
#include "Tree.h"

/*
 * Central zip directory entry
 */

class DirectoryEntry_Key {
private:
  const char * const str;

public:
  explicit DirectoryEntry_Key(const char *s) : str(s) { }
  DirectoryEntry_Key(const DirectoryEntry_Key &other) : str(other.str) { }

  bool operator== (const DirectoryEntry_Key &other) const {
    return PL_strcasecmp(str, other.str) == 0;
  }

  bool operator< (const DirectoryEntry_Key &other) const {
    return PL_strcasecmp(str, other.str) < 0;
  }
};

struct DirectoryEntry : public TreeNode<DirectoryEntry> {
  const char *fn;               // file name
  Uint32 len;                   // file size
  Uint32 size;                  // file compressed size
  Int32 method;                 // compression method
  Int32 mod;                    // file modification time
  Int32 off;                    // file contents offset

  // Bits to interface with the tree code.
  DirectoryEntry_Key getKey() const {
    return DirectoryEntry_Key(fn);
  }
};


/*
 * An object that represents a zip archive.
 */

class ZipArchive {
public:
  // Create a zip archive reader from a zip archive whose
  // canonical pathname is archiveName. Pool is used to allocate
  // memory for data structures. Sets status to true if it was
  // able to successfully open the archive; returns false if
  // it could not.
  ZipArchive(const char *archiveName, Pool &pool, bool &status);

  ~ZipArchive();

  // Read a file given by fileName from the zip archive into
  // buf, allocating as much memory as necessary. This memory
  // is allocated using the pool passed into the constructor
  // and will go away when the pool is destroyed. On success,
  // returns true and sets len to the length of the file read.
  // Returns false if the file was not found in the archive, or
  // if there was an error.
  bool get(const char *fileName, char *&buf, Int32 &len) {
    const DirectoryEntry *entry = lookup(fileName);

    if (!entry)
      return false;

    return get(entry, buf, len);
  }
  
  // Exactly like get() above, but works with a directory entry
  // obtained via lookup().
  bool get(const DirectoryEntry *entry, char *&buf, Int32 &len);

  // Returns true if the given file exists in the archive, false
  // otherwise.
  bool exists(const char *fileName) {
    return (lookup(fileName) != 0);
  }

  // Lookup a file and return it's directory entry if it exists.
  // If not, return false.
  const DirectoryEntry *lookup(const char *fileName) {
    return dir.find(DirectoryEntry_Key(fileName));
  }

  // Return number of elements in the zip archive whose filenames
  // have the suffix indicated by fileNameSuffix.
  Uint32 getNumElements(const char *fileNameSuffix);
  
  // Gets the names of all elements in the zip archive whose filenames
  // have the suffix indicated by fileNameSuffix. Returns the number
  // of matching elements. Memory for buf is allocated using the
  // pool passed into the constructor and is destroyed when the
  // pool is destroyed.
  Uint32 listElements(const char *fileNameSuffix, char **&buf);

private:
  Pool &pool;                   // Pool used to allocate internal memory
  PRFileDesc *fp;               // File descriptor
  SortedTree<DirectoryEntry,DirectoryEntry_Key> dir; // zip file directory
  Uint32 nel;                   // number of directory entries
  Uint32 cenoff;                // Offset of central directory (CEN)
  Uint32 endoff;                // Offset of end-of-central-directory record

  bool initReader();
  bool findEnd();
  bool readFully(void *buf, Int32 len);
  bool inflateFully(Uint32 size, void *buf, Uint32 len);
};		      

#endif /* _ZIP_ARCHIVE_H_ */
