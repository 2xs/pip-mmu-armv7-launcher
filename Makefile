###############################################################################
#  © Université de Lille, The Pip Development Team (2015-2024)                #
#                                                                             #
#  This software is a computer program whose purpose is to run a minimal,     #
#  hypervisor relying on proven properties such as memory isolation.          #
#                                                                             #
#  This software is governed by the CeCILL license under French law and       #
#  abiding by the rules of distribution of free software.  You can  use,      #
#  modify and/ or redistribute the software under the terms of the CeCILL     #
#  license as circulated by CEA, CNRS and INRIA at the following URL          #
#  "http://www.cecill.info".                                                  #
#                                                                             #
#  As a counterpart to the access to the source code and  rights to copy,     #
#  modify and redistribute granted by the license, users are provided only    #
#  with a limited warranty  and the software's author,  the holder of the     #
#  economic rights,  and the successive licensors  have only  limited         #
#  liability.                                                                 #
#                                                                             #
#  In this respect, the user's attention is drawn to the risks associated     #
#  with loading,  using,  modifying and/or developing or reproducing the      #
#  software by the user in light of its specific status of free software,     #
#  that may mean  that it is complicated to manipulate,  and  that  also      #
#  therefore means  that it is reserved for developers  and  experienced      #
#  professionals having in-depth computer knowledge. Users are therefore      #
#  encouraged to load and test the software's suitability as regards their    #
#  requirements in conditions enabling the security of their systems and/or   #
#  data to be ensured and,  more generally, to use and operate it in the      #
#  same conditions as regards security.                                       #
#                                                                             #
#  The fact that you are presently reading this means that you have had       #
#  knowledge of the CeCILL license and that you accept its terms.             #
###############################################################################

include ../toolchain.mk

CC_INSTALL_PATH = $(shell $(CC) -print-search-dirs | sed -n -e 's/^install:\s\(.*\)$$/\1/p')

CFLAGS     = -Wall
CFLAGS    += -Wextra
CFLAGS    += -Werror
CFLAGS    += -mcpu=cortex-a7
CFLAGS    += -marm
CFLAGS    += -ffreestanding
CFLAGS    += -c
CFLAGS    += -O2
CFLAGS    += -I./include
CFLAGS    += -I$(LIBPIP)/include/
CFLAGS    += -I$(LIBPIP)/arch/armv7/include/

LDFLAGS    = -L$(LIBPIP)/lib
LDFLAGS   += -L$(CC_INSTALL_PATH)

ASFLAGS    = $(CFLAGS)

CSOURCES   = $(wildcard *.c)
ASSOURCES  = $(wildcard *.S)

ASOBJ      = $(ASSOURCES:.S=.o)
COBJ       = $(CSOURCES:.c=.o)

EXEC       = $(shell basename $$(pwd)).bin

all: dep $(EXEC)
	@echo Done.

clean:
	rm -f $(ASOBJ) $(COBJ) $(EXEC)

$(EXEC): $(ASOBJ) $(COBJ)
	$(LD) $(LDFLAGS) $^ -T link.ld -o $@ -lpip -lgcc

dep:
	make -C minimal clean all

doc:
	doxygen

%.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

.PHONY: all clean dep doc
