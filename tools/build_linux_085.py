"""Build static Linux UCI engines with a pinned Zig C++/musl cross toolchain."""
import argparse,hashlib,json,re,subprocess,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--target',choices=['x86_64','armv6','armv7','all'],default='all');a=p.parse_args()
 sources=re.search(r'set\(VIRGIN_SOURCES\s+(.*?)\n\)',(ROOT/'CMakeLists.txt').read_text(),re.S).group(1).split()
 targets={'x86_64':('x86_64-linux-musl','baseline'),'armv6':('arm-linux-musleabihf','arm1176jzf_s'),'armv7':('arm-linux-musleabihf','generic+v7a+vfp3d16')}
 out=ROOT/'dist/virgin-0.8.5-20261107/linux';out.mkdir(parents=True,exist_ok=True);reports=ROOT/'reports';reports.mkdir(exist_ok=True)
 for name,(triple,cpu) in targets.items():
  if a.target not in ('all',name):continue
  exe=out/f'virgin-0.8.5-linux-{name}'
  cmd=[str(a.zig),'c++','-target',triple,'-mcpu='+cpu,'-std=c++20','-O3','-DNDEBUG','-Wno-error=date-time','-pthread','-static','-s','-fno-rtti',*sources,'-o',str(exe)]
  print('BUILD LINUX',name,flush=True)
  with (reports/f'build-linux-{name}.log').open('w') as log:subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
  print('LINUX READY',name,exe.stat().st_size,flush=True)
 shutil.copytree(ROOT/'data',out/'data',dirs_exist_ok=True)
 for f in (out/'data').glob('*'):
  if f.is_file():f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
 (out/'targets.json').write_text(json.dumps({'compiler':'Zig 0.15.2','libc':'statically linked musl','targets':targets,'source_version':'0.8.5'},indent=2))
if __name__=='__main__':main()
