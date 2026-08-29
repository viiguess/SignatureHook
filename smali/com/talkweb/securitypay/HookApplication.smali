.class public Lcom/talkweb/securitypay/HookApplication;
.super Lcom/talkweb/securitypay/TwApplication;

.implements Ljava/lang/reflect/InvocationHandler;

.field private static nativeInited:Z

.method public constructor <init>()V
    .locals 0
    invoke-direct {p0}, Lcom/talkweb/securitypay/TwApplication;-><init>()V
    return-void
.end method

.method protected attachBaseContext(Landroid/content/Context;)V
    .locals 10

    invoke-super {p0, p1}, Lcom/talkweb/securitypay/TwApplication;->attachBaseContext(Landroid/content/Context;)V

    const-string v0, "SignatureHook"
    invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V

    invoke-static {p1}, Landroidx/multidex/MultiDex;->install(Landroid/content/Context;)V

    sget-boolean v0, Lcom/talkweb/securitypay/HookApplication;->nativeInited:Z
    if-nez v0, :skip_init

    invoke-virtual {p0, p1}, Lcom/talkweb/securitypay/HookApplication;->init(Landroid/content/Context;)V

    const/4 v0, 0x1
    sput-boolean v0, Lcom/talkweb/securitypay/HookApplication;->nativeInited:Z

    :skip_init

    const-string v0, "android.content.pm.IPackageManager"
    invoke-static {v0}, Ljava/lang/Class;->forName(Ljava/lang/String;)Ljava/lang/Class;
    move-result-object v1

    const/4 v2, 0x1
    new-array v2, v2, [Ljava/lang/Class;
    const/4 v3, 0x0
    aput-object v1, v2, v3

    invoke-virtual {v1}, Ljava/lang/Class;->getClassLoader()Ljava/lang/ClassLoader;
    move-result-object v4

    invoke-static {v4, v2, p0}, Ljava/lang/reflect/Proxy;->newProxyInstance(Ljava/lang/ClassLoader;[Ljava/lang/Class;Ljava/lang/reflect/InvocationHandler;)Ljava/lang/Object;
    move-result-object v5

    const-string v6, "android.app.ActivityThread"
    invoke-static {v6}, Ljava/lang/Class;->forName(Ljava/lang/String;)Ljava/lang/Class;
    move-result-object v6

    const-string v7, "sPackageManager"
    invoke-virtual {v6, v7}, Ljava/lang/Class;->getDeclaredField(Ljava/lang/String;)Ljava/lang/reflect/Field;
    move-result-object v7

    const/4 v8, 0x1
    invoke-virtual {v7, v8}, Ljava/lang/reflect/Field;->setAccessible(Z)V

    const/4 v8, 0x0
    invoke-virtual {v7, v8, v5}, Ljava/lang/reflect/Field;->set(Ljava/lang/Object;Ljava/lang/Object;)V

    return-void
.end method

.method public static native patchLibSrc()V
.end method

.method public invoke(Ljava/lang/Object;Ljava/lang/reflect/Method;[Ljava/lang/Object;)Ljava/lang/Object;
    .locals 6
    invoke-virtual {p0, p1, p2, p3}, Lcom/talkweb/securitypay/HookApplication;->nativeInvoke(Ljava/lang/Object;Ljava/lang/reflect/Method;[Ljava/lang/Object;)Ljava/lang/Object;
    move-result-object v2
    return-object v2
.end method

.method public native init(Landroid/content/Context;)V
.end method

.method public native nativeInvoke(Ljava/lang/Object;Ljava/lang/reflect/Method;[Ljava/lang/Object;)Ljava/lang/Object;
.end method
