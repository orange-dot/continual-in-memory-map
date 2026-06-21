CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c11
NATIVE_CFLAGS ?= -O3 -march=native -Wall -Wextra -Wpedantic -std=c11
VEC_CFLAGS ?= -O3 -march=native -fopt-info-vec-optimized -fopt-info-vec-missed -Wall -Wextra -Wpedantic -std=c11

preference: cinm.c task_preference.c cinm.h
	$(CC) $(CFLAGS) -o preference cinm.c task_preference.c

bench: bench_crossbar.c cinm.h
	$(CC) $(CFLAGS) -o bench bench_crossbar.c

cmp: cmp_update.c cinm.c cinm.h
	$(CC) $(CFLAGS) -o cmp cmp_update.c cinm.c -lm

learning: learning_experiment.c cinm.c cinm.h
	$(CC) $(CFLAGS) -o learning learning_experiment.c cinm.c -lm

txn: txn_check.c cinm.c cinm.h
	$(CC) $(CFLAGS) -o txn txn_check.c cinm.c

replay: replay_check.c cinm_log.c cinm.c cinm.h cinm_log.h
	$(CC) $(CFLAGS) -o replay replay_check.c cinm_log.c cinm.c

log-invariants: log_invariants_check.c cinm_log.c cinm.c cinm.h cinm_log.h
	$(CC) $(CFLAGS) -o log-invariants log_invariants_check.c cinm_log.c cinm.c

memory: memory_bench.c cinm_log.c cinm.c cinm.h cinm_log.h
	$(CC) $(CFLAGS) -o memory memory_bench.c cinm_log.c cinm.c

hdc-bits: hdc_bits.c
	$(CC) $(CFLAGS) -o hdc-bits hdc_bits.c

preference-native: cinm.c task_preference.c cinm.h
	$(CC) $(NATIVE_CFLAGS) -o preference-native cinm.c task_preference.c

vec-report:
	$(CC) $(VEC_CFLAGS) -c cinm.c -o /tmp/cinm-vec-report.o

run: preference
	./preference

run-bench: bench
	./bench

run-cmp: cmp
	./cmp

run-learning: learning
	./learning

run-txn: txn
	./txn

run-replay: replay
	./replay

run-log-invariants: log-invariants
	./log-invariants

run-memory: memory
	./memory

run-hdc: hdc-bits
	./hdc-bits

native: preference-native
	./preference-native

clean:
	rm -f preference bench cmp learning txn replay log-invariants memory hdc-bits preference-native *.o

.PHONY: run run-bench run-cmp run-learning run-txn run-replay run-log-invariants run-memory run-hdc native vec-report clean
