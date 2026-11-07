"""Package the current Java wrapper and the four freshly built JNI libraries."""
from pathlib import Path
import os,subprocess,zipfile
ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'dist/virgin-0.8.5-20261107/android'
JDK=Path(os.environ.get('JAVA_HOME',r'C:\Program Files\Android\Android Studio\jbr'))
classes=ROOT/'build/release-0.8.5-aar-classes';classes.mkdir(parents=True,exist_ok=True)
sources=list((ROOT/'android/library/src/main/java').rglob('*.java'))
subprocess.run([str(JDK/'bin/javac.exe'),'-encoding','UTF-8','-source','8','-target','8','-d',str(classes)]+[str(f) for f in sources],check=True)
jar=classes.parent/'release-0.8.5-classes.jar'
with zipfile.ZipFile(jar,'w',zipfile.ZIP_DEFLATED) as z:
 for f in classes.rglob('*.class'):z.write(f,f.relative_to(classes).as_posix())
with zipfile.ZipFile(DEST/'virgin-0.8.5.aar','w',zipfile.ZIP_DEFLATED) as z:
 z.writestr('AndroidManifest.xml','<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="de.dirksommerfeld.virgin"><uses-sdk android:minSdkVersion="21" /></manifest>\n')
 z.write(jar,'classes.jar');z.write(ROOT/'android/library/consumer-rules.pro','proguard.txt');z.writestr('R.txt','')
 for abi in ['arm64-v8a','armeabi-v7a','x86_64','x86']:
  z.write(DEST/'jniLibs'/abi/'libvirgin.so','jni/'+abi+'/libvirgin.so')
print('Fresh AAR:',DEST/'virgin-0.8.5.aar')
