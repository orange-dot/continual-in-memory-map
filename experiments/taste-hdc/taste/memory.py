"""TasteMemory — a two-timescale associative memory (CPU + RAM).

Fast/episodic store  -> every write() lands here instantly; nothing forgotten.
Slow/prototype store -> abstractions distilled from episodes by consolidate().

Mirrors Complementary Learning Systems: hippocampus (fast, specific) + neocortex
(slow, general). write() is learning, query() is inference, both over the same
memory — there is no separate training phase and no backprop.
"""
from .hdc import similarity, bundle


class TasteMemory:
    def __init__(self, encoder):
        self.encoder = encoder
        self.episodic = []     # dicts {"hv","events","label"} — fast, appended on write
        self.prototypes = []   # dicts {"hv","n"} — slow, produced by consolidate

    def write(self, events, label="like"):
        """Continual, O(1), no forgetting: encode and append to episodic."""
        hv = self.encoder.encode(events)
        self.episodic.append({"hv": hv, "events": list(events), "label": label})
        return hv

    def query(self, events):
        """Inference = recall: best similarity to anything we currently hold."""
        cue = self.encoder.encode(events)
        store = self.prototypes + self.episodic
        if not store:
            return 0.0
        return max(similarity(cue, item["hv"]) for item in store)

    def consolidate(self, threshold=0.2):
        """Slow path ('sleep'): distill episodic items into prototypes.

        The harness is here; the *policy* — how episodes become prototypes — is
        the real design decision, and it's yours.
        """
        if not self.episodic:
            return

        # NOTE: unimplemented in this parked seed. The active contribution moved
        # to the C-first CINM kernel (experiments/c-kernel/, decision D011). Kept
        # as a reference sketch only: build `new_prototypes` from self.episodic.
        #
        # Group similar episodic items, then bundle() each group into ONE
        # prototype so a cluster of liked patterns collapses to a single
        # "taste" vector. You decide HOW to group, e.g.:
        #   - greedy: take an item as a seed, pull in every remaining item with
        #     similarity(seed["hv"], other["hv"]) >= threshold, bundle the group;
        #   - or fixed-k clustering; or simply one prototype per label.
        # Each prototype is a dict: {"hv": <bundled hv>, "n": <#items merged>}.
        new_prototypes = []

        self.prototypes.extend(new_prototypes)
        self.episodic.clear()   # episodes consolidated -> empty the fast store
