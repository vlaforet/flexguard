PLATFORM_NUMA=1

ifeq ($(DEBUG),1)
  DEBUG_FLAGS=-Wall -ggdb -DDEBUG
  COMPILE_FLAGS=-O0 -DADD_PADDING -fno-inline -fPIC
else
  DEBUG_FLAGS=-Wall
  COMPILE_FLAGS=-O3 -DADD_PADDING -fPIC
endif

ifndef NOBPF
OUTPUT := .output
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
VMLINUX := ./vmlinux/$(ARCH)/vmlinux.h
BPFINCLUDES := -I$(OUTPUT) -I../libbpf/include/uapi -I$(dir $(VMLINUX))
COMPILE_FLAGS += -DBPF

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
endif

CORE_NUM := $(shell nproc)
ifneq ($(CORE_NUM), )
CORE_NUM_FLAGS += -DCORE_NUM=${CORE_NUM}
else
CORE_NUM_FLAGS += -DCORE_NUM=8
endif
COMPILE_FLAGS += $(CORE_NUM_FLAGS)
$(info ********************************** Using as a default number of cores: $(CORE_NUM) on 1 socket)
$(info ********************************** Is this correct? If not, fix it in platform_defs.h)

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
	GCC:=gcc
	LIBS := -lrt -lpthread -lnuma
endif

ifndef LOCK_VERSION
  # LOCK_VERSION=-DUSE_HCLH_LOCKS
  # LOCK_VERSION=-DUSE_TTAS_LOCKS
  # LOCK_VERSION=-DUSE_SPINLOCK_LOCKS
  LOCK_VERSION=-DUSE_HYBRIDLOCK_LOCKS
  # LOCK_VERSION=-DUSE_HYBRIDSPIN_LOCKS
  # LOCK_VERSION=-DUSE_MCS_LOCKS
  # LOCK_VERSION=-DUSE_ARRAY_LOCKS
  # LOCK_VERSION=-DUSE_RW_LOCKS
  # LOCK_VERSION=-DUSE_CLH_LOCKS
  # LOCK_VERSION=-DUSE_TICKET_LOCKS
  # LOCK_VERSION=-DUSE_MUTEX_LOCKS
  # LOCK_VERSION=-DUSE_FUTEX_LOCKS
  # LOCK_VERSION=-DUSE_HTICKET_LOCKS
endif

TOP := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

SRCPATH := $(TOP)/src
MAININCLUDE := $(TOP)/include

INCLUDES := $(BPFINCLUDES) -I$(MAININCLUDE)
OBJ_FILES :=  mcs.o clh.o ttas.o spinlock.o rw_ttas.o ticket.o alock.o hclh.o gl_lock.o htlock.o hybridlock.o hybridspin.o futex.o

ifndef NOBPF
OBJ_FILES += $(LIBBPF_OBJ)
LIBS += -lelf -lz
endif

ALL := bank scheduling bank_one bank_simple test_array_alloc test_trylock sample_generic sample_mcs test_correctness stress_one stress_test stress_latency atomic_bench individual_ops uncontended measure_contention libsync.a
ifeq ($(LOCK_VERSION), -DUSE_HTICKET_LOCKS)
ALL += htlock_test
endif

all: $(ALL)
	@echo "############### Used: " $(LOCK_VERSION)

BPF_SKELETON :=
ifndef NOBPF
$(OUTPUT) $(OUTPUT)/libbpf $(BPFTOOL_OUTPUT):
	mkdir -p $@

# Build libbpf
$(LIBBPF_OBJ): $(wildcard $(LIBBPF_SRC)/*.[ch] $(LIBBPF_SRC)/Makefile) | $(OUTPUT)/libbpf
	$(MAKE) -C $(LIBBPF_SRC) BUILD_STATIC_ONLY=1		\
		    OBJDIR=$(dir $@)/libbpf DESTDIR=$(dir $@)	\
		    INCLUDEDIR= LIBDIR= UAPIDIR=			       	\
		    install

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
	$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) -c $(filter %.c,$^) -o $@
	$(LLVM_STRIP) -g $@ # strip useless DWARF info

# Generate BPF skeletons
$(OUTPUT)/%.skel.h: $(OUTPUT)/%.bpf.o | $(OUTPUT) $(BPFTOOL)
	$(BPFTOOL) gen skeleton $< > $@

# Build user-space code
$(patsubst %,$(OUTPUT)/%.o,$(APPS)): %.o: %.skel.h

BPF_SKELETON += $(OUTPUT)/hybridlock.skel.h
endif

litl: include/lock_if.h libsync.a
	$(MAKE) -C litl/ LOCK_VERSION=$(LOCK_VERSION) CORE_NUM_FLAGS=$(CORE_NUM_FLAGS)

libsync.a: ttas.o rw_ttas.o ticket.o clh.o mcs.o hclh.o alock.o htlock.o spinlock.o futex.o hybridlock.o hybridspin.o include/atomic_ops.h include/utils.h include/lock_if.h
	ar -r libsync.a ttas.o rw_ttas.o ticket.o clh.o mcs.o hclh.o alock.o htlock.o spinlock.o futex.o hybridlock.o hybridspin.o

ttas.o: src/ttas.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ttas.c $(LIBS)

spinlock.o: src/spinlock.c
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/spinlock.c $(LIBS)

futex.o: src/futex.c
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/futex.c $(LIBS)

hybridlock.o: src/hybridlock.c $(BPF_SKELETON)
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/hybridlock.c $(LIBS)

hybridspin.o: src/hybridspin.c $(BPF_SKELETON)
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/hybridspin.c $(LIBS)

rw_ttas.o: src/rw_ttas.c
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/rw_ttas.c $(LIBS)

ticket.o: src/ticket.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ticket.c $(LIBS)

ticket_contention.o: src/ticket.c 
	$(GCC) -D_GNU_SOURCE -DMEASURE_CONTENTION $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ticket.c -o ticket_contention.o $(LIBS)

gl_lock.o: src/gl_lock.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/gl_lock.c $(LIBS)

mcs.o: src/mcs.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/mcs.c $(LIBS)

clh.o: src/clh.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/clh.c $(LIBS)

hclh.o: src/hclh.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/hclh.c $(LIBS)

alock.o: src/alock.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/alock.c $(LIBS)

htlock.o: src/htlock.c include/htlock.h
	 $(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/htlock.c $(LIBS) 

bank: bmarks/bank_th.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/bank_th.c -o bank $(LIBS)

scheduling: bmarks/scheduling.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/scheduling.c -o scheduling $(LIBS)

bank_one: bmarks/bank_one.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/bank_one.c -o bank_one $(LIBS)

bank_simple: bmarks/bank_simple.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/bank_simple.c -o bank_simple $(LIBS)

stress_test: bmarks/stress_test.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/stress_test.c -o stress_test $(LIBS)

measure_contention: bmarks/measure_contention.c $(OBJ_FILES) ticket_contention.o Makefile
	$(GCC) -DUSE_TICKET_LOCKS -DMEASURE_CONTENTION -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) ticket_contention.o bmarks/measure_contention.c -o measure_contention $(LIBS)

stress_one: bmarks/stress_one.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/stress_one.c -o stress_one $(LIBS)

test_correctness: bmarks/test_correctness.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/test_correctness.c -o test_correctness $(LIBS)

sample_generic: samples/sample_generic.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) samples/sample_generic.c -o sample_generic $(LIBS)

sample_mcs: samples/sample_mcs.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) samples/sample_mcs.c -o sample_mcs $(LIBS)

test_trylock: bmarks/test_trylock.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/test_trylock.c -o test_trylock $(LIBS)

test_array_alloc: bmarks/test_array_alloc.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/test_array_alloc.c -o test_array_alloc $(LIBS)

stress_latency: bmarks/stress_latency.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/stress_latency.c -o stress_latency $(LIBS)

individual_ops: bmarks/individual_ops.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/individual_ops.c -o individual_ops $(LIBS)

uncontended: bmarks/uncontended.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/uncontended.c -o uncontended $(LIBS)

atomic_bench: bmarks/atomic_bench.c Makefile
	$(GCC) $(PRIMITIVE) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) bmarks/atomic_bench.c -o atomic_bench $(LIBS)

ifeq ($(LOCK_VERSION), -DUSE_HTICKET_LOCKS)
htlock_test: htlock.o bmarks/htlock_test.c Makefile
	$(GCC) -O0 -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) bmarks/htlock_test.c -o htlock_test htlock.o $(LIBS)
endif


clean:
	rm -rf $(OUTPUT) *.o locks mcs_test hclh_test bank_one bank_simple bank* stress_latency* test_array_alloc test_trylock sample_* test_correctness stress_one stress_test* atomic_bench uncontended individual_ops trylock_test htlock_test measure_contention libsync.a scheduling
	$(MAKE) -C litl/ clean
