#!/usr/bin/env python3
"""Deterministic multi-symbol ITCH workloads with an independent final-state oracle."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import random
import struct

PROFILES = {
    'balanced': (32, 256, 64),
    'deep': (1, 32768, 4096),
    'wide': (256, 64, 32),
    'tiny': (3, 16, 8),
}
MASK = (1 << 64) - 1
PRIME = 1099511628211

class Digest:
    def __init__(self):
        self.value = 14695981039346656037
    def number(self, value):
        self.value = ((self.value ^ value) * PRIME) & MASK
    def text(self, value):
        value = value.rstrip(' ')
        self.number(len(value))
        for byte in value.encode('ascii'):
            self.number(byte)


def generate(path, profile, updates, seed):
    symbols, per_symbol, levels = PROFILES[profile]
    rng = random.Random(seed)
    counts = Counter()
    digest, sha = Digest(), hashlib.sha256()
    messages, next_id = 0, 1
    slots = []
    peak_orders = symbols * per_symbol
    def symbol(locate):
        return f'S{locate:06d}'
    def price(locate, side):
        displacement = 100 + 10 * rng.randrange(levels)
        return 1000000 + locate * 10000 + (-displacement if side == 'B' else displacement)
    with path.open('wb') as stream:
        def emit(kind, locate, payload, fields):
            nonlocal messages
            timestamp = 34200000000000 + messages * 1000
            tracking = messages % 65536
            body = (kind.encode() + struct.pack('>HH', locate, tracking)
                    + timestamp.to_bytes(6, 'big') + payload)
            frame = struct.pack('>H', len(body)) + body
            stream.write(frame); sha.update(frame)
            for value in [ord(kind), locate, tracking, timestamp]:
                digest.number(value)
            for value in fields:
                if isinstance(value, str): digest.text(value)
                else: digest.number(value)
            messages += 1; counts[kind] += 1
        emit('S', 0, b'O', [ord('O')])
        for locate in range(1, symbols + 1):
            name = symbol(locate)
            # R: name, category, status, lot, lot-only, class, subtype,
            # authenticity, threshold, IPO, LULD tier, ETP, leverage, inverse.
            payload = (name.encode().ljust(8) + b'QN' + struct.pack('>I', 100)
                       + b'NC  TNN1N' + struct.pack('>I', 0) + b'N')
            assert len(payload) == 28
            emit('R', locate, payload, [name, ord('Q'), ord('N'), 100])
            emit('H', locate, name.encode().ljust(8) + b'T     ', [name, ord('T'), ''])
        def add_slot(locate, side):
            nonlocal next_id
            order = [next_id, locate, side, rng.randrange(100, 1001), price(locate, side)]
            next_id += 1
            kind = 'F' if rng.randrange(5) == 0 else 'A'
            participant = 'TEST' if kind == 'F' else ''
            name = symbol(locate)
            payload = (struct.pack('>QcI', order[0], side.encode(), order[3])
                       + name.encode().ljust(8) + struct.pack('>I', order[4])
                       + participant.encode())
            emit(kind, locate, payload, [order[0], ord(side), order[3], order[4], name, participant])
            return order
        for locate in range(1, symbols + 1):
            for i in range(per_symbol):
                slots.append(add_slot(locate, 'B' if i % 2 == 0 else 'S'))
        for _ in range(updates):
            index = rng.randrange(len(slots))
            order = slots[index]
            ref, locate, side, quantity, old_price = order
            choice = rng.randrange(100)
            if choice < 25:
                new_qty, new_price = rng.randrange(100, 1001), price(locate, side)
                emit('U', locate, struct.pack('>QQII', ref, next_id, new_qty, new_price),
                     [ref, next_id, new_qty, new_price])
                order[0], order[3], order[4] = next_id, new_qty, new_price
                next_id += 1
            elif choice < 45:
                emit('D', locate, struct.pack('>Q', ref), [ref])
                slots[index] = add_slot(locate, side)
            else:
                kind = 'X' if choice < 65 else 'E' if choice < 90 else 'C'
                removed = rng.randrange(1, quantity + 1)
                match = messages if kind != 'X' else 0
                printable = ord('Y') if kind == 'C' else 0
                execution_price = old_price if kind == 'C' else 0
                payload = struct.pack('>QI', ref, removed)
                if kind != 'X': payload += struct.pack('>Q', match)
                if kind == 'C': payload += b'Y' + struct.pack('>I', execution_price)
                emit(kind, locate, payload, [ref, removed, match, printable, execution_price])
                order[3] -= removed
                if not order[3]: slots[index] = add_slot(locate, side)
        emit('S', 0, b'C', [ord('C')])
    aggregated = defaultdict(lambda: [0, 0])
    for ref, locate, side, quantity, order_price in slots:
        value = aggregated[(locate, side, order_price)]
        value[0] += quantity; value[1] += 1
    rows = [[loc, ord(side), p, q, n] for (loc, side, p), (q, n) in sorted(aggregated.items())]
    expected = dict(profile=profile, updates=updates, seed=seed, symbols=symbols,
                    orders_per_symbol=per_symbol, candidate_levels_per_side=levels,
                    peak_active_orders=peak_orders, active_orders=len(slots),
                    messages=messages, by_type=dict(sorted(counts.items())),
                    sha256=sha.hexdigest(), decoded_digest=digest.value, levels=rows)
    path.with_suffix('.expected.json').write_text(json.dumps(expected, indent=2) + '\n')
    return expected

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--profile', choices=PROFILES, default='balanced')
    parser.add_argument('--updates', type=int, default=1000000)
    parser.add_argument('--seed', type=int, default=20260924)
    args = parser.parse_args()
    if args.updates < 0: parser.error('--updates must be nonnegative')
    result = generate(args.output, args.profile, args.updates, args.seed)
    print(json.dumps({k: v for k, v in result.items() if k != 'levels'}, indent=2))
