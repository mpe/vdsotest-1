#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "compiler.h"
#include "vdsotest.h"

static int (*getcpu)(unsigned *cpu, unsigned *node, void *tcache);

static int getcpu_syscall_wrapper(unsigned *cpu, unsigned *node, void *tcache)
{
	return syscall(SYS_getcpu, cpu, node, tcache);
}

static void getcpu_syscall_nofail(unsigned *cpu, unsigned *node, void *tcache)
{
	int err;

	err = getcpu_syscall_wrapper(cpu, node, tcache);
	if (err)
		error(EXIT_FAILURE, errno, "SYS_getcpu");
}

static void getcpu_nofail(unsigned *cpu, unsigned *node, void *tcache)
{
	int err;

	err = getcpu(cpu, node, tcache);
	if (err)
		error(EXIT_FAILURE, errno, "getcpu");
}

/* Set affinity to the current CPU */
static void getcpu_setup(const struct ctx *ctx)
{
	unsigned int cpu;
	cpu_set_t mask;

	getcpu_syscall_nofail(&cpu, NULL, NULL);

	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);

	if (sched_setaffinity(getpid(), sizeof(mask), &mask))
		error(EXIT_FAILURE, errno, "sched_setaffinity");
}

/* Force the scheduler to migrate us off the current cpu */
static void migrate(const struct ctx *ctx, cpu_set_t *cpus_allowed)
{
	unsigned int cpu;

	if (sched_getaffinity(getpid(), sizeof(cpu_set_t), cpus_allowed))
		error(EXIT_FAILURE, errno, "sched_getaffinity");

	getcpu_syscall_nofail(&cpu, NULL, NULL);

	assert(CPU_ISSET(cpu, cpus_allowed));

	CPU_CLR(cpu, cpus_allowed);

	if (CPU_COUNT(cpus_allowed) == 0) {
		*cpus_allowed = ctx->cpus_allowed;
	}

	if (sched_setaffinity(getpid(), sizeof(cpu_set_t), cpus_allowed))
		error(EXIT_FAILURE, errno, "sched_setaffinity");
}

static void getcpu_bench(struct ctx *ctx, struct bench_results *res)
{
	uint64_t calls;
	unsigned int cpu;

	getcpu_setup(ctx);

	ctx_start_timer(ctx);

	bench_interval_begin(&res->vdso_interval, calls);

	while (!test_should_stop(ctx)) {
		getcpu(&cpu, NULL, NULL);
		calls++;
	}

	bench_interval_end(&res->vdso_interval, calls);

	ctx_start_timer(ctx);

	bench_interval_begin(&res->sys_interval, calls);

	while (!test_should_stop(ctx)) {
		getcpu_syscall_wrapper(&cpu, NULL, NULL);
		calls++;
	}

	bench_interval_end(&res->sys_interval, calls);
}

static void getcpu_verify(struct ctx *ctx)
{
	getcpu_setup(ctx);

	ctx_start_timer(ctx);

	while (!test_should_stop(ctx)) {
		cpu_set_t cpus_allowed;
		unsigned long loops;
		unsigned long i;

		migrate(ctx, &cpus_allowed);
		loops = random() % 1000000;

		debug(ctx, "loops = %ld\n", loops);

		for (i = 0; i < loops && !test_should_stop(ctx); i++) {
			unsigned int cpu;

			getcpu_nofail(&cpu, NULL, NULL);

			if (!CPU_ISSET(cpu, &cpus_allowed)) {
				log_failure(ctx, "sched_getcpu returned "
					    "unallowed cpu %d\n", cpu);
			}

			getcpu_syscall_nofail(&cpu, NULL, NULL);

			if (!CPU_ISSET(cpu, &cpus_allowed)) {
				log_failure(ctx, "SYS_getcpu returned "
					    "unallowed cpu %d\n", cpu);
			}
		}
	}
}

static void getcpu_notes(struct ctx *ctx)
{
	if (getcpu == getcpu_syscall_wrapper)
		printf("Note: vDSO version of getcpu not found\n");
}

static const struct test_suite getcpu_ts = {
	.name = "getcpu",
	.bench = getcpu_bench,
	.verify = getcpu_verify,
	.notes = getcpu_notes,
};

static const char *getcpu_vdso_names[] = {
	"__kernel_getcpu",
	"__vdso_getcpu",
};

static void __constructor getcpu_init(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(getcpu_vdso_names); i++) {
		getcpu = get_vdso_sym(getcpu_vdso_names[i]);
		if (getcpu)
			break;
	}

	if (!getcpu)
		getcpu = getcpu_syscall_wrapper;

	register_testsuite(&getcpu_ts);
}
