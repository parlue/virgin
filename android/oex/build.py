"""Build a signed universal OEX APK using installed SDK/NDK tools, without Gradle."""
import argparse,hashlib,json,os,secrets,shutil,subprocess,zipfile
from pathlib import Path

project=Path(__file__).resolve().parent
root=project.parent.parent
parser=argparse.ArgumentParser()
parser.add_argument('--sdk',default=os.environ.get('ANDROID_HOME',str(Path(os.environ['LOCALAPPDATA'])/'Android/Sdk')))
parser.add_argument('--ndk')
parser.add_argument('--jdk',default=os.environ.get('JAVA_HOME',r'C:\Program Files\Android\Android Studio\jbr'))
parser.add_argument('--skip-native',action='store_true')
parser.add_argument('--signing-dir',type=Path,default=Path(os.environ.get('VIRGIN_SIGNING_DIR',str(project/'private'))))
args=parser.parse_args()
sdk=Path(args.sdk);jdk=Path(args.jdk)
ndk=Path(args.ndk) if args.ndk else sorted((sdk/'ndk').iterdir())[-1]
tools=sdk/'build-tools/36.0.0';android=sdk/'platforms/android-36/android.jar'
cmake=shutil.which('cmake') or r'C:\Program Files (x86)\CMake\bin\cmake.exe'
ninja=shutil.which('ninja') or r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
abis=['arm64-v8a','armeabi-v7a','x86_64','x86']
build=project/'build';build.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['JAVA_HOME']=str(jdk)
def run(command):
    command=[str(c) for c in command]
    print('RUN',command[0],command[1] if len(command)>1 else '',flush=True)
    subprocess.run(command,check=True,env=env)
for abi in abis:
    native=root/('build/android-oex-'+abi)
    if not args.skip_native:
        print('NATIVE',abi,flush=True)
        run([cmake,'-S',root,'-B',native,'-G','Ninja',f'-DCMAKE_MAKE_PROGRAM={ninja}',
             f'-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake',
             f'-DANDROID_ABI={abi}','-DANDROID_PLATFORM=android-21','-DANDROID_STL=c++_static',
             '-DCMAKE_BUILD_TYPE=Release','-DVIRGIN_LIBRARY=OFF','-DVIRGIN_EMBEDDED=OFF',
             '-DCMAKE_EXE_LINKER_FLAGS=-Wl,-z,max-page-size=16384'])
        run([cmake,'--build',native,'--parallel','2'])
    assert (native/'virgin').is_file(),native

compiled=build/'resources.zip';base=build/'resources.apk'
run([tools/'aapt2.exe','compile','--dir',project/'src/main/res','-o',compiled])
gen=build/'generated';gen.mkdir(exist_ok=True)
run([tools/'aapt2.exe','link','-I',android,'--manifest',project/'src/main/AndroidManifest.xml',
     '-R',compiled,'--java',gen,'-o',base,'--min-sdk-version','21','--target-sdk-version','36'])
classes=build/'classes';classes.mkdir(exist_ok=True)
sources=list((project/'src/main/java').rglob('*.java'))+list(gen.rglob('*.java'))
run([jdk/'bin/javac.exe','-encoding','UTF-8','-source','8','-target','8','-bootclasspath',str(android)+os.pathsep+str(tools/'core-lambda-stubs.jar'),'-d',classes]+sources)
dex=build/'dex';dex.mkdir(exist_ok=True)
run([jdk/'bin/java.exe','-cp',tools/'lib/d8.jar','com.android.tools.r8.D8','--lib',android,'--min-api','21','--output',dex]+list(classes.rglob('*.class')))
unsigned=build/'unsigned.apk';shutil.copyfile(base,unsigned)
with zipfile.ZipFile(unsigned,'a') as apk:
    apk.write(dex/'classes.dex','classes.dex',compress_type=zipfile.ZIP_DEFLATED)
    for abi in abis:
        engine=root/('build/android-oex-'+abi)/'virgin'
        filename='libvirgin-'+abi+'.so'
        apk.write(engine,'assets/'+filename,compress_type=zipfile.ZIP_STORED)
        apk.write(engine,'lib/'+abi+'/'+filename,compress_type=zipfile.ZIP_STORED)
    apk.writestr('assets/ENGINE-LICENSE.txt',
        'MIT License\n\nCopyright (c) 2026 Dirk D. Sommerfeld\n\n'
        'Permission is hereby granted, free of charge, to any person obtaining a copy '
        'of this software and associated documentation files (the "Software"), to deal '
        'in the Software without restriction, including without limitation the rights '
        'to use, copy, modify, merge, publish, distribute, sublicense, and/or sell '
        'copies of the Software, and to permit persons to whom the Software is '
        'furnished to do so, subject to the following conditions:\n\n'
        'The above copyright notice and this permission notice shall be included '
        'in all copies or substantial portions of the Software.\n\n'
        'THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS '
        'OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, '
        'FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL '
        'THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER '
        'LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING '
        'FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS '
        'IN THE SOFTWARE.\n')
    for name in ['NOTICE','NOTICE.toolchain']:
        if (ndk/name).exists():apk.write(ndk/name,'assets/NDK-'+name,compress_type=zipfile.ZIP_DEFLATED)
aligned=build/'aligned.apk'
run([tools/'zipalign.exe','-P','16','-f','4',unsigned,aligned])
private=args.signing_dir;private.mkdir(exist_ok=True)
passwordFile=private/'signing-password.txt'
if not passwordFile.exists():passwordFile.write_text(secrets.token_urlsafe(32),encoding='utf-8')
env['VIRGIN_OEX_KEY_PASSWORD']=passwordFile.read_text(encoding='utf-8').strip()
key=private/'virgin-oex-release.jks'
if not key.exists():
    run([jdk/'bin/keytool.exe','-genkeypair','-keystore',key,'-storetype','JKS',
         '-alias','virgin-oex','-keyalg','RSA','-keysize','3072','-validity','10950',
         '-storepass:env','VIRGIN_OEX_KEY_PASSWORD','-keypass:env','VIRGIN_OEX_KEY_PASSWORD',
         '-dname','CN=Dirk D. Sommerfeld, O=Virgin Chess, C=DE'])
dist=root/'dist/virgin-oex-0.8.5-20261107';dist.mkdir(parents=True,exist_ok=True)
apkPath=dist/'Virgin-0.8.5-OEX.apk'
signer=[jdk/'bin/java.exe','-jar',tools/'lib/apksigner.jar']
run(signer+['sign','--ks',key,'--ks-key-alias','virgin-oex','--ks-pass','env:VIRGIN_OEX_KEY_PASSWORD',
            '--key-pass','env:VIRGIN_OEX_KEY_PASSWORD','--out',apkPath,aligned])
run(signer+['verify','--verbose','--print-certs',apkPath])
run([tools/'zipalign.exe','-c','-P','16','4',apkPath])
shutil.copyfile(project/'README.md',dist/'README.md')
manifest={'engine':'Virgin 0.8.5','author':'Dirk D. Sommerfeld','package':'de.dirksommerfeld.virgin.oex',
          'abis':abis,'apk':apkPath.name,'sha256':hashlib.sha256(apkPath.read_bytes()).hexdigest(),
          'minSdk':21,'targetSdk':36,'external_engine_files_required':False,'elf_page_alignment':16384,
          'signature':'dedicated local release key; v1/v2/v3 verified','engines':{}}
readelf=ndk/'toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-readelf.exe'
for abi in abis:
    exe=root/('build/android-oex-'+abi)/'virgin'
    text=subprocess.check_output([str(readelf),'-h','-l','-d',str(exe)],text=True)
    (dist/('elf-'+abi+'.txt')).write_text(text,encoding='utf-8')
    assert '0x4000' in text,text
    manifest['engines'][abi]={'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'bytes':exe.stat().st_size}
(dist/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print('APK READY:',apkPath,flush=True)
