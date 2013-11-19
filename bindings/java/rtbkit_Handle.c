#include <jni.h>
#include <stdio.h>

#include "rtbkit_api.h"
#include "rtbkit_Handle.h"

JNIEXPORT jlong JNICALL Java_rtbkit_Handle_initialize
  (JNIEnv * env, jobject obj, jstring bootstrap)
{
    char const * string = (*env)->GetStringUTFChars(env, bootstrap, 0);
    struct rtbkit_handle * item = rtbkit_initialize(string);
    (*env)->ReleaseStringUTFChars(env, bootstrap, string);
    return *(jlong *) &item;
}

JNIEXPORT void JNICALL Java_rtbkit_Handle_shutdown
  (JNIEnv * env, jobject obj, jlong handle)
{
    struct rtbkit_handle * item = *(struct rtbkit_handle **) &handle;
    rtbkit_shutdown(item);
}

