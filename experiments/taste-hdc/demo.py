"""Run the playground:  python demo.py

Teach a few liked drum patterns, then see how strongly new patterns match the
learned 'taste'. consolidate() is the one piece left to implement (memory.py).
"""
from taste.encoder import Encoder
from taste.memory import TasteMemory


def main():
    mem = TasteMemory(Encoder(seed=1))

    # Teach a taste: steady kicks + backbeat snares, a few close variations.
    mem.write([("kick", 0), ("kick", 8), ("snare", 4), ("snare", 12)])
    mem.write([("kick", 0), ("kick", 8), ("snare", 4), ("snare", 12), ("hat", 2)])
    mem.write([("kick", 0), ("kick", 6), ("snare", 4), ("snare", 12)])

    # A close cousin of what we taught vs. something alien (offbeat hats only).
    close = [("kick", 0), ("kick", 8), ("snare", 4), ("snare", 12), ("hat", 6)]
    alien = [("hat", 1), ("hat", 3), ("hat", 5), ("hat", 7), ("hat", 9)]

    print(f"taste match (close):            {mem.query(close):+.3f}")
    print(f"taste match (alien):            {mem.query(alien):+.3f}")
    print(f"episodic items held:            {len(mem.episodic)}")
    print(f"prototypes (after consolidate): {len(mem.prototypes)}")


if __name__ == "__main__":
    main()
