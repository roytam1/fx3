/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1995 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1997 by Silicon Graphics.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */
/* Boehm, October 9, 1995 1:16 pm PDT */
# include "gc_priv.h"

void GC_default_print_heap_obj_proc();
GC_API void GC_register_finalizer_no_order
        GC_PROTO((GC_PTR obj, GC_finalization_proc fn, GC_PTR cd,
          GC_finalization_proc *ofn, GC_PTR *ocd));

/* Do we want to and know how to save the call stack at the time of */
/* an allocation?  How much space do we want to use in each object? */

# define START_FLAG ((word)0xfedcedcb)
# define END_FLAG ((word)0xbcdecdef)
    /* Stored both one past the end of user object, and one before  */
    /* the end of the object as seen by the allocator.      */


/* Object header */
typedef struct {
    char * oh_string;       /* object descriptor string */
    word oh_int;        /* object descriptor integers   */
#   ifdef NEED_CALLINFO
      struct callinfo oh_ci[NFRAMES];
#   endif
    word oh_sz;         /* Original malloc arg.     */
    word oh_sf;         /* start flag */
} oh;
/* The size of the above structure is assumed not to dealign things,    */
/* and to be a multiple of the word length.             */

#define DEBUG_BYTES (sizeof (oh) + sizeof (word))
#undef ROUNDED_UP_WORDS
#define ROUNDED_UP_WORDS(n) BYTES_TO_WORDS((n) + WORDS_TO_BYTES(1) - 1)


#ifdef SAVE_CALL_CHAIN
#   define ADD_CALL_CHAIN(base, ra) GC_save_callers(((oh *)(base)) -> oh_ci)
#   define PRINT_CALL_CHAIN(base) GC_print_callers(((oh *)(base)) -> oh_ci)
#else
# ifdef GC_ADD_CALLER
#   define ADD_CALL_CHAIN(base, ra) ((oh *)(base)) -> oh_ci[0].ci_pc = (ra)
#   define PRINT_CALL_CHAIN(base) GC_print_callers(((oh *)(base)) -> oh_ci)
# else
#   define ADD_CALL_CHAIN(base, ra)
#   define PRINT_CALL_CHAIN(base)
# endif
#endif

/* Check whether object with base pointer p has debugging info  */ 
/* p is assumed to point to a legitimate object in our part */
/* of the heap.                         */
GC_bool GC_has_debug_info(p)
ptr_t p;
{
    register oh * ohdr = (oh *)p;
    register ptr_t body = (ptr_t)(ohdr + 1);
    register word sz = GC_size((ptr_t) ohdr);
    
    if (HBLKPTR((ptr_t)ohdr) != HBLKPTR((ptr_t)body)
        || sz < sizeof (oh)) {
        return(FALSE);
    }
    if (ohdr -> oh_sz == sz) {
        /* Object may have had debug info, but has been deallocated */
        return(FALSE);
    }
    if (ohdr -> oh_sf == (START_FLAG ^ (word)body)) return(TRUE);
    if (((word *)ohdr)[BYTES_TO_WORDS(sz)-1] == (END_FLAG ^ (word)body)) {
        return(TRUE);
    }
    return(FALSE);
}

/* Store debugging info into p.  Return displaced pointer. */
/* Assumes we don't hold allocation lock.          */
ptr_t GC_store_debug_info(p, sz, string, integer)
register ptr_t p;   /* base pointer */
word sz;    /* bytes */
char * string;
word integer;
{
    register oh * ohdr = (oh *)p;
    register word * result = (word *)(ohdr + 1);
    DCL_LOCK_STATE;
    
    /* There is some argument that we should dissble signals here.  */
    /* But that's expensive.  And this way things should only appear    */
    /* inconsistent while we're in the handler.             */
    LOCK();
    ohdr -> oh_string = string;
    ohdr -> oh_int = integer;
    ohdr -> oh_sz = sz;
    ohdr -> oh_sf = START_FLAG ^ (word)result;
    ((word *)p)[BYTES_TO_WORDS(GC_size(p))-1] =
         result[ROUNDED_UP_WORDS(sz)] = END_FLAG ^ (word)result;
    UNLOCK();
    return((ptr_t)result);
}

/* Check the object with debugging info at p        */
/* return NIL if it's OK.  Else return clobbered    */
/* address.                     */
ptr_t GC_check_annotated_obj(ohdr)
register oh * ohdr;
{
    register ptr_t body = (ptr_t)(ohdr + 1);
    register word gc_sz = GC_size((ptr_t)ohdr);
    if (ohdr -> oh_sz + DEBUG_BYTES > gc_sz) {
        return((ptr_t)(&(ohdr -> oh_sz)));
    }
    if (ohdr -> oh_sf != (START_FLAG ^ (word)body)) {
        return((ptr_t)(&(ohdr -> oh_sf)));
    }
    if (((word *)ohdr)[BYTES_TO_WORDS(gc_sz)-1] != (END_FLAG ^ (word)body)) {
        return((ptr_t)((word *)ohdr + BYTES_TO_WORDS(gc_sz)-1));
    }
    if (((word *)body)[ROUNDED_UP_WORDS(ohdr -> oh_sz)]
        != (END_FLAG ^ (word)body)) {
        return((ptr_t)((word *)body + ROUNDED_UP_WORDS(ohdr -> oh_sz)));
    }
    return(0);
}

extern const char* getTypeName(void* ptr);

void GC_print_obj(p)
ptr_t p;
{
    register oh * ohdr = (oh *)GC_base(p);
    register word *wp, *wend;
    
    wp = (word*)((unsigned long)ohdr + sizeof(oh));

    GC_err_printf3("0x%08lX <%s> (%ld)\n", wp, getTypeName(wp),
                   (unsigned long)(ohdr -> oh_sz));

    /* print all potential references held by this object. */
    wend = (word*)((unsigned long)wp + ohdr -> oh_sz);
    while (wp < wend) GC_err_printf1("\t0x%08lX\n", *wp++);

    PRINT_CALL_CHAIN(ohdr);
}

#if defined(SAVE_CALL_CHAIN)

#include "call_tree.h"

#define CALL_TREE(ohdr) ((call_tree*)ohdr->oh_ci[0].ci_pc)
#define NEXT_WORD(ohdr) (ohdr->oh_ci[1].ci_pc)
#define NEXT_OBJECT(ohdr) (*(oh**)&ohdr->oh_ci[1].ci_pc)
#define IS_PLAUSIBLE_POINTER(p) ((p >= GC_least_plausible_heap_addr) && (p < GC_greatest_plausible_heap_addr))

void GC_mark_object(ptr_t p, word mark)
{
    p = GC_base(p);
    if (p && GC_has_debug_info(p)) {
        oh *ohdr = (oh *)p;
        NEXT_WORD(ohdr) = mark;
    }
}

void GC_print_call_tree(call_tree* tree);

/**
 * Compresses a call tree (prints it upside down as well) by assigning
 * unique id values to each node when they are printed the first time.
 * Uses a pseudo XML syntax:
 *   <c id=i pid=p>function[file,offset]</c> <!-- uncompressed form. -->
 *   <c id=i/>                               <!-- compressed form. -->
 */
static void print_compressed_call_tree(call_tree* tree, unsigned* next_id)
{
    call_tree* parent = tree->parent;
    if (parent) {
        if (tree->id) {
            /* id already assigned, print compressed form. */
            GC_err_printf1("<c id=%d/>\n", tree->id);
        } else {
            if (parent->id == 0) {
                /* parent needs an id as well. */
                print_compressed_call_tree(parent, next_id);
            }
            tree->id = (*next_id)++;
            GC_err_printf2("<c id=%d pid=%d>", tree->id, parent->id);
            GC_print_call_tree(tree);
            GC_err_printf0("</c>\n");
        }
    }
}

/**
 * Starting from specified object, traces through the entire graph of reachable objects.
 * Uses extra word stored in debugging header for alignment purposes.
 */
void GC_trace_object(ptr_t p, int verbose)
{
    register oh *head, *scan, *tail;
    register word *wp, *wend;
    word total = 0;
    call_tree* tree;
    unsigned next_id = 1;
    DCL_LOCK_STATE;
    
    DISABLE_SIGNALS();
    LOCK();
    STOP_WORLD();

    p = GC_base(p);
    if (p && GC_has_debug_info(p)) {
        head = scan = tail = (oh *)p;

        /* invariant:  end of list always marked with value 1. */
        NEXT_WORD(tail) = 1;
        
        /* trace through every object reachable from this starting point. */
        for (;;) {
            /* print ADDRESS <type> (size) for each object. */
            wp = (word*)((unsigned long)scan + sizeof(oh));
            GC_err_printf3("0x%08lX <%s> (%ld)\n", wp, getTypeName(wp), scan->oh_sz);
            total += scan->oh_sz;

            /* scan/print all plausible references held by this object. */
            wend = (word*)((word)wp + scan->oh_sz);
            while (wp < wend) {
                p = (ptr_t) *wp++;
                if (verbose) GC_err_printf1("\t0x%08lX\n", p);
                if (IS_PLAUSIBLE_POINTER(p)) {
                    p = GC_base(p);
                    if (p && GC_has_debug_info(p)) {
                        oh *ohdr = (oh *)p;
                        if (NEXT_WORD(ohdr) == 0) {
                            NEXT_OBJECT(tail) = ohdr;
                            tail = ohdr;
                            NEXT_WORD(tail) = 1;
                        }
                    }
                }
            }
            if (verbose) {
                /* to save space, compress call trees. */
                tree = CALL_TREE(scan);
                if (tree) print_compressed_call_tree(tree, &next_id);
            }
            if (NEXT_WORD(scan) == 1)
                break;
            scan = NEXT_OBJECT(scan);
        }
        GC_printf1("GC_trace_object: total = %ld\n", total);

        /* clear all marks. */
        scan = head;
        NEXT_WORD(tail) = 0;
        while (scan) {
            tail = NEXT_OBJECT(scan);
            NEXT_WORD(scan) = 0;
            tree = CALL_TREE(scan);
            while (tree && tree->id) {
                tree->id = 0;
                tree = tree->parent;
            }
            scan = tail;
        }
    }

    START_WORLD();
    UNLOCK();
    ENABLE_SIGNALS();
}

#endif /* SAVE_CALL_CHAIN */

void GC_debug_print_heap_obj_proc(p)
ptr_t p;
{
    if (GC_has_debug_info(p)) {
        GC_print_obj(p);
    } else {
        GC_default_print_heap_obj_proc(p);
    }
}

void GC_print_smashed_obj(p, clobbered_addr)
ptr_t p, clobbered_addr;
{
    register oh * ohdr = (oh *)GC_base(p);
    
    GC_err_printf2("0x%lx in object at 0x%lx(", (unsigned long)clobbered_addr,
                                (unsigned long)p);
    if (clobbered_addr <= (ptr_t)(&(ohdr -> oh_sz))
        || ohdr -> oh_string == 0) {
        GC_err_printf1("<smashed>, appr. sz = %ld)\n",
                   (GC_size((ptr_t)ohdr) - DEBUG_BYTES));
    } else {
        if (ohdr -> oh_string[0] == '\0') {
            GC_err_puts("EMPTY(smashed?)");
        } else {
            GC_err_puts(ohdr -> oh_string);
        }
        GC_err_printf2(":%ld, sz=%ld)\n", (unsigned long)(ohdr -> oh_int),
                              (unsigned long)(ohdr -> oh_sz));
        PRINT_CALL_CHAIN(ohdr);
    }
}

void GC_check_heap_proc();

void GC_start_debugging()
{
    GC_check_heap = GC_check_heap_proc;
    GC_print_heap_obj = GC_debug_print_heap_obj_proc;
    GC_debugging_started = TRUE;
    GC_register_displacement((word)sizeof(oh));
}

# if defined(__STDC__) || defined(__cplusplus)
    void GC_debug_register_displacement(GC_word offset)
# else
    void GC_debug_register_displacement(offset) 
    GC_word offset;
# endif
{
    GC_register_displacement(offset);
    GC_register_displacement((word)sizeof(oh) + offset);
}

# ifdef GC_ADD_CALLER
#   define EXTRA_ARGS word ra, char * s, int i
#   define OPT_RA ra,
# else
#   define EXTRA_ARGS char * s, int i
#   define OPT_RA
# endif

# ifdef __STDC__
    GC_PTR GC_debug_malloc(size_t lb, EXTRA_ARGS)
# else
    GC_PTR GC_debug_malloc(lb, s, i)
    size_t lb;
    char * s;
    int i;
#   ifdef GC_ADD_CALLER
    --> GC_ADD_CALLER not implemented for K&R C
#   endif
# endif
{
    GC_PTR result = GC_malloc(lb + DEBUG_BYTES);
    
    if (result == 0) {
        GC_err_printf1("GC_debug_malloc(%ld) returning NIL (",
                   (unsigned long) lb);
        GC_err_puts(s);
        GC_err_printf1(":%ld)\n", (unsigned long)i);
        return(0);
    }
    if (!GC_debugging_started) {
        GC_start_debugging();
    }
    ADD_CALL_CHAIN(result, ra);
    return (GC_store_debug_info(result, (word)lb, s, (word)i));
}

#ifdef STUBBORN_ALLOC
# ifdef __STDC__
    GC_PTR GC_debug_malloc_stubborn(size_t lb, EXTRA_ARGS)
# else
    GC_PTR GC_debug_malloc_stubborn(lb, s, i)
    size_t lb;
    char * s;
    int i;
# endif
{
    GC_PTR result = GC_malloc_stubborn(lb + DEBUG_BYTES);
    
    if (result == 0) {
        GC_err_printf1("GC_debug_malloc(%ld) returning NIL (",
                   (unsigned long) lb);
        GC_err_puts(s);
        GC_err_printf1(":%ld)\n", (unsigned long)i);
        return(0);
    }
    if (!GC_debugging_started) {
        GC_start_debugging();
    }
    ADD_CALL_CHAIN(result, ra);
    return (GC_store_debug_info(result, (word)lb, s, (word)i));
}

void GC_debug_change_stubborn(p)
GC_PTR p;
{
    register GC_PTR q = GC_base(p);
    register hdr * hhdr;
    
    if (q == 0) {
        GC_err_printf1("Bad argument: 0x%lx to GC_debug_change_stubborn\n",
                   (unsigned long) p);
        ABORT("GC_debug_change_stubborn: bad arg");
    }
    hhdr = HDR(q);
    if (hhdr -> hb_obj_kind != STUBBORN) {
        GC_err_printf1("GC_debug_change_stubborn arg not stubborn: 0x%lx\n",
                   (unsigned long) p);
        ABORT("GC_debug_change_stubborn: arg not stubborn");
    }
    GC_change_stubborn(q);
}

void GC_debug_end_stubborn_change(p)
GC_PTR p;
{
    register GC_PTR q = GC_base(p);
    register hdr * hhdr;
    
    if (q == 0) {
        GC_err_printf1("Bad argument: 0x%lx to GC_debug_end_stubborn_change\n",
                   (unsigned long) p);
        ABORT("GC_debug_end_stubborn_change: bad arg");
    }
    hhdr = HDR(q);
    if (hhdr -> hb_obj_kind != STUBBORN) {
        GC_err_printf1("debug_end_stubborn_change arg not stubborn: 0x%lx\n",
                   (unsigned long) p);
        ABORT("GC_debug_end_stubborn_change: arg not stubborn");
    }
    GC_end_stubborn_change(q);
}

#endif /* STUBBORN_ALLOC */

# ifdef __STDC__
    GC_PTR GC_debug_malloc_atomic(size_t lb, EXTRA_ARGS)
# else
    GC_PTR GC_debug_malloc_atomic(lb, s, i)
    size_t lb;
    char * s;
    int i;
# endif
{
    GC_PTR result = GC_malloc_atomic(lb + DEBUG_BYTES);
    
    if (result == 0) {
        GC_err_printf1("GC_debug_malloc_atomic(%ld) returning NIL (",
                  (unsigned long) lb);
        GC_err_puts(s);
        GC_err_printf1(":%ld)\n", (unsigned long)i);
        return(0);
    }
    if (!GC_debugging_started) {
        GC_start_debugging();
    }
    ADD_CALL_CHAIN(result, ra);
    return (GC_store_debug_info(result, (word)lb, s, (word)i));
}

# ifdef __STDC__
    GC_PTR GC_debug_malloc_uncollectable(size_t lb, EXTRA_ARGS)
# else
    GC_PTR GC_debug_malloc_uncollectable(lb, s, i)
    size_t lb;
    char * s;
    int i;
# endif
{
    GC_PTR result = GC_malloc_uncollectable(lb + DEBUG_BYTES);
    
    if (result == 0) {
        GC_err_printf1("GC_debug_malloc_uncollectable(%ld) returning NIL (",
                  (unsigned long) lb);
        GC_err_puts(s);
        GC_err_printf1(":%ld)\n", (unsigned long)i);
        return(0);
    }
    if (!GC_debugging_started) {
        GC_start_debugging();
    }
    ADD_CALL_CHAIN(result, ra);
    return (GC_store_debug_info(result, (word)lb, s, (word)i));
}

#ifdef ATOMIC_UNCOLLECTABLE
# ifdef __STDC__
    GC_PTR GC_debug_malloc_atomic_uncollectable(size_t lb, EXTRA_ARGS)
# else
    GC_PTR GC_debug_malloc_atomic_uncollectable(lb, s, i)
    size_t lb;
    char * s;
    int i;
# endif
{
    GC_PTR result = GC_malloc_atomic_uncollectable(lb + DEBUG_BYTES);
    
    if (result == 0) {
        GC_err_printf1(
        "GC_debug_malloc_atomic_uncollectable(%ld) returning NIL (",
                (unsigned long) lb);
        GC_err_puts(s);
        GC_err_printf1(":%ld)\n", (unsigned long)i);
        return(0);
    }
    if (!GC_debugging_started) {
        GC_start_debugging();
    }
    ADD_CALL_CHAIN(result, ra);
    return (GC_store_debug_info(result, (word)lb, s, (word)i));
}
#endif /* ATOMIC_UNCOLLECTABLE */

# ifdef __STDC__
    void GC_debug_free(GC_PTR p)
# else
    void GC_debug_free(p)
    GC_PTR p;
# endif
{
    register GC_PTR base = GC_base(p);
    register ptr_t clobbered;
    
    /* ignore free(NULL) */
    if (p == 0)
      return;

    if (base == 0) {
        GC_err_printf1("Attempt to free invalid pointer %lx\n",
                   (unsigned long)p);
        if (p != 0) ABORT("free(invalid pointer)");
    }
    if ((ptr_t)p - (ptr_t)base != sizeof(oh)) {
        GC_err_printf1(
              "GC_debug_free called on pointer %lx wo debugging info\n",
              (unsigned long)p);
    } else {
      oh * ohdr = (oh *)base;
      clobbered = GC_check_annotated_obj(ohdr);
      if (clobbered != 0) {
        if (ohdr -> oh_sz == GC_size(base)) {
            GC_err_printf0(
                  "GC_debug_free: found previously deallocated (?) object at ");
        } else {
            GC_err_printf0("GC_debug_free: found smashed object at ");
        }
        GC_print_smashed_obj(p, clobbered);
      }
      /* Invalidate size */
      ohdr -> oh_sz = GC_size(base);
    }
#   ifdef FIND_LEAK
        GC_free(base);
#   else
    {
        register hdr * hhdr = HDR(p);
        GC_bool uncollectable = FALSE;

        if (hhdr ->  hb_obj_kind == UNCOLLECTABLE) {
        uncollectable = TRUE;
        }
#       ifdef ATOMIC_UNCOLLECTABLE
        if (hhdr ->  hb_obj_kind == AUNCOLLECTABLE) {
            uncollectable = TRUE;
        }
#       endif
        if (uncollectable) GC_free(base);
    }
#   endif
}

# ifdef __STDC__
    GC_PTR GC_debug_realloc(GC_PTR p, size_t lb, EXTRA_ARGS)
# else
    GC_PTR GC_debug_realloc(p, lb, s, i)
    GC_PTR p;
    size_t lb;
    char *s;
    int i;
# endif
{
    register GC_PTR base = GC_base(p);
    register ptr_t clobbered;
    register GC_PTR result;
    register size_t copy_sz = lb;
    register size_t old_sz;
    register hdr * hhdr;
    
    if (p == 0) return(GC_debug_malloc(lb, OPT_RA s, i));
    if (base == 0) {
        GC_err_printf1(
              "Attempt to reallocate invalid pointer %lx\n", (unsigned long)p);
        ABORT("realloc(invalid pointer)");
    }
    if ((ptr_t)p - (ptr_t)base != sizeof(oh)) {
        GC_err_printf1(
            "GC_debug_realloc called on pointer %lx wo debugging info\n",
            (unsigned long)p);
        return(GC_realloc(p, lb));
    }
    hhdr = HDR(base);
    switch (hhdr -> hb_obj_kind) {
#    ifdef STUBBORN_ALLOC
      case STUBBORN:
        result = GC_debug_malloc_stubborn(lb, OPT_RA s, i);
        break;
#    endif
      case NORMAL:
        result = GC_debug_malloc(lb, OPT_RA s, i);
        break;
      case PTRFREE:
        result = GC_debug_malloc_atomic(lb, OPT_RA s, i);
        break;
      case UNCOLLECTABLE:
    result = GC_debug_malloc_uncollectable(lb, OPT_RA s, i);
    break;
#    ifdef ATOMIC_UNCOLLECTABLE
      case AUNCOLLECTABLE:
    result = GC_debug_malloc_atomic_uncollectable(lb, OPT_RA s, i);
    break;
#    endif
      default:
        GC_err_printf0("GC_debug_realloc: encountered bad kind\n");
        ABORT("bad kind");
    }
    clobbered = GC_check_annotated_obj((oh *)base);
    if (clobbered != 0) {
        GC_err_printf0("GC_debug_realloc: found smashed object at ");
        GC_print_smashed_obj(p, clobbered);
    }
    old_sz = ((oh *)base) -> oh_sz;
    if (old_sz < copy_sz) copy_sz = old_sz;
    if (result == 0) return(0);
    BCOPY(p, result,  copy_sz);
    GC_debug_free(p);
    return(result);
}

/* Check all marked objects in the given block for validity */
/*ARGSUSED*/
void GC_check_heap_block(hbp, dummy)
register struct hblk *hbp;  /* ptr to current heap block        */
word dummy;
{
    register struct hblkhdr * hhdr = HDR(hbp);
    register word sz = hhdr -> hb_sz;
    register int word_no;
    register word *p, *plim;
    
    p = (word *)(hbp->hb_body);
    word_no = HDR_WORDS;
    if (sz > MAXOBJSZ) {
    plim = p;
    } else {
        plim = (word *)((((word)hbp) + HBLKSIZE) - WORDS_TO_BYTES(sz));
    }
    /* go through all words in block */
    while( p <= plim ) {
        if( mark_bit_from_hdr(hhdr, word_no)
            && GC_has_debug_info((ptr_t)p)) {
            ptr_t clobbered = GC_check_annotated_obj((oh *)p);
            
            if (clobbered != 0) {
                GC_err_printf0(
                    "GC_check_heap_block: found smashed object at ");
                GC_print_smashed_obj((ptr_t)p, clobbered);
            }
        }
        word_no += sz;
        p += sz;
    }
}


/* This assumes that all accessible objects are marked, and that    */
/* I hold the allocation lock.  Normally called by collector.       */
void GC_check_heap_proc()
{
#   ifndef SMALL_CONFIG
    if (sizeof(oh) & (2 * sizeof(word) - 1) != 0) {
        ABORT("Alignment problem: object header has inappropriate size\n");
    }
#   endif
    GC_apply_to_all_blocks(GC_check_heap_block, (word)0);
}

struct closure {
    GC_finalization_proc cl_fn;
    GC_PTR cl_data;
};

# ifdef __STDC__
    void * GC_make_closure(GC_finalization_proc fn, void * data)
# else
    GC_PTR GC_make_closure(fn, data)
    GC_finalization_proc fn;
    GC_PTR data;
# endif
{
    struct closure * result =
            (struct closure *) GC_malloc(sizeof (struct closure));
    
    result -> cl_fn = fn;
    result -> cl_data = data;
    return((GC_PTR)result);
}

# ifdef __STDC__
    void GC_debug_invoke_finalizer(void * obj, void * data)
# else
    void GC_debug_invoke_finalizer(obj, data)
    char * obj;
    char * data;
# endif
{
    register struct closure * cl = (struct closure *) data;
    
    (*(cl -> cl_fn))((GC_PTR)((char *)obj + sizeof(oh)), cl -> cl_data);
} 


# ifdef __STDC__
    void GC_debug_register_finalizer(GC_PTR obj, GC_finalization_proc fn,
                         GC_PTR cd, GC_finalization_proc *ofn,
                     GC_PTR *ocd)
# else
    void GC_debug_register_finalizer(obj, fn, cd, ofn, ocd)
    GC_PTR obj;
    GC_finalization_proc fn;
    GC_PTR cd;
    GC_finalization_proc *ofn;
    GC_PTR *ocd;
# endif
{
    ptr_t base = GC_base(obj);
    if (0 == base || (ptr_t)obj - base != sizeof(oh)) {
        GC_err_printf1(
        "GC_register_finalizer called with non-base-pointer 0x%lx\n",
        obj);
    }
    GC_register_finalizer(base, GC_debug_invoke_finalizer,
                  GC_make_closure(fn,cd), ofn, ocd);
}

# ifdef __STDC__
    void GC_debug_register_finalizer_no_order
                        (GC_PTR obj, GC_finalization_proc fn,
                         GC_PTR cd, GC_finalization_proc *ofn,
                     GC_PTR *ocd)
# else
    void GC_debug_register_finalizer_no_order
                        (obj, fn, cd, ofn, ocd)
    GC_PTR obj;
    GC_finalization_proc fn;
    GC_PTR cd;
    GC_finalization_proc *ofn;
    GC_PTR *ocd;
# endif
{
    ptr_t base = GC_base(obj);
    if (0 == base || (ptr_t)obj - base != sizeof(oh)) {
        GC_err_printf1(
      "GC_register_finalizer_no_order called with non-base-pointer 0x%lx\n",
      obj);
    }
    GC_register_finalizer_no_order(base, GC_debug_invoke_finalizer,
                          GC_make_closure(fn,cd), ofn, ocd);
 }

# ifdef __STDC__
    void GC_debug_register_finalizer_ignore_self
                        (GC_PTR obj, GC_finalization_proc fn,
                         GC_PTR cd, GC_finalization_proc *ofn,
                     GC_PTR *ocd)
# else
    void GC_debug_register_finalizer_ignore_self
                        (obj, fn, cd, ofn, ocd)
    GC_PTR obj;
    GC_finalization_proc fn;
    GC_PTR cd;
    GC_finalization_proc *ofn;
    GC_PTR *ocd;
# endif
{
    ptr_t base = GC_base(obj);
    if (0 == base || (ptr_t)obj - base != sizeof(oh)) {
        GC_err_printf1(
        "GC_register_finalizer_ignore_self called with non-base-pointer 0x%lx\n",
        obj);
    }
    GC_register_finalizer_ignore_self(base, GC_debug_invoke_finalizer,
                          GC_make_closure(fn,cd), ofn, ocd);
}
