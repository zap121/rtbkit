#include <jni.h>
#include <stdio.h>

#include "rtbkit_api.h"
#include "rtbkit_Object.h"

JNIEXPORT void JNICALL Java_rtbkit_Object_release
  (JNIEnv * env, jobject obj, jlong handle)
{
    struct rtbkit_object * item = *(struct rtbkit_object **) &handle;
    rtbkit_release(item);
}

