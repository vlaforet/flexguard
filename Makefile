GCC:=gcc
LIBS := -lrt -lpthread -lnuma
COMPILE_FLAGS := -fPIC -Wall
DEFINED := -D_GNU_SOURCE

ifeq ($(DEBUG),1)
	COMPILE_FLAGS += -O0 -fno-inline -ggdb
	DEFINED += -DDEBUG
else
	COMPILE_FLAGS += -O3
endif

ifneq ($(ADD_PADDING),0)
	DEFINED += -DADD_PADDING
endif

ifeq ($(USE_REAL_PTHREAD),1)
	DEFINED += -DUSE_REAL_PTHREAD=1
	OBJ_FILES += interpose.o
endif

ifeq ($(TRACING),1)
	DEFINED += -DTRACING
endif

ifeq ($(HYBRIDV2_NEXT_WAITER_DETECTION),1)
	DEFINED += -DHYBRIDV2_NEXT_WAITER_DETECTION=1
else ifeq ($(HYBRIDV2_NEXT_WAITER_DETECTION),2)
	DEFINED += -DHYBRIDV2_NEXT_WAITER_DETECTION=2
endif

ifeq ($(HYBRIDV2_LOCAL_PREEMPTIONS),1)
	DEFINED += -DHYBRIDV2_LOCAL_PREEMPTIONS
endif

# LOCK_VERSION in (SPINLOCK, HYBRIDLOCK, HYBRIDV2, MCS, MCSTAS, CLH, TICKET, MUTEX, FUTEX)
ifndef LOCK_VERSION
	LOCK_VERSION=HYBRIDV2
endif
DEFINED += -DUSE_$(LOCK_VERSION)_LOCKS
OBJ_FILES += $(shell echo $(LOCK_VERSION).o | tr '[:upper:]' '[:lower:]')

ifneq (,$(findstring HYBRID,$(LOCK_VERSION)))
# HYBRID_VERSION in (MCS, CLH, TICKET)
	ifndef HYBRID_VERSION
		HYBRID_VERSION=MCS
	endif
	DEFINED += -DHYBRID_$(HYBRID_VERSION)

	ifeq ($(HYBRID_EPOCH), 1)
		DEFINED += -DHYBRID_EPOCH
	endif

	ifndef NOBPF
		NOBPF=0
	endif

	ifeq ($(NOBPF), 0)
		BPF_SKELETON += $(OUTPUT)/$(shell echo $(LOCK_VERSION) | tr '[:upper:]' '[:lower:]').skel.h
	endif
else
	NOBPF=1
endif

ifeq ($(NOBPF), 0)
	OUTPUT := $(abspath .output)
	CLANG ?= clang
	LLVM_STRIP ?= llvm-strip
	LIBBPF_SRC := $(abspath ./libbpf/src)
	BPFTOOL_SRC := $(abspath ./bpftool/src)
	LIBBPF_OBJ := $(abspath $(OUTPUT)/libbpf.a)
	BPFTOOL_OUTPUT ?= $(abspath $(OUTPUT)/bpftool)
	BPFTOOL ?= $(BPFTOOL_OUTPUT)/bootstrap/bpftool
	ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/' | sed 's/ppc64le/powerpc/' | sed 's/mips.*/mips/')
	VMLINUX := $(abspath ./vmlinux/$(ARCH)/vmlinux.h)
	BPFINCLUDES := -I$(OUTPUT) -I$(abspath ../libbpf/include/uapi) -I$(dir $(VMLINUX))

	DEFINED += -DBPF

	# Get Clang's default includes on this system. We'll explicitly add these dirs
	# to the includes list when compiling with `-target bpf` because otherwise some
	# architecture-specific dirs will be "missing" on some architectures/distros -
	# headers such as asm/types.h, asm/byteorder.h, asm/socket.h, asm/sockios.h,
	# sys/cdefs.h etc. might be missing.
	#
	# Use '-idirafter': Don't interfere with include mechanics except where the
	# build would have failed anyways.
	CLANG_BPF_SYS_INCLUDES = $(shell $(CLANG) -v -E - </dev/null 2>&1 \
		| sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')

	LIBS += -lelf -lz
endif

INCLUDES := $(BPFINCLUDES) -I$(abspath ./include/)

ifeq ($(NOBPF), 0)
$(OUTPUT) $(OUTPUT)/libbpf $(BPFTOOL_OUTPUT):
	mkdir -p $@

# Build libbpf
$(LIBBPF_OBJ): $(wildcard $(LIBBPF_SRC)/*.[ch] $(LIBBPF_SRC)/Makefile) | $(OUTPUT)/libbpf
	$(MAKE) -C $(LIBBPF_SRC) BUILD_STATIC_ONLY=1		\
		    OBJDIR=$(dir $@)/libbpf DESTDIR=$(dir $@)	\
		    INCLUDEDIR= LIBDIR= UAPIDIR=			       	\
		    EXTRA_CFLAGS=-fPIC install

# Build bpftool
$(BPFTOOL): | $(BPFTOOL_OUTPUT)
	$(MAKE) ARCH= CROSS_COMPILE= OUTPUT=$(BPFTOOL_OUTPUT)/ -C $(BPFTOOL_SRC) bootstrap

# Build BPF code
$(OUTPUT)/%.bpf.o: src/%.bpf.c $(LIBBPF_OBJ) $(wildcard %.h) $(VMLINUX) | $(OUTPUT)
	$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) $(DEFINED) -c $(filter %.c,$^) -o $@
	$(LLVM_STRIP) -g $@ # strip useless DWARF info

# Generate BPF skeletons
$(OUTPUT)/%.skel.h: $(OUTPUT)/%.bpf.o | $(OUTPUT) $(BPFTOOL)
	$(BPFTOOL) gen skeleton $< > $@
endif

litl: include/lock_if.h libsync.a
	$(MAKE) -C litl/ EXTERNAL_CFLAGS="$(DEFINED) $(INCLUDES)"

interpose.sh: interpose.in interpose.so
	cat interpose.in | sed -e "s/@base_dir@/$$(echo $$(cd .; pwd) | sed -e 's/\([\/&]\)/\\\1/g')/g" > $@
	chmod a+x $@

interpose.so: include/atomic_ops.h include/utils.h include/lock_if.h interpose.o libsync.a
	$(GCC) -shared $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) $^ -o interpose.so $(LIBS) -Wl,--version-script=src/interpose.map

libsync.a: $(OBJ_FILES) $(LIBBPF_OBJ) include/atomic_ops.h include/utils.h include/lock_if.h $(BPF_SKELETON)
	ar -rc libsync.a $(OBJ_FILES)
ifneq ($(LIBBPF_OBJ),) # Add libbpf to the archive
	echo "OPEN libsync.a\n ADDLIB $(LIBBPF_OBJ)\n SAVE\n END" | ar -M
endif

%.o: src/%.c $(BPF_SKELETON)
	$(GCC) $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) -c $(filter %.c,$^) -o $@ $(LIBS)
ifeq ($(ASSEMBLY_DUMP),1) # Produces a %.s and %.odump files containing the compiled-unassembled code
	$(GCC) -S -fverbose-asm $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) -c $(filter %.c,$^) $(LIBS)
	objdump -d $@ > ${@}dump
endif

scheduling: bmarks/scheduling.c libsync.a
	$(GCC) $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) $^ -o $@ $(LIBS)

buckets: src/hash_map.c bmarks/buckets.c libsync.a
	$(GCC) $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) $^ -o $@ -lm $(LIBS)

test_correctness: bmarks/test_correctness.c libsync.a
	$(GCC) $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) $^ -o $@ $(LIBS)

all: scheduling test_correctness buckets libsync.a interpose.so interpose.sh
	@echo "############### Used lock:" $(LOCK_VERSION)
	@echo "############### CFLAGS =" $(INCLUDES) $(DEFINED)

clean:
	rm -rf $(OUTPUT) interpose.so interpose.sh *.o *.s libsync.a *.odump test_correctness scheduling buckets
	$(MAKE) -C litl/ clean

cleanall: clean
	rm -rf interpose_*.so interpose_*.sh libsync*.a test_correctness_* scheduling_* buckets_*
