"""Assemble the already built and checked Virgin 0.8.2 release. No builds/uploads."""
from pathlib import Path
import hashlib,json,shutil,zipfile
ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'dist/virgin-0.8.2-20261104'
def copy(src,dest):
 dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
for name,src in {'virgin-0.8.2.exe':'build/release-0.8.2-win/virgin.exe','virgin.dll':'build/release-0.8.2-library/virgin.dll','virgin_native.h':'src/virgin_native.h'}.items():copy(ROOT/src,DEST/'windows'/name)
web=ROOT/'dist/virgin-web-0.8.2-20261104'
shutil.copytree(web,DEST/'web',dirs_exist_ok=True)
for env in ('mini_no_psram','mini_psram_qspi','mini_psram_opi'):
 for name in ('bootloader.bin','partitions.bin','firmware.bin'):assert (DEST/'esp32-s3'/env/name).is_file(),(env,name)
assert (DEST/'android/Virgin-0.8.2-OEX.apk').is_file()
assert (DEST/'android/virgin-0.8.2.aar').is_file()
copy(ROOT/'docs/RELEASE_0.8.2.md',DEST/'README.md')
copy(ROOT/'web/ENGINE-LICENSE.txt',DEST/'ENGINE-LICENSE.txt')
for name in ['NOTICE','NOTICE.toolchain']:
 notice=Path.home()/'AppData/Local/Android/Sdk/ndk/27.0.12077973'/name
 if notice.is_file():copy(notice,DEST/'licenses'/('NDK-'+name))
checks=DEST/'checks';checks.mkdir(exist_ok=True)
for n in ['selftest-0.8.2-win.log','selftest-0.8.2-compact.log','uci-0.8.2-win.json','adaptive-oracle-0.8.2.json','native-api-0.8.2.log','android-0.8.2-uci.json','artifact-check-0.8.2.json','web-browser-test-0.8.2-20261104.json']:
 copy(ROOT/'reports'/n,checks/n)
for n in ['selftest-0.8.2-win.log','selftest-0.8.2-compact.log']:
 data=(checks/n).read_bytes();text=data.decode('utf-16') if data[:2] in (b'\xff\xfe',b'\xfe\xff') else data.decode('utf-8',errors='replace')
 assert 'RESULT: PASSED' in text,n
assert json.loads((checks/'uci-0.8.2-win.json').read_text())['status']=='PASS'
assert not json.loads((checks/'adaptive-oracle-0.8.2.json').read_text())['differences']
api=json.loads((ROOT/'virgin-library-test.json').read_text(encoding='utf-8'))
assert any('12 create/search/quit/destroy cycles' in c for c in api['passed'])
(checks/'native-api-0.8.2.json').write_text(json.dumps(api,indent=2),encoding='utf-8')
assert json.loads((checks/'android-0.8.2-uci.json').read_text())['status']=='PASS'
assert json.loads((checks/'web-browser-test-0.8.2-20261104.json').read_text())['status']=='PASS'
source=DEST/'Virgin-0.8.2-source.zip'
with zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED) as z:
 for folder in ['src','data','web','esp32/main','android/library/src','android/oex/src']:
  for f in (ROOT/folder).rglob('*'):
   if f.is_file() and '__pycache__' not in f.parts:z.write(f,f.relative_to(ROOT).as_posix())
 names=['CMakeLists.txt','build_web.py','build_win.ps1','build_android.ps1','build_android_library.ps1',
 'esp32/CMakeLists.txt','esp32/platformio.ini','esp32/partitions.csv','esp32/sdkconfig.defaults',
 'esp32/sdkconfig.mini_no_psram','esp32/sdkconfig.mini_psram_qspi','esp32/sdkconfig.mini_psram_opi',
 'android/library/consumer-rules.pro','android/oex/build.py','android/oex/README.md',
 'docs/RELEASE_0.8.2.md','tools/build_082_targets.py','tools/package_android_082.py',
 'tools/test_web.cjs','tools/test_android_082.py','tools/virgin_082_test.py','tools/virgin_adaptive_uci_test.py']
 for n in names:z.write(ROOT/n,n)
 z.writestr('README.md',(ROOT/'docs/RELEASE_0.8.2.md').read_text(encoding='utf-8'))
 z.writestr('LICENSE',(ROOT/'web/ENGINE-LICENSE.txt').read_text(encoding='utf-8'))
 z.writestr('requirements-tests.txt','python-chess==1.999\n')
expected=json.loads((ROOT/'backups/before-0.8.2-20261104/core2-sha256.json').read_text())
assert all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in expected.items())
manifest={'engine':'Virgin 0.8.2','author':'Dirk D. Sommerfeld','date':'2026-11-04',
 'automatic_max_ms':{'normal':5000,'adaptive':10000},'explicit_limits_take_precedence':True,
 'opening_window_cp':10,'opening_min_depth':6,'opening_previous_depth_required':True,
 'no_external_opening_book':True,'core2_project_unchanged':True,'hardware_flashed':False,
 'verification':{'windows_selftest':'PASS','compact_host_selftest':'PASS','uci_integration':'PASS',
 'native_api':'PASS','adaptive_oracle_cases':680,'adaptive_oracle_differences':0,
 'browser':'PASS','android_x86_64_runtime':'PASS','android_arm_runtime':'not run on a physical ARM device',
 'esp32_s3':'three firmware profiles built; no hardware flash/runtime check in this release'},'files':{}}
for f in sorted(DEST.rglob('*')):
 if f.is_file() and f.name not in ('manifest.json','SHA256SUMS.txt'):
  manifest['files'][f.relative_to(DEST).as_posix()]={'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
(DEST/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
(DEST/'SHA256SUMS.txt').write_text(''.join(v['sha256']+'  '+n+'\n' for n,v in manifest['files'].items()),encoding='utf-8')
archive=ROOT/'dist/Virgin-0.8.2.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
 for f in sorted(DEST.rglob('*')):
  if f.is_file():z.write(f,Path('Virgin-0.8.2')/f.relative_to(DEST))
print('RELEASE READY:',archive)
print('Bytes:',archive.stat().st_size)
