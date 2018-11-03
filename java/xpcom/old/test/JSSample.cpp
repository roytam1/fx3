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
 * The Initial Developer of the Original Code is Frank
 * Mitchell. Portions created by Frank Mitchell are
 * Copyright (C) 1999 Frank Mitchell. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *    Frank Mitchell (frank.mitchell@sun.com)
 */
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#include <iostream.h>
#include "pratom.h"
#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "JSISample.h"
#include "JSSample.h"

class JSSample: public JSISample {  
public:  
  // Constructor and Destructor  
  JSSample();  
  virtual ~JSSample();  

  // nsISupports methods  
  NS_DECL_ISUPPORTS

  // JSISample methods
  NS_DECL_JSISAMPLE

private:
  /* attribute long someInt; */
  PRInt32 someInt_;

  /* attribute boolean someBool; */
  PRBool someBool_;

  /* readonly attribute long roInt; */
  PRInt32 roInt_;

  /* attribute double someDouble; */
  double someDouble_;

  /* attribute string someName; */
  char *someName_;

  /* readonly attribute string roString; */
  char *roString_;
};  

// Globals, need to check if safe to unload module
//static PRInt32 gLockCnt = 0; 
// static PRInt32 gInstanceCnt = 0; 

static NS_DEFINE_CID(kSampleCID, JS_SAMPLE_CID);

/* 
 * JSSampleClass Declaration: see JSSample_private.h
 */  

/*  
 * JSComplex Declaration  
 */  
class JSComplex : public JSIComplex {
public: 
  // Constructor and Destructor  
  JSComplex();  
  JSComplex(int aReal, int aImaginary);
  virtual ~JSComplex();  

  // nsISupports methods  
  NS_DECL_ISUPPORTS

  // nsISupports methods  
  NS_DECL_JSICOMPLEX

private:
  PRInt32 real_;
  PRInt32 imaginary_;
};

/*  
 * JSSampleFactory Declaration  
 */  

class JSSampleFactory: public nsIFactory {
  public:  
  JSSampleFactory();  
  virtual ~JSSampleFactory();  

  // nsISupports methods  
  NS_DECL_ISUPPORTS

  // nsIFactory methods  
  NS_IMETHOD CreateInstance(nsISupports *aOuter,  
                            const nsIID &aIID,  
                            void **aResult);  

  NS_IMETHOD LockFactory(PRBool aLock);  
};

/*  
 * JSSample Implementation  
 */  

JSSample::JSSample()  
{  
  // Zero reference counter
  NS_INIT_ISUPPORTS();
  // PR_AtomicIncrement(&gInstanceCnt);
}

JSSample::~JSSample()  
{  
  // Make sure there are no dangling pointers to us,
  // debug only
  NS_ASSERTION(mRefCnt == 0,"Wrong ref count");
  // PR_AtomicDecrement(&gInstanceCnt);
}  


// Use the official macros to implement nsISupports
NS_IMPL_ISUPPORTS1(JSSample, JSISample)


NS_IMETHODIMP JSSample::PrintStats()  
{  
  cout << "Stats would go here" << endl;
  return NS_OK;
}

  /* attribute boolean someInt; */
NS_IMETHODIMP JSSample::GetSomeInt(PRInt32 *aSomeInt) {
  *aSomeInt = someInt_;
  cout << "--> JSSample::GetSomeInt() : " << *aSomeInt << endl;
  return NS_OK;
}

NS_IMETHODIMP JSSample::SetSomeInt(PRInt32 aSomeInt) {
  cout << "--> JSSample::SetSomeInt(" << aSomeInt << ")" << endl;
  someInt_ = aSomeInt;
  return NS_OK;
}

  /* attribute boolean someBool; */
NS_IMETHODIMP JSSample::GetSomeBool(PRInt32 *aSomeBool) {
  *aSomeBool = someBool_;
  cout << "--> JSSample::GetSomeBool() : " << *aSomeBool << endl;
  return NS_OK;
}

NS_IMETHODIMP JSSample::SetSomeBool(PRInt32 aSomeBool) {
  cout << "--> JSSample::SetSomeBool(" << aSomeBool << ")" << endl;
  someBool_ = aSomeBool;
  return NS_OK;
}

/* readonly attribute long roInt; */
NS_IMETHODIMP JSSample::GetRoInt(PRInt32 *aRoInt) {
  *aRoInt = roInt_;
  cout << "--> JSSample::GetRoInt() : " << *aRoInt << endl;
  return NS_OK;
}


/* attribute double someDouble; */
NS_IMETHODIMP JSSample::GetSomeDouble(double *aSomeDouble) {
  *aSomeDouble = someDouble_;
  cout << "--> JSSample::GetSomeDouble() : " << *aSomeDouble << endl;
  return NS_OK;
}

NS_IMETHODIMP JSSample::SetSomeDouble(double aSomeDouble) {
  cout << "--> JSSample::SetSomeDouble(" << aSomeDouble << ")" << endl;
  someDouble_ = aSomeDouble;
  return NS_OK;
}


  /* attribute string someName; */
NS_IMETHODIMP JSSample::GetSomeName(char * *aSomeName) {
  *aSomeName = new char[strlen(someName_) + 1];
  strcpy(*aSomeName, someName_);
  cout << "--> JSSample::GetSomeName() : '" << *aSomeName << '\'' << endl;
  return NS_OK;
}

NS_IMETHODIMP JSSample::SetSomeName(const char * aSomeName) {
  cout << "--> JSSample::SetSomeName('" << aSomeName << "')" << endl;
  if (aSomeName != someName_) {
    delete someName_;
    someName_ = new char[strlen(aSomeName) + 1];
    strcpy(someName_, aSomeName);
  }
  return NS_OK;
}


  /* readonly attribute string roString; */
NS_IMETHODIMP JSSample::GetRoString(char * *aRoString) {
  *aRoString = new char[sizeof("Read Only String") + 1];
  strcpy(*aRoString, "Read Only String");
  cout << "--> JSSample::GetRoString() : " << *aRoString << '\'' << endl;
  return NS_OK;
}

  /* void TakeInt (in long anInt); */
NS_IMETHODIMP JSSample::TakeInt(PRInt32 anInt) {
  cout << "--> JSSample::TakeInt(" << anInt << ")" << endl;
  return NS_OK;
}


  /* long GiveInt (); */
NS_IMETHODIMP JSSample::GiveInt(PRInt32 *_retval) {
  *_retval = 42;
  cout << "--> JSSample::GiveInt() : " << *_retval << endl;
  return NS_OK;
}

  /* long GiveAndTake (inout long anInt); */
NS_IMETHODIMP JSSample::GiveAndTake(PRInt32 *anInt, PRInt32 *_retval) {
  cout << "--> JSSample::GiveAndTake(" << *anInt << ") => ";
  *_retval = *anInt;
  *anInt *= 2;
  cout << "( " << *anInt << ") : " << *_retval << endl;
  return NS_OK;
}

  /* string TooManyArgs (in short oneInt, in short twoInt, inout long redInt, out short blueInt, in double orNothing, in long johnSilver, in boolean algebra); */
NS_IMETHODIMP JSSample::TooManyArgs(PRInt16 oneInt, 
                                    PRInt16 twoInt, 
                                    PRInt32 *redInt, 
                                    PRInt16 *blueInt, 
                                    double orNothing, 
                                    PRInt64 johnSilver, 
                                    PRBool algebra, 
                                    char **_retval) {
  cout << "--> JSSample::TooManyArgs(" 
       << oneInt << ", " 
       << twoInt << ", " 
       << *redInt << ", " 
       << '-' << ", " 
       << orNothing << ", " 
       << johnSilver << ", " 
       << algebra << ") => ";
  *redInt = oneInt + twoInt;
  *blueInt = oneInt - twoInt;
  this->GetRoString(_retval);
  cout << "("
       << oneInt << ", " 
       << twoInt << ", " 
       << *redInt << ", " 
       << *blueInt << ", " 
       << orNothing << ", " 
       << johnSilver << ", " 
       << algebra
       << ") : " 
       << *_retval << endl;
  return NS_OK;
}

  /* string CatStrings (in string str1, in string str2); */
NS_IMETHODIMP JSSample::CatStrings(const char *str1, const char *str2, char **_retval) {
  cout << "--> JSSample::CatStrings(" << str1 << ", "  << str2 << ") : ";
  *_retval = new char[strlen(str1) + strlen(str2) + 1];
  strcpy(*_retval, str1);
  strcat(*_retval, str2);
  cout << '\'' << *_retval << '\'' << endl;
  return NS_OK;
}

  /* void AppendString (inout string str1, in string str2); */
NS_IMETHODIMP JSSample::AppendString(char **str1, const char *str2) {
  cout << "--> JSSample::AppendString('" << *str1 << "', '" << str2 << "') => ";
  char* tmp = new char[strlen(*str1) + strlen(str2) + 1];
  strcpy(tmp, *str1);
  strcat(tmp, str2);
  *str1 = tmp;    // XXX potential memory leak
  cout << "('" << *str1 << "')" << endl;
  return NS_OK;
}

  /* JSIComplex AddComplex (in JSIComplex complex1, in JSIComplex complex2); */
NS_IMETHODIMP JSSample::AddComplex(JSIComplex *complex1, JSIComplex *complex2, JSIComplex **_retval) {
  cout << "--> JSSample::AddComplex(" 
       << complex1 << ", " 
       << complex2 << ") : ";

  PRInt32 r1, i1, r2, i2;
  complex1->GetReal(&r1);
  complex1->GetImaginary(&i1);
  complex2->GetReal(&r2);
  complex2->GetImaginary(&i2);
  *_retval = new JSComplex(r1 + r2, i1 + i2);

  cout << *_retval << endl;
  return NS_OK;
}

  /* JSIComplex NewComplex (in long aReal, in long aImaginary); */
NS_IMETHODIMP JSSample::NewComplex(PRInt32 aReal, PRInt32 aImaginary, JSIComplex **_retval) {
  cout << "--> JSSample::AddComplex(" 
       << aReal << ", " 
       << aImaginary << ") : ";

  *_retval = new JSComplex(aReal, aImaginary);

  cout << *_retval << endl;
  return NS_OK;
}


  /* void AddInPlace (inout JSIComplex complex1, in JSIComplex complex2); */
NS_IMETHODIMP JSSample::AddInPlace(JSIComplex **complex1, JSIComplex *complex2) {
  cout << "--> JSSample::AddInPlace('" 
       << *complex1 << ", " 
       << complex2 << ") => ";

  PRInt32 r1, i1, r2, i2;
  (*complex1)->GetReal(&r1);
  (*complex1)->GetImaginary(&i1);
  complex2->GetReal(&r2);
  complex2->GetImaginary(&i2);
  r1 += r2;
  i1 += i2;
  (*complex1)->SetReal(r1);
  (*complex1)->SetImaginary(i1);
  cout << "(" 
       << *complex1 << ", " 
       << complex2 << ")"
       << endl;
  return NS_OK;
}


  /* long AddTwoInts (in long int1, in long int2); */
NS_IMETHODIMP JSSample::AddTwoInts(PRInt32 int1, 
                                   PRInt32 int2, 
                                   PRInt32 *_retval) {
  *_retval = int1 + int2;
  return NS_OK;
}

/*  
 * JSComplex Implementation  
 */  

JSComplex::JSComplex()  
{  
  // Zero reference counter
  NS_INIT_ISUPPORTS();
  // PR_AtomicIncrement(&gInstanceCnt);
}

JSComplex::JSComplex(int aReal, int aImaginary) : 
    real_(aReal), imaginary_(aImaginary)
{  
  // Zero reference counter
  NS_INIT_ISUPPORTS();
  // PR_AtomicIncrement(&gInstanceCnt);
}

JSComplex::~JSComplex()  
{  
  // Make sure there are no dangling pointers to us,
  // debug only
  NS_ASSERTION(mRefCnt == 0,"Wrong ref count");
  // PR_AtomicDecrement(&gInstanceCnt);
}  


// Use the official macros to implement nsISupports
NS_IMPL_ISUPPORTS1(JSComplex, JSIComplex)


/* attribute long real; */
NS_IMETHODIMP JSComplex::GetReal(PRInt32 *aReal) {
  *aReal = real_;
  cout << "--> JSComplex::GetReal() : " << *aReal << endl;
  return NS_OK;
}

NS_IMETHODIMP JSComplex::SetReal(PRInt32 aReal) {
  cout << "--> JSComplex::SetReal(" << aReal << ")" << endl;
  real_ = aReal;
  return NS_OK;
}

/* attribute long imaginary; */
NS_IMETHODIMP JSComplex::GetImaginary(PRInt32 *aImaginary) {
  *aImaginary = imaginary_;
  cout << "--> JSComplex::GetImaginary() : " << *aImaginary << ")" << endl;
  return NS_OK;
}

NS_IMETHODIMP JSComplex::SetImaginary(PRInt32 aImaginary) {
  cout << "--> JSComplex::SetImaginary(" << aImaginary << ")" << endl;
  imaginary_ = aImaginary;
  return NS_OK;
}



/*  
 * JSSampleFactory Implementation  
 */

JSSampleFactory::JSSampleFactory()  
{  
  // Zero reference counter
  NS_INIT_ISUPPORTS();
  // PR_AtomicIncrement(&gInstanceCnt);
}

JSSampleFactory::~JSSampleFactory()  
{  
  // Make sure there are no dangling pointers to us,
  // debug only
  NS_ASSERTION(mRefCnt == 0,"Wrong ref count");
  // PR_AtomicDecrement(&gInstanceCnt);
}  


// Use the official macros to implement nsISupports
NS_IMPL_ISUPPORTS1(JSSampleFactory, nsIFactory)


NS_IMETHODIMP JSSampleFactory::CreateInstance(nsISupports *aOuter,
					      const nsIID &aIID, 
					      void **aResult)
{
  if (!aResult) { 
    return NS_ERROR_NULL_POINTER; 
  }

  *aResult = nsnull; 

  nsISupports* inst = new JSSample(); 

  if (!inst) { 
	return NS_ERROR_OUT_OF_MEMORY; 
  }

  nsresult res = inst->QueryInterface(aIID, aResult); 

  if (NS_FAILED(res)) { 
	// We didn't get the right interface, so clean up 
	delete inst; 
  }

  return res; 
} 

NS_IMETHODIMP JSSampleFactory::LockFactory(PRBool aLock) 
{ 
  if (aLock) { 
      // PR_AtomicIncrement(&gLockCnt); 
  } else { 
      // PR_AtomicDecrement(&gLockCnt); 
  } 

  return NS_OK;
} 


/////////////////////////////////////////////////////////////////////
// Exported functions. With these in place we can compile
// this module into a dynamically loaded and registered
// component.

extern "C" {
  NS_EXPORT nsresult NSGetFactory(nsISupports *serviceMgr,
				  const nsCID &aCID,
				  const char *aClassName,
				  const char *aProgID,
				  nsIFactory **aResult) { 
    if (!aResult) {
      return NS_ERROR_NULL_POINTER; 
    }

    *aResult = nsnull; 

    nsISupports *inst; 

    if (aCID.Equals(kSampleCID)) {
      // Ok, we know this CID and here is the factory
      // that can manufacture the objects
      inst = new JSSampleFactory(); 
    } else { 
      return NS_ERROR_NO_INTERFACE; 
    } 

    if (!inst) {
      return NS_ERROR_OUT_OF_MEMORY; 
    }

    nsresult rv = inst->QueryInterface(NS_GET_IID(nsIFactory), 
				       (void **) aResult); 

    if (NS_FAILED(rv)) { 
      delete inst; 
    } 

    return rv; 
  }

  NS_EXPORT PRBool NSCanUnload(nsISupports* serviceMgr) {
      return PR_FALSE;
      // return PRBool(gInstanceCnt == 0 && gLockCnt == 0); 
  }

  NS_EXPORT nsresult NSRegisterSelf(nsISupports* serviceMgr, 
				    const char *path) {
    return nsComponentManager::RegisterComponent(kSampleCID, nsnull, nsnull, 
					   path, PR_TRUE, PR_TRUE);
  }

  NS_EXPORT nsresult NSUnregisterSelf(nsISupports* serviceMgr, 
				      const char *path) {
    return nsComponentManager::UnregisterComponent(kSampleCID, path);
  }
}
