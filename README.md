This is the example of my [Android Inline Hook Project](https://github.com/GToad/Android_Inline_Hook.git) in arm mode. So I will only update that repo.

The target APP is `arm32-example.apk` and our .so file has already been compiled in `/libs/armeabi-v7a/libInlineArmHook.so`.

In this APP, you should touch the button more than 10 and it will show `Enough` in a toast. After the example `libInlineArmHook.so` is effective, the register R0 will be set to 0x333 (>10) so you can touch it just once to get `Enough`.

`notArmHooked.mp4` shows the APP run in a normal environment.

`ArmHooked.mp4` shows the APP run in a hooked environment.

The pictures of effect are showed below:

![](https://gtoad.github.io/img/in-post/post-android-native-hook-practice/notArmHooked.png)
![](https://gtoad.github.io/img/in-post/post-android-native-hook-practice/ArmHooked.png)


# Android Inline Hook

This project make an Android .so file that can automatically do some native hook works.

It mainly use Android Inline Hook, not PLT Hook.

If you can read Chinese or wanna see more picture, I've wrote some articles about this repo and the first one is the main article. `I highly recommend you to read the articles before reading the code.` These article will save you a lot of time, I promise.

1. [Android Inline Hook Practice](https://gtoad.github.io/2018/07/06/Android-Native-Hook-Practice/)
2. [Opcode Fix In Android Inline Hook](https://gtoad.github.io/2018/07/13/Android-Inline-Hook-Fix/)
3. [An Introduction to Android Native Hook](https://gtoad.github.io/2018/07/05/Android-Native-Hook/)

# How To Use

The only thing you have to change is the code in `InlineHook.cpp`.

You can name the `__attribute__((constructor)) ModifyIBored()` function at your will and change the follow arg in it:

1. `pModuleBaseAddr` is the address of your target so.
2. `target_offset` is the offset of your hook point in the target so.
3. `is_target_thumb` shows the hook point's CPU mode. You can know this information in the work of reversing before the hook work.

`EvilHookStubFunctionForIBored` function is the thing you really wanna do when the hook works. You can name at your will, but keep the arg `(pt_regs *regs)`. It brings you the power to control the registers, like set r0 to 0x333 : `regs->uregs[0]=0x333;`.

After you finish the args above, just `ndk-build` and you will get your .so file.

# Example

I've make some examples in other repo, it includes code and the target APK file.

1. [thumb-2 example](https://github.com/GToad/Android_Inline_Hook_Thumb_Example.git)
2. [arm32 example](https://github.com/GToad/Android_Inline_Hook_Arm_Example.git)

# Contact

I believe that this project still has some problems. If you find some bugs or have some problems, you can send e-mail to `gtoad1994@aliyun.com`. I wish we can fix it together!

# Reference

[Game Security Lab of Tencent](http://gslab.qq.com/portal.php?mod=view&aid=168)

[Ele7enxxh's Blog](http://ele7enxxh.com/Android-Arm-Inline-Hook.html)




