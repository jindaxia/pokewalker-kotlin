#include <jni.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pw_android.h"

/*
 * JNI glue between com.picowalker.android.WalkerCore and the driver layer.
 */

static JavaVM *g_vm = NULL;
static jclass g_core_class = NULL;
static jmethodID g_on_frame = NULL;
static jintArray g_pixels = NULL;

static __thread bool t_attached = false;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)reserved;
    g_vm = vm;

    JNIEnv *env = NULL;
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_VERSION_1_6;
    }

    jclass cls = (*env)->FindClass(env, "com/picowalker/android/WalkerCore");
    if (cls == NULL) {
        return JNI_VERSION_1_6; /* class not found, natives will no-op */
    }
    g_core_class = (jclass)(*env)->NewGlobalRef(env, cls);
    (*env)->DeleteLocalRef(env, cls);

    g_on_frame = (*env)->GetStaticMethodID(env, g_core_class, "onNativeFrame", "([I)V");

    return JNI_VERSION_1_6;
}

void pw_and_jni_attach(void) {
    if (g_vm == NULL) return;

    void *env = NULL;
    if ((*g_vm)->GetEnv(g_vm, &env, JNI_VERSION_1_6) == JNI_OK) return;

    if ((*g_vm)->AttachCurrentThread(g_vm, &env, NULL) == JNI_OK) {
        t_attached = true;
    }
}

void pw_and_jni_detach(void) {
    if (t_attached && g_vm != NULL) {
        (*g_vm)->DetachCurrentThread(g_vm);
        t_attached = false;
    }
}

void pw_and_notify_frame(const uint32_t *pixels, size_t n) {
    if (g_vm == NULL || g_pixels == NULL || g_core_class == NULL || g_on_frame == NULL) return;

    JNIEnv *env = NULL;
    bool attached_here = false;
    if ((*g_vm)->GetEnv(g_vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if ((*g_vm)->AttachCurrentThread(g_vm, (void **)&env, NULL) != JNI_OK) return;
        attached_here = true;
    }

    (*env)->SetIntArrayRegion(env, g_pixels, 0, (jsize)n, (const jint *)pixels);
    (*env)->CallStaticVoidMethod(env, g_core_class, g_on_frame, g_pixels);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }

    if (attached_here) {
        (*g_vm)->DetachCurrentThread(g_vm);
    }
}

/* ======================================================================
 * Native methods of com.picowalker.android.WalkerCore
 * ====================================================================== */

JNIEXPORT void JNICALL
Java_com_picowalker_android_WalkerCore_nativeStart(JNIEnv *env, jobject thiz, jstring path, jintArray pixels) {
    (void)thiz;

    /* make sure no loop thread is still using the old frame array */
    pw_and_runner_stop();

    if (path != NULL) {
        const char *p = (*env)->GetStringUTFChars(env, path, NULL);
        if (p != NULL) {
            pw_and_eeprom_set_path(p);
            (*env)->ReleaseStringUTFChars(env, path, p);
        }
    }

    if (pixels != NULL) {
        if (g_pixels != NULL) {
            (*env)->DeleteGlobalRef(env, g_pixels);
        }
        g_pixels = (jintArray)(*env)->NewGlobalRef(env, pixels);
    }

    pw_and_runner_start();
}

JNIEXPORT void JNICALL Java_com_picowalker_android_WalkerCore_nativeStop(JNIEnv *env, jobject thiz) {
    (void)thiz;

    pw_and_runner_stop();

    /* the loop thread is gone, safe to drop the frame array */
    if (g_pixels != NULL) {
        (*env)->DeleteGlobalRef(env, g_pixels);
        g_pixels = NULL;
    }
}

JNIEXPORT void JNICALL
Java_com_picowalker_android_WalkerCore_nativeButton(JNIEnv *env, jobject thiz, jint mask, jboolean pressed) {
    (void)env;
    (void)thiz;
    pw_and_button_event((pw_buttons_t)mask, pressed != JNI_FALSE);
}

JNIEXPORT void JNICALL
Java_com_picowalker_android_WalkerCore_nativeAddSteps(JNIEnv *env, jobject thiz, jint steps) {
    (void)env;
    (void)thiz;
    if (steps > 0) {
        pw_and_accel_add_steps((uint32_t)steps);
    }
}

JNIEXPORT void JNICALL Java_com_picowalker_android_WalkerCore_nativeSetBattery(
    JNIEnv *env, jobject thiz, jint percent, jboolean charging, jboolean plugged) {
    (void)env;
    (void)thiz;

    uint8_t p = (uint8_t)(percent < 0 ? 0 : (percent > 100 ? 100 : percent));
    pw_and_power_set_battery(p, charging != JNI_FALSE, plugged != JNI_FALSE);
}

JNIEXPORT void JNICALL Java_com_picowalker_android_WalkerCore_nativeSetIrConfig(
    JNIEnv *env, jobject thiz, jstring host, jint port, jboolean enabled) {
    (void)thiz;

    const char *h = (*env)->GetStringUTFChars(env, host, NULL);
    pw_and_ir_set_config(h != NULL ? h : "", (int)port, enabled != JNI_FALSE);
    if (h != NULL) {
        (*env)->ReleaseStringUTFChars(env, host, h);
    }
}

JNIEXPORT jint JNICALL Java_com_picowalker_android_WalkerCore_nativeIrStatus(JNIEnv *env, jobject thiz) {
    (void)env;
    (void)thiz;
    return (jint)pw_and_ir_status();
}
