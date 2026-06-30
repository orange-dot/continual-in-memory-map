CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c23
NATIVE_CFLAGS ?= -O3 -march=native -Wall -Wextra -Wpedantic -std=c23
VEC_CFLAGS ?= -O3 -march=native -fopt-info-vec-optimized -fopt-info-vec-missed -Wall -Wextra -Wpedantic -std=c23

# Scale benchmark (A: systems/scale). Capacity is a build-time parameter; the map
# is heap-allocated by the bench so a large MAX_CELLS does not hit the stack.
# Three profiles answer doc-11 rule #8 (-O2 vs -O3 native vs -ffast-math).
SCALE_MAX           ?= 1048576
SCALE_CFLAGS        ?= -O2 -Wall -Wextra -Wpedantic -std=c23 -DCINM_MAX_CELLS=$(SCALE_MAX)
SCALE_NATIVE_CFLAGS ?= -O3 -march=native -Wall -Wextra -Wpedantic -std=c23 -DCINM_MAX_CELLS=$(SCALE_MAX)
SCALE_FAST_CFLAGS   ?= -O3 -march=native -ffast-math -Wall -Wextra -Wpedantic -std=c23 -DCINM_MAX_CELLS=$(SCALE_MAX)

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

# Faza 3a: exact-key index equivalence gate. Links the untouched kernel (cinm.c) and the
# auxiliary index (cinm_index.c); asserts the index selects the identical cell as the scan.
scale-index-check: scale_index_check.c cinm.c cinm_index.c cinm.h cinm_index.h
	$(CC) $(CFLAGS) -o scale-index-check scale_index_check.c cinm.c cinm_index.c

# Faza 3b: NN (prototype) index equivalence gate. Same shape as scale-index-check but for the
# KD-tree (cinm_nn_index.c); asserts the index reproduces cinm_address_nn's cell, ties included.
scale-nn-index-check: scale_nn_index_check.c cinm.c cinm_nn_index.c cinm.h cinm_nn_index.h
	$(CC) $(CFLAGS) -o scale-nn-index-check scale_nn_index_check.c cinm.c cinm_nn_index.c

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

scale: scale_bench.c cinm.c cinm.h
	$(CC) $(SCALE_CFLAGS) -o scale scale_bench.c cinm.c

# Faza 3a payoff: index find vs scan address across the sweep (links the untouched kernel
# + the auxiliary index). Large capacity like the scale bench so the map is heap-allocated.
scale-index-bench: scale_index_bench.c cinm.c cinm_index.c cinm.h cinm_index.h
	$(CC) $(SCALE_CFLAGS) -o scale-index-bench scale_index_bench.c cinm.c cinm_index.c

# Faza 3b payoff: KD-tree NN find vs scan cinm_address_nn across the sweep. Same large
# capacity so the map is heap-allocated; links the untouched kernel + the auxiliary index.
scale-nn-index-bench: scale_nn_index_bench.c cinm.c cinm_nn_index.c cinm.h cinm_nn_index.h
	$(CC) $(SCALE_CFLAGS) -o scale-nn-index-bench scale_nn_index_bench.c cinm.c cinm_nn_index.c

# Faza 3b regime probe: does the O(count) NN scan even matter in the regime NN addressing
# produces? Bounded-radius pruning + clustered-stream count saturation. Closes the question.
nn-regime-probe: nn_regime_probe.c cinm.c cinm_nn_index.c cinm.h cinm_nn_index.h
	$(CC) $(SCALE_CFLAGS) -o nn-regime-probe nn_regime_probe.c cinm.c cinm_nn_index.c

scale-native: scale_bench.c cinm.c cinm.h
	$(CC) $(SCALE_NATIVE_CFLAGS) -o scale-native scale_bench.c cinm.c

scale-fast: scale_bench.c cinm.c cinm.h
	$(CC) $(SCALE_FAST_CFLAGS) -o scale-fast scale_bench.c cinm.c

# Auto-vectorization report for the scale build (cinm.c hot loops at -march=native).
vec-report-scale:
	$(CC) $(SCALE_NATIVE_CFLAGS) -fopt-info-vec-optimized -fopt-info-vec-missed -c cinm.c -o /tmp/cinm-scale-vec.o

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

run-scale-index-check: scale-index-check
	./scale-index-check

run-scale-nn-index-check: scale-nn-index-check
	./scale-nn-index-check

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

run-scale: scale
	./scale

run-scale-index-bench: scale-index-bench
	./scale-index-bench

run-scale-nn-index-bench: scale-nn-index-bench
	./scale-nn-index-bench

run-nn-regime-probe: nn-regime-probe
	./nn-regime-probe

run-scale-native: scale-native
	./scale-native

run-scale-fast: scale-fast
	./scale-fast

clean:
	rm -f preference bench cmp learning txn core-check replay log-invariants memory ledger consolidate nn-address scale-index-check scale-index-bench scale-nn-index-check scale-nn-index-bench nn-regime-probe undo taste selfadapt drum-adapter drum-features drum-e2e drum-gate groove-archive drum-render hdc-bits preference-native scale scale-native scale-fast *.o
	rm -rf runs

.PHONY: run run-bench run-cmp run-learning run-txn run-core-check run-replay run-log-invariants run-memory run-ledger run-consolidate run-nn-address run-scale-index-check run-scale-nn-index-check run-scale-index-bench run-scale-nn-index-bench run-nn-regime-probe run-undo run-taste-loop run-self-adapt run-drum-adapter run-drum-features run-drum-e2e run-drum-gate run-groove-archive run-drum-render run-hdc native vec-report vec-report-scale run-scale run-scale-native run-scale-fast clean
