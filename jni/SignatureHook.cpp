#include "SignatureHook.h"

HookApplicationWrapper::~HookApplicationWrapper()
{
    JNIEnv* env = getEnv();
    if (env) {
        if (baseObject) env->DeleteGlobalRef(baseObject);
        if (signArray) env->DeleteGlobalRef(signArray);
        if (appPkgName) env->DeleteGlobalRef(appPkgName);
    }
}

JNIEnv* HookApplicationWrapper::getEnv() {
    JNIEnv* env = nullptr;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return nullptr;
        }
    }
    return env;
}

jobjectArray HookApplicationWrapper::decodeSignatures(JNIEnv* env) {
    jclass base64Class = env->FindClass("android/util/Base64");
    if (!base64Class) return nullptr;

    jmethodID decodeMethod = env->GetStaticMethodID(base64Class, "decode", "(Ljava/lang/String;I)[B");
    if (!decodeMethod) return nullptr;

    jstring base64JStr = env->NewStringUTF(hardcodedBase64.c_str());
    const int FLAG = 2; // Base64.DEFAULT

    jbyteArray decodedBytes = (jbyteArray)env->CallStaticObjectMethod(base64Class, decodeMethod, base64JStr, FLAG);
    env->DeleteLocalRef(base64JStr);
    if (!decodedBytes) return nullptr;

    jsize len = env->GetArrayLength(decodedBytes);
    std::vector<uint8_t> raw(len);
    env->GetByteArrayRegion(decodedBytes, 0, len, reinterpret_cast<jbyte*>(raw.data()));
    env->DeleteLocalRef(decodedBytes);

    if (len < 1) return nullptr;

    uint8_t count = raw[0];
    size_t pos = 1;

    jclass byteArrayClass = env->FindClass("[B");
    if (!byteArrayClass) return nullptr;

    jobjectArray resultArray = env->NewObjectArray(count, byteArrayClass, nullptr);

    for (uint8_t i = 0; i < count; ++i) {
        if (pos + 4 > raw.size()) break;
        int sigLen = (raw[pos] << 24) | (raw[pos + 1] << 16) | (raw[pos + 2] << 8) | raw[pos + 3];
        pos += 4;
        if (pos + sigLen > raw.size()) break;

        jbyteArray sigBytes = env->NewByteArray(sigLen);
        env->SetByteArrayRegion(sigBytes, 0, sigLen, reinterpret_cast<const jbyte*>(raw.data() + pos));
        pos += sigLen;

        env->SetObjectArrayElement(resultArray, i, sigBytes);
        env->DeleteLocalRef(sigBytes);
    }
    env->DeleteLocalRef(byteArrayClass);
    return resultArray;
}

bool HookApplicationWrapper::init(jobject context) {
    JNIEnv* env = getEnv();
    if (!env) {
        return false;
    }

    jclass contextCls = env->GetObjectClass(context);
    jmethodID getPackageNameMethod = env->GetMethodID(contextCls, "getPackageName", "()Ljava/lang/String;");
    if (!getPackageNameMethod) {
        env->DeleteLocalRef(contextCls);
        return false;
    }
    jstring packageName = (jstring)env->CallObjectMethod(context, getPackageNameMethod);
    env->DeleteLocalRef(contextCls);
    if (!packageName) {
        return false;
    }

    appPkgName = (jstring)env->NewGlobalRef(packageName);
    env->DeleteLocalRef(packageName);

    jobjectArray arr = decodeSignatures(env);
    if (!arr) {
        return false;
    }
    signArray = (jobjectArray)env->NewGlobalRef(arr);
    env->DeleteLocalRef(arr);

    jclass activityThreadCls = env->FindClass("android/app/ActivityThread");
    if (!activityThreadCls) {
        return false;
    }

    jmethodID currentActivityThreadMethod = env->GetStaticMethodID(activityThreadCls, "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!currentActivityThreadMethod) {
        env->DeleteLocalRef(activityThreadCls);
        return false;
    }

    jobject activityThread = env->CallStaticObjectMethod(activityThreadCls, currentActivityThreadMethod);
    if (!activityThread) {
        env->DeleteLocalRef(activityThreadCls);
        return false;
    }

    jfieldID sPackageManagerField = env->GetStaticFieldID(activityThreadCls, "sPackageManager", "Landroid/content/pm/IPackageManager;");
    if (!sPackageManagerField) {
        env->DeleteLocalRef(activityThreadCls);
        env->DeleteLocalRef(activityThread);
        return false;
    }

    jobject realPackageManager = env->GetStaticObjectField(activityThreadCls, sPackageManagerField);
    if (!realPackageManager) {
        env->DeleteLocalRef(activityThreadCls);
        env->DeleteLocalRef(activityThread);
        return false;
    }

    baseObject = env->NewGlobalRef(realPackageManager);

    env->DeleteLocalRef(activityThreadCls);
    env->DeleteLocalRef(activityThread);
    env->DeleteLocalRef(realPackageManager);

    return true;
}

jobject HookApplicationWrapper::invoke(JNIEnv* env, jobject proxy, jobject method, jobjectArray args) {
    jclass methodCls = env->GetObjectClass(method);
    jmethodID getNameMethod = env->GetMethodID(methodCls, "getName", "()Ljava/lang/String;");
    if (!getNameMethod) {
        env->DeleteLocalRef(methodCls);
        return nullptr;
    }

    jstring methodName = (jstring)env->CallObjectMethod(method, getNameMethod);
    if (!methodName) {
        env->DeleteLocalRef(methodCls);
        return nullptr;
    }

    const char* methodNameChars = env->GetStringUTFChars(methodName, nullptr);
    std::string methodNameStr(methodNameChars);
    env->ReleaseStringUTFChars(methodName, methodNameChars);
    env->DeleteLocalRef(methodName);

    if (methodNameStr == "getPackageInfo" && args != nullptr) {
        jobject argPkgName = env->GetObjectArrayElement(args, 0);
        jobject argFlags = env->GetObjectArrayElement(args, 1);

        if (!argPkgName || !argFlags) {
            goto default_invoke;
        }

        const char* pkgNameChars = env->GetStringUTFChars((jstring)argPkgName, nullptr);
        std::string argPkgNameStr(pkgNameChars);
        env->ReleaseStringUTFChars((jstring)argPkgName, pkgNameChars);

        jclass integerCls = env->FindClass("java/lang/Integer");
        jmethodID intValueMethod = env->GetMethodID(integerCls, "intValue", "()I");
        jint flags = env->CallIntMethod(argFlags, intValueMethod);
        env->DeleteLocalRef(integerCls);

        if ((flags & GET_SIGNATURES) == 0) {
            goto default_invoke;
        }

        jclass stringCls = env->FindClass("java/lang/String");
        jmethodID equalsMethod = env->GetMethodID(stringCls, "equals", "(Ljava/lang/Object;)Z");
        jboolean isPkgMatch = env->CallBooleanMethod(appPkgName, equalsMethod, argPkgName);
        env->DeleteLocalRef(stringCls);

        if (!isPkgMatch || !baseObject || !signArray) {
            goto default_invoke;
        }

        jmethodID invokeMethod = env->GetMethodID(methodCls, "invoke",
            "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
        if (!invokeMethod) {
            goto default_invoke;
        }

        jobject result = env->CallObjectMethod(method, invokeMethod, baseObject, args);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return nullptr;
        }
        if (!result) {
            return nullptr;
        }

        jclass pkgInfoCls = env->GetObjectClass(result);
        jfieldID signaturesField = env->GetFieldID(pkgInfoCls, "signatures", "[Landroid/content/pm/Signature;");
        if (!signaturesField) {
            env->DeleteLocalRef(pkgInfoCls);
            return result;
        }

        jclass signatureCls = env->FindClass("android/content/pm/Signature");
        if (!signatureCls) {
            env->DeleteLocalRef(pkgInfoCls);
            return result;
        }

        jmethodID sigCtor = env->GetMethodID(signatureCls, "<init>", "([B)V");
        if (!sigCtor) {
            env->DeleteLocalRef(signatureCls);
            env->DeleteLocalRef(pkgInfoCls);
            return result;
        }

        jsize sigCount = env->GetArrayLength(signArray);
        jobjectArray newSignatures = env->NewObjectArray(sigCount, signatureCls, nullptr);

        for (jsize i = 0; i < sigCount; ++i) {
            jbyteArray sigBytes = (jbyteArray)env->GetObjectArrayElement(signArray, i);
            jobject signatureObj = env->NewObject(signatureCls, sigCtor, sigBytes);
            env->SetObjectArrayElement(newSignatures, i, signatureObj);
            env->DeleteLocalRef(signatureObj);
            env->DeleteLocalRef(sigBytes);
        }

        env->SetObjectField(result, signaturesField, newSignatures);

        env->DeleteLocalRef(newSignatures);
        env->DeleteLocalRef(signatureCls);
        env->DeleteLocalRef(pkgInfoCls);

        return result;
    }

default_invoke:
    if (baseObject) {
        jmethodID invokeMethod = env->GetMethodID(methodCls, "invoke",
            "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
        if (invokeMethod) {
            jobject res = env->CallObjectMethod(method, invokeMethod, baseObject, args);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                return nullptr;
            }
            env->DeleteLocalRef(methodCls);
            return res;
        }
    }
    env->DeleteLocalRef(methodCls);

    return nullptr;
}
 
static std::unique_ptr<HookApplicationWrapper> g_hookApp;
static bool g_monitorStarted = false;

static void* HooksMonitorThread(void* arg)
{
    (void)arg;
    while (!ResolveLibraryBounds("libSrc.so")) {
        struct timespec ts = {0, 500 * 1000000L};
        nanosleep(&ts, nullptr);
    }
    HooksInit();
    return nullptr;
}

static void EnsureMonitorThread()
{
    if (g_monitorStarted) {
        return;
    }
    g_monitorStarted = true;
    pthread_t tid;
    pthread_create(&tid, nullptr, HooksMonitorThread, nullptr);
    pthread_detach(tid);
}

extern "C"
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)reserved;
    g_vm = vm;
    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_talkweb_securitypay_HookApplication_patchLibSrc(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;
    AccountBindingBypass();
    EnsureMonitorThread();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_talkweb_securitypay_HookApplication_init(JNIEnv* env, jobject thiz, jobject context) {
    if (!g_hookApp) {
        g_hookApp = std::make_unique<HookApplicationWrapper>();
    }
    g_hookApp->init(context);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_talkweb_securitypay_HookApplication_nativeInvoke(JNIEnv* env, jobject thiz, jobject proxy, jobject method, jobjectArray args) {
    if (!g_hookApp) {
        return nullptr;
    }
    return g_hookApp->invoke(env, proxy, method, args);
}
