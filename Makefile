#===- ./Makefile -------------------------------------------*- Makefile -*--===#
# 
#                     The LLVM Compiler Infrastructure
#
# This file was developed by the LLVM research group and is distributed under
# the University of Illinois Open Source License. See LICENSE.TXT for details.
# 
#===------------------------------------------------------------------------===#
LEVEL = .
DIRS = lib/System lib/Support utils lib tools 


ifneq ($(MAKECMDGOALS),tools-only)
DIRS += runtime
OPTIONAL_DIRS = examples projects
endif

EXTRA_DIST := test llvm.spec include

include $(LEVEL)/Makefile.common

dist-hook::
	@$(ECHO) Eliminating CVS directories from distribution
	$(VERB) rm -rf `find $(TopDistDir) -type d -name CVS -print`
	@$(ECHO) Eliminating files constructed by configure
	$(VERB) rm -f \
	  $(TopDistDir)/include/llvm/ADT/hash_map  \
	  $(TopDistDir)/include/llvm/ADT/hash_set  \
	  $(TopDistDir)/include/llvm/ADT/iterator  \
	  $(TopDistDir)/include/llvm/Config/config.h  \
	  $(TopDistDir)/include/llvm/Support/DataTypes.h  \
	  $(TopDistDir)/include/llvm/Support/ThreadSupport.h

test :: all
	cd test; $(MAKE)

tools-only: all
