package de.dirksommerfeld.virgin.oex;

import android.content.*;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import java.io.*;
import java.util.Arrays;

/** Read-only Open Exchange binary provider. No chess state or UCI service. */
public final class EngineProvider extends ContentProvider {
 private static final String[] FILES={"libvirgin-arm64-v8a.so","libvirgin-armeabi-v7a.so","libvirgin-x86_64.so","libvirgin-x86.so"};
 @Override public boolean onCreate(){return true;}
 private String file(Uri uri,String mode)throws FileNotFoundException {
  String name=uri.getLastPathSegment();
  if(!"r".equals(mode)||uri.getPathSegments().size()!=1||!Arrays.asList(FILES).contains(name))throw new FileNotFoundException("Unknown engine or non-read access");
  return name;
 }
 @Override public AssetFileDescriptor openAssetFile(Uri uri,String mode)throws FileNotFoundException {
  String name=file(uri,mode);
  try{return getContext().getAssets().openFd(name);}catch(IOException ex){throw new FileNotFoundException(ex.toString());}
 }
 @Override public ParcelFileDescriptor openFile(Uri uri,String mode)throws FileNotFoundException {
  final String name=file(uri,mode);
  // openFile must expose only the engine, not the APK behind an asset offset.
  try {
   final ParcelFileDescriptor[] pipe=ParcelFileDescriptor.createPipe();
   new Thread(()->{
    try(InputStream in=getContext().getAssets().open(name);OutputStream out=new ParcelFileDescriptor.AutoCloseOutputStream(pipe[1])) {
     byte[] bytes=new byte[16384];int n;while((n=in.read(bytes))!=-1)out.write(bytes,0,n);
    }catch(IOException ignored){}
   },"Virgin-OEX-read").start();
   return pipe[0];
  }catch(IOException ex){throw new FileNotFoundException(ex.toString());}
 }
 @Override public String getType(Uri uri){return "application/x-chess-engine";}
 @Override public Cursor query(Uri u,String[] p,String s,String[] a,String o){throw new UnsupportedOperationException("Read-only engine files");}
 @Override public Uri insert(Uri u,ContentValues v){throw new UnsupportedOperationException("Read-only");}
 @Override public int update(Uri u,ContentValues v,String s,String[] a){throw new UnsupportedOperationException("Read-only");}
 @Override public int delete(Uri u,String s,String[] a){throw new UnsupportedOperationException("Read-only");}
}
