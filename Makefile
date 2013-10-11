# author: Sébastien Boisvert
#
# based on http://www.ravnborg.org/kbuild/makefiles.html
#

VERSION = 1
PATCHLEVEL = 1
SUBLEVEL = 2
EXTRAVERSION = -devel
NAME = Nested Droids of Luck

RAYPLATFORM_VERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)

include common.mk

all: libRayPlatform.a

libRayPlatform.a: $(obj-y)
	$(Q)$(ECHO) "  AR $@"
	$(Q)$(AR) rcs $@ $^

# compile assertions

CONFIG_ASSERT=$(ASSERT)

CONFIG_FLAGS-y=
CONFIG_FLAGS-$(CONFIG_ASSERT) += -D CONFIG_ASSERT -D ASSERT
CONFIG_FLAGS=$(CONFIG_FLAGS-y)

# inference rule
%.o: %.cpp
	$(Q)$(ECHO) "  CXX $@"
	$(Q)$(MPICXX) $(CXXFLAGS) $(CONFIG_FLAGS) -D RAYPLATFORM_VERSION=\"$(RAYPLATFORM_VERSION)\" -I. -c -o $@ $<

clean:
	$(Q)$(ECHO) CLEAN RayPlatform
	$(Q)$(RM) -f libRayPlatform.a $(obj-y)

