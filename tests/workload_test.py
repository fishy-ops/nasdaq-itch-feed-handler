"""Check the independent workload generator against the compiled decoder/book."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

executable, generator = sys.argv[1:]
spec = importlib.util.spec_from_file_location('workload', generator)
workload = importlib.util.module_from_spec(spec)
spec.loader.exec_module(workload)
with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    for seed, updates in [(1, 0), (20260924, 1000), (7919, 3000)]:
        path = root / f'{seed}.itch'
        expected = workload.generate(path, 'tiny', updates, seed)
        result = subprocess.run([executable, str(path), 'verify'], check=True,
                                capture_output=True, text=True, timeout=30)
        actual = json.loads(result.stdout)
        for key in ['messages', 'decoded_digest', 'levels']:
            assert expected[key] == actual[key], (seed, key)
        assert sum(row[4] for row in actual['levels']) == expected['active_orders']
        other = root / 'repeated.itch'
        assert workload.generate(other, 'tiny', updates, seed) == expected
        assert other.read_bytes() == path.read_bytes()
print('Independent multi-symbol workload oracle passed for three seeds/history lengths.')
