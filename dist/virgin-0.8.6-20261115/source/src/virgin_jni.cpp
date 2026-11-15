#include "virgin_native.h"
#include <jni.h>
#include <cstdint>
#include <vector>
namespace {
void* ptr(jlong h) {return reinterpret_cast<void*>(static_cast<intptr_t>(h));}
void fail(JNIEnv* env,const char* type,const char* text) {
    jclass c=env->FindClass(type);if(c) env->ThrowNew(c,text);
}
}
extern "C" JNIEXPORT jlong JNICALL Java_de_dirksommerfeld_virgin_VirginEngine_nativeCreate(JNIEnv*,jclass) {
    return static_cast<jlong>(reinterpret_cast<intptr_t>(virgin_create()));
}
extern "C" JNIEXPORT jint JNICALL Java_de_dirksommerfeld_virgin_VirginEngine_nativeSend(JNIEnv* env,jclass,jlong h,jstring line) {
    if(!line) return -1;
    const char* text=env->GetStringUTFChars(line,nullptr);if(!text) return -1;
    int r=virgin_send(ptr(h),text);env->ReleaseStringUTFChars(line,text);return r;
}
extern "C" JNIEXPORT jstring JNICALL Java_de_dirksommerfeld_virgin_VirginEngine_nativeRead(JNIEnv* env,jclass,jlong h,jint ms) {
    try {
        std::vector<char> buffer(4096);
        int r=virgin_read(ptr(h),buffer.data(),static_cast<int>(buffer.size()),ms);
        while(r==-3 && buffer.size()<1024*1024) {
            buffer.resize(buffer.size()*2);
            r=virgin_read(ptr(h),buffer.data(),static_cast<int>(buffer.size()),0);
        }
        if(r==1) return env->NewStringUTF(buffer.data());
        if(r==-2 || r==0) return nullptr;
        fail(env,"java/lang/IllegalStateException","Virgin native read failed");return nullptr;
    } catch (...) {fail(env,"java/lang/OutOfMemoryError","Virgin native read allocation failed");return nullptr;}
}
extern "C" JNIEXPORT void JNICALL Java_de_dirksommerfeld_virgin_VirginEngine_nativeClose(JNIEnv*,jclass,jlong h) {
    virgin_destroy(ptr(h));
}
