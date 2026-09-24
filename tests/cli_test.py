import csv, json, pathlib, subprocess, sys, tempfile
exe, fixture = sys.argv[1:]
def run(*args):
    return subprocess.run([exe, *map(str, args)], capture_output=True, text=True, timeout=15)
with tempfile.TemporaryDirectory() as tmp:
    p = pathlib.Path(tmp)
    report, snapshots = p/'report.json', p/'snapshots.csv'
    result = run('--input', fixture, '--symbol', 'AAPL', '--report', report,
                 '--snapshots', snapshots, '--snapshot-every', 8)
    assert result.returncode == 0, result.stderr
    data = json.loads(report.read_text())
    assert data['complete'] and data['messages'] == 16 and data['active_orders'] == 2
    assert data['unsupported'] == 1 and data['filtered'] == 1
    rows = list(csv.DictReader(snapshots.open()))
    assert rows[-2]['price'] == '150.0100' and rows[-1]['quantity'] == '110'
    assert 'MSFT' not in result.stdout
    expected = json.loads(pathlib.Path(fixture).with_name('expected.json').read_text())
    assert expected['aapl_bid'] in result.stdout and expected['aapl_ask'] in result.stdout
    for args in [[], ['--unknown','x'], ['--input',fixture,'--depth','0'],
                 ['--input',fixture,'--depth','-1'], ['--input',fixture,'--depth','2x'],
                 ['--input',fixture,'--symbol','MISSING'], ['--input',fixture,'--report',fixture],
                 ['--input',fixture,'--report',report,'--snapshots',report]]:
        assert run(*args).returncode != 0, args
    truncated = p/'truncated.itch'
    truncated.write_bytes(pathlib.Path(fixture).read_bytes()[:-1])
    result = run('--input',truncated,'--report',report)
    assert result.returncode != 0 and 'offset=' in result.stderr
    data = json.loads(report.read_text())
    assert not data['complete'] and data['errors'] == 1
    empty = p/'empty.itch'; empty.write_bytes(b'')
    assert run('--input',empty).returncode == 0
    alias = p/'alias.itch'; alias.symlink_to(pathlib.Path(fixture).resolve())
    assert run('--input',fixture,'--report',alias).returncode != 0
print('CLI integration passed')
