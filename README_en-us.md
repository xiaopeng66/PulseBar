**[简体中文](./README.md) | English**

[![LICENSE](https://img.shields.io/badge/license-Anti%20996-blue.svg?style=flat-square)](https://github.com/996icu/996.ICU/blob/master/LICENSE)

# PulseBar

PulseBar is a Windows taskbar network-speed and resource monitor. It shows current network speed, CPU and memory usage inside the taskbar, with history traffic statistics and a plugin system. This project is a fork of [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor), reworked to focus on the taskbar experience.

## Features

* Embedded taskbar display: network speed, CPU / memory usage, optional hardware temperature
* Four tray icon styles: blue / light / dark / auto (follows taskbar theme)
* Live speed rendered as the tray icon
* Multi-NIC auto/manual selection, network details
* History traffic statistics
* Plugin system
* Light/dark theme adaptation, DPI scaling

## Build

* Open `PulseBar.sln` with Visual Studio 2022, or run `build-sln.bat`
* Output: `Bin/x64/Release/PulseBar.exe`
* Requires the Microsoft Visual C++ runtime: [latest supported VC++ redistributable](https://docs.microsoft.com/en-US/cpp/windows/latest-supported-vc-redist?view=msvc-170)

## Links

* Help: [Help_en-us.md](./Help_en-us.md)
* Issues: [Issues](https://github.com/xiaopeng66/PulseBar/issues)

## License

This project is based on [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) by zhongyang219 and is distributed under the [Anti-996 License](./LICENSE); original copyright notices are kept in [LICENSE](./LICENSE) and [LICENSE_CN](./LICENSE_CN).
