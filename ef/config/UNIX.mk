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

LIB_SUFFIX		= a
DLL_SUFFIX		= so
AR				= ar cr $@

ifdef BUILD_OPT
OPTIMIZER		= -O
DEFINES			= -DXP_UNIX -UDEBUG -DNDEBUG
OBJDIR_TAG		= _OPT
else
OPTIMIZER		= -g
DEFINES			= -DXP_UNIX -DDEBUG -UNDEBUG -DDEBUG_$(shell whoami) -DPR_LOGGING
OBJDIR_TAG		= _DBG
endif

#
# Name of the binary code directories
#
ifndef IMPL_STRATEGY
IMPL_STRATEGY		= _PTH
endif

OBJDIR_NAME	= $(OS_TARGET)$(CPU_ARCH_TAG)$(OBJDIR_TAG).OBJ
NSPR_OBJDIR     = $(OS_CONFIG)$(CPU_ARCH_TAG)$(IMPL_STRATEGY)$(OBJDIR_TAG).OBJ

#
# Install
#

####################################################################
#
# One can define the makefile variable NSDISTMODE to control
# how files are published to the 'dist' directory.  If not
# defined, the default is "install using relative symbolic
# links".  The two possible values are "copy", which copies files
# but preserves source mtime, and "absolute_symlink", which
# installs using absolute symbolic links.  The "absolute_symlink"
# option requires NFSPWD.
#
####################################################################

NSINSTALL_DIR  = $(DEPTH)/config
NSINSTALL      = $(DEPTH)/config/$(OBJDIR_NAME)/nsinstall

MKDEPEND_DIR	= $(DEPTH)/config/mkdepend
MKDEPEND	= $(MKDEPEND_DIR)/$(OBJDIR_NAME)/mkdepend
MKDEPENDENCIES	= $(OBJDIR_NAME)/depend.mk

ifeq ($(NSDISTMODE),copy)
        # copy files, but preserve source mtime
        INSTALL  = $(NSINSTALL)
        INSTALL += -t
else
        ifeq ($(NSDISTMODE),absolute_symlink)
                # install using absolute symbolic links
                INSTALL  = $(NSINSTALL)
                INSTALL += -L `$(NFSPWD)`
        else
                # install using relative symbolic links
                INSTALL  = $(NSINSTALL)
                INSTALL += -R
        endif
endif

define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); $(NSINSTALL) -D $(@D); fi
endef
