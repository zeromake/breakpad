# Google Breakpad (zeromake Fork)

## 项目概述

这是 [Google Breakpad](https://chromium.googlesource.com/breakpad/breakpad/) 的一个 fork 分支，由 zeromake 维护。Breakpad 是一个跨平台的崩溃报告库，支持在客户端捕获崩溃转储(minidump)，并在服务端进行符号化和分析。

### 核心能力

- **客户端崩溃捕获 (Client)**: 在 Linux、Windows、macOS 上捕获程序崩溃，生成 minidump
- **符号提取 (dump_syms)**: 从调试信息(DWARF, PDB)中提取符号文件(.sym)
- **Minidump 分析 (minidump_stackwalk)**: 解析 minidump 并生成可读的堆栈回溯
- **符号上传 (symupload)**: 将符号文件上传到崩溃收集服务

## 项目结构

```
./
├── xmake.lua              # 主构建配置 (xmake)
├── src/
│   ├── client/            # 客户端崩溃捕获库
│   │   ├── linux/         #   Linux 客户端 (信号处理器)
│   │   ├── mac/           #   macOS 客户端 (Mach 异常)
│   │   └── windows/       #   Windows 客户端 (异常处理器)
│   ├── common/            #  客户端/服务端共享代码
│   │   ├── linux/         #   Linux 工具 (ELF 解析, DWARF, file_id 等)
│   │   ├── mac/           #   macOS 工具
│   │   └── windows/       #   Windows 工具 (PE 解析等)
│   ├── google_breakpad/   #   公共 C++ API 头文件
│   ├── processor/         #   Minidump 处理器/堆栈回溯器
│   ├── tools/             #   命令行工具
│   │   ├── linux/         #     Linux 工具 (dump_syms, pid2md, core2md 等)
│   │   ├── mac/           #     macOS 工具
│   │   ├── windows/       #     Windows 工具 (dump_syms, symupload)
│   │   └── python/        #     Python 辅助脚本
│   └── third_party/       #   第三方依赖
├── android/               # Android NDK 构建支持
├── docs/                  # 文档
└── autotools/             # 遗留 autotools 配置 (废弃)
```

## 构建系统

本项目使用 **xmake** 作为主要构建系统。autotools 文件已移除。

```bash
# 构建所有目标
xmake

# 构建指定目标
xmake breakpad                # 仅构建核心库
xmake dump_syms               # 仅构建符号提取工具
xmake minidump_stackwalk      # 仅构建 minidump 分析工具

# 运行测试
xmake run <target_name>

# 调试构建
xmake f -m debug
xmake
```

### 核心构建目标

| 目标 | 描述 |
|------|------|
| `breakpad` | 核心崩溃客户端库（静态库） |
| `dump_syms` | 符号提取命令行工具 |
| `minidump_stackwalk` | Minidump 堆栈回溯工具 |

## 平台支持

| 平台 | 客户端 | 工具 |
|------|--------|------|
| Linux (x86, x86-64, ARM, ARM64, MIPS, RISC-V, PPC, SPARC) | ✅ | ✅ |
| Windows (x86, x86-64) | ✅ | ✅ |
| macOS (x86-64, ARM64) | ✅ | ✅ |
| Android (ARM, ARM64, x86) | ✅ | 部分 |
| Solaris | ❌ | 部分 |

## 编码规范

### C++ 标准
- 默认 C++17 (`.clang-format` 配置)
- 提交 `d4afec77` 将 dump_syms 更新为 C++20

### 代码风格 (`src/.clang-format`)
- 基于 Google C++ Style
- 缩进: 2 空格
- 列限制: 100 字符
- 命名空间: 不使用缩进
- 访问修饰符: 1 空格缩进
- Include 分组: Google 风格 (`main` 相关头文件 → 项目头文件 → 第三方 → 标准库)

### 命名约定
- **命名空间**: `google_breakpad`
- **类/类型**: PascalCase
- **函数/方法**: camelCase
- **成员变量**: 下划线后缀 (`member_`)
- **常量/宏**: UPPER_SNAKE_CASE
- **文件**: snake_case (`.cc`, `.h`)

### 关键实践
- 所有文件包含 Google BSD 许可证头
- 使用 `#ifndef` + `#define` 头文件保护
- 使用 `-inl.h` 后缀标记模板内联实现
- 异常处理场景使用 C 风格兼容设计（因在崩溃处理器中使用）

## 关键组件

### 客户端库 (`src/client/`)
- **Linux**: 通过信号处理 (`SIGSEGV` 等) 捕获崩溃，生成 minidump
- **Windows**: 通过 `SetUnhandledExceptionFilter` 捕获异常
- **macOS**: 通过 Mach 异常处理机制

### 处理器 (`src/processor/`)
- 支持多种 CPU 架构的堆栈回溯
  - x86, x86-64, ARM, ARM64, MIPS, PPC, PPC64, RISC-V (32/64), SPARC
- `minidump_stackwalk` 是主要的分析入口
- 支持 `minidump-2-core` 将 minidump 转为 Linux core dump

### 符号文件格式 (`.sym`)
提取的符号文件使用文本格式：
```
MODULE <os> <arch> <id> <name>
FILE <index> <path>
FUNC <address> <size> <param_size> <name>
<line> <address> <size> <file_index>
PUBLIC <address> <param_size> <name>
STACK CFI <address> <size> <register_rules>
STACK WIN <type> <base> <size> ...
```

### Fork 特有变更
- **xmake 构建系统**: 上游 autotools 被 xmake 替代 (`ac2d9055`)
- **DWARF 5 支持**: 处理嵌套 Objective-C 方法声明 (`8ef56734`)
- **Windows 构建修复**: 移除 `stdext` 使用 (`327991c7`)
- **各种 AI 辅助修复**: 指针对齐、溢出修复等
- **git submodules**: 用于依赖管理 (`6276a37f`)

## 测试

使用 Google Test (gtest) 框架。测试文件以 `_unittest.cc` 或 `_test.cc` 后缀命名。

### 主要测试位置
- `src/processor/*_unittest.cc`
- `src/client/linux/*_unittest.cc`
- `src/client/mac/tests/`
- `src/client/windows/unittests/`
- `src/common/linux/*_unittest.cc`

```bash
# 运行所有测试
xmake run

# 运行特定测试
xmake run <test_target_name>
```

## 常用工作流

### 1. 为新的崩溃捕获目标构建客户端
```bash
# 构建静态库
xmake breakpad
# 链接: 链接 libbreakpad.a 并包含 src/client/ 头文件
```

### 2. 从二进制提取符号
```bash
# Linux | Windows
xmake dump_syms
xmake run dump_syms /path/to/binary > output.sym

# 另一种方式直接创建好目录放置对应的 sym
xmake run dump_syms /path/to/binary /path/to/dir
```

### 3. 分析 minidump
```bash
# 方式一: 使用符号目录 (需要固定目录结构 <symbol_path>/<module>/<debug_id>/<module>.sym)
xmake
xmake run minidump_stackwalk /path/to/minidump /path/to/symbols/

# 方式二: 旧式用法, 后续参数为符号目录 (保持兼容)
xmake run minidump_stackwalk -m /path/to/minidump /path/to/symbols/
```

> **注意**: `-F <file>` 选项是本 fork 新增的功能，允许直接指定单个 `.sym` 文件，
> 不再需要按照 `<symbol_path>/<module>/<debug_id>/<module>.sym` 的固定目录结构放置符号文件。

## 关键限制

- 客户端库设计为在崩溃处理上下文中运行，有严格的信号安全约束
- 不要从客户端库的崩溃处理回调中调用非信号安全函数
- Windows 客户端需要 DbgHelp.dll
- Linux 客户端需要提供 `/proc/` 文件系统访问权限

## 维护者

- **zeromake** — fork 维护者