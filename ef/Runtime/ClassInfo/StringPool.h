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
#ifndef _STRING_POOL_H_
#define _STRING_POOL_H_

#include "HashTable.h"
#include "JavaString.h"

class StrNode {
public:
  JavaString *str;  /* The string object corresponding to this char */
};

class StringPool : private HashTable<StrNode> {
public:
  StringPool(Pool &p) : HashTable<StrNode>(p) { }
  
  /* Intern "str" if not already interned and return a pointer to
   * the interned string
   */
  const char *intern(const char *str) {
    int hashIndex;
    const char *key;
    
    node.str = 0;

    if ((key = HashTable<StrNode>::get(str, &node, hashIndex)) != 0)
      return key;
    
    return add(str, node, hashIndex);
  }

  /* return the String object corresponding to this string. */
  JavaString *getStringObj(const char *str) {
    int hashIndex;
    const char *key;

    if ((key = HashTable<StrNode>::get(str, &node, hashIndex)) != 0) {
      if (!node.str) {
	node.str = new (pool) JavaString(key);
	HashTable<StrNode>::set(str, &node, hashIndex);
      }

      return node.str;
    }

    return 0;
  }


  /* Returns a pointer to the interned string if it exists, NULL 
   * otherwise
   */
  const char *get(const char *str) {
    int hashIndex;

    return HashTable<StrNode>::get(str, 0, hashIndex);
  }

  /* Get a vector, allocated using the same internal pool, that contains
   * all the elements in the string pool
   */
  operator Vector<char *>& () {
    Vector<char *> *vector = new (pool) Vector<char *>;

    for (Uint32 i = 0; i < _NBUCKETS_; i++) {
      DoublyLinkedList<HashTableEntry<StrNode> >& list = buckets[i];
      if (!list.empty())
        for (DoublyLinkedNode *j = list.begin(); !list.done(j); j = list.advance(j)) {
	      vector->append(const_cast<char *>(list.get(j).key));
        }
    }
    
    return *vector;
  }

private:
  StrNode node;
};

#endif /* _STRING_POOL_H_ */


