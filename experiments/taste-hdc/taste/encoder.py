"""Encoder: a drum pattern -> hypervector. This is the frozen 'prior'.

Each symbol (an instrument name, or a step marker like "@4") gets one fixed
random hypervector, memoized in a codebook. An event (instrument, step) is the
binding instrument (x) step; a pattern is the bundle of its events, so patterns
that share events come out similar. Swap this for a learned encoder over real
groove features (onsets, velocity, micro-timing) later — nothing downstream of
encode() needs to change.
"""
import numpy as np

from .hdc import random_hv, bind, bundle, DIM


class Encoder:
    def __init__(self, seed=0, d=DIM):
        self._rng = np.random.default_rng(seed)
        self._codebook = {}   # symbol -> fixed hypervector
        self._d = d

    def _sym(self, name):
        hv = self._codebook.get(name)
        if hv is None:
            hv = random_hv(self._rng, self._d)
            self._codebook[name] = hv
        return hv

    def encode(self, events):
        """Encode an iterable of (instrument, step) events into one pattern hv."""
        bound = [bind(self._sym(inst), self._sym(f"@{step}")) for inst, step in events]
        if not bound:
            raise ValueError("encode() needs at least one event")
        return bundle(bound)
