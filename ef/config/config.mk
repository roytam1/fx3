#! gmake
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

ifndef EF_CONFIG_MK

EF_CONFIG_MK=1

include $(DEPTH)/config/arch.mk
include $(DEPTH)/config/command.mk
include $(DEPTH)/config/$(OS_ARCH).mk
PLATFORM = $(OBJDIR_NAME)
include $(DEPTH)/config/location.mk
include $(DEPTH)/config/prefix.mk
include $(DEPTH)/config/suffix.mk

ifeq ($(OS_ARCH),WINNT)
	CCC = cl
	EXC_FLAGS = -GX
	OS_DLLFLAGS = -nologo -DLL -incremental:yes -subsystem:console -machine:I386 wsock32.lib
	EF_LIBS = $(DIST)/lib/EF.lib $(DIST)/lib/DebuggerChannel.lib $(DIST)/lib/EFDisassemble
	EF_LIB_FILES = $(EF_LIBS)
	NSPR_LIBS = $(DIST)/lib/libnspr3.lib $(DIST)/lib/libplc3_s.lib
	BROWSE_INFO_DIR = $(DEPTH)/$(OBJDIR)/BrowseInfo
	BROWSE_INFO_OBJS = $(wildcard $(BROWSE_INFO_DIR)/*.sbr)
	BROWSE_INFO_PROGRAM = bscmake 
	BROWSE_INFO_FLAGS = -nologo -incremental:yes
	BROWSE_INFO_FILE = $(BROWSE_INFO_DIR)/ef.bsc
	YACC = "$(NSTOOLS)/bin/yacc$(PROG_SUFFIX)" -l -b y.tab
	LN = "$(NSTOOLS)/bin/ln$(PROG_SUFFIX)" -f

# Flag to generate pre-compiled headers
	CFLAGS += -YX -Fp$(OBJDIR)/ef.pch -Fd$(OBJDIR)/ef.pdb
	MKDIR = mkdir
else
	CCC = g++
	AS = gcc
	ASFLAGS += -x assembler-with-cpp
	EXC_FLAGS = -fexceptions
	EF_LIBS = -L$(DIST)/lib -lEF -lDebuggerChannel -lEFDisassemble
	EF_LIB_FILES = $(DIST)/lib/libEF.a $(DIST)/lib/libDebuggerChannel.a $(DIST)/lib/libEFDisassemble.a
	NSPR_LIBS = $(NSPR_THREAD_LIBS) -L$(NSPR_PREFIX)/lib -lnspr3 -lplc3
	MKDIR = mkdir -p
	LN = ln -s -f
endif

CFLAGS += $(EXC_FLAGS)
#LDFLAGS += $(NSPR_LIBS)

ifdef USE_JVMDI
CFLAGS += -DUSE_JVMDI
endif

ifeq ($(CPU_ARCH),x86)
ARCH_DEFINES	+= -DGENERATE_FOR_X86
NO_GENERIC	= 1
endif

ifeq ($(CPU_ARCH),ppc)
ARCH_DEFINES	+= -DGENERATE_FOR_PPC
endif

ifeq ($(CPU_ARCH),sparc)
# No CodeGenerator for sparc yet.
ARCH_DEFINES    += -DGENERATE_FOR_X86
CPU_ARCH		= x86
endif

ifeq ($(CPU_ARCH),hppa)
# No CodeGenerator for hppa yet.
ARCH_DEFINES    += -DGENERATE_FOR_X86
CPU_ARCH		= x86
endif

ifneq ($(NO_GENERIC),1)
GENERIC	= generic
endif

ARCH_DEFINES	+= -DTARGET_CPU=$(CPU_ARCH)
CFLAGS += $(ARCH_DEFINES)

#
# Some tools.
#
NFSPWD = 		$(DEPTH)/config/$(OBJDIR)/nfspwd
JAVAH =			LD_LIBRARY_PATH=$(DIST)/lib $(DIST)/bin/javah$(PROG_SUFFIX)
BURG = 			$(DEPTH)/Tools/Burg/$(OBJDIR)/burg$(PROG_SUFFIX)
PERL = 			perl

HEADER_GEN_DIR = 	$(DEPTH)/GenIncludes
LOCAL_EXPORT_DIR = 	$(DEPTH)/LocalIncludes

INCLUDES += -I$(LOCAL_EXPORT_DIR) -I$(LOCAL_EXPORT_DIR)/md/$(CPU_ARCH) -I$(DIST)/include -I$(XPDIST)/public -I$(DEPTH)/Includes -I$(NSPR_PREFIX)/include

ifneq ($(HEADER_GEN),)
INCLUDES += -I $(HEADER_GEN_DIR)
CFLAGS += -DIMPORTING_VM_FILES
PACKAGE_CLASSES	= 	$(HEADER_GEN)
PATH_CLASSES	= 	$(subst .,/,$(PACKAGE_CLASSES))
CLASS_FILE_NAMES = 	$(subst .,_,$(PACKAGE_CLASSES))
HEADER_INCLUDES = 	$(patsubst %,$(HEADER_GEN_DIR)/%.h,$(CLASS_FILE_NAMES))
endif

MKDEPEND_DIR    = $(DEPTH)/config/mkdepend
MKDEPEND        = $(MKDEPEND_DIR)/$(OBJDIR_NAME)/mkdepend
MKDEPENDENCIES  = $(OBJDIR_NAME)/depend.mk

endif


