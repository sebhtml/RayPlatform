# author: Sébastien Boisvert
#
# based on http://www.ravnborg.org/kbuild/makefiles.html
#

VERSION = 1
PATCHLEVEL = 0
SUBLEVEL = 2
EXTRAVERSION = 
NAME = Timely Gate of Yields

RAYPLATFORM_VERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)

include common.mk

all: libRayPlatform.a

libRayPlatform.a: $(obj-y)
	$(AR) rcs $@ $^

# inference rule
%.o: %.cpp
	$(MPICXX) $(CXXFLAGS) -D RAYPLATFORM_VERSION=\"$(RAYPLATFORM_VERSION)\" -I. -c -o $@ $<

clean:
	$(RM) -f libRayPlatform.a $(obj-y)

