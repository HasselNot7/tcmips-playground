# TCMIPS-C

基于 TCMIPS 自定义 MIPS32 CPU(游戏 Turing Complete 内嵌架构)的 C/C++ 小项目集。

使用官方 LLVM 交叉工具链把标准 C/C++ 源码编译为 `.tcm` 可执行文件,可直接在游戏内的
TCMIPS CPU 文件加载器(File Loader)中运行。目标硬件外设包括:像素屏、ASCII 文本屏、
键盘、双排八位七段数码管、Unix 时间戳 syscall。

## 项目列表

| 可执行文件 | 说明 | 操作 |
|-----------|------|------|
| `tcmips_c` | Hello World 示例 | - |
| `tcmips_snake` | 贪吃蛇:吃食物变长加速,分数/长度实时显示在七段数码管 | 方向键移动,Q 退出,R 重开 |
| `tcmips_breakout` | 打砖块:三色砖块六关连闯,三条命 | 左右键移动挡板,上键发球,Q/R |
| `tcmips_guess` | 猜数字 0~9999,尝试次数与答案显示在七段数码管 | ASCII 屏键盘输入 |
| `tcmips_reaction` | 反应速度测试:等待 "NOW!" 出现后按键,微秒级计时 | 任意键,Q 退出 |
| `tcmips_clock` | 电子钟:天数 + 时:分:秒 | Q 退出 |

## 目录结构

```
.
├── CMakeLists.txt                  # 构建配置(需指定工具链文件)
├── main.cpp                        # Hello World
└── src/
    ├── tcm_util.h                  # 延时 / 随机数工具(基于时间戳 syscall)
    ├── snake.cpp                   # 贪吃蛇
    ├── breakout.cpp                # 打砖块
    ├── guess.cpp                   # 猜数字
    ├── reaction.cpp                # 反应速度测试
    └── clock.cpp                   # 电子钟
```

## 构建

从本仓库 Release 页面下载打包好的 TCMIPS 工具链,按系统选择其一,解压到项目根目录:

- `tcmips-2.1-toolchains-x86_64-linux-gnu.zip` — Linux
- `tcmips-2.1-toolchains-x86_64-mingw64.zip` — Windows

```
toolchains/
├── cmake/tcmips.toolchain.cmake
├── llvm/bin/{clang,clang++,llvm-link-tcmips}
└── sysroot/
```

命令行构建:

```bash
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

> `CMakeLists.txt` 已内置 `CMAKE_TOOLCHAIN_FILE` 路径,只要工具链已解压到项目根目录,
> 无需再手动指定。CLion 中同样直接创建一个 CMake 配置(生成器选 Ninja)即可构建。

构建产物 `build/tcmips_*.tcm` 即为可加载的固件镜像。

## 致谢

本项目基于 [zhangjiantao/tcmips](https://github.com/zhangjiantao/tcmips) 的 TCMIPS CPU 架构、
交叉编译工具链与 sysroot 开发,原作者为 [zhangjiantao](https://github.com/zhangjiantao)。

## 注意事项

- C++ 编译时禁用了异常与 RTTI(`-fno-exceptions -fno-rtti`)
- 目标为 `mipsel` MIPS32r2 软浮点,64 位整数与浮点由编译器 runtime 软件模拟
- 各游戏共用 `src/tcm_util.h` 中基于时间戳 syscall 的忙等待延时(裸机环境无可靠 sleep)