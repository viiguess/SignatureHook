#pragma once
#include <jni.h>
#include <memory>
#include <pthread.h>
#include <string>
#include <thread>
#include <chrono>
#include "Hooks.h"
#include "MemUtils.h"

static JavaVM* g_vm = nullptr;

struct HookApplicationWrapper {
    jobject baseObject = nullptr;
    jobjectArray signArray = nullptr;
    jstring appPkgName = nullptr;

    const jint GET_SIGNATURES = 0x40;

    std::string hardcodedBase64 = "AQAAA2YwggNiMIICSqADAgECAgRQUn1jMA0GCSqGSIb3DQEBBQUAMHIxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJXQTEQMA4GA1UEBxMHU2VhdHRsZTEYMBYGA1UEChMPRWxlY3Ryb25pYyBBcnRzMRUwEwYDVQQLEwxQb3BDYXAgR2FtZXMxEzARBgNVBAMTCkpvZSBNb2JsZXkwIBcNMTIwOTE0MDA0MjExWhgPMjA2MjA5MDIwMDQyMTFaMHIxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJXQTEQMA4GA1UEBxMHU2VhdHRsZTEYMBYGA1UEChMPRWxlY3Ryb25pYyBBcnRzMRUwEwYDVQQLEwxQb3BDYXAgR2FtZXMxEzARBgNVBAMTCkpvZSBNb2JsZXkwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCxnJ8iNZfUKW+d6z9lqcmkr3Edp2FWdsDOjggBBr+6uUo/7kr4t1zo7yxmeNWMNAXjca+muNnMiTxFT+MG9abXJpbNeFcJgiZ+iMh28lHjcjXYDpCmWMzFC6UDiiVnkk/FTh869XqjVfKEqLhIKx/P5k2q1A7CO3oK/H9jlexmBXFfVUtJbyt5yoDgr6/rZQhlaGYA2abWHnlaVM9qnfE7Rc+UJmM9klKcvH/Vf1ZM348xI7wTMugaG9o8RD8k98lAWIlNwG99lSJZ+iQwlwAvEGQmDpbdlfddYzOisswOAASNW7mlqd9+GMqfWN9D/TgwDpPKQej3HLly8xxnUDlhAgMBAAEwDQYJKoZIhvcNAQEFBQADggEBAJzqbbwaR1X4Mx9lA4UJv5VvTbd6GEYxRxCxF2DafDCFwb3yuMCfIylMr+V2P1ptZIBVvkY4PyZgojbeXaUZalemKjbMaCsOn7VO/mJwsyzHSaTa50hzkec1C8xqF1/ofSIPV3JAAgQFJAU8jIXJEwrvcduLVXjaaRhbTm5l68aaElZhhKL9rYzO86O7twgvS9u4wb5mC9+gfJiwgCIbMOJ4F1NUaDX//EG05yUXv6P5+wADnLEGoGEl0ZSNBGXaJCGajHvGXQEkmdvGTSnH3HkLidQRJzXq4jaZa/FcNk9CYgVt2CAqUWLhPY1PoQ3Mq7x89quKzm5dtslT8LR3sHk=";

    ~HookApplicationWrapper();
    JNIEnv* getEnv();
    jobjectArray decodeSignatures(JNIEnv* env);
    bool init(jobject context);
    jobject invoke(JNIEnv* env, jobject proxy, jobject method, jobjectArray args);
};