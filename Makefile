# Makefile
antopDIR=$(shell pwd)

SRC =	main.c libnetfilter_queue.c libnfnetlink.c attr.c nlmsg.c socket.c callback.c rt_table.c utils.c hcb_address.c antop_socket.c if_info.c if_addr.c timer_queue.c \
        heart_beat.c pkt_handle.c rendezvous.c funciones.c fragmentacion.c mezcla.c secondary.c ping6.c

OBJS =	$(SRC:%.c=%.o)

KERNEL=$(shell uname -r)
# Change to compile against different kernel (can be overridden):
KERNEL_DIR=/lib/modules/$(KERNEL)/build
KERNEL_INC=$(KERNEL_DIR)/include

# Compiler and options:
# ##### for RCP use: big-endian
CC=gcc
LD=ld
OPTS=-Wall -O3

# Comment out to disable debug operation...
DEBUG=-g -DDEBUG
# Add extra functionality. Uncomment or use "make XDEFS=-D<feature>" on 
# the command line.
XDEFS=-DDEBUG
DEFS=-DCONFIG_GATEWAY #-DLLFEEDBACK
CFLAGS=$(OPTS) $(DEBUG) $(DEFS) $(XDEFS)
LD_OPTS=-lm

#ifneq (,$(findstring CONFIG_GATEWAY,$(DEFS)))
#SRC:=$(SRC) locality.c
#endif
#ifneq (,$(findstring LLFEEDBACK,$(DEFS)))
#SRC:=$(SRC) llf.c
#LD_OPTS:=$(LD_OPTS) -liw
#endif

# Archiver and options
AR=ar
AR_FLAGS=rc

.PHONY: default clean install uninstall depend tags antopd-arm docs kantop kantop-arm kantop-mips

default: antopd kantop

endian.h:
	$(CC) $(CFLAGS) -o endian endian.c
	./endian > endian.h

$(OBJS): %.o: %.c Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

antopd: $(OBJS) Makefile
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LD_OPTS)

# Kernel module:

kantop:
	$(MAKE) -C $(antopDIR)/lnx KERNEL_DIR=$(KERNEL_DIR) KCC=$(CC) XDEFS=$(XDEFS)

tags: TAGS
TAGS: lnx/TAGS
	etags *.c *.h

lnx/TAGS:
	cd lnx && $(MAKE) TAGS

indent:
	indent -kr -l 80 *.c \
	$(filter-out $(SRC_NS_CPP:%.cc=%.h),$(wildcard *.h))
	$(MAKE) -C lnx indent
depend:
	@echo "Updating Makefile dependencies..."
	@makedepend -Y./ -- $(DEFS) -- $(SRC) &>/dev/null
	@makedepend -a -Y./ -- $(KDEFS) kantop.c &>/dev/null

install: default
	install -s -m 755 antopd /usr/sbin/antopd
	@if [ ! -d /lib/modules/$(KERNEL)/antop ]; then \
		mkdir /lib/modules/$(KERNEL)/antop; \
	fi

	@echo "Installing kernel module in /lib/modules/$(KERNEL)/antop/";
	@if [ -f ./kantop.ko ]; then \
		install -m 644 kantop.ko /lib/modules/$(KERNEL)/antop/kantop.ko; \
	else \
		install -m 644 kantop.o /lib/modules/$(KERNEL)/antop/kantop.o; \
	fi
	/sbin/depmod -a
uninstall:
	rm -f /usr/sbin/antopd
	rm -rf /lib/modules/$(KERNEL)/antop

docs:
	cd docs && $(MAKE) all
clean: 
	rm -f antopd *~ *.o core kantop.ko endian endian.h
	cd lnx && $(MAKE) clean
#cd docs && $(MAKE) clean

# DO NOT DELETE



#libnetfilter_queue.o: libnetfilter_queue.h
#main.o: defs.h params.h ipv6_utils.h packet_input.h packet_queue.h
#nl.o: defs.h lnx/kantop-netlink.h params.h
#packet_queue.o: libipq.h ipv6_utils.h defs.h
#packet_input.o: 
