# This is an automatically generated record.
# The area between QNX Internal Start and QNX Internal End is controlled by
# the QNX IDE properties.

ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

#===== USEFILE - the file containing the usage message for the application. 
USEFILE=

#===== EXTRA_SRCVPATH - a space-separated list of directories to search for source files.
EXTRA_SRCVPATH+=$(PROJECT_ROOT)/src

#===== EXTRA_INCVPATH - a space-separated list of directories to search for include files.
EXTRA_INCVPATH+= \
	$(PROJECT_ROOT_minicloud)/inc  \
	$(PROJECT_ROOT)/inc  \
	$(PROJECT_ROOT_expat)/lib  \
	$(PROJECT_ROOT_brushstring)/inc

#===== EXTRA_LIBVPATH - a space-separated list of directories to search for library files.
EXTRA_LIBVPATH+= \
	$(PROJECT_ROOT_minicloud)/$(CPU)/$(patsubst o%,so%,$(notdir $(CURDIR)))  \
	$(PROJECT_ROOT_expat)/$(CPU)/$(patsubst o%,a%,$(notdir $(CURDIR)))  \
	$(PROJECT_ROOT_brushstring)/$(CPU)/$(patsubst o%,a%,$(notdir $(CURDIR)))  \
	$(PROJECT_ROOT_brushstring)/$(CPU)/$(patsubst o%,so%,$(notdir $(CURDIR)))  \
	$(PROJECT_ROOT_expat)/$(CPU)/$(patsubst o%,so%,$(notdir $(CURDIR)))

#===== LIBS - a space-separated list of library items to be included in the link.
LIBS+=^minicloud ^brushstring ^expat

#===== CCFLAGS - add the flags to the C compiler command line. 
CCFLAGS+=-Wp,-MMD,$(basename $@).d

#===== EXTRA_CLEAN - additional files to delete when cleaning the project.
EXTRA_CLEAN+=*.d

include $(MKFILES_ROOT)/qmacros.mk
ifndef QNX_INTERNAL
QNX_INTERNAL=$(PROJECT_ROOT)/.qnx_internal.mk
endif
include $(QNX_INTERNAL)

-include *.d

include $(MKFILES_ROOT)/qtargets.mk

OPTIMIZE_TYPE_g=none
OPTIMIZE_TYPE=$(OPTIMIZE_TYPE_$(filter g, $(VARIANTS)))

