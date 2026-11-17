# SignatureHook

Library project for PvZ2 Chinese Version with a Signature Verification bypss and pattern-based patches for `libSrc.so` that are applied at runtime. The patches are pattern based to escape changing the hook offsets every time the new version of the game is released or when libSrc is updated through hot-update.

## Build

Install the [Android NDK](https://developer.android.com/ndk/downloads)

From the repository root, run the ndk-build script located in your NDK installation folder.

## Integration

1. Decode the APK with your APK rebuild tool.
2. Copy each built library into the corresponding decoded APK directory:

   ```text
   lib/armeabi-v7a/libSignatureHook.so
   lib/arm64-v8a/libSignatureHook.so
   ```

3. Copy `smali` folder into the decoded APK folder.
4. Set the manifest's `<application android:name>` to `com.talkweb.securitypay.HookApplication` in `AndroidManifest.xml`.
5. Add a call to `patchLibSrc()` at each libSrc loading point in `com/popcap/pvz2cthddev/SexyAppFrameworkActivity.smali`:

   ```smali
   invoke-static {}, Lcom/talkweb/securitypay/HookApplication;->patchLibSrc()V
   ```

   The supplied smali application wrapper declares this method but does **not** call it. Loading `libSignatureHook.so` alone does not start the native patches.

6. Rebuild the APK.

If you rename or relocate `HookApplication`, update the Smali class references, manifest entry, and matching `Java_com_talkweb_securitypay_HookApplication_*` JNI exports in `jni/SignatureHook.cpp`.
