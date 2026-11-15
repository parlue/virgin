"""Package Android UCI executables as a signed Open Exchange APK. Python 3.9+."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import struct
import subprocess
import tempfile
import zipfile
from xml.sax.saxutils import escape, quoteattr

HERE = Path(__file__).resolve().parent
MACHINES = {'arm64-v8a': (2, 183), 'armeabi-v7a': (1, 40), 'x86_64': (2, 62), 'x86': (1, 3)}


def validate_elf(path, abi):
    data = path.read_bytes()
    if len(data) < 64 or data[:4] != b'\x7fELF' or data[5] != 1:
        raise ValueError(f'{path}: expected a little-endian Android ELF executable, not a Windows EXE or APK')
    bits, machine = MACHINES[abi]
    if (data[4], struct.unpack_from('<H', data, 18)[0]) != (bits, machine):
        raise ValueError(f'{path}: ELF architecture does not match {abi}')
    if struct.unpack_from('<H', data, 16)[0] != 3:
        raise ValueError(f'{path}: Android requires a PIE executable (ELF type DYN)')
    if bits == 2:
        offset = struct.unpack_from('<Q', data, 32)[0]
        size, count = struct.unpack_from('<HH', data, 54)
        fmt = '<IIQQQQQQ'
    else:
        offset = struct.unpack_from('<I', data, 28)[0]
        size, count = struct.unpack_from('<HH', data, 42)
        fmt = '<IIIIIIII'
    interpreter = False
    loads = 0
    for i in range(count):
        p = struct.unpack_from(fmt, data, offset + i * size)
        if p[0] == 3:
            start, length = (p[2], p[5]) if bits == 2 else (p[1], p[4])
            expected = b'/system/bin/linker64\0' if bits == 2 else b'/system/bin/linker\0'
            interpreter = data[start:start + length] == expected
        if p[0] == 1:
            loads += 1
            if p[7] < 16384:
                raise ValueError(f'{path}: rebuild with -Wl,-z,max-page-size=16384 for 16 KB pages')
    if not interpreter or not loads:
        raise ValueError(f'{path}: expected Android PIE with /system/bin/linker[64]')
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    parser.add_argument('--sdk', default=os.environ.get('ANDROID_HOME') or os.environ.get('ANDROID_SDK_ROOT') or str(Path(os.environ.get('LOCALAPPDATA', '~')) / 'Android/Sdk'))
    parser.add_argument('--jdk', default=os.environ.get('JAVA_HOME', r'C:\Program Files\Android\Android Studio\jbr'))
    parser.add_argument('--output', type=Path)
    parser.add_argument('--validate-only', action='store_true')
    args = parser.parse_args()
    config_file = args.config.resolve()
    c = json.loads(config_file.read_text(encoding='utf-8-sig'))
    base = config_file.parent
    def path(value):
        return (base / value).resolve()
    package = c['package']
    if not re.fullmatch(r'[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+', package):
        raise ValueError('package must be a lowercase Java package, e.g. org.example.engine.oex')
    for key in ('name', 'author', 'version'):
        if not isinstance(c[key], str) or not c[key].strip():
            raise ValueError(f'{key} must be a nonempty string')
    code = int(c['version_code'])
    minimum, target = int(c.get('min_sdk', 21)), int(c.get('target_sdk', 36))
    if not 0 < code <= 2100000000 or not 21 <= minimum <= target:
        raise ValueError('invalid version_code or SDK range (min_sdk >= 21)')
    engines = c['binaries']
    if not engines or not set(engines).issubset(MACHINES):
        raise ValueError('binaries must map one or more supported Android ABIs to executable paths')
    hashes = {abi: validate_elf(path(value), abi) for abi, value in engines.items()}
    licenses = [path(value) for value in c.get('licenses', [])]
    if not licenses or any(not p.is_file() for p in licenses):
        raise ValueError('licenses must list the engine and required dependency license/notice files')
    print('Validated Android executables:', ', '.join(engines), flush=True)
    if args.validate_only:
        return
    sdk, jdk = Path(args.sdk), Path(args.jdk)
    tools = sdk / 'build-tools' / c.get('build_tools', '36.0.0')
    android = sdk / 'platforms' / f'android-{target}' / 'android.jar'
    for required in (android, tools / 'aapt2.exe', tools / 'zipalign.exe', tools / 'core-lambda-stubs.jar', tools / 'lib/d8.jar', tools / 'lib/apksigner.jar', jdk / 'bin/javac.exe', jdk / 'bin/java.exe', jdk / 'bin/keytool.exe'):
        if not required.is_file():
            raise ValueError(f'Missing Android/JDK tool: {required}')
    output = (args.output or path(c.get('output', 'dist'))).resolve()
    output.mkdir(parents=True, exist_ok=True)
    build = Path(tempfile.mkdtemp(prefix='oex-build-', dir=output))
    env = os.environ.copy()
    env['JAVA_HOME'] = str(jdk)
    def run(command):
        subprocess.run([str(x) for x in command], check=True, env=env)
    def write(relative, value):
        dest = build / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(value, encoding='utf-8')
    names = {abi: f'libengine-{abi}.so' for abi in engines}
    q = quoteattr
    write('AndroidManifest.xml', f'''<manifest xmlns:android="http://schemas.android.com/apk/res/android" package={q(package)} android:versionCode="{code}" android:versionName={q(c['version'])}>
 <uses-sdk android:minSdkVersion="{minimum}" android:targetSdkVersion="{target}"/>
 <application android:label={q(c['name'] + ' OEX')} android:icon="@drawable/engine_icon" android:theme="@android:style/Theme.Material.NoActionBar" android:allowBackup="false" android:extractNativeLibs="true" android:usesCleartextTraffic="false">
  <activity android:name=".MainActivity" android:exported="true">
   <intent-filter><action android:name="android.intent.action.MAIN"/><category android:name="android.intent.category.LAUNCHER"/></intent-filter>
   <intent-filter><action android:name="intent.chess.provider.ENGINE"/><category android:name="android.intent.category.DEFAULT"/></intent-filter>
   <meta-data android:name="chess.provider.engine.authority" android:value="{package}.engines"/>
  </activity>
  <provider android:name=".EngineProvider" android:authorities="{package}.engines" android:exported="true" android:grantUriPermissions="true"/>
 </application>
</manifest>''')
    write('res/xml/enginelist.xml', '<engines>\n' + '\n'.join(f'<engine name={q(c["name"])} filename={q(names[abi])} target={q(abi)}/>' for abi in engines) + '\n</engines>')
    write('res/drawable/engine_icon.xml', (HERE / 'templates/engine_icon.xml').read_text(encoding='utf-8'))
    provider = (HERE / 'templates/EngineProvider.java').read_text(encoding='utf-8')
    write('java/EngineProvider.java', provider.replace('@PACKAGE@', package).replace('@FILES@', ','.join(json.dumps(n) for n in names.values())))
    text = c['name'] + '\n' + c['author'] + '\nVersion ' + c['version'] + '\n\nInstall this package, then select the engine in an Open Exchange compatible chess app. Configure UCI options in that app.'
    write('java/MainActivity.java', f'''package {package};
import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;
public final class MainActivity extends Activity {{
 @Override public void onCreate(Bundle state) {{ super.onCreate(state);
  TextView view = new TextView(this); view.setText({json.dumps(text, ensure_ascii=True)});
  view.setTextSize(21); view.setPadding(28,48,28,28);
  if(android.os.Build.VERSION.SDK_INT>=20) view.setOnApplyWindowInsetsListener((v,i)->{{v.setPadding(28,48+i.getSystemWindowInsetTop(),28,28+i.getSystemWindowInsetBottom());return i;}});
  setContentView(view);
 }}
}}''')
    run([tools/'aapt2.exe', 'compile', '--dir', build/'res', '-o', build/'resources.zip'])
    (build/'generated').mkdir()
    run([tools/'aapt2.exe', 'link', '-I', android, '--manifest', build/'AndroidManifest.xml', '-R', build/'resources.zip', '--java', build/'generated', '-o', build/'base.apk'])
    (build/'classes').mkdir()
    run([jdk/'bin/javac.exe', '-encoding', 'UTF-8', '-source', '8', '-target', '8', '-bootclasspath', str(android)+os.pathsep+str(tools/'core-lambda-stubs.jar'), '-d', build/'classes'] + list((build/'java').glob('*.java')) + list((build/'generated').rglob('*.java')))
    (build/'dex').mkdir()
    run([jdk/'bin/java.exe', '-cp', tools/'lib/d8.jar', 'com.android.tools.r8.D8', '--lib', android, '--min-api', minimum, '--output', build/'dex'] + list((build/'classes').rglob('*.class')))
    shutil.copyfile(build/'base.apk', build/'unsigned.apk')
    with zipfile.ZipFile(build/'unsigned.apk', 'a') as apk:
        apk.write(build/'dex/classes.dex', 'classes.dex', compress_type=zipfile.ZIP_DEFLATED)
        for abi, value in engines.items():
            for entry in ('assets/'+names[abi], 'lib/'+abi+'/'+names[abi]):
                apk.write(path(value), entry, compress_type=zipfile.ZIP_STORED)
        for i, license_file in enumerate(licenses):
            apk.write(license_file, f'assets/licenses/{i}-{license_file.name}')
    run([tools/'zipalign.exe', '-P', '16', '-f', '4', build/'unsigned.apk', build/'aligned.apk'])
    private = path(c.get('signing_directory', 'private/'+package))
    private.mkdir(parents=True, exist_ok=True)
    password_file, key = private/'password.txt', private/'release.jks'
    if key.exists() and not password_file.exists():
        raise ValueError('Signing key exists but password.txt is missing; restore the original password')
    if not password_file.exists():
        password_file.write_text(secrets.token_urlsafe(32), encoding='utf-8')
    env['OEX_SIGN_PASSWORD'] = password_file.read_text(encoding='utf-8').strip()
    if not key.exists():
        run([jdk/'bin/keytool.exe', '-genkeypair', '-keystore', key, '-storetype', 'JKS', '-alias', 'oex', '-keyalg', 'RSA', '-keysize', '3072', '-validity', '10950', '-storepass:env', 'OEX_SIGN_PASSWORD', '-keypass:env', 'OEX_SIGN_PASSWORD', '-dname', 'CN=OEX Engine Publisher'])
    apk_path = output / (package+'-'+re.sub(r'[^a-zA-Z0-9._-]', '_', c['version'])+'.apk')
    signer = [jdk/'bin/java.exe', '-jar', tools/'lib/apksigner.jar']
    run(signer+['sign', '--ks', key, '--ks-key-alias', 'oex', '--ks-pass', 'env:OEX_SIGN_PASSWORD', '--key-pass', 'env:OEX_SIGN_PASSWORD', '--out', apk_path, build/'aligned.apk'])
    run(signer+['verify', '--verbose', apk_path])
    run([tools/'zipalign.exe', '-c', '-P', '16', '4', apk_path])
    report = {'package': package, 'name': c['name'], 'version': c['version'], 'apk': apk_path.name, 'sha256': hashlib.sha256(apk_path.read_bytes()).hexdigest(), 'engine_sha256': hashes, 'build_directory': str(build)}
    apk_path.with_suffix('.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('APK READY:', apk_path, flush=True)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError, struct.error, subprocess.CalledProcessError) as error:
        raise SystemExit(f'Build failed: {error}')
