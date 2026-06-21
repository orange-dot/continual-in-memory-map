"""Hyperdimensional computing primitives — bipolar vectors, CPU only.

Plain NumPy so the whole playground lives in RAM. Hypervectors are bipolar
(+1/-1) of dimension DIM; the high dimension is what buys huge capacity and
noise-robustness (cf. modern Hopfield networks / VSA).
"""
import numpy as np

DIM = 10_000  # high-D is the point: ~10 KB/vector, but enormous capacity


def random_hv(rng, d=DIM):
    """A fresh random bipolar hypervector — the atom of representation."""
    return rng.choice(np.array([-1, 1], dtype=np.int8), size=d)


def bundle(hvs):
    """Superpose hypervectors into one by elementwise majority vote.

    Bundling keeps the result *similar* to every input, so it's how a set of
    liked patterns becomes a single 'prototype'. Ties break to +1.
    """
    if len(hvs) == 0:
        raise ValueError("bundle() needs at least one hypervector")
    total = np.stack(hvs).astype(np.int32).sum(axis=0)
    return np.where(total >= 0, 1, -1).astype(np.int8)


def bind(a, b):
    """Bind two hypervectors (elementwise product) — a reversible association.

    Binding ties a role to a filler (instrument <-> step). The result is
    *dissimilar* to both inputs — the complement of bundle.
    """
    return (a * b).astype(np.int8)


def similarity(a, b):
    """Normalized agreement of two bipolar vectors, in [-1, 1]."""
    return float(a.astype(np.int32) @ b.astype(np.int32)) / a.shape[0]
