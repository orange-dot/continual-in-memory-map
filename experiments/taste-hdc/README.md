# taste-hdc — a continual-learning taste memory (CPU + RAM playground)

A small playground for the idea we sketched: model "taste" not as the weights of
a big trained net, but as a **two-timescale associative memory** you can keep
teaching, one example at a time, on plain CPU + RAM.

- **In-memory:** the store *is* the model; querying is computing over it.
- **Continual:** `write()` adds an example instantly — nothing is retrained,
  nothing is forgotten.
- **Two timescales (CLS):** a fast *episodic* store catches every example; a
  slow `consolidate()` step distills episodes into general *prototypes*
  (hippocampus -> neocortex).

Everything is bipolar hyperdimensional computing (HDC) in NumPy — no GPU.

## Layout
- `taste/hdc.py` — hypervector primitives (random / bundle / bind / similarity)
- `taste/encoder.py` — pattern -> hypervector (the frozen "prior")
- `taste/memory.py` — `TasteMemory`: `write`, `query`, `consolidate`
- `demo.py` — teach a few drum patterns, query taste

## Run
```
python demo.py
```

## The teaching loop
```python
mem = TasteMemory(Encoder())
mem.write([("kick", 0), ("snare", 4)])   # learn, instantly
mem.query([("kick", 0), ("snare", 4)])   # how well does this fit the taste?
mem.consolidate()                         # sleep: episodes -> prototypes
```

`consolidate()` is the heart — that's where episodes become abstraction.

## Polyglot (the experiment)

The same kernel, in several languages, for the in-memory / neuromorphic core:

- `taste/` — Python reference (int8 +1/-1, clarity first)
- `c/`     — C, bit-packed *binary* hypervectors: bind=XOR, similarity=popcount,
             bundle=majority. The substrate-honest, neuromorphic-flavored version.
- `cpp/`   — (coming) C++ take
- `rust/`  — (coming) Rust take

Shared kernel contract every language implements:
`random_hv`, `bind` (reversible assoc.), `bundle` (superpose), `similarity`.

Build & run the C kernel:
```
cc -O2 -Wall -o c/hdc c/hdc.c && ./c/hdc
```
