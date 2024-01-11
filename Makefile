COMPILE_FLAGS=-fPIC -Wall

ifeq ($(DEBUG),1)
  COMPILE_FLAGS += -O0 -fno-inline -ggdb
	DEFINED := -DDEBUG
else
  COMPILE_FLAGS += -O3 -Wall
endif

ifeq ($(DEBUG_LITL),1)
	DEFINED += -DDEBUG_LITL
endif

ifneq ($(ADD_PADDING),0)
	DEFINED += -DADD_PADDING
endif

# Produces a hybridlock.s file containing the compiled-unassembled code
ifeq ($(HYBRID_ASSEMBLY),1) 
	ASSEMBLY_DEFINE := -S -g -fverbose-asm
endif

# LOCK_VERSION in (SPINLOCK, HYBRIDLOCK, HYBRIDSPIN, MCS, CLH, TICKET, MUTEX, FUTEX)
ifndef LOCK_VERSION
  LOCK_VERSION=HYBRIDLOCK
endif
DEFINED += -DUSE_$(LOCK_VERSION)_LOCKS
OBJ_FILES := $(shell echo $(LOCK_VERSION).o | tr '[:upper:]' '[:lower:]')

ifeq ($(LOCK_VERSION), HYBRIDLOCK)
# HYBRID_VERSION in (MCS, CLH, TICKET)
	ifndef HYBRID_VERSION
		HYBRID_VERSION=MCS
	endif
	DEFINED += -DHYBRID_$(HYBRID_VERSION)

	ifeq ($(HYBRID_GLOBAL_STATE), 1)
		DEFINED += -DHYBRID_GLOBAL_STATE
	endif

		ifeq ($(HYBRID_EPOCH), 1)
		DEFINED += -DHYBRID_EPOCH
	endif

	ifndef NOBPF
		NOBPF=0
	endif
endif

ifeq ($(LOCK_VERSION), MUTEX)
	OBJ_FILES :=
endif

CORE_NUM := $(shell nproc)
ifeq ($(CORE_NUM), )
	CORE_NUM := 8
endif
DEFINED += -DCORE_NUM=${CORE_NUM}

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
	GCC:=gcc
	LIBS := -lrt -lpthread -lnuma
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
	LIBBLAZESYM_SRC := $(abspath ./blazesym/)
	LIBBLAZESYM_OBJ := $(abspath $(OUTPUT)/libblazesym.a)
	LIBBLAZESYM_HEADER := $(abspath $(OUTPUT)/blazesym.h)
	ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/' | sed 's/ppc64le/powerpc/' | sed 's/mips.*/mips/')
	VMLINUX := $(abspath ./vmlinux/$(ARCH)/vmlinux.h)
	BPFINCLUDES := -I$(OUTPUT) -I$(abspath ../libbpf/include/uapi) -I$(dir $(VMLINUX))

	DEFINED += -DBPF
	BPF_SKELETON += $(OUTPUT)/hybridlock.skel.h

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

SRCPATH := $(abspath ./src/)
MAININCLUDE := $(abspath ./include/)
INCLUDES := $(BPFINCLUDES) -I$(MAININCLUDE)

all: scheduling test_correctness libsync.a
	@echo "############### Used lock:" $(LOCK_VERSION)

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

$(LIBBLAZESYM_SRC)/target/release/libblazesym.a::
	cd $(LIBBLAZESYM_SRC) && $(CARGO) build --features=cheader --release

$(LIBBLAZESYM_OBJ): $(LIBBLAZESYM_SRC)/target/release/libblazesym.a | $(OUTPUT)
	cp $(LIBBLAZESYM_SRC)/target/release/libblazesym.a $@

$(LIBBLAZESYM_HEADER): $(LIBBLAZESYM_SRC)/target/release/libblazesym.a | $(OUTPUT)
	cp $(LIBBLAZESYM_SRC)/target/release/blazesym.h $@

# Build BPF code
$(OUTPUT)/%.bpf.o: src/%.bpf.c $(LIBBPF_OBJ) $(wildcard %.h) $(VMLINUX) | $(OUTPUT)
	$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) $(DEFINED) -c $(filter %.c,$^) -o $@
	$(LLVM_STRIP) -g $@ # strip useless DWARF info

# Generate BPF skeletons
$(OUTPUT)/%.skel.h: $(OUTPUT)/%.bpf.o | $(OUTPUT) $(BPFTOOL)
	$(BPFTOOL) gen skeleton $< > $@

# Build user-space code
$(patsubst %,$(OUTPUT)/%.o,$(APPS)): %.o: %.skel.h
endif

litl: include/lock_if.h libsync.a $(LIBBPF_OBJ)
	$(MAKE) -C litl/ EXTERNAL_CFLAGS="$(DEFINED) $(INCLUDES)"

libsync.a: $(OBJ_FILES) include/atomic_ops.h include/utils.h include/lock_if.h $(BPF_SKELETON)
	ar -r libsync.a $(OBJ_FILES)

spinlock.o: src/spinlock.c
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) -c src/spinlock.c $(LIBS)

futex.o: src/futex.c
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) -c src/futex.c $(LIBS)

clh.o: src/clh.c
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) -c src/clh.c $(LIBS)

hybridlock.o: src/hybridlock.c $(BPF_SKELETON)
	$(GCC) $(ASSEMBLY_DEFINE) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) -c src/hybridlock.c $(LIBS)

hybridspin.o: src/hybridspin.c $(BPF_SKELETON)
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) -c src/hybridspin.c $(LIBS)

ticket.o: src/ticket.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) -c src/ticket.c $(LIBS)

mcs.o: src/mcs.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) -c src/mcs.c $(LIBS)

scheduling: bmarks/scheduling.c $(OBJ_FILES) $(LIBBPF_OBJ)
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) $(OBJ_FILES) $(LIBBPF_OBJ) bmarks/scheduling.c -o scheduling $(LIBS)

test_correctness: bmarks/test_correctness.c $(OBJ_FILES) $(LIBBPF_OBJ)
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEFINED) $(INCLUDES) $(OBJ_FILES) $(LIBBPF_OBJ) bmarks/test_correctness.c -o test_correctness $(LIBS)

clean:
	rm -rf $(OUTPUT) *.o test_correctness scheduling libsync.a 
	$(MAKE) -C litl/ clean
