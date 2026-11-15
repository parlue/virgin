"""Assemble the non-binary parts of dist/virgin-0.8.6-20261115 (docs, source, checks, manifest, sums)."""
from pathlib import Path
import shutil,json,hashlib,re,os
R=Path(__file__).resolve().parents[1];O=R/'dist/virgin-0.8.5-20261107';N=R/'dist/virgin-0.8.6-20261115'
def conv(t):return t.replace('0.8.5','0.8.6').replace('20261107','20261115').replace('7. November 2026','15. November 2026').replace('2026-11-07','2026-11-15').replace('085','086')
def copy_conv(src,dst):
 dst.parent.mkdir(parents=True,exist_ok=True)
 dst.write_text(conv(src.read_text(encoding='utf-8')),encoding='utf-8',newline='\n')
# docs / license / android notes from 0.8.5 templates
for rel in ['README.md','ENGINE-LICENSE.txt','android/EINBINDEN.md']:copy_conv(O/rel,N/rel)
shutil.copy2(R/'docs/RELEASE_0.8.6.md',N/'RELEASE_0.8.6.md')
copy_conv(R/'docs/APP_PERFORMANCE_0.8.5.md',N/'APP_PERFORMANCE_0.8.6.md');shutil.copy2(N/'APP_PERFORMANCE_0.8.6.md',R/'docs/APP_PERFORMANCE_0.8.6.md')
# linux extras from the template if missing
for rel in ['linux/README.md']:
 if not (N/rel).exists():copy_conv(O/rel,N/rel)
if (O/'linux/licenses').exists():shutil.copytree(O/'linux/licenses',N/'linux/licenses',dirs_exist_ok=True)
# source snapshot
S=N/'source'
if S.exists():shutil.rmtree(S)
S.mkdir()
def cp(p,dst=None,ignore=None):
 d=S/(dst or p);d.parent.mkdir(parents=True,exist_ok=True)
 if (R/p).is_dir():shutil.copytree(R/p,d,ignore=ignore)
 else:shutil.copy2(R/p,d)
for p in ['CMakeLists.txt','build086.cmd','build086extra.cmd','build_web.py','data','src']:cp(p)
cp('android',ignore=shutil.ignore_patterns('build','.gradle','*.apk','*.so','*.class','__pycache__','local.properties','private','*.jks','*.keystore','*.p12','*.pem','signing-password*'))
cp('esp32',ignore=shutil.ignore_patterns('.pio','.vscode','managed_components','build','__pycache__','dependencies.lock'))
cp('web',ignore=shutil.ignore_patterns('build','node_modules','__pycache__','*.wasm','*.js.map')) if (R/'web').exists() else None
(S/'docs').mkdir()
for f in (R/'docs').glob('*.md'):shutil.copy2(f,S/'docs'/f.name)
(S/'tools').mkdir()
for n in ['build_086_targets.py','build_linux_086.py','package_android_086.py','test_086_performance.py','test_086_uci.py','test_web.cjs','verify_linux_086.py','virgin_086_test.py','virgin_adaptive_uci_test.py','virgin_native_library_test.py','assemble_release_086.py','ab_run.sh']:
 if (R/'tools'/n).exists():shutil.copy2(R/'tools'/n,S/'tools'/n)
# android native source (same layout as 0.8.5)
NS=N/'android/native-source'
if NS.exists():shutil.rmtree(NS)
NS.mkdir(parents=True)
for p in ['src','data']:shutil.copytree(R/p,NS/p)
shutil.copy2(R/'CMakeLists.txt',NS/'CMakeLists.txt')
(NS/'android/library/src/main/java/de/dirksommerfeld/virgin').mkdir(parents=True)
shutil.copy2(R/'android/library/consumer-rules.pro',NS/'android/library/consumer-rules.pro')
for f in (R/'android/library/src/main/java/de/dirksommerfeld/virgin').glob('*.java'):shutil.copy2(f,NS/'android/library/src/main/java/de/dirksommerfeld/virgin'/f.name)
if (O/'android/native-source/BUILD.txt').exists():copy_conv(O/'android/native-source/BUILD.txt',NS/'BUILD.txt')
# checks
C=N/'checks';C.mkdir(exist_ok=True)
rep=R/'reports'
for src,dst in [('uci-0.8.6-new.json','uci-0.8.6-new.json'),('performance-0.8.6.json','performance-0.8.6.json'),('linux-elf-086.json','linux-elf-086.json'),('linux-uci-086.json','linux-uci-086.json'),('web-browser-test-0.8.6-20261115.json','web-browser-test-0.8.6-20261115.json'),('linux-x86_64-selftest.log','linux-x86_64-selftest.log')]:
 if (rep/src).exists():shutil.copy2(rep/src,C/dst)
(C/'source-sha256.json').write_text(json.dumps({p.relative_to(R).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((R/'src').glob('*'))},indent=2))
# android per-file sums / check
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
A=N/'android'
(A/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.relative_to(A).as_posix()}\n' for p in sorted(A.rglob('*')) if p.is_file() and p.name!='SHA256SUMS.txt' and 'native-source' not in p.parts and 'java' not in p.parts),encoding='utf-8')
# manifest + top level sums
req={}
for rel in ['windows/virgin-0.8.6.exe','windows/virgin.dll','android/virgin-0.8.6.aar','android/Virgin-0.8.6-OEX.apk','web/virgin.js','web/virgin.wasm']+[f'android/uci/virgin-{a}' for a in ['arm64-v8a','armeabi-v7a','x86_64','x86']]+[f'android/jniLibs/{a}/libvirgin.so' for a in ['arm64-v8a','armeabi-v7a','x86_64','x86']]+[f'esp32-s3/{v}/{f}' for v in ['mini_no_psram','mini_psram_qspi','mini_psram_opi'] for f in ['bootloader.bin','partitions.bin','firmware.bin']]+[f'linux/virgin-0.8.6-linux-{a}' for a in ['x86_64','armv6','armv7']]:
 req[rel]=sha(N/rel)
(N/'manifest.json').write_text(json.dumps({'engine':'Virgin','version':'0.8.6','date':'2026-11-15','status':'beta','runtime_tested':['Windows x64 UCI','Windows x64 DLL (build)','compact host core','WebAssembly headless Chrome','Linux x86_64 WSL'],'hardware_runtime_untested':['Android','Linux ARMv6','Linux ARMv7','ESP32-S3'],'core2_updated':False,'required_artifacts':req},indent=2),encoding='utf-8')
(N/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.relative_to(N).as_posix()}\n' for p in sorted(N.rglob('*')) if p.is_file() and p.name!='SHA256SUMS.txt' and 'source' not in p.relative_to(N).parts[:1]),encoding='utf-8')
print('assembled',N,len(req),'required artifacts')
