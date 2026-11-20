from pathlib import Path
import os,subprocess,sys,shutil,concurrent.futures,json
ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'dist/virgin-0.8.7-20261120'
(ROOT/'reports').mkdir(exist_ok=True)
CM=shutil.which('cmake') or r'C:\Program Files (x86)\CMake\bin\cmake.exe'
NINJA=r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
SDK=Path(os.environ['LOCALAPPDATA'])/'Android/Sdk';NDK=SDK/'ndk/27.0.12077973'
def run(cmd,log):
 with (ROOT/'reports'/log).open('a',encoding='utf-8') as f:
  f.write('COMMAND '+str(cmd)+'\n');f.flush();subprocess.run([str(x) for x in cmd],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,check=True)
def android_abi(abi):
 log='build-android-'+abi+'.log';common=['-G','Ninja','-DCMAKE_MAKE_PROGRAM='+NINJA,'-DCMAKE_TOOLCHAIN_FILE='+str(NDK/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI='+abi,'-DANDROID_PLATFORM=android-21','-DANDROID_STL=c++_static','-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON','-DCMAKE_BUILD_TYPE=Release']
 for lib in [False,True]:
  b=ROOT/'build'/('android-library-' if lib else 'android-oex-')/abi
  if not lib:b=ROOT/'build'/('android-oex-'+abi)
  run([CM,'-S',ROOT,'-B',b,*common,'-DVIRGIN_LIBRARY='+('ON' if lib else 'OFF'),'-DCMAKE_EXE_LINKER_FLAGS=-Wl,-z,max-page-size=16384'],log)
  run([CM,'--build',b,'--parallel','2'],log)
  target=DEST/'android'/('jniLibs/'+abi if lib else 'uci');target.mkdir(parents=True,exist_ok=True)
  shutil.copy2(b/('libvirgin.so' if lib else 'virgin'),target/('libvirgin.so' if lib else 'virgin-'+abi))
 print('ANDROID ABI BUILT',abi,flush=True)
def android():
 with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:list(pool.map(android_abi,['arm64-v8a','armeabi-v7a','x86_64','x86']))
 run([sys.executable,ROOT/'android/oex/build.py','--skip-native','--ndk',NDK],'build-oex-apk.log')
 run([sys.executable,ROOT/'tools/package_android_087.py'],'build-aar.log')
 shutil.copy2(ROOT/'dist/virgin-oex-0.8.7-20261120/Virgin-0.8.7-OEX.apk',DEST/'android/Virgin-0.8.7-OEX.apk')
 shutil.copy2(ROOT/'src/virgin_native.h',DEST/'android/virgin_native.h')
 shutil.copytree(ROOT/'android/library/src/main/java',DEST/'android/java',dirs_exist_ok=True)
 print('ANDROID PACKAGES READY',flush=True)
def esp():
 for name in ['mini_no_psram','mini_psram_qspi','mini_psram_opi']:
  run([str(Path.home()/'.platformio/penv/Scripts/pio.exe'),'run','-d',ROOT/'esp32','-e',name,'-j','2'],'build-087-esp-'+name+'.log')
  target=DEST/'esp32-s3'/name;target.mkdir(parents=True,exist_ok=True)
  for n in ['bootloader.bin','partitions.bin','firmware.bin']:shutil.copy2(ROOT/'esp32/.pio/build'/name/n,target/n)
  print('ESP BUILT',name,flush=True)
def web():
 run([sys.executable,ROOT/'build_web.py','--emsdk',os.environ.get('EMSDK',str(ROOT/'tools/emsdk'))],'build-web.log')
 shutil.copytree(ROOT/'dist/virgin-web-0.8.7-20261120',DEST/'web',dirs_exist_ok=True);print('WEB READY',flush=True)

def core2():
 run([str(Path.home()/'.platformio/penv/Scripts/pio.exe'),'run','-d',ROOT/'esp32_core2','-e','m5stack_core2','-j','3'],'build-087-core2.log')
 target=DEST/'esp32-core2';target.mkdir(parents=True,exist_ok=True)
 for name in ['bootloader.bin','partitions.bin','firmware.bin']:
  shutil.copy2(ROOT/'esp32_core2/.pio/build/m5stack_core2'/name,target/name)
 shutil.copy2(ROOT/'esp32_core2/README.md',target/'README.md')
 print('CORE2 BUILT',flush=True)
def windows():
 run(['cmd.exe','/d','/c',ROOT/'build087.cmd'],'build-win-engine.log')
 run(['cmd.exe','/d','/c',ROOT/'build087extra.cmd'],'build-win-library.log')
 for lib in [False,True]:
  b=ROOT/'build'/('087-library' if lib else '087-win')
  target=DEST/'windows';target.mkdir(parents=True,exist_ok=True)
  shutil.copy2(b/('virgin.dll' if lib else 'virgin.exe'),target/('virgin.dll' if lib else 'virgin-0.8.7.exe'))
 shutil.copy2(ROOT/'src/virgin_native.h',DEST/'windows/virgin_native.h')
 shutil.copy2(ROOT/'build/087-compact/virgin.exe',DEST/'windows/virgin-0.8.7-compact.exe')
 shutil.copytree(ROOT/'data',DEST/'windows/data',dirs_exist_ok=True)
if __name__=='__main__':
 DEST.mkdir(parents=True,exist_ok=True)
 {'windows':windows,'android':android,'esp':esp,'web':web,'core2':core2}[sys.argv[1]]()
