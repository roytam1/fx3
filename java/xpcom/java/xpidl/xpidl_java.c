/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, 
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *    Michael Allen (michael.allen@sun.com)
 *    Frank Mitchell (frank.mitchell@sun.com)
 *    Denis Sharypov (sdv@sparc.spb.su)
 *    Igor Kushnirskiy (idk@eng.sun.com)
 */

/*
 * Generate Java interfaces from XPIDL.
 */

#include "xpidl.h"
#include <ctype.h>
#include <glib.h>


/*
 * Write a one-line comment containing IDL
 * source decompiled from state->tree.
 */
void write_comment(TreeState *state);
char* subscriptIdentifier(TreeState *state, char *str);
char* subscriptMethodName(TreeState *state, char *str);

struct java_priv_data {
    GHashTable *typedefTable;
    GHashTable *keywords;    
    FILE *file;
};

char* javaKeywords[] = {
    "abstract", "default", "if"        , "private"     , "this"     ,
    "boolean" , "do"     , "implements", "protected"   , "throw"    ,
    "break"   , "double" , "import",     "public"      , "throws"   ,
    "byte"    , "else"   , "instanceof", "return"      , "transient",
    "case"    , "extends", "int"       , "short"       , "try"      ,
    "catch"   , "final"  , "interface" , "static"      , "void"     ,
    "char"    , "finally", "long"      , "strictfp"    , "volatile" ,
    "class"   , "float"  , "native"    , "super"       , "while"    ,
    "const"   , "for"    , "new"       , "switch"      , 
    "continue", "goto"   , "package"   , "synchronized"};

#define TYPEDEFS(state)     (((struct java_priv_data *)state->priv)->typedefTable)
#define KEYWORDS(state)     (((struct java_priv_data *)state->priv)->keywords)
#define FILENAME(state)     (((struct java_priv_data *)state->priv)->file)

static gboolean
write_classname_iid_define(FILE *file, const char *className)
{
    const char *iidName;
    if (className[0] == 'n' && className[1] == 's') {
        /* backcompat naming styles */
        fputs("NS_", file);
        iidName = className + 2;
    } else {
        iidName = className;
    }

    while (*iidName) {
        fputc(toupper(*iidName++), file);
    }

    fputs("_IID", file);
     
    return TRUE;
}

static gboolean
java_prolog(TreeState *state)
{
    int len, i;
    state->priv = calloc(1, sizeof(struct java_priv_data));
    if (!state->priv)
        return FALSE;
    TYPEDEFS(state) = 0;
    TYPEDEFS(state) = g_hash_table_new(g_str_hash, g_str_equal);
    if (!TYPEDEFS(state)) {
        /* XXX report error */
        free(state->priv);
        return FALSE;
    }
    KEYWORDS(state) = 0;
    KEYWORDS(state) = g_hash_table_new(g_str_hash, g_str_equal);
    if (!KEYWORDS(state)) {
        g_hash_table_destroy(TYPEDEFS(state));
        free(state->priv);
        return FALSE;
    }
    len = sizeof(javaKeywords)/sizeof(*javaKeywords);
    for (i = 0; i < len; i++) {
        g_hash_table_insert(KEYWORDS(state),
                            javaKeywords[i],
                            javaKeywords[i]);
    }
    return TRUE;
}

static gboolean 
java_epilog(TreeState *state)
{
    /* points to other elements of the tree, so just destroy the table */
    g_hash_table_destroy(TYPEDEFS(state));
    g_hash_table_destroy(KEYWORDS(state));
    free(state->priv);
    state->priv = NULL;
    
    return TRUE;
}

static gboolean
forward_declaration(TreeState *state) 
{
    /*
     * Java doesn't need forward declarations unless the declared 
     * class resides in a different package.
     */
#if 0
    IDL_tree iface = state->tree;
    const char *className = IDL_IDENT(IDL_FORWARD_DCL(iface).ident).str;
    const char *pkgName = "org.mozilla.xpcom";
    if (!className)
        return FALSE;
    /* XXX: Get package name and compare */
    fprintf(FILENAME(state), "import %s.%s;\n", pkgName, className);
#endif
    return TRUE;
}


static gboolean
interface_declaration(TreeState *state) 
{

    char *outname;
    IDL_tree interface = state->tree;
    IDL_tree iterator = NULL;
    char *interface_name = 
        subscriptIdentifier(state, IDL_IDENT(IDL_INTERFACE(interface).ident).str);
    const char *iid = NULL;
    GSList *doc_comments = IDL_IDENT(IDL_INTERFACE(interface).ident).comments;
    char *prefix = IDL_GENTREE(IDL_NS(state->ns).current)._cur_prefix;

    /*
     * Each interface decl is a single file
     */
    outname = g_strdup_printf("%s.%s", interface_name, "java");
    FILENAME(state) =  fopen(outname, "w");
    if (!FILENAME(state)) {
        perror("error opening output file");
        return FALSE;
    }

    fputs("/*\n * ************* DO NOT EDIT THIS FILE ***********\n",
          FILENAME(state));
    
    fprintf(FILENAME(state), 
            " *\n * This file was automatically generated from %s.idl.\n", 
            state->basename);
    
    fputs(" */\n\n", FILENAME(state));
	
    if (prefix) {
        if (strlen(prefix))
            fprintf(FILENAME(state), "\npackage %s;\n\n", prefix);
        fputs("import org.mozilla.xpcom.*;\n\n", FILENAME(state));
    } else {
        fputs("\npackage org.mozilla.xpcom;\n\n", FILENAME(state));
    }

    /*
     * Write out JavaDoc comment
     */

    fprintf(FILENAME(state), "\n/**\n * Interface %s\n", interface_name);

#ifndef LIBIDL_MAJOR_VERSION
    iid = IDL_tree_property_get(interface, "uuid");
#else
    iid = IDL_tree_property_get(IDL_INTERFACE(interface).ident, "uuid");
#endif

    if (iid != NULL) {
        fprintf(FILENAME(state), " *\n * IID: 0x%s\n */\n\n", iid);
    } else {
        fputs(" */\n\n", FILENAME(state));
    }

    if (doc_comments != NULL)
        printlist(FILENAME(state), doc_comments);

    /*
     * Write "public interface <foo>"
     */

    fprintf(FILENAME(state), "public interface %s ", interface_name);

    /*
     * Check for inheritence, and iterator over the inherited names,
     * if any.
     */

    if ((iterator = IDL_INTERFACE(interface).inheritance_spec)) {
        fputs("extends ", FILENAME(state));

        do {

            fprintf(FILENAME(state), "%s", 
                    IDL_IDENT(IDL_LIST(iterator).data).str);
	    
            if (IDL_LIST(iterator).next) {
                fputs(", ", FILENAME(state));
            }
        } while ((iterator = IDL_LIST(iterator).next));

    }

    fputs("\n{\n", FILENAME(state));
    
    if (iid) {
        /*
         * Write interface constants for IID
         */

/*          fputs("    public static final String ", FILENAME(state)); */

        /* XXX s.b just "IID" ? */
/*          if (!write_classname_iid_define(FILENAME(state), interface_name)) { */
/*              return FALSE; */
/*          } */

/*          fprintf(FILENAME(state), "_STRING =\n        \"%s\";\n\n", iid); */

/*          fputs("    public static final nsID ", FILENAME(state)); */

        /* XXX s.b just "IID" ? */
/*          if (!write_classname_iid_define(FILENAME(state), interface_name)) { */
/*              return FALSE; */
/*          } */

/*          fprintf(FILENAME(state), " =\n        new nsID(\"%s\");\n\n", iid); */
        fprintf(FILENAME(state), "    public static final IID IID =\n       new IID(\"%s\");\n\n", iid);
    }

    /*
     * Advance the state of the tree, go on to process more
     */
    
    state->tree = IDL_INTERFACE(interface).body;

    if (state->tree && !xpidl_process_node(state)) {
        return FALSE;
    }


    fputs("\n}\n", FILENAME(state));
    fprintf(FILENAME(state), "\n/*\n * end\n */\n");
    fclose(FILENAME(state));
    free(outname);

    return TRUE;
}

static gboolean
process_list(TreeState *state)
{
    IDL_tree iter;
    gint type;
    for (iter = state->tree; iter; iter = IDL_LIST(iter).next) {
        state->tree = IDL_LIST(iter).data;
        type = IDL_NODE_TYPE(state->tree);
        if (!xpidl_process_node(state))
            return FALSE;
    }
    return TRUE;
}

static gboolean 
xpcom_to_java_type (TreeState *state) 
{
    IDL_tree real_type;

    if (!state->tree) {
        fputs("Object", FILENAME(state));
        return TRUE;
    }

    /* Could be a typedef; try to map it to the real type */

    real_type = find_underlying_type(state->tree);
    state->tree = real_type ? real_type : state->tree;

    switch(IDL_NODE_TYPE(state->tree)) {

    case IDLN_TYPE_INTEGER: {

        switch(IDL_TYPE_INTEGER(state->tree).f_type) {

        case IDL_INTEGER_TYPE_SHORT:
            fputs("short", FILENAME(state));
            break;

        case IDL_INTEGER_TYPE_LONG:
            fputs("int", FILENAME(state));
            break;

        case IDL_INTEGER_TYPE_LONGLONG:
            fputs("long", FILENAME(state));
            break;
	    
        default:
            g_error("   Unknown integer type: %d\n",
                    IDL_TYPE_INTEGER(state->tree).f_type);
            return FALSE;

        }

        break;
    }

    case IDLN_TYPE_CHAR:
    case IDLN_TYPE_WIDE_CHAR:
        fputs("char", FILENAME(state));
        break;

    case IDLN_TYPE_WIDE_STRING:
    case IDLN_TYPE_STRING:
        fputs("String", FILENAME(state));
        break;

    case IDLN_TYPE_BOOLEAN:
        fputs("boolean", FILENAME(state));
        break;

    case IDLN_TYPE_OCTET:
        fputs("byte", FILENAME(state));
        break;

    case IDLN_TYPE_FLOAT:
        switch(IDL_TYPE_FLOAT(state->tree).f_type) {

        case IDL_FLOAT_TYPE_FLOAT:
            fputs("float", FILENAME(state));
            break;

        case IDL_FLOAT_TYPE_DOUBLE:
            fputs("double", FILENAME(state));
            break;
	    
        default:
            g_error("    Unknown floating point typ: %d\n",
                    IDL_NODE_TYPE(state->tree));
            break;
        }
        break;


    case IDLN_IDENT:
        if (IDL_NODE_UP(state->tree) &&
            IDL_NODE_TYPE(IDL_NODE_UP(state->tree)) == IDLN_NATIVE) {
            const char *user_type = IDL_NATIVE(IDL_NODE_UP(state->tree)).user_type;
            const char *ident_str = IDL_IDENT(IDL_NATIVE(IDL_NODE_UP(state->tree)).ident).str;
            if (strcmp(user_type, "void") == 0) { /*it should not happend for scriptable methods*/
                fputs("Object", FILENAME(state));
            }
            /* XXX: s.b test for "id" attribute */
            /* XXX: special class for nsIDs */
            else if (strcmp(user_type, "nsID") == 0) {
                fputs("ID", FILENAME(state));
            } else if (strcmp(user_type, "nsIID") == 0) {
                fputs("IID", FILENAME(state));
            } else if (strcmp(user_type, "nsCID") == 0) { 
                fputs("CID", FILENAME(state));
            }
            else {
                /* XXX: special class for opaque types */
                /*it should not happend for scriptable methods*/
                fputs("OpaqueValue", FILENAME(state)); 
            }
        } else {
            const char *ident_str = IDL_IDENT(state->tree).str;

            /* XXX: big kludge; s.b. way to match to typedefs */
            if (strcmp(ident_str, "PRInt8") == 0 ||
                strcmp(ident_str, "PRUint8") == 0) {
                fputs("byte", FILENAME(state));
            }
            else if (strcmp(ident_str, "PRInt16") == 0 ||
                     strcmp(ident_str, "PRUint16") == 0) {
                fputs("short", FILENAME(state));
            }
            else if (strcmp(ident_str, "PRInt32") == 0 ||
                     strcmp(ident_str, "PRUint32") == 0) {
                fputs("int", FILENAME(state));
            }
            else if (strcmp(ident_str, "PRInt64") == 0 ||
                     strcmp(ident_str, "PRUint64") == 0) {
                fputs("long", FILENAME(state));
            }
            else if (strcmp(ident_str, "PRBool") == 0) {
                fputs("boolean", FILENAME(state));
            }
            else if (strcmp(ident_str, "nsrefcnt") == 0) {
                fputs("int", FILENAME(state));
            }
            else {
                IDL_tree real_type = 
                    g_hash_table_lookup(TYPEDEFS(state), ident_str);

                if (real_type) {
                    IDL_tree orig_tree = state->tree;
                    state->tree = real_type;
                    xpcom_to_java_type(state);

                    state->tree = orig_tree;
                }
                else {
                    fputs(subscriptIdentifier(state, (char*) ident_str), FILENAME(state));
                }
            }
        }

        break;

    case IDLN_TYPE_ENUM:
    case IDLN_TYPE_OBJECT:
    default:
        g_error("    Unknown type: %d\n",
                IDL_TYPE_FLOAT(state->tree).f_type);
        break;
    }

    return TRUE;

}

static gboolean
xpcom_to_java_param(TreeState *state) 
{
    IDL_tree param = state->tree;
    state->tree = IDL_PARAM_DCL(param).param_type_spec;

    /*
     * Put in type of parameter
     */

    if (!xpcom_to_java_type(state)) {
        return FALSE;
    }

    /*
     * If the parameter is out or inout, make it a Java array of the
     * appropriate type
     */

    if (IDL_PARAM_DCL(param).attr != IDL_PARAM_IN) {
        fputs("[]", FILENAME(state));
    }

    /*
     * If the parameter is an array make it a Java array
     */
    if (IDL_tree_property_get(IDL_PARAM_DCL(param).simple_declarator, "array"))
        fputs("[]", FILENAME(state));

    /*
     * Put in name of parameter 
     */

    fputc(' ', FILENAME(state));

    fputs(subscriptIdentifier(state, 
                              IDL_IDENT(IDL_PARAM_DCL(param).simple_declarator).str), 
          FILENAME(state));
    return TRUE;
}


static gboolean
type_declaration(TreeState *state) 
{
    /*
     * Unlike C, Java has no type declaration directive.
     * Instead, we record the mapping, and look up the actual type
     * when needed.
     */
    IDL_tree type = IDL_TYPE_DCL(state->tree).type_spec;
    IDL_tree dcls = IDL_TYPE_DCL(state->tree).dcls;
 
    
    /* XXX: check for illegal types */

    g_hash_table_insert(TYPEDEFS(state),
                        IDL_IDENT(IDL_LIST(dcls).data).str,
                        type);

    return TRUE;
}

static gboolean
method_declaration(TreeState *state) 
{
    /* IDL_tree method_tree = state->tree; */
    const char* array = NULL;
    GSList *doc_comments = IDL_IDENT(IDL_OP_DCL(state->tree).ident).comments;
    struct _IDL_OP_DCL *method = &IDL_OP_DCL(state->tree);
    gboolean method_notxpcom = 
        (IDL_tree_property_get(method->ident, "notxpcom") != NULL);
    gboolean method_noscript = 
        (IDL_tree_property_get(method->ident, "noscript") != NULL);
    IDL_tree iterator = NULL;
    IDL_tree retval_param = NULL;
    char *method_name = 
        g_strdup_printf("%c%s", 
                        tolower(IDL_IDENT(method->ident).str[0]), 
                        IDL_IDENT(method->ident).str + 1);

    if (doc_comments != NULL) {
        fputs("    ", FILENAME(state));
        printlist(FILENAME(state), doc_comments);
    }

    if (method_notxpcom || method_noscript)
        return TRUE;

    if (!verify_method_declaration(state->tree))
        return FALSE;

    fputc('\n', FILENAME(state));
    write_comment(state);

    /*
     * Write beginning of method declaration
     */
    fputs("    ", FILENAME(state));
    if (!method_noscript) {
        /* Nonscriptable methods become package-protected */
        fputs("public ", FILENAME(state));
    }

    /*
     * Write return type
     * Unlike C++ headers, Java interfaces return the declared 
     * return value; an exception indicates XPCOM method failure.
     */
    if (method_notxpcom || method->op_type_spec) {
        state->tree = method->op_type_spec;
        if (!xpcom_to_java_type(state)) {
            return FALSE;
        }
    } else {

        /* Check for retval attribute */
        for (iterator = method->parameter_dcls; iterator != NULL; 
             iterator = IDL_LIST(iterator).next) {

            IDL_tree original_tree = state->tree;

            state->tree = IDL_LIST(iterator).data;

            if (IDL_tree_property_get(IDL_PARAM_DCL(state->tree).simple_declarator, 
                                      "retval")) {
                retval_param = iterator;

                array = 
                    IDL_tree_property_get(IDL_PARAM_DCL(state->tree).simple_declarator,
                                          "array");
                state->tree = IDL_PARAM_DCL(state->tree).param_type_spec;
                /*
                 * Put in type of parameter
                 */
                if (!xpcom_to_java_type(state)) {
                    return FALSE;
                }
                if (array)
                    fputs("[]", FILENAME(state));

            }

            state->tree = original_tree;
        }

        if (retval_param == NULL) {
            fputs("void", FILENAME(state));
        }
    }
 
    /*
     * Write method name
     */
    fprintf(FILENAME(state), " %s(", subscriptMethodName(state, method_name));
    free(method_name);

    /*
     * Write parameters
     */
    for (iterator = method->parameter_dcls; iterator != NULL; 
         iterator = IDL_LIST(iterator).next) {

        /* Skip "retval" */
        if (iterator == retval_param) {
            continue;
        }


        if (iterator != method->parameter_dcls) {
            fputs(", ", FILENAME(state));
        }
        
        state->tree = IDL_LIST(iterator).data;
        if (!xpcom_to_java_param(state)) {
            return FALSE;
        }
    }

    fputs(")", FILENAME(state));

    if (method->raises_expr) {
        IDL_tree iter = method->raises_expr;
        IDL_tree dataNode = IDL_LIST(iter).data;

        fputs(" throws ", FILENAME(state));
        fputs(IDL_IDENT(dataNode).str, FILENAME(state));
        iter = IDL_LIST(iter).next;

        while (iter) {
            dataNode = IDL_LIST(iter).data;
            fprintf(FILENAME(state), ", %s", IDL_IDENT(dataNode).str);
            iter = IDL_LIST(iter).next;
        }
    }

    fputs(";\n", FILENAME(state));

    return TRUE;
    
}


static gboolean
constant_declaration(TreeState *state)
{
    /*
     * The C++ header XPIDL module only allows for shorts and longs (ints)
     * to be constants, so we will follow the same convention
     */

    struct _IDL_CONST_DCL *declaration = &IDL_CONST_DCL(state->tree);
    const char *name = IDL_IDENT(declaration->ident).str;

    IDL_tree real_type;

    gboolean success;
    gboolean isshort = FALSE;
    GSList *doc_comments = IDL_IDENT(declaration->ident).comments;

    /* Could be a typedef; try to map it to the real type. */
    real_type = find_underlying_type(declaration->const_type);
    real_type = real_type ? real_type : declaration->const_type;
    
    /*
     * Consts must be in an interface
     */

    if (!IDL_NODE_UP(IDL_NODE_UP(state->tree)) ||
        IDL_NODE_TYPE(IDL_NODE_UP(IDL_NODE_UP(state->tree))) != 
        IDLN_INTERFACE) {

        XPIDL_WARNING((state->tree, IDL_WARNING1,
                       "A constant \"%s\" was declared outside an interface."
                       "  It was ignored.", name));

        return TRUE;
    }

    /*
     * Make sure this is a numeric short or long constant.
     */

    success = (IDLN_TYPE_INTEGER == IDL_NODE_TYPE(real_type));

    if (success) {
        /*
         * We aren't successful yet, we know it's an integer, but what *kind*
         * of integer?
         */

        switch(IDL_TYPE_INTEGER(real_type).f_type) {

        case IDL_INTEGER_TYPE_SHORT:
            /*
             * We're OK
             */
            isshort = TRUE;
            break;

        case IDL_INTEGER_TYPE_LONG:
            /*
             * We're OK
             */            
            break;
            
        default:
            /*
             * Whoops, it's some other kind of number
             */            
            success = FALSE;
        }	
    } else {
            IDL_tree_error(state->tree,
                           "const declaration \'%s\' must be of type short or long",
                           name);
            return FALSE;
    }

    if (doc_comments != NULL) {
        fputs("    ", FILENAME(state));
        printlist(FILENAME(state), doc_comments);
    }

    if (success) {
        fputc('\n', FILENAME(state));
        write_comment(state);

        fprintf(FILENAME(state), "    public static final %s %s = %d;\n",
                (isshort ? "short" : "int"),
                subscriptIdentifier(state, (char*) name),
                (int) IDL_INTEGER(declaration->const_exp).value);
    } else {
        XPIDL_WARNING((state->tree, IDL_WARNING1,
                       "A constant \"%s\" was not of type short or long."
                       "  It was ignored.", name));	
    }

    return TRUE;

}

#define ATTR_IDENT(tree) (IDL_IDENT(IDL_LIST(IDL_ATTR_DCL((tree)).simple_declarations).data))
#define ATTR_PROPS(tree) (IDL_LIST(IDL_ATTR_DCL((tree)).simple_declarations).data)
#define ATTR_TYPE_DECL(tree) (IDL_ATTR_DCL((tree)).param_type_spec)


static gboolean
attribute_declaration(TreeState *state)
{
    gboolean read_only = IDL_ATTR_DCL(state->tree).f_readonly;
    char *attribute_name = ATTR_IDENT(state->tree).str;

    gboolean method_noscript = 
        (IDL_tree_property_get(ATTR_PROPS(state->tree), "noscript") != NULL);

    gboolean method_notxpcom = 
        (IDL_tree_property_get(ATTR_PROPS(state->tree), "notxpcom") != NULL);

    GSList *doc_comments =
        IDL_IDENT(IDL_LIST(IDL_ATTR_DCL
                           (state->tree).simple_declarations).data).comments;

    if (doc_comments != NULL) {
        fputs("    ", FILENAME(state));
        printlist(FILENAME(state), doc_comments);
    }
    

#if 0
    /*
     * Disabled here because I can't verify this check against possible
     * users of the java xpidl backend.
     */
    if (!verify_attribute_declaration(state->tree))
        return FALSE;
#endif

    /* Comment */
    fputc('\n', FILENAME(state));
    write_comment(state);

    if (method_notxpcom || method_noscript)
        return TRUE;

    state->tree = ATTR_TYPE_DECL(state->tree);

    /*
     * Write access permission ("public" unless nonscriptable)
     */
    fputs("    ", FILENAME(state));
    if (!method_noscript) {
        fputs("public ", FILENAME(state));
    }

    /*
     * Write the proper Java return value for the get operation
     */
    if (!xpcom_to_java_type(state)) {
        return FALSE;
    }
    
    /*
     * Write the name of the accessor ("get") method.
     */
    fprintf(FILENAME(state), " get%c%s();\n",
            toupper(attribute_name[0]), attribute_name + 1);


    if (!read_only) {
        /* Nonscriptable methods become package-protected */
        fputs("    ", FILENAME(state));
        if (!method_noscript) {
            fputs("public ", FILENAME(state));
        }

        /*
         * Write attribute access method name and return type
         */
        fprintf(FILENAME(state), "void set%c%s(",
                toupper(attribute_name[0]), 
                attribute_name+1);
        
        /*
         * Write the proper Java type for the set operation
         */
        if (!xpcom_to_java_type(state)) {
            return FALSE;
        }

        /*
         * Write the name of the formal parameter.
         */
        fputs(" value);\n", FILENAME(state));
    }

    return TRUE;
}


static gboolean
enum_declaration(TreeState *state)
{
    XPIDL_WARNING((state->tree, IDL_WARNING1,
                   "enums not supported, enum \'%s\' ignored",
                   IDL_IDENT(IDL_TYPE_ENUM(state->tree).ident).str));
    return TRUE;
}

static gboolean
module_declaration(TreeState *state)
{
    /* do not use modules yet */
#if 0
    IDL_tree scope =
        IDL_tree_get_scope(state->tree);

    char *module_name = IDL_IDENT(IDL_MODULE(state->tree).ident).str;
    printf("\n\n I've go a module declared!!! \n name: %s \n\n",
           module_name);

    fprintf(FILENAME(state), "package %s;\n", module_name);
    state->tree = IDL_MODULE(state->tree).definition_list;

    type = IDL_NODE_TYPE(state->tree);
    printf("\n type: %d\n\n", type);

    return process_list(state);
#endif
    return TRUE;
}

backend *
xpidl_java_dispatch(void)
{
    static backend result;
    static nodeHandler table[IDLN_LAST];
    static gboolean initialized = FALSE;

    result.emit_prolog = java_prolog;
    result.emit_epilog = java_epilog;

    if (!initialized) {
        table[IDLN_INTERFACE] = interface_declaration;
        table[IDLN_LIST] = process_list;

        table[IDLN_OP_DCL] = method_declaration;
        table[IDLN_ATTR_DCL] = attribute_declaration;
        table[IDLN_CONST_DCL] = constant_declaration;

        table[IDLN_TYPE_DCL] = type_declaration;
        /*        table[IDLN_FORWARD_DCL] = forward_declaration;*/

        table[IDLN_TYPE_ENUM] = enum_declaration;
        /*        table[IDLN_MODULE] = module_declaration;*/

        initialized = TRUE;
    }

    result.dispatch_table = table;
    return &result;
}

void write_comment(TreeState *state)
{
    fprintf(FILENAME(state), "    /* ");
    IDL_tree_to_IDL(state->tree, state->ns, FILENAME(state),
                    IDLF_OUTPUT_NO_NEWLINES |
                    IDLF_OUTPUT_NO_QUALIFY_IDENTS |
                    IDLF_OUTPUT_PROPERTIES);
    fputs(" */\n", FILENAME(state));
}

char* subscriptMethodName(TreeState *state, char *str)
{
    char *sstr = NULL;
    if (strcmp(str, "toString") &&
        strcmp(str, "clone") &&
        strcmp(str, "finalize") &&
        strcmp(str, "equals") &&
        strcmp(str, "hashCode")) {
        return subscriptIdentifier(state, str);
    } 
    sstr = g_strdup_printf("%s_", str);
    return sstr;
}

char* subscriptIdentifier(TreeState *state, char *str)
{
    char *sstr = NULL;
    char *keyword = g_hash_table_lookup(KEYWORDS(state), str);
    if (keyword) {
        sstr = g_strdup_printf("%s_", keyword);
        return sstr;
    }
    return str;
}
