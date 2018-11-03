/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Igor Kushnirskiy <idk@eng.sun.com>
 */

#include "nsIAllocator.h"
#include "nsCOMPtr.h"
#include "bcJavaMarshalToolkit.h"
#include "bcIIDJava.h"
#include "bcJavaStubsAndProxies.h"
#include "nsIServiceManager.h"
#include "bcJavaGlobal.h"
#include <string.h>

jclass bcJavaMarshalToolkit::objectClass = NULL;
jclass bcJavaMarshalToolkit::objectArrayClass = NULL;
jclass bcJavaMarshalToolkit::booleanClass = NULL;
jclass bcJavaMarshalToolkit::booleanArrayClass = NULL;
jmethodID bcJavaMarshalToolkit::booleanInitMID = NULL;
jmethodID bcJavaMarshalToolkit::booleanValueMID = NULL;

jclass bcJavaMarshalToolkit::charClass = NULL;
jclass bcJavaMarshalToolkit::charArrayClass = NULL;
jmethodID bcJavaMarshalToolkit::charInitMID = NULL;
jmethodID bcJavaMarshalToolkit::charValueMID = NULL;

jclass bcJavaMarshalToolkit::byteClass = NULL;
jclass bcJavaMarshalToolkit::byteArrayClass = NULL;
jmethodID bcJavaMarshalToolkit::byteInitMID = NULL;
jmethodID bcJavaMarshalToolkit::byteValueMID = NULL;

jclass bcJavaMarshalToolkit::shortClass = NULL;
jclass bcJavaMarshalToolkit::shortArrayClass = NULL;
jmethodID bcJavaMarshalToolkit::shortInitMID = NULL;
jmethodID bcJavaMarshalToolkit::shortValueMID = NULL;

jclass bcJavaMarshalToolkit::intClass = NULL;
jclass bcJavaMarshalToolkit::intArrayClass = NULL;
jmethodID bcJavaMarshalToolkit::intInitMID = NULL;
jmethodID bcJavaMarshalToolkit::intValueMID = NULL;

jclass bcJavaMarshalToolkit::longClass = NULL;
jclass bcJavaMarshalToolkit::longArrayClass = NULL;
jmethodID bcJavaMarshalToolkit::longInitMID = NULL;
jmethodID bcJavaMarshalToolkit::longValueMID = NULL;

jclass bcJavaMarshalToolkit::floatClass = NULL;
jclass bcJavaMarshalToolkit::floatArrayClass = NULL;
jmethodID bcJavaMarshalToolkit::floatInitMID = NULL;
jmethodID bcJavaMarshalToolkit::floatValueMID = NULL;

jclass bcJavaMarshalToolkit::doubleClass = NULL;
jclass bcJavaMarshalToolkit::doubleArrayClass = NULL;
jmethodID bcJavaMarshalToolkit::doubleInitMID = NULL;
jmethodID bcJavaMarshalToolkit::doubleValueMID = NULL;

jclass bcJavaMarshalToolkit::stringClass = NULL;
jclass bcJavaMarshalToolkit::stringArrayClass = NULL;

jclass bcJavaMarshalToolkit::iidClass = NULL;
jclass bcJavaMarshalToolkit::iidArrayClass = NULL;

jmethodID bcJavaMarshalToolkit::getClassMID = NULL;



//maping from java types to *connect types

#define boolean_map PRBool
#define byte_map PRInt8
#define short_map PRInt16
#define int_map PRInt32
#define long_map PRInt64
#define float_map float
#define double_map double
#define char_map PRInt16

bcJavaMarshalToolkit::bcJavaMarshalToolkit(PRUint16 _methodIndex,
				       nsIInterfaceInfo *_interfaceInfo, jobjectArray _args, JNIEnv *_env, int  isOnServer, bcIORB *_orb) {
    env = _env;
    callSide = (isOnServer) ? onServer : onClient;
    methodIndex = _methodIndex;
    interfaceInfo = _interfaceInfo;
    interfaceInfo->GetMethodInfo(methodIndex,(const nsXPTMethodInfo **)&info);  // These do *not* make copies ***explicit bending of XPCOM rules***
    args = _args;
    if(!objectClass) {
        InitializeStatic();
        if(!objectClass) {
            //nb ? we do not have java classes. What could we do?
        }
    }
    orb = _orb;
}

bcJavaMarshalToolkit::~bcJavaMarshalToolkit() {
}

class javaAllocator : public bcIAllocator {
public:
    javaAllocator(nsIAllocator *_allocator) {
        allocator = _allocator;
    }
    virtual ~javaAllocator() {}
    virtual void * Alloc(size_t size) {
        return allocator->Alloc(size);
    }
    virtual void Free(void *ptr) {
        allocator->Free(ptr);
    }
    virtual void * Realloc(void* ptr, size_t size) {
        return allocator->Realloc(ptr,size);
    }
private:
    nsCOMPtr<nsIAllocator> allocator;
};

nsresult bcJavaMarshalToolkit::Marshal(bcIMarshaler *m, jobject retval) {
    retV = retval;
    return Marshal(m);
}

nsresult bcJavaMarshalToolkit::Marshal(bcIMarshaler *m) {
    PRUint32 paramCount = info->GetParamCount();
    nsresult r = NS_OK;
    for (unsigned int i = 0; (i < paramCount) && NS_SUCCEEDED(r); i++) {
        nsXPTParamInfo param = info->GetParam(i);
        if ((callSide == onClient  && !param.IsIn())
            || (callSide == onServer && !param.IsOut())) {
            continue;
        } else if (param.IsRetval()  && callSide == onServer) {
            r = MarshalElement(m, retV, PR_FALSE, &param, XPTType2bcXPType(param.GetType().TagPart()), i);
        } else {
            jobject object = env->GetObjectArrayElement(args,i);
            EXCEPTION_CHECKING(env);
            r = MarshalElement(m, object, param.IsOut(), &param, XPTType2bcXPType(param.GetType().TagPart()), i);
        }
    }
    return r;
    
}

nsresult bcJavaMarshalToolkit::UnMarshal(bcIUnMarshaler *um, jobject *retval) {
    nsresult r = UnMarshal(um);
    *retval = retV;
    return r;
}

nsresult bcJavaMarshalToolkit::UnMarshal(bcIUnMarshaler *um) {
    PRLogModuleInfo * log = bcJavaGlobal::GetLog();
    PR_LOG(log, PR_LOG_DEBUG,("--nsresult bcJavaMarshalToolkit::UnMarshal\n"));
    bcIAllocator * allocator = new javaAllocator(nsAllocator::GetGlobalAllocator());
    PRUint32 paramCount = info->GetParamCount();
    retV = NULL;
    jobject value;
    for (unsigned int i = 0; i < paramCount; i++) {
        nsXPTParamInfo param = info->GetParam(i);
        PRBool isOut = param.IsOut();
        nsXPTType type = param.GetType();
        if (param.IsRetval() && callSide == onServer) {
            PR_LOG(log,PR_LOG_DEBUG,("** bcJavaMarshalToolkit::UnMarshal skipping retval\n"));            
            PR_LOG(log,PR_LOG_DEBUG,("**unmarshall: call side: %d\n", callSide));
            continue;
        }
        if ( (callSide == onServer && !param.IsIn()
              || (callSide == onClient && !param.IsOut()))){
            if (callSide == onServer
                && isOut) { //we need to allocate memory for out parametr
                UnMarshalElement(&value, i, NULL, 1, &param, XPTType2bcXPType(type.TagPart()),allocator);
                env->SetObjectArrayElement(args,i,value);
                EXCEPTION_CHECKING(env);
            }
            continue;
        }
        if (param.IsRetval()) {
            UnMarshalElement(&value, i, um, PR_FALSE, &param, XPTType2bcXPType(type.TagPart()),allocator);
            retV = value;
        } else {
            if (isOut) {
                value = env->GetObjectArrayElement(args,i);
                EXCEPTION_CHECKING(env);
            }
            UnMarshalElement(&value, i, um, isOut, &param, XPTType2bcXPType(type.TagPart()),allocator);
            env->SetObjectArrayElement(args,i,value);
            EXCEPTION_CHECKING(env);
        }
    }
    delete allocator;
    return NS_OK;
}

/* 
 *JNIEnv *env; jobject value; PRBool isOut; ArrayModifier modifier; bcIMarshaler *m;
 * should be defined before calling this mavro
 *
 */

#define MARSHAL_SIMPLE_ELEMENT(_type_,_Type_)                                                       \
    do {                                                                                            \
        int indexInArray;                                                                           \
        j##_type_ data;                                                                             \
        if (! isOut                                                                                 \
            && (modifier == none)) {                                                                \
            data = env->Call##_Type_##Method(value,_type_##ValueMID);                               \
            EXCEPTION_CHECKING(env);                                                                \
        } else if (isOut && (modifier == array)) {                                                  \
            /* could not happend. We take care about it in T_ARRAY case */                          \
        } else if (modifier == arrayElement                                                         \
                   || (isOut && (modifier == none))) {                                              \
            indexInArray = (modifier == arrayElement) ? ind : 0;                                    \
            env->Get##_Type_##ArrayRegion((j##_type_##Array)value, indexInArray, 1, &data);         \
            EXCEPTION_CHECKING(env);                                                                \
        }                                                                                           \
        _type_##_map tmpData = data;                                                                \
        m->WriteSimple(&tmpData,type);                                                              \
    } while (0)

nsresult
bcJavaMarshalToolkit::MarshalElement(bcIMarshaler *m, jobject value,  PRBool isOut, nsXPTParamInfo * param,
                                     bcXPType type, uint8 ind, ArrayModifier modifier) {
    nsresult r = NS_OK;
    PRLogModuleInfo * log = bcJavaGlobal::GetLog();

    switch(type) {
        case  bc_T_I8:
        case  bc_T_U8:
            {
                MARSHAL_SIMPLE_ELEMENT(byte,Byte);
                break;
            }
        case bc_T_I16:
        case bc_T_U16:
            {
                MARSHAL_SIMPLE_ELEMENT(short,Short);
                break;
            };
        case bc_T_I32:
        case bc_T_U32:
            {
                MARSHAL_SIMPLE_ELEMENT(int,Int);
                break;
            }
        case bc_T_I64:
        case bc_T_U64:
            {
                MARSHAL_SIMPLE_ELEMENT(long,Long);
                break;
            }
        case bc_T_FLOAT:
            {
                MARSHAL_SIMPLE_ELEMENT(float,Float);
                break;
            }

        case bc_T_DOUBLE:
            {
                MARSHAL_SIMPLE_ELEMENT(double,Double);
                break;
            }
        case bc_T_BOOL:
            {
                MARSHAL_SIMPLE_ELEMENT(boolean,Boolean);
                break;
            }
        case bc_T_CHAR:
        case bc_T_WCHAR:
            {
                MARSHAL_SIMPLE_ELEMENT(char,Char);
                break;
            }
        case bc_T_CHAR_STR:
        case bc_T_WCHAR_STR:                        //nb not sure about this
            {
                int indexInArray;
                jstring data = NULL;
                if (! isOut 
                    && (modifier == none)) {
                    data = (jstring)value;
                } else if (modifier == arrayElement                                                         
                           || (isOut && (modifier == none))) {                                              
                    indexInArray = (modifier == arrayElement) ? ind : 0;                                    
                    data = (jstring)env->GetObjectArrayElement((jobjectArray)value,indexInArray);
                    EXCEPTION_CHECKING(env);
                }                                                                                           
                char * str = NULL;
                char * tmpStr = NULL;
                if (data) {
                    size_t length = 0;
                    if (type == bc_T_CHAR_STR) {
                        str = (char*)env->GetStringUTFChars((jstring)data,NULL);
                        length = strlen(str)+1;
                        tmpStr = str;
                    } else {
                        str = (char*)env->GetStringChars((jstring)data,NULL);
                        length = env->GetStringLength((jstring)data);
                        length *= 2; //nb
                        length += 2;
                        tmpStr = new char[length];
                        memcpy(tmpStr,str,length-2);
                        tmpStr[length-1] = tmpStr[length-2] = 0;
                        {
                            for (int i = 0; i < length && type == bc_T_WCHAR_STR; i++) {
                                char c = tmpStr[i];
                                PR_LOG(log,PR_LOG_DEBUG,("--[c++] bcJavaMarshalToolkit::MarshalElement T_WCHAR_STR [%d] = %d %c\n",i,c,c));
                            }
                        }

                    }
                    EXCEPTION_CHECKING(env);
                    m->WriteString(tmpStr,length);
                    if (type == bc_T_CHAR_STR) {
                        env->ReleaseStringUTFChars(data,str);
                    } else {
                        env->ReleaseStringChars(data,(const jchar*)str);
                        delete[] tmpStr;
                    }
                    EXCEPTION_CHECKING(env);
                } else {
                    m->WriteString(str,0);
                }
                break;
            }
        case bc_T_IID:
            {
                int indexInArray;
                jobject data = NULL;
                if (! isOut 
                    && (modifier == none)) {
                    data = value;
                } else if (modifier == arrayElement                                                         
                           || (isOut && (modifier == none))) {                                              
                    indexInArray = (modifier == arrayElement) ? ind : 0;                                    
                    data = (jstring)env->GetObjectArrayElement((jobjectArray)value,indexInArray);
                    EXCEPTION_CHECKING(env);
                }
                nsIID iid = bcIIDJava::GetIID(data);
                m->WriteSimple(&iid, type);
                break;
            }

        case bc_T_INTERFACE:
            {
                int indexInArray;
                jobject data = NULL;
                PR_LOG(log,PR_LOG_DEBUG,("--marshalElement we got interface\n"));
                bcOID oid = 0;
                nsIID *iid;
                if (! isOut
                    && (modifier == none)) {
                    data = value;
                } else if (modifier == arrayElement
                           || (isOut && (modifier == none))) {
                    indexInArray = (modifier == arrayElement) ? ind : 0;                                    
                    data = env->GetObjectArrayElement((jobjectArray)value,indexInArray);
                    EXCEPTION_CHECKING(env);
                }
                if (data != NULL) {
                    nsCOMPtr<bcIJavaStubsAndProxies> javaStubsAndProxies = do_GetService(BC_JAVASTUBSANDPROXIES_ContractID,&r);
                    if (NS_FAILED(r)) {
                        return NS_ERROR_FAILURE;
                    }

                    javaStubsAndProxies->GetOID(data, orb, &oid);
                }
                m->WriteSimple(&oid,type);

                if (param->GetType().TagPart() == nsXPTType::T_INTERFACE) {
                    if(NS_FAILED(r = interfaceInfo->
                                 GetIIDForParam(methodIndex, param, &iid))) {
                        return r;
                    }
                    m->WriteSimple(iid,bc_T_IID);
                } else {
                    uint8 argnum;
                    if (NS_FAILED(r = interfaceInfo->GetInterfaceIsArgNumberForParam(methodIndex,
                                                                                     param, &argnum))) {
                        return r;
                    }
                    const nsXPTParamInfo& arg_param = info->GetParam(argnum);
                    jobject object = env->GetObjectArrayElement(args,argnum);
                    EXCEPTION_CHECKING(env);
                    r = MarshalElement(m, object, arg_param.IsOut(),(nsXPTParamInfo*)&arg_param, 
                                       XPTType2bcXPType(arg_param.GetType().TagPart()), (uint8)0);
                }
                break;
            }
 	    case bc_T_ARRAY:
            {
                nsXPTType datumType;
                if(NS_FAILED(interfaceInfo->GetTypeForParam(methodIndex, param, 1,&datumType))) {
                    return NS_ERROR_FAILURE;
                }
                bcXPType type = XPTType2bcXPType(datumType.TagPart());
                jobject arrayValue = value;
                if (isOut) {
                    arrayValue = env->GetObjectArrayElement((jobjectArray)value,0);
                    EXCEPTION_CHECKING(env);
                } 
                if (m != NULL) {
                    PRUint32 arraySize = (arrayValue == NULL) ? 0 : env->GetArrayLength((jarray)arrayValue);
                    EXCEPTION_CHECKING(env);
                    m->WriteSimple(&arraySize,bc_T_U32);
                    for (PRUint32 i = 0; i < arraySize; i++) {
                        MarshalElement(m,arrayValue,PR_FALSE,param,type,i,arrayElement);
                    }
                }
                break;
            }
	    default:
                PR_LOG(log,PR_LOG_DEBUG,("--it should not happend\n"));
            ;
    }
    return r;
}


#define UNMARSHAL_SIMPLE_ELEMENT(_type_,_Type_) \
             do {                         \
                int indexInArray;      \
                j##_type_ data;            \
                _type_##_map tmpData;      \
                if (um) {              \
                    um->ReadSimple(&tmpData,type); \
                    data = tmpData;             \
                }                               \
                if ( ! isOut                    \
                     && (modifier == none) ) {  \
                    *value = env->NewObject(_type_##Class,_type_##InitMID,data);   \
                    EXCEPTION_CHECKING(env);                                       \
                } else if (isOut && (modifier == array)) {                         \
                      *value = env->NewObjectArray(1, _type_##ArrayClass, NULL);   \
                } else if ( (isOut && callSide == onServer)                        \
                           || (modifier == array)) {                               \
                               int arraySize;                                         \
                               arraySize = (modifier == array) ? ind : 1;             \
                               *value = env->New##_Type_##Array(arraySize);                 \
                               EXCEPTION_CHECKING(env);                            \
                }                                                          \
                if (modifier == arrayElement                               \
                    || (isOut && (modifier == none))                       \
                   ) {                                                     \
                     indexInArray = (modifier == arrayElement) ? ind : 0;  \
                     env->Set##_Type_##ArrayRegion((j##_type_##Array)*value, indexInArray, 1, &data); \
                     EXCEPTION_CHECKING(env);                                                         \
                }                                                                                     \
            } while(0)

nsresult
bcJavaMarshalToolkit::UnMarshalElement(jobject *value, uint8 ind, bcIUnMarshaler *um, int isOut, nsXPTParamInfo * param,
                                       bcXPType type, bcIAllocator *allocator, ArrayModifier modifier) {
    PRLogModuleInfo *log = bcJavaGlobal::GetLog();
    switch(type) {
        case  bc_T_I8:
        case  bc_T_U8:
            {
                UNMARSHAL_SIMPLE_ELEMENT(byte,Byte);
                break;
            }
        case bc_T_I16:
        case bc_T_U16:
            {
                UNMARSHAL_SIMPLE_ELEMENT(short,Short);
                break;
            };
        case bc_T_I32:
        case bc_T_U32:
            {
                UNMARSHAL_SIMPLE_ELEMENT(int,Int);
                break;
            }
        case bc_T_I64:
        case bc_T_U64:
            {
                UNMARSHAL_SIMPLE_ELEMENT(long,Long);
                break;
            }
        case bc_T_FLOAT:
            {
                UNMARSHAL_SIMPLE_ELEMENT(float,Float);
                break;
            }

        case bc_T_DOUBLE:
            {
                UNMARSHAL_SIMPLE_ELEMENT(double,Double);
                break;
            }
        case bc_T_BOOL:
            {
                UNMARSHAL_SIMPLE_ELEMENT(boolean,Boolean);
                break;
            }
        case bc_T_CHAR:
        case bc_T_WCHAR:
            {
                UNMARSHAL_SIMPLE_ELEMENT(char,Char);
                break;
            }
        case bc_T_CHAR_STR:
        case bc_T_WCHAR_STR:                        //nb not sure about this
            {
                int indexInArray;
                size_t size;
                jstring data = NULL;
                if (um) {
                    char *str;
                    um->ReadString(&str,&size,allocator);
                    if (str != NULL) {
                        {
                            for (int i = 0; i < size && type == bc_T_WCHAR_STR; i++) {
                                char c = str[i];
                                PR_LOG(log, PR_LOG_DEBUG,("--[c++] bcJavaMarshalToolkit::UnMarshalElement T_WCHAR_STR [%d] = %d %c\n",i,c,c));
                            }
                        }
                        if (type == bc_T_CHAR_STR) {
                            data = env->NewStringUTF((const char*)str);
                        } else {
                            size-=2; size/=2;
                            data = env->NewString((const jchar*)str,size);
                        }
                        allocator->Free(str);
                        EXCEPTION_CHECKING(env);
                    }
                }
                if ( ! isOut
                     && (modifier == none) ) {
                    *value = data;
                } else if (isOut && (modifier == array)) {
                    *value = env->NewObjectArray(1, stringArrayClass, NULL);
                    EXCEPTION_CHECKING(env);
                } else if ( (isOut && callSide == onServer)
                           || (modifier == array)) {
                    int arraySize;
                    arraySize = (modifier == array) ? ind : 1;
                    *value = env->NewObjectArray(arraySize,stringClass,NULL);
                    EXCEPTION_CHECKING(env);
                }
                if (modifier == arrayElement
                    || (isOut && (modifier == none))
                    ) {
                    indexInArray = (modifier == arrayElement) ? ind : 0;
                    env->SetObjectArrayElement((jobjectArray)*value, indexInArray, data);
                    EXCEPTION_CHECKING(env);
                }
                break;
            }
        case bc_T_IID:
            {
                int indexInArray = 0;
                jobject data = NULL;
                if (um) {
                    nsIID iid;
                    um->ReadSimple(&iid,type);
                    data = bcIIDJava::GetObject(&iid);
                }
                if ( ! isOut
                     && (modifier == none) ) {
                    *value = data;
                } else if (isOut && (modifier == array)) {
                    *value = env->NewObjectArray(1, iidArrayClass, NULL);
                    EXCEPTION_CHECKING(env);
                } else if ( (isOut && callSide == onServer)
                           || (modifier == array)) {
                    int arraySize;
                    arraySize = (modifier == array) ? ind : 1;
                    *value = env->NewObjectArray(arraySize,iidClass,NULL);
                    EXCEPTION_CHECKING(env);
                }
                if (modifier == arrayElement
                    || (isOut && (modifier == none))
                    ) {
                    indexInArray = (modifier == arrayElement) ? ind : 0;
                    env->SetObjectArrayElement((jobjectArray)*value, indexInArray, data);
                    EXCEPTION_CHECKING(env);
                }
                break;
            }

        case bc_T_INTERFACE:
            {
                PR_LOG(log, PR_LOG_DEBUG,("--[c++] bcJavaMarshalToolkit::UnMarshalElement we have an interface\n"));
                int indexInArray = 0;
                jobject data = NULL;
                bcOID oid = 0;
                nsIID iid;
                nsresult r;
                jclass clazz = objectClass;
                if (um) {
                    um->ReadSimple(&oid,type);
                    um->ReadSimple(&iid,bc_T_IID);
                    PR_LOG(log,PR_LOG_DEBUG,("%d oid\n",(int) oid));
                    nsCOMPtr<bcIJavaStubsAndProxies> javaStubsAndProxies = do_GetService(BC_JAVASTUBSANDPROXIES_ContractID,&r);
                    if (NS_FAILED(r)) {
                        return NS_ERROR_FAILURE;
                    }

                    if (oid != 0) {
                        javaStubsAndProxies->GetProxy(oid, iid, orb, &data);
                    }
                    javaStubsAndProxies->GetInterface(iid,&clazz);
                }

                if ( ! isOut
                     && (modifier == none) ) {
                    *value = data;
                } else if ( isOut && (modifier == array)) { //we are  creating type[][]
                    jobject arrayObject;
                    arrayObject = env->NewObjectArray(1,clazz,NULL);
                    EXCEPTION_CHECKING(env);
                    jclass arrayClass = (jclass) env->CallObjectMethod(arrayObject,getClassMID); //nb how to to it better ?
                    EXCEPTION_CHECKING(env);
                    *value = env->NewObjectArray(1, arrayClass, NULL);
                    EXCEPTION_CHECKING(env);
                } else if ( (isOut && callSide == onServer)
                           || (modifier == array)) {
                    int arraySize;
                    arraySize = (modifier == array) ? ind : 1;
                    *value = env->NewObjectArray(arraySize,clazz,NULL);
                    EXCEPTION_CHECKING(env);
                }
                if (modifier == arrayElement
                    || (isOut && (modifier == none))
                    ) {
                    indexInArray = (modifier == arrayElement) ? ind : 0;
                    env->SetObjectArrayElement((jobjectArray)*value, indexInArray, data);
                    EXCEPTION_CHECKING(env);
                }
                    
                break;
            }
 	    case bc_T_ARRAY:
            {
                nsXPTType datumType;
                if(NS_FAILED(interfaceInfo->GetTypeForParam(methodIndex, param, 1,&datumType))) {
                    return NS_ERROR_FAILURE;
                }
                bcXPType type = XPTType2bcXPType(datumType.TagPart());
                if (isOut && callSide == onServer) {
                    UnMarshalElement(value,ind,NULL,isOut,param,type,allocator,array);
                }
                if (um != NULL) {
                    PRUint32 arraySize;
                    um->ReadSimple(&arraySize,bc_T_U32);
                    jobject arrayValue = NULL;
                    UnMarshalElement(&arrayValue,arraySize,NULL,0,param,type,allocator,array);
                    if (isOut) {
                        env->SetObjectArrayElement((jobjectArray)*value,0,arrayValue);
                        EXCEPTION_CHECKING(env);
                    } else {
                        *value = arrayValue;
                    }
                    for (PRUint32 i = 0; i < arraySize; i++) {
                        UnMarshalElement(&arrayValue,i,um,0,param,type,allocator,arrayElement);
                    }
                }
                break;


            }
	    default:
            ;
    }
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        return NS_ERROR_FAILURE;
    }

    return NS_OK;

}

bcXPType bcJavaMarshalToolkit::XPTType2bcXPType(uint8 type) {
    switch(type) {
        case nsXPTType::T_I8  :
            return bc_T_I8;
        case nsXPTType::T_U8  :
            return bc_T_U8;
        case nsXPTType::T_I16 :
            return bc_T_I16;
        case nsXPTType::T_U16 :
            return bc_T_U16;
        case nsXPTType::T_I32 :
            return bc_T_I32;
        case nsXPTType::T_U32 :
            return bc_T_U32;
        case nsXPTType::T_I64 :
            return bc_T_I64;
        case nsXPTType::T_U64 :
            return bc_T_U64;
        case nsXPTType::T_FLOAT  :
            return bc_T_FLOAT;
        case nsXPTType::T_DOUBLE :
            return bc_T_DOUBLE;
        case nsXPTType::T_BOOL   :
            return bc_T_BOOL;
        case nsXPTType::T_CHAR   :
            return bc_T_CHAR;
        case nsXPTType::T_WCHAR  :
            return bc_T_WCHAR;
        case nsXPTType::T_IID  :
            return bc_T_IID;
        case nsXPTType::T_CHAR_STR  :
        case nsXPTType::T_PSTRING_SIZE_IS:
            return bc_T_CHAR_STR;
        case nsXPTType::T_WCHAR_STR :
        case nsXPTType::T_PWSTRING_SIZE_IS:
            return bc_T_WCHAR_STR;
        case nsXPTType::T_INTERFACE :
        case nsXPTType::T_INTERFACE_IS :
            return bc_T_INTERFACE;
        case nsXPTType::T_ARRAY:
            return bc_T_ARRAY;
        default:
            return bc_T_UNDEFINED;
    }
}


void bcJavaMarshalToolkit::InitializeStatic() {
    jclass clazz;
    if (!(clazz = env->FindClass("java/lang/Object"))
        || !(objectClass = (jclass) env->NewGlobalRef(clazz))
        ) {
        return;
    }

    if (!(clazz = env->FindClass("java/lang/Boolean"))
        || !(booleanClass = (jclass) env->NewGlobalRef(clazz))
        || !(clazz = env->FindClass("[Z"))
        || !(booleanArrayClass = (jclass) env->NewGlobalRef(clazz))
        ) {
        DeInitializeStatic();
        return;
    }

    if (!(clazz = env->FindClass("java/lang/Character"))
        || !(charClass = (jclass) env->NewGlobalRef(clazz))
        || !(clazz = env->FindClass("[C"))
        || !(charArrayClass = (jclass) env->NewGlobalRef(clazz))
        ) {
        DeInitializeStatic();
        return;
    }
    if (!(clazz = env->FindClass("java/lang/Byte"))
        || !(byteClass = (jclass) env->NewGlobalRef(clazz))
        || !(clazz = env->FindClass("[B"))
        || !(byteArrayClass = (jclass) env->NewGlobalRef(clazz))
        ) {
        DeInitializeStatic();
        return;
    }
    if (!(clazz = env->FindClass("java/lang/Short"))
        || !(shortClass = (jclass) env->NewGlobalRef(clazz))
        || !(clazz = env->FindClass("[[S"))
        || !(shortArrayClass = (jclass) env->NewGlobalRef(clazz))
        ) {
        DeInitializeStatic();
        return;
    }
    if (!(clazz = env->FindClass("java/lang/Integer"))
        || !(intClass = (jclass) env->NewGlobalRef(clazz))
        || !(clazz = env->FindClass("[I"))
        || !(intArrayClass = (jclass) env->NewGlobalRef(clazz))
        ) {
        DeInitializeStatic();
        return;
    }
    if (!(clazz = env->FindClass("java/lang/Long"))
        || !(longClass = (jclass) env->NewGlobalRef(clazz))
        || !(clazz = env->FindClass("[J"))
        ) {
        DeInitializeStatic();
        return;
    }
    if (!(clazz = env->FindClass("java/lang/Float"))
        || !(floatClass = (jclass) env->NewGlobalRef(clazz))
        || !(clazz = env->FindClass("[F"))
        || !(floatArrayClass = (jclass) env->NewGlobalRef(clazz))
        ) {
        DeInitializeStatic();
        return;
    }
    if (!(clazz = env->FindClass("java/lang/Double"))
        || !(doubleClass = (jclass) env->NewGlobalRef(clazz))
        || !(clazz = env->FindClass("[D"))
        || !(doubleArrayClass = (jclass) env->NewGlobalRef(clazz))
        ) {
        DeInitializeStatic();
        return;
    }

    if (!(clazz = env->FindClass("java/lang/String"))
        || !(stringClass = (jclass) env->NewGlobalRef(clazz))
        || !(clazz = env->FindClass("[Ljava/lang/String;"))
        || !(stringArrayClass = (jclass) env->NewGlobalRef(clazz))
        ) {
        DeInitializeStatic();
        return;
    }

    if (!(clazz = env->FindClass("org/mozilla/xpcom/IID"))
        || !(iidClass = (jclass) env->NewGlobalRef(clazz))
        || !(clazz = env->FindClass("[Lorg/mozilla/xpcom/IID;"))
        || !(iidArrayClass = (jclass) env->NewGlobalRef(clazz))
        ) {
        DeInitializeStatic();
        return;
    }

    if (!(booleanInitMID = env->GetMethodID(booleanClass,"<init>","(Z)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(booleanValueMID = env->GetMethodID(booleanClass,"booleanValue","()Z"))) {
        DeInitializeStatic();
        return;
    }

    if (!(charInitMID = env->GetMethodID(charClass,"<init>","(C)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(charValueMID = env->GetMethodID(charClass,"charValue","()C"))) {
        DeInitializeStatic();
        return;
    }

    if (!(byteInitMID = env->GetMethodID(byteClass,"<init>","(B)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(byteValueMID = env->GetMethodID(byteClass,"byteValue","()B"))) {
        DeInitializeStatic();
        return;
    }
    if (!(shortInitMID = env->GetMethodID(shortClass,"<init>","(S)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(shortValueMID = env->GetMethodID(shortClass,"shortValue","()S"))) {
        DeInitializeStatic();
        return;
    }

    if (!(intInitMID = env->GetMethodID(intClass,"<init>","(I)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(intValueMID = env->GetMethodID(intClass,"intValue","()I"))) {
        DeInitializeStatic();
        return;
    }

    if (!(longInitMID = env->GetMethodID(longClass,"<init>","(J)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(longValueMID = env->GetMethodID(longClass,"longValue","()J"))) {
        DeInitializeStatic();
        return;
    }

    if (!(floatInitMID = env->GetMethodID(floatClass,"<init>","(F)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(floatValueMID = env->GetMethodID(floatClass,"floatValue","()F"))) {
        DeInitializeStatic();
        return;
    }

    if (!(doubleInitMID = env->GetMethodID(doubleClass,"<init>","(D)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(doubleValueMID = env->GetMethodID(doubleClass,"doubleValue","()D"))) {
        DeInitializeStatic();
        return;
    }

    if (!(getClassMID = env->GetMethodID(objectClass,"getClass","()Ljava/lang/Class;"))) {
        DeInitializeStatic();
        return;
    }


}

void bcJavaMarshalToolkit::DeInitializeStatic() {     //nb need to do
    PRLogModuleInfo * log = bcJavaGlobal::GetLog();
    PR_LOG(log, PR_LOG_DEBUG,("--[c++]void bcJavaMarshalToolkit::DeInitializeStatic() - boomer \n"));
}

