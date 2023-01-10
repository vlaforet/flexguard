#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include "sched_switch.skel.h"
#include "sched_switch.h"
#include <time.h>

struct hybridlock *lock;

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	return vfprintf(stderr, format, args);
}

int main(int argc, char **argv)
{
	struct sched_switch_bpf *skel;
	int err;

	/* Set up libbpf errors and debug info callback */
	libbpf_set_print(libbpf_print_fn);

	skel = sched_switch_bpf__open();
	if (!skel)
	{
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	lock = malloc(sizeof(struct hybridlock));
	lock->pid = 0;
	lock->spinning = 1;

	skel->bss->input_pid = &lock->pid;
	skel->bss->input_spinning = &lock->spinning;

	err = sched_switch_bpf__load(skel);
	if (err)
	{
		fprintf(stderr, "Failed to load and verify BPF skeleton\n");
		goto cleanup;
	}

	/* Attach tracepoint handler */
	err = sched_switch_bpf__attach(skel);
	if (err)
	{
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		goto cleanup;
	}

	while (lock->spinning)
		lock->pid = getpid();

	printf("Done.");

cleanup:
	sched_switch_bpf__destroy(skel);
	return -err;
}
