CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c23
NATIVE_CFLAGS ?= -O3 -march=native -Wall -Wextra -Wpedantic -std=c23
VEC_CFLAGS ?= -O3 -march=native -fopt-info-vec-optimized -fopt-info-vec-missed -Wall -Wextra -Wpedantic -std=c23

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

core-check: core_check.c cinm.c cinm.h
	$(CC) $(CFLAGS) -o core-check core_check.c cinm.c

replay: replay_check.c cinm_log.c cinm.c cinm.h cinm_log.h
	$(CC) $(CFLAGS) -o replay replay_check.c cinm_log.c cinm.c

log-invariants: log_invariants_check.c cinm_log.c cinm.c cinm.h cinm_log.h
	$(CC) $(CFLAGS) -o log-invariants log_invariants_check.c cinm_log.c cinm.c

memory: memory_bench.c cinm_log.c cinm.c cinm.h cinm_log.h
	$(CC) $(CFLAGS) -o memory memory_bench.c cinm_log.c cinm.c

ledger: ledger_check.c cinm_ledger.c cinm_ledger.h
	$(CC) $(CFLAGS) -o ledger ledger_check.c cinm_ledger.c

consolidate: consolidate_check.c cinm.c cinm_log.c cinm_ledger.c cinm.h cinm_log.h cinm_ledger.h
	$(CC) $(CFLAGS) -o consolidate consolidate_check.c cinm.c cinm_log.c cinm_ledger.c

nn-address: nn_address_check.c cinm.c cinm_ledger.c cinm.h cinm_ledger.h
	$(CC) $(CFLAGS) -o nn-address nn_address_check.c cinm.c cinm_ledger.c

undo: undo_check.c cinm.c cinm_log.c cinm_undo.c cinm.h cinm_log.h cinm_undo.h
	$(CC) $(CFLAGS) -o undo undo_check.c cinm.c cinm_log.c cinm_undo.c

taste: taste_check.c cinm.c cinm_log.c cinm_undo.c cinm.h cinm_log.h cinm_undo.h
	$(CC) $(CFLAGS) -o taste taste_check.c cinm.c cinm_log.c cinm_undo.c

selfadapt: selfadapt_check.c cinm.c cinm_ledger.c cinm_selfadapt.c cinm.h cinm_ledger.h cinm_selfadapt.h
	$(CC) $(CFLAGS) -o selfadapt selfadapt_check.c cinm.c cinm_ledger.c cinm_selfadapt.c

drum-adapter: drum_adapter_check.c cinm_drum_adapter.c cinm.c cinm_log.c cinm.h cinm_log.h cinm_drum_adapter.h
	$(CC) $(CFLAGS) -o drum-adapter drum_adapter_check.c cinm_drum_adapter.c cinm.c cinm_log.c

drum-features: drum_features_check.c cinm_drum_adapter.c cinm.c cinm_log.c cinm.h cinm_log.h cinm_drum_adapter.h
	$(CC) $(CFLAGS) -o drum-features drum_features_check.c cinm_drum_adapter.c cinm.c cinm_log.c

drum-e2e: drum_e2e_check.c cinm_drum_adapter.c cinm.c cinm_log.c cinm.h cinm_log.h cinm_drum_adapter.h
	$(CC) $(CFLAGS) -o drum-e2e drum_e2e_check.c cinm_drum_adapter.c cinm.c cinm_log.c

drum-gate: drum_gate_check.c cinm_drum_adapter.c cinm.c cinm_log.c cinm.h cinm_log.h cinm_drum_adapter.h
	$(CC) $(CFLAGS) -o drum-gate drum_gate_check.c cinm_drum_adapter.c cinm.c cinm_log.c

groove-archive: groove_check.c cinm.c cinm_log.c cinm_drum_adapter.c cinm_groove.c cinm.h cinm_log.h cinm_drum_adapter.h cinm_groove.h
	$(CC) $(CFLAGS) -o groove-archive groove_check.c cinm.c cinm_log.c cinm_drum_adapter.c cinm_groove.c

drum-render: drum_render_check.c cinm.c cinm_log.c cinm_drum_adapter.c cinm_groove.c cinm_drum_render.c cinm.h cinm_log.h cinm_drum_adapter.h cinm_groove.h cinm_drum_render.h
	$(CC) $(CFLAGS) -o drum-render drum_render_check.c cinm.c cinm_log.c cinm_drum_adapter.c cinm_groove.c cinm_drum_render.c

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

run-core-check: core-check
	./core-check

run-replay: replay
	./replay

run-log-invariants: log-invariants
	./log-invariants

run-memory: memory
	./memory

run-ledger: ledger
	./ledger

run-consolidate: consolidate
	./consolidate

run-nn-address: nn-address
	./nn-address

run-undo: undo
	./undo

run-taste-loop: taste
	./taste

run-self-adapt: selfadapt
	./selfadapt

run-drum-adapter: drum-adapter
	./drum-adapter

run-drum-features: drum-features
	./drum-features

run-drum-e2e: drum-e2e
	./drum-e2e

run-drum-gate: drum-gate
	mkdir -p runs/cim-drum-m1-gate
	./drum-gate

run-groove-archive: groove-archive
	./groove-archive

run-drum-render: drum-render
	mkdir -p runs/cim-drum-m2-render
	./drum-render

run-hdc: hdc-bits
	./hdc-bits

native: preference-native
	./preference-native

clean:
	rm -f preference bench cmp learning txn core-check replay log-invariants memory ledger consolidate nn-address undo taste selfadapt drum-adapter drum-features drum-e2e drum-gate groove-archive drum-render hdc-bits preference-native *.o
	rm -rf runs

.PHONY: run run-bench run-cmp run-learning run-txn run-core-check run-replay run-log-invariants run-memory run-ledger run-consolidate run-nn-address run-undo run-taste-loop run-self-adapt run-drum-adapter run-drum-features run-drum-e2e run-drum-gate run-groove-archive run-drum-render run-hdc native vec-report clean
