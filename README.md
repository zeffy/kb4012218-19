# wufuc
[![Donate Bitcoin](https://cdn.rawgit.com/zeffy/wufuc/badges/bitcoin.svg)](https://admin.gear.mycelium.com/gateways/3554/orders/new) [![AppVeyor Builds](https://img.shields.io/appveyor/ci/zeffy/wufuc.svg?logo=appveyor&style=flat-square)][AppVeyor] [![Chat on Telegram](https://cdn.rawgit.com/zeffy/wufuc/badges/telegram.svg)](https://t.me/joinchat/HEo6LUvV_83O92WzbYXLeQ) [![All Releases](https://img.shields.io/github/downloads/zeffy/wufuc/total.svg?style=flat-square)][Latest]

[:de:] [:es:] [:fr:] [:it:] [:hungary:] [:brazil:] [:ru:] [:cn:] [:taiwan:]

Disables the "Unsupported Hardware" message in Windows Update, and allows you to continue installing updates on Windows 7 and 8.1 systems with Intel Kaby Lake, AMD Ryzen, or other unsupported processors.

## Downloads 

**[Latest stable build][Latest] - Most people will want this version.**

[Unstable builds][AppVeyor] - Probably contains bugs; do not report issues with these builds.

## Donate :heart:

[**Click here for donation options!**](https://github.com/zeffy/wufuc/blob/master/DONATE.md)

## Background

The release notes for Windows updates KB4012218 and KB4012219 included the following:

> Enabled detection of processor generation and hardware support when PC tries to scan or download updates through Windows Update.

These updates marked the implementation of a [policy change](https://blogs.windows.com/windowsexperience/2016/01/15/windows-10-embracing-silicon-innovation/) they announced some time ago, where Microsoft stated that they would not be supporting Windows 7 or 8.1 on next-gen Intel, AMD and Qualcomm processors.

This is essentially a big middle finger to anyone who decides to not "upgrade" to Windows 10,
and it is especially unfortunate considering the extended support periods for Windows 7 and 8.1 won't be ending until January 4, 2020 and January 10, 2023 respectively.

Some people with older Intel and AMD processors are also affected! I've received user reports of the following CPUs all being blocked from receiving updates:

- [Intel Atom Z530](https://github.com/zeffy/wufuc/issues/7)
- [Intel Atom D525](https://github.com/zeffy/wufuc/issues/34)
- [Intel Core i5-M 560](https://github.com/zeffy/wufuc/issues/23)
- [Intel Core i5-4300M](https://github.com/zeffy/wufuc/issues/24)
- [Intel Pentium B940](https://github.com/zeffy/wufuc/issues/63)
- [AMD FX-8350](https://github.com/zeffy/wufuc/issues/32)
- [AMD Turion 64 Mobile Technology ML-34](https://github.com/zeffy/wufuc/issues/80)

## Bad Microsoft!

If you are interested, you can read my original write-up on discovering the CPU check [here](https://github.com/zeffy/wufuc/tree/old-kb4012218-19).

The tl;dr version is basically, inside a system file named `wuaueng.dll`, there are two functions responsible for the CPU check: `IsDeviceServiceable(void)` and `IsCPUSupported(void)`. 
`IsDeviceServiceable` simply calls `IsCPUSupported` once, and then re-uses the result that it receives on subsequent calls.

## Features

- Enables Windows Update on PCs with unsupported processors.
- Written in C, the best programming language. :sunglasses:
- Completely free (as in freedom) software.
- Does not modify any system files.
- Byte pattern-based patching, which means it will usually keep working even after new updates come out.
- No dependencies.

## Frequently Asked Questions

See [FAQ.md](https://github.com/zeffy/wufuc/blob/master/FAQ.md).

## How it works

This is a basic run-down of what wufuc does when you install it:

- The installer registers a scheduled task that automatically starts wufuc on system boot/user log on.
- Depending on how the Windows Update service is configured to run, wufuc will:
    * **Shared process**: inject itself into the service host process that Windows Update will run in when it starts.
    * **Own process**: wait for the Windows Update service to start and then inject into it.
- Once injected, wufuc will hook some functions where appropriate:
    * `LoadLibraryExW` hook will automatically hook the `IsDeviceServiceable()` function inside `wuaueng.dll` when it is loaded.
    * `RegQueryValueExW` hook is necessary to provide compatibility with [UpdatePack7R2](../../issues/100). This hook not applied when `wuauserv` is configured to run in its own process.

## Sponsors

### [Advanced Installer](https://www.advancedinstaller.com/)

The installer packages are created with Advanced Installer using an [open source license](http://www.advancedinstaller.com/free-license.html). 
Advanced Installer's intuitive and friendly user interface allowed me to quickly create a feature complete installer with minimal effort. Check it out!

## Special thanks

- Wen Jia Liu ([@wj32](https://github.com/wj32)) for his awesome program [Process Hacker](https://github.com/processhacker2/processhacker), and also for his [phnt headers](https://github.com/processhacker2/processhacker/tree/master/phnt).
- Duncan Ogilvie ([@mrexodia](https://github.com/mrexodia)) for [x64dbg](https://github.com/x64dbg/x64dbg), its [`patternfind.cpp`](https://github.com/x64dbg/x64dbg/blob/development/src/dbg/patternfind.cpp) algorithm, and its issue template which I adapted for this project.
- Tsuda Kageyu ([@TsudaKageyu](https://github.com/TsudaKageyu)) for his excellent [minhook](https://github.com/TsudaKageyu/minhook) library.

[Latest]: https://github.com/zeffy/wufuc/releases/latest
[AppVeyor]: https://ci.appveyor.com/project/zeffy/wufuc
[:de:]: https://github.com/zeffy/wufuc/wiki/README-(Deutsch)
[:es:]: https://github.com/zeffy/wufuc/wiki/README-(Espa%C3%B1ol)
[:fr:]: https://github.com/zeffy/wufuc/wiki/README-(Fran%C3%A7ais)
[:it:]: https://github.com/zeffy/wufuc/wiki/README-(Italiano)
[:hungary:]: https://github.com/zeffy/wufuc/wiki/README-(Magyar)
[:brazil:]: https://github.com/zeffy/wufuc/wiki/README-(Portugu%C3%AAs%20Brasileiro)
[:ru:]: https://github.com/zeffy/wufuc/wiki/README-(%D1%80%D1%83%D1%81%D1%81%D0%BA%D0%B8%D0%B9)
[:cn:]: https://github.com/zeffy/wufuc/wiki/README-(%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87)
[:taiwan:]: https://github.com/zeffy/wufuc/wiki/README-(%E7%B9%81%E9%AB%94%E4%B8%AD%E6%96%87)
