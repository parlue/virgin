"""Assemble the 0.8.7 release after builds and checks have completed."""
from pathlib import Path
import hashlib
import json
import shutil
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'dist/virgin-0.8.7-20261120'
OLD = ROOT / 'dist/virgin-0.8.6-20261115'
DEST.mkdir(parents=True, exist_ok=True)


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def converted(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    text = source.read_text(encoding='utf-8-sig')
    text = text.replace('0.8.6', '0.8.7').replace('20261115', '20261120')
    target.write_text(text, encoding='utf-8')


for source, target in [('RELEASE_README_0.8.7.md', 'README.md'),
                       ('RELEASE_0.8.7.md', 'RELEASE_0.8.7.md'),
                       ('OPENING_BOOK.md', 'OPENING_BOOK.md')]:
    copy(ROOT / 'docs' / source, DEST / target)
copy(OLD / 'ENGINE-LICENSE.txt', DEST / 'ENGINE-LICENSE.txt')
converted(OLD / 'android/EINBINDEN.md', DEST / 'android/EINBINDEN.md')
converted(OLD / 'linux/README.md', DEST / 'linux/README.md')
converted(ROOT / 'docs/APP_PERFORMANCE_0.8.6.md', DEST / 'APP_PERFORMANCE_0.8.7.md')
if (OLD / 'linux/licenses').exists():
    shutil.copytree(OLD / 'linux/licenses', DEST / 'linux/licenses', dirs_exist_ok=True)
copy(ROOT / 'docs/ESP32_S3_0.8.7.md', DEST / 'esp32-s3/README.md')
core2_reference = ROOT / 'dist/virgin-core2-0.8.0-20261102-certabo-spectrum'
shutil.copytree(core2_reference / 'licenses', DEST / 'esp32-core2/licenses', dirs_exist_ok=True)
for component in ['M5GFX', 'M5Unified', 'esp-nimble-cpp']:
    copy(ROOT / 'esp32_core2/components' / component / 'LICENSE',
         DEST / 'esp32-core2/licenses' / (component + '.txt'))
copy(ROOT / 'esp32_core2/main/PIECES-LICENSE.txt', DEST / 'esp32-core2/PIECES-LICENSE.txt')
shutil.copytree(core2_reference / 'sources/merida-original',
                DEST / 'esp32-core2/sources/merida-original', dirs_exist_ok=True)
shutil.copytree(ROOT / 'esp32_core2/assets', DEST / 'esp32-core2/sources/assets', dirs_exist_ok=True)
shutil.copytree(ROOT / 'esp32_core2/main', DEST / 'esp32-core2/sources/main', dirs_exist_ok=True)
if (OLD / 'android/native-source/BUILD.txt').is_file():
    converted(OLD / 'android/native-source/BUILD.txt', DEST / 'android/native-source/BUILD.txt')

# Reproducible source snapshot. Never include signing keys or local build trees.
ignore = shutil.ignore_patterns('.pio', '.gradle', '.git', '.vscode', 'build',
    'node_modules', '__pycache__', 'private', 'managed_components', '*.apk',
    '*.so', '*.class', '*.jks', '*.keystore', '*.p12', '*.pem',
    'signing-password*', 'local.properties')
snapshot = DEST / 'source'
for directory in ['src', 'data', 'docs', 'android', 'esp32', 'esp32_core2', 'web']:
    shutil.copytree(ROOT / directory, snapshot / directory, ignore=ignore, dirs_exist_ok=True)
for name in ['CMakeLists.txt', 'README.md', 'build087.cmd', 'build087extra.cmd',
             'build_android.ps1', 'build_android_library.ps1', 'build_web.py']:
    copy(ROOT / name, snapshot / name)
tools = ['build_087_targets.py', 'build_linux_087.py', 'package_android_087.py',
    'verify_linux_087.py', 'test_087_performance.py', 'test_087_uci.py',
    'virgin_087_test.py', 'virgin_adaptive_uci_test.py', 'virgin_native_library_test.py',
    'generate_opening_book.py', 'test_opening_book.py', 'test_web.cjs',
    'assemble_release_087.py', 'verify_release_087.py', 'ab_run.sh']
for name in tools:
    copy(ROOT / 'tools' / name, snapshot / 'tools' / name)
for directory in ['src', 'data']:
    shutil.copytree(ROOT / directory, DEST / 'android/native-source' / directory, dirs_exist_ok=True)
copy(ROOT / 'CMakeLists.txt', DEST / 'android/native-source/CMakeLists.txt')
shutil.copytree(ROOT / 'android/library', DEST / 'android/native-source/android/library',
                ignore=ignore, dirs_exist_ok=True)

checks = DEST / 'checks'
checks.mkdir(exist_ok=True)
patterns = ['*087*.json', '*087*.txt', '*087*.log', '*0.8.7*.json',
            'opening-book-uci-087.txt',
            'linux-x86_64-selftest.log']
for pattern in patterns:
    for report in (ROOT / 'reports').glob(pattern):
        if report.is_file():
            copy(report, checks / report.name)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


(checks / 'source-sha256.json').write_text(json.dumps({
    p.relative_to(ROOT).as_posix(): sha(p) for p in sorted((ROOT / 'src').glob('*'))
    if p.is_file()}, indent=2), encoding='utf-8')
if '--prepare-only' in sys.argv:
    print('Prepared release documentation, source snapshot and check reports')
    raise SystemExit(0)
required = ['windows/virgin-0.8.7.exe', 'windows/virgin.dll',
    'windows/virgin-0.8.7-compact.exe', 'android/virgin-0.8.7.aar',
    'android/Virgin-0.8.7-OEX.apk', 'web/virgin.js', 'web/virgin.wasm']
abis = ['arm64-v8a', 'armeabi-v7a', 'x86_64', 'x86']
required += ['android/uci/virgin-' + abi for abi in abis]
required += ['android/jniLibs/' + abi + '/libvirgin.so' for abi in abis]
required += ['linux/virgin-0.8.7-linux-' + arch for arch in ['x86_64', 'armv6', 'armv7']]
required += ['esp32-s3/' + variant + '/' + name
    for variant in ['mini_no_psram', 'mini_psram_qspi', 'mini_psram_opi']
    for name in ['bootloader.bin', 'partitions.bin', 'firmware.bin']]
required += ['esp32-core2/' + name for name in ['bootloader.bin', 'partitions.bin', 'firmware.bin']]
for name in required:
    assert (DEST / name).is_file(), 'Missing release artifact: ' + name
manifest = {'engine': 'Virgin', 'version': '0.8.7', 'date': '2026-11-20',
    'status': 'beta', 'book_data_bytes': 2037, 'book_budget_bytes': 10000,
    'book_default_enabled': True, 'book_lines': 90, 'book_plies': 10,
    'adaptive_equal_cp': 20, 'core2_updated': True,
    'runtime_tested': ['Windows x64 UCI', 'Windows x64 DLL', 'Compact host core',
                       'Linux x86_64 WSL', 'WebAssembly headless Chrome'],
    'hardware_runtime_untested': ['Android', 'Linux ARMv6', 'Linux ARMv7', 'ESP32-S3', 'ESP32 Core2'],
    'required_artifacts': {name: sha(DEST / name) for name in required}}
(DEST / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
files = [p for p in sorted(DEST.rglob('*')) if p.is_file() and p.name != 'SHA256SUMS.txt']
for p in files:
    assert not any(part.lower() == 'private' for part in p.parts)
    assert p.suffix.lower() not in ['.jks', '.keystore', '.p12', '.pem']
(DEST / 'SHA256SUMS.txt').write_text(''.join(
    sha(p) + '  ' + p.relative_to(DEST).as_posix() + '\n' for p in files), encoding='utf-8')
archive = ROOT / 'dist/Virgin-0.8.7-20261120.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
    for p in sorted(DEST.rglob('*')):
        if p.is_file():
            output.write(p, DEST.name + '/' + p.relative_to(DEST).as_posix())
with zipfile.ZipFile(archive) as output:
    assert output.testzip() is None
print(f'RELEASE READY: {DEST}; {len(required)} required artifacts; {len(files)} hashed files')
print(f'ZIP: {archive}; {archive.stat().st_size} bytes; SHA256 {sha(archive)}')
