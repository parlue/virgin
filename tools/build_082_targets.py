"""Rebuild the 0.8.2 Android, Web or ESP outputs; never uploads firmware."""
import argparse,os,shutil,subprocess,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CM=shutil.which('cmake') or r'C:\Program Files (x86)\CMake\bin\cmake.exe'
NINJA=shutil.which('ninja') or r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
SDK=Path(os.environ.get('LOCALAPPDATA',''))/'Android/Sdk'
NDK=SDK/'ndk/27.0.12077973'
DEST=ROOT/'dist/virgin-0.8.2-20261104'
def run(cmd):
 print('RUN', ' '.join(str(x) for x in cmd),flush=True)
 subprocess.run([str(x) for x in cmd],cwd=ROOT,check=True)
def android():
 run([sys.executable,ROOT/'android/oex/build.py'])
 abis=['arm64-v8a','armeabi-v7a','x86_64','x86']
 for abi in abis:
  directory=ROOT/'build'/('release-0.8.2-library-'+abi)
  run([CM,'-S',ROOT,'-B',directory,'-G','Ninja','-DCMAKE_MAKE_PROGRAM='+NINJA,
       '-DCMAKE_TOOLCHAIN_FILE='+str(NDK/'build/cmake/android.toolchain.cmake'),
       '-DANDROID_ABI='+abi,'-DANDROID_PLATFORM=android-21','-DANDROID_STL=c++_static',
       '-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON','-DVIRGIN_LIBRARY=ON','-DCMAKE_BUILD_TYPE=Release'])
  run([CM,'--build',directory,'--parallel','2'])
  target=DEST/'android/jniLibs'/abi;target.mkdir(parents=True,exist_ok=True)
  shutil.copy2(directory/'libvirgin.so',target/'libvirgin.so')
  target=DEST/'android/uci';target.mkdir(parents=True,exist_ok=True)
  shutil.copy2(ROOT/'build'/('android-oex-'+abi)/'virgin',target/('virgin-'+abi))
 run([sys.executable,ROOT/'tools/package_android_082.py'])
 shutil.copy2(ROOT/'dist/virgin-oex-0.8.2-20261104/Virgin-0.8.2-OEX.apk',DEST/'android/Virgin-0.8.2-OEX.apk')
 shutil.copy2(ROOT/'src/virgin_native.h',DEST/'android/virgin_native.h')
 shutil.copytree(ROOT/'android/library/src/main/java',DEST/'android/java',dirs_exist_ok=True)
def esp():
 for env in ['mini_no_psram','mini_psram_qspi','mini_psram_opi']:
  # PlatformIO 6.8 caches this list even when CMake's component set changes.
  stale=ROOT/'esp32/.pio/build'/env/'ldgen_libraries_pio'
  if stale.is_file():stale.unlink()
  run([sys.executable,'-m','platformio','run','-d','esp32','-e',env,'-j','6'])
  target=DEST/'esp32-s3'/env;target.mkdir(parents=True,exist_ok=True)
  for name in ['bootloader.bin','partitions.bin','firmware.bin']:
   shutil.copy2(ROOT/'esp32/.pio/build'/env/name,target/name)
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('target',choices=['android','web','esp']);args=parser.parse_args()
 DEST.mkdir(parents=True,exist_ok=True)
 if args.target=='android':android()
 elif args.target=='web':run([sys.executable,ROOT/'build_web.py'])
 else:esp()
