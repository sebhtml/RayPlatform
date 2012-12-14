# author: Sébastien Boisvert
#
# based on http://www.ravnborg.org/kbuild/makefiles.html
#

VERSION = 7
PATCHLEVEL = 0
SUBLEVEL = 0
EXTRAVERSION = -devel
NAME = Nested Droids of Luck

RAYPLATFORM_VERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)

include common.mk

all: libRayPlatform.a

libRayPlatform.a: $(obj-y)
	$(Q)$(ECHO) "  AR $@"
	$(Q)$(AR) rcs $@ $^

# inference rule
%.o: %.cpp
	$(Q)$(ECHO) "  CXX RayPlatform/$@"
	$(Q)$(MPICXX) $(CXXFLAGS) -D RAYPLATFORM_VERSION=\"$(RAYPLATFORM_VERSION)\" -I. -c -o $@ $<

clean:
	$(Q)$(ECHO) CLEAN RayPlatform
	$(Q)$(RM) -f libRayPlatform.a $(obj-y)

