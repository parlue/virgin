"""Verify packaged cross-build identities and ABI payloads without installing."""
from pathlib import Path
import hashlib
import json
import os
import re
import struct
import subprocess
import zipfile

root = Path(__file__).resolve().parents[1]
release = root / 'dist/virgin-0.8.7-20261120'
sdk = Path(os.environ['LOCALAPPDATA']) / 'Android/Sdk'
readelf = sdk / 'ndk/27.0.12077973/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-readelf.exe'
results = []


def record(label, **details):
    results.append({'check': label, 'status': 'PASS', **details})
    print('PASS', label, flush=True)


for abi, machine in [('arm64-v8a', 183), ('armeabi-v7a', 40), ('x86_64', 62), ('x86', 3)]:
    for relative in ['android/uci/virgin-' + abi, 'android/jniLibs/' + abi + '/libvirgin.so']:
        path = release / relative
        data = path.read_bytes()
        assert data[:4] == b'\x7fELF' and struct.unpack_from('<H', data, 18)[0] == machine
        assert b'0.8.7' in data and b'OwnBook' in data
        description = subprocess.check_output([str(readelf), '-l', str(path)], text=True)
        loads = [line for line in description.splitlines() if line.strip().startswith('LOAD')]
        assert loads and all(int(line.split()[-1], 16) >= 16384 for line in loads), loads
    record('Android ' + abi + ' UCI/JNI identity, machine and 16 KiB alignment')

with zipfile.ZipFile(release / 'android/virgin-0.8.7.aar') as aar, \
        zipfile.ZipFile(release / 'android/Virgin-0.8.7-OEX.apk') as apk:
    assert aar.testzip() is None and apk.testzip() is None
    for abi in ['arm64-v8a', 'armeabi-v7a', 'x86_64', 'x86']:
        assert aar.read('jni/' + abi + '/libvirgin.so') == (release / 'android/jniLibs' / abi / 'libvirgin.so').read_bytes()
        assert apk.read('lib/' + abi + '/libvirgin-' + abi + '.so') == (release / 'android/uci' / ('virgin-' + abi)).read_bytes()
record('AAR/APK contain the freshly built four-ABI payloads')
tools = sdk / 'build-tools/36.0.0'
badging = subprocess.check_output([str(tools / 'aapt2.exe'), 'dump', 'badging',
                                  str(release / 'android/Virgin-0.8.7-OEX.apk')], text=True)
assert "versionCode='80701'" in badging and "versionName='0.8.7'" in badging
record('APK package version 0.8.7 / 80701')

for family, variants in [('esp32-s3', ['mini_no_psram', 'mini_psram_qspi', 'mini_psram_opi']),
                          ('esp32-core2', [''])]:
    for variant in variants:
        directory = release / family / variant
        firmware = (directory / 'firmware.bin').read_bytes()
        assert firmware[0] == 0xE9
        assert b'0.8.7\0' in firmware and b'OwnBook\0' in firmware
        assert (directory / 'bootloader.bin').read_bytes()[0] == 0xE9
        assert (directory / 'partitions.bin').stat().st_size > 0
        record(f'{family}/{variant or "m5stack_core2"} firmware identity and image headers', bytes=len(firmware))

for name in ['windows/virgin-0.8.7.exe', 'windows/virgin-0.8.7-compact.exe', 'windows/virgin.dll']:
    data = (release / name).read_bytes()
    assert data[:2] == b'MZ' and b'0.8.7' in data and b'OwnBook' in data
record('All Windows PE files identify as 0.8.7 with OwnBook')
report = root / 'reports/release-087-structural.json'
report.write_text(json.dumps({'status': 'PASS', 'checks': results}, indent=2), encoding='utf-8')
