/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef pathsub_h___
#define pathsub_h___
/*
** Pathname subroutines.
**
** Brendan Eich, 8/29/95
*/
#include <limits.h>
#include <sys/types.h>

#if SUNOS4
#include "../pr/include/md/sunos4.h"
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

/*
 * Just prevent stupidity
 */
#undef NAME_MAX
#define NAME_MAX 256

extern char *program;

extern void fail(char *format, ...);
extern char *getcomponent(char *path, char *name);
extern char *ino2name(ino_t ino, char *dir);
extern void *xmalloc(size_t size);
extern char *xstrdup(char *s);
extern char *xbasename(char *path);
extern void xchdir(char *dir);

/* Relate absolute pathnames from and to returning the result in outpath. */
extern int relatepaths(char *from, char *to, char *outpath);

/* XXX changes current working directory -- caveat emptor */
extern void reversepath(char *inpath, char *name, int len, char *outpath);

#endif /* pathsub_h___ */
