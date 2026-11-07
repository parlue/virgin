"""Build Virgin's browser/Web Worker release with a locally installed Emscripten SDK."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
ROOT = Path(__file__).resolve().parent

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emsdk', type=Path, default=Path(os.environ.get('EMSDK', str(ROOT/'tools/emsdk'))))
    parser.add_argument('--output', type=Path, default=ROOT/'dist/virgin-web-0.8.5-20261107')
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    sdk = args.emsdk.resolve()
    emscripten = sdk/'upstream/emscripten'
    if not (emscripten/'emcmake.py').is_file():
        raise SystemExit('Install and activate Emscripten first; see web/README.md')
    cmake = shutil.which('cmake') or r'C:\Program Files (x86)\CMake\bin\cmake.exe'
    ninja = shutil.which('ninja') or r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
    env = os.environ.copy()
    env["EMSDK_PYTHON"] = sys.executable
    env["PATH"] = str(Path(sys.executable).parent) + os.pathsep + env.get("PATH", "")
    if (sdk/'.emscripten').is_file(): env['EM_CONFIG'] = str(sdk/'.emscripten')
    build = ROOT/'build/web'
    def run(command):
        subprocess.run([str(x) for x in command], check=True, env=env)
    run([sys.executable, emscripten/'emcmake.py', cmake, '-S', ROOT/'web', '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_MAKE_PROGRAM='+ninja])
    run([cmake, '--build', build, '--parallel', args.jobs])
    dist = args.output.resolve()
    dist.mkdir(parents=True, exist_ok=True)
    names = ['virgin.js', 'virgin.wasm']
    # Some older Emscripten releases create an extra worker runtime file.
    names += [f.name for f in build.glob('virgin*.worker.js')]
    for name in names: shutil.copyfile(build/name, dist/name)
    for name in ('engine-worker.js', 'index.html', 'serve.py', 'README.md'):
        shutil.copyfile(ROOT/'web'/name, dist/name)
    shutil.copyfile(ROOT/'web/ENGINE-LICENSE.txt', dist/'ENGINE-LICENSE.txt')
    licenses = dist/'licenses'; licenses.mkdir(exist_ok=True)
    notice = emscripten/'LICENSE'
    if notice.is_file(): shutil.copyfile(notice, licenses/'EMSCRIPTEN-LICENSE.txt')
    runtime = emscripten/'system/lib/libcxx/LICENSE.TXT'
    if runtime.is_file(): shutil.copyfile(runtime, licenses/'LIBCXX-LICENSE.txt')
    source_zip = dist/'Virgin-0.8.5-Web-source.zip'
    with zipfile.ZipFile(source_zip, 'w', zipfile.ZIP_DEFLATED) as archive:
        for item in (ROOT/'src').glob('*'):
            if item.is_file() and (item.suffix in ('.cpp','.h','.inc') or item.name == 'LICENSE'):
                archive.write(item, 'src/'+item.name)
        for item in (ROOT/'web').iterdir():
            if item.is_file(): archive.write(item, 'web/'+item.name)
        archive.write(ROOT/'build_web.py', 'build_web.py')
        archive.write(ROOT/'tools/test_web.cjs', 'tools/test_web.cjs')
    version_file = emscripten/'emscripten-version.txt'
    manifest = {'engine':'Virgin 0.8.5', 'author':'Dirk D. Sommerfeld',
      'emscripten':version_file.read_text().strip().strip(chr(34)) if version_file.exists() else 'see SDK',
      'uci_threads_max':4, 'hash_default_mib':32, 'hash_max_mib':256,
      'wasm_memory_initial_mib':128, 'wasm_memory_max_mib':512,
      'requires_cross_origin_isolation':True,
      'external_network_files_required':False, 'files':{}}
    for item in sorted(dist.rglob('*')):
        if item.is_file() and item.name not in ('manifest.json','SHA256SUMS.txt'):
            manifest['files'][item.relative_to(dist).as_posix()] = {'bytes':item.stat().st_size,'sha256':hashlib.sha256(item.read_bytes()).hexdigest()}
    (dist/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    (dist/'SHA256SUMS.txt').write_text(''.join(info['sha256']+'  '+name+'\n' for name,info in manifest['files'].items()),encoding='utf-8')
    print('WEB RELEASE READY:',dist,flush=True)
if __name__ == '__main__':
    main()
