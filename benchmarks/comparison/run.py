#!/usr/bin/env python3
"""Run a correctness-gated, paired same-host comparison; never compare README numbers."""
import argparse
import hashlib
import json
import platform
from pathlib import Path
import statistics
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]
PEER_COMMIT = '8af52e5ebcc1a112749a28510d998f56a031db8f'


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def call(executable, source, mode):
    result = subprocess.run([str(executable), str(source), mode], capture_output=True,
                            text=True, check=True, timeout=300)
    return json.loads(result.stdout)


def check_result(result, expected):
    for key in ('messages', 'decoded_digest', 'levels'):
        if result[key] != expected[key]:
            raise ValueError(f"{result['engine']}: {key} differs from independent oracle")
    actual_orders = sum(level[4] for level in result['levels'])
    if actual_orders != expected['active_orders']:
        raise ValueError('active order count differs from independent oracle')


def cache_values(build):
    values = {}
    for line in (build / 'CMakeCache.txt').read_text().splitlines():
        if line.startswith(('CMAKE_BUILD_TYPE:', 'CMAKE_CXX_COMPILER:', 'CMAKE_CXX_FLAGS:',
                            'CMAKE_CXX_FLAGS_RELEASE:', 'ITCH_COMPARISON_PEER:',
                            'CMAKE_INTERPROCEDURAL_OPTIMIZATION:')):
            key, value = line.split('=', 1)
            values[key.split(':')[0]] = value
    return values


def compare(build, source, rounds, hardware, verify_only=False, initial_executable=None, initial_result=None):
    expected_path = source.with_suffix('.expected.json')
    expected = json.loads(expected_path.read_text())
    if sha256(source) != expected['sha256']:
        raise ValueError('Input SHA-256 differs from its oracle manifest')
    executables = {'fishy-ops': build / 'itch_compare_self',
                   'bbalouki-itchcpp': build / 'itch_compare_peer'}
    initial_metadata = None
    if initial_executable is not None:
        initial_metadata = json.loads(initial_result.read_text())
        if initial_metadata['workload']['sha256'] != expected['sha256']:
            raise ValueError('Initial result must describe this exact input')
        if sha256(initial_executable) != initial_metadata['binary_sha256']['fishy-ops']:
            raise ValueError('Initial executable differs from its recorded baseline hash')
        executables['fishy-ops-initial'] = initial_executable
    cache = cache_values(build)
    if not verify_only and cache.get('CMAKE_BUILD_TYPE') != 'Release':
        raise ValueError('Published timing requires a Release build')
    peer = Path(cache['ITCH_COMPARISON_PEER'])
    peer_revision = subprocess.check_output(['git', '-C', str(peer), 'rev-parse', 'HEAD'], text=True).strip()
    peer_dirty = subprocess.check_output(['git', '-C', str(peer), 'status', '--porcelain'], text=True).strip()
    if peer_revision != PEER_COMMIT or peer_dirty:
        raise ValueError('Peer must be an unmodified checkout of the pinned revision')
    checks = {}
    for engine, executable in executables.items():
        result = call(executable, source, 'verify')
        check_result(result, expected)
        checks[engine] = {key: value for key, value in result.items() if key != 'levels'}
    if checks['fishy-ops']['sink'] != checks['bbalouki-itchcpp']['sink']:
        raise ValueError('Header sinks differ')
    result = {'schema_version': 1, 'utc': datetime.now(timezone.utc).isoformat(),
              'hardware': hardware, 'platform': platform.platform(),
              'python': sys.version, 'build': cache,
              'self_commit': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
              'self_dirty': bool(subprocess.check_output(['git', '-C', str(ROOT), 'status', '--porcelain'], text=True).strip()),
              'peer_repository': 'https://github.com/bbalouki/itchcpp', 'peer_commit': peer_revision,
              'input': source.name, 'input_bytes': source.stat().st_size,
              'expected_sha256': sha256(expected_path),
              'workload': {k: v for k, v in expected.items() if k != 'levels'},
              'binary_sha256': {name: sha256(exe) for name, exe in executables.items()},
              'validation': checks, 'samples': [], 'summary': {},
              'initial_self_commit': initial_metadata['self_commit'] if initial_metadata else None}
    compiler = cache['CMAKE_CXX_COMPILER']
    result['compiler_version'] = subprocess.check_output([compiler, '--version'], text=True).strip()
    if verify_only:
        return result
    for mode in ('decode', 'book'):
        for pair in range(rounds):
            order = list(executables)
            rotation = pair % len(order)
            order = order[rotation:] + order[:rotation]
            for position, engine in enumerate(order):
                sample = call(executables[engine], source, mode)
                if sample['messages'] != expected['messages'] or sample['sink'] != checks[engine]['sink']:
                    raise ValueError('Timed run differs from validated message count/header sink')
                if sample['seconds'] <= 0:
                    raise ValueError('Nonpositive duration')
                sample.pop('levels'); sample.pop('decoded_digest')
                sample['reported_engine'] = sample['engine']
                sample['engine'] = engine
                sample.update(pair=pair, position=position,
                              messages_per_second=sample['messages'] / sample['seconds'])
                result['samples'].append(sample)
        summaries = {}
        for engine in executables:
            samples = [s for s in result['samples'] if s['mode'] == mode and s['engine'] == engine]
            rates = [s['messages_per_second'] for s in samples]
            summaries[engine] = dict(median_messages_per_second=statistics.median(rates),
                                     min_messages_per_second=min(rates), max_messages_per_second=max(rates),
                                     median_peak_rss_bytes=statistics.median(s['peak_rss_bytes'] for s in samples))
        ratios = []
        for pair in range(rounds):
            entries = {s['engine']: s['messages_per_second'] for s in result['samples']
                       if s['mode'] == mode and s['pair'] == pair}
            ratios.append(entries['fishy-ops'] / entries['bbalouki-itchcpp'])
        summaries['self_over_peer_paired_ratio'] = dict(median=statistics.median(ratios),
                                                       minimum=min(ratios), maximum=max(ratios))
        if initial_metadata:
            initial_ratios = []
            for pair in range(rounds):
                entries = {s['engine']: s['messages_per_second'] for s in result['samples']
                           if s['mode'] == mode and s['pair'] == pair}
                initial_ratios.append(entries['fishy-ops'] / entries['fishy-ops-initial'])
            summaries['self_over_initial_paired_ratio'] = dict(median=statistics.median(initial_ratios),
                                                              minimum=min(initial_ratios), maximum=max(initial_ratios))
        result['summary'][mode] = summaries
    return result

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=7)
    parser.add_argument('--hardware', default='unspecified; do not publish without recording hardware')
    parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('--initial-executable', type=Path)
    parser.add_argument('--initial-result', type=Path)
    args = parser.parse_args()
    if args.rounds < 3: parser.error('--rounds must be at least 3')
    if bool(args.initial_executable) != bool(args.initial_result):
        parser.error('--initial-executable and --initial-result must be used together')
    try:
        result = compare(args.build.resolve(), args.input.resolve(), args.rounds, args.hardware, args.verify_only,
                         args.initial_executable.resolve() if args.initial_executable else None,
                         args.initial_result.resolve() if args.initial_result else None)
    except (ValueError, KeyError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, f'Comparison rejected: {error}\n')
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result['summary'] or {'validation': 'passed'}, indent=2))
