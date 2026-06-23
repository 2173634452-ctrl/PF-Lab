# 修改内容声明 — 模块一至三 Bug 修复

> **日期**：2026-06-18（首次），2026-06-23（补充修复）
> **修改人**：Claude Code (JackieLai 审查)
> **范围**：模块一（数据基础操作）、模块二（数据预处理）、模块三（统计分析）、模块五（系统集成）

---

## 一、修复概览

在对模块一至三的合规性审查中，发现 2 个严重逻辑错误、3 个中等错误和 3 个小问题。本次提交修复了其中 6 个问题，涉及 3 个源文件。后续模块五审查中又发现 2 个 UI 层问题，也已一并修复。2026-06-23 代码审查中发现 UI 函数签名不匹配导致权限系统失效的关键 Bug，本次一并修复。

---

## 二、修复清单

### 🔴 严重逻辑错误

#### 修复1：缺失值被置为 0.0 而非 NaN

- **文件**：`src/data_io.c`（L221-234）
- **问题**：`load_csv_data` 中 `memset(rec, 0, ...)` 将全部字段初始化为 0.0。当 `parse_double_field` 返回 false（空值/NaN/-999/-9999）时，不修改字段值，导致缺失字段保持为 0.0。后续 `fill_missing_values` 通过 `isnan()` 检测缺失值，但 `isnan(0.0) = false`，导致所有缺失值从未被填充，且记录被错误标记为 `valid=true`。
- **修复**：`parse_double_field` 返回 false 时，显式将对应字段设为 `NAN`。
- **影响文件**：`src/data_io.c`

#### 修复2：异常值检测跳过含缺失值的记录

- **文件**：`src/preprocess.c`（L192-222, L270-272）
- **问题 A**：`count_outliers` 中 `is_param_in_range` 对 NaN 返回 false，导致 NaN 被计为异常值。但 NaN 是缺失值，应归模块2.2处理，不应计入异常值。
- **修复 A**：每个参数增加 `!isnan()` 前置判断，NaN 跳过不计为异常值。
- **问题 B**：`detect_and_handle_outliers` 中有 `if (!rec->valid) continue;`，导致含缺失值（valid=false）的记录被完全跳过。如果此类记录同时含有真正的异常值（如某字段解析成功但数值溢出），异常值永远不会被检测到。
- **修复 B**：删除该跳过逻辑，使异常检测扫描所有记录。
- **影响文件**：`src/preprocess.c`

---

### 🟡 中等错误

#### 修复3：save_csv_data 静默丢弃无效记录

- **文件**：`src/data_io.c`（L301-316）
- **问题**：`save_csv_data` 中 `if (rec->valid)` 条件导致无效记录完全不被写入。而 `backup_dataset` 对无效记录写 NaN 标记。两个函数行为不一致，用户"保存"操作会丢失数据。
- **修复**：对齐 `backup_dataset` 行为——无效记录输出 NaN 占位。
- **影响文件**：`src/data_io.c`

#### 修复4：UI 函数签名不匹配导致权限控制失效

- **文件**：`src/ui.c`
- **日期**：2026-06-23
- **问题**：`ui.h` 中声明 `show_main_menu(UserRole role)` 和 `handle_menu_choice(int, WaterDataset**, UserRole)`，但 `ui.c` 中对应的函数定义为 `show_main_menu(void)` 和 `handle_menu_choice(int, WaterDataset**)`，缺少 `UserRole role` 参数。`main.c` 调用时按头文件签名传入 `role`，而实际函数不接受该参数，属于 C 语言中实参与形参不匹配的未定义行为。同时 `auth.c` 中已实现的 `has_permission()` 从未被 UI 层调用，导致 guest 用户能看到并操作所有功能菜单，权限控制形同虚设。
- **修复**：
  - `show_main_menu` 增加 `UserRole role` 参数，根据 admin/guest 角色分别显示不同的菜单项——admin 看到全部 10 个选项，guest 只能看到 [5] 数据概览、[7] 分析报告、[9] 清屏、[0] 退出
  - `handle_menu_choice` 增加 `UserRole role` 参数，在 switch 分发前调用 `has_permission(role, choice)` 做权限守卫，guest 尝试访问未授权功能时直接提示"权限不足"并 return
  - 添加 `display_text_file` 的前向声明，修复因该 static 函数被主菜单处理函数调用但定义在后部而产生的隐式声明编译错误
- **编译验证**：`gcc -std=c11 -Wall -Iinclude src/*.c -o seawater_analysis -lm` 零错误零警告通过
- **影响文件**：`src/ui.c`

#### 修复5：Windows 下 mkdir 编译兼容性

- **文件**：`src/backup.c`（L11, L42-43）
- **问题**：`#include <sys/stat.h>` 在 MinGW 中声明 `mkdir` 需要两个参数（POSIX 签名），但 Windows 分支调用 `mkdir(directory)` 只传一个参数，部分 MinGW 版本编译报错。
- **修复**：Windows 下增加 `#include <direct.h>`，改用 MSVC 兼容的 `_mkdir(directory)`。
- **影响文件**：`src/backup.c`

---

### 🟢 小问题

#### 修复6：split_csv_line 换行符处理

- **文件**：`src/data_io.c`（L47-50）
- **问题**：原代码只检查行尾最后一个字符是否为 `\n` 或 `\r`，若行尾为 `\r\n` 组合（非文本模式或跨平台场景），只删除 `\n`，残留 `\r` 污染最后一个字段。
- **修复**：改用 while 循环逐个清除行尾所有 `\r`/`\n` 字符。
- **影响文件**：`src/data_io.c`

---

### 🟡 模块五集成审查发现的问题

#### 修复7：退出确认双重打印提示

- **文件**：`src/main.c`（L39-40）
- **问题**：`printf("确认退出系统？(y/n): ");` 后紧接 `confirm_action("退出系统")`，而 `confirm_action` 内部也打印 `退出系统 (y/n): `，导致用户看到两条提示，体验混乱。
- **修复**：删除冗余的 `printf("确认退出系统？(y/n): ");`，仅保留 `confirm_action("退出系统")` 统一处理。
- **影响文件**：`src/main.c`

#### 修复8：输入错误时 clear_console 调用顺序不当

- **文件**：`src/main.c`（L33-34）
- **问题**：`clear_console()` 在错误消息 `printf` 之前调用，导致错误消息显示在清屏后的空白屏幕上，随即循环回到 `show_main_menu` 重绘菜单，用户几乎看不到错误提示。
- **修复**：将 `clear_console()` 移至错误消息输出和输入缓冲区清理之后，确保用户先看到错误提示，再清屏重绘菜单。
- **影响文件**：`src/main.c`

---

### 🔵 模块二、五功能缺口修复（2026-06-23）

#### 修复9：移动平均滤波新增全窗口自动对比

- **文件**：`src/preprocess.c`
- **问题**：任务书 2.3(1) 要求"分别用窗口3、5、7、9、11"对四个参数进行滤波，但 `interactive_moving_average()` 只支持用户手动选一个窗口，无法一次性跑完五个窗口做横向对比。任务书 2.3(4) 要求的"确定最佳滤波窗口"也因此无从自动化。
- **修复**：
  - 抽离 `apply_ma_to_array()` 公共函数，将移动平均核心算法从 `apply_moving_average` 中独立出来，接受任意 double 数组，不修改原数据
  - 新增 `compute_noise_reduction()` 函数，对单参数数组计算指定窗口下的降噪率，用于对比阶段预判效果
  - 新增 `compare_filter_windows()` 函数，一次性用五个窗口计算四个参数的降噪率，输出横向对比表，自动标注最高降噪窗口并分析窗口大小与细节保留的权衡，最后由用户选择应用哪个窗口（对比阶段不修改数据）
  - `interactive_moving_average()` 新增输入 `0` 进入自动对比模式
  - `apply_moving_average()` 内层循环替换为 `apply_ma_to_array()` 调用，消除重复代码
- **影响文件**：`src/preprocess.c`

#### 修复10：清理 display_report_menu 死代码

- **文件**：`include/ui.h`、`src/ui.c`
- **问题**：`display_report_menu()` 在头文件中声明、源文件中实现为一句"报告查看功能尚未实现"的空壳，但该函数从未被任何地方调用——主菜单已通过 case 5/6/7 处理所有报告查看。属于残留的未完成代码。
- **修复**：从 `ui.h` 和 `ui.c` 中删除声明和空壳实现。
- **影响文件**：`include/ui.h`、`src/ui.c`

---

## 三、修改文件汇总

| 文件 | 修改类型 | 说明 |
|------|---------|------|
| `src/data_io.c` | 修改 | Fix#1: 缺失值→NaN; Fix#3: save_csv_data保留无效记录; Fix#6: 换行符处理 |
| `src/preprocess.c` | 修改 | Fix#2: count_outliers跳过NaN + 不再跳过valid=false记录; Fix#9: 全窗口自动对比 + 抽离公共函数 |
| `src/backup.c` | 修改 | Fix#5: Windows _mkdir兼容 |
| `src/ui.c` | 修改 (2026-06-23) | Fix#4: 修复函数签名不匹配 + 权限检查 + 动态菜单 + display_text_file前向声明; Fix#10: 移除display_report_menu死代码 |
| `include/ui.h` | 修改 (2026-06-23) | Fix#10: 移除display_report_menu声明 |
| `src/main.c` | 修改 | Fix#7: 删除双重退出提示; Fix#8: 修正clear_console顺序 |

---

## 四、对处理流程的影响

修复后的正确数据处理流程：

```
加载CSV数据
    ↓ （缺失字段设为 NaN，其他字段正常解析）
异常值检测 (2.1)
    ↓ （扫描全部记录，NaN不计为异常）
    ↓ （≥3个真正异常值 → 删除；<3 → 均值逼近填充）
缺失值填充 (2.2)
    ↓ （isnan() 正确检测到 NaN，用均值逼近法填充）
移动平均滤波 (2.3)
    ↓
统计分析 (模块三)
```

**关键逻辑变更**：
- 修复1保证了 `fill_missing_values` 能够真正检测并填充缺失值
- 修复2保证了含缺失值记录中的真正异常值不会被遗漏
- 两修复协同工作：NaN ← 缺失值（2.2处理） | 非NaN且超出范围 ← 异常值（2.1处理）

---

## 五、编译验证

```
gcc -std=c11 -Wall -Iinclude src/*.c -o seawater_analysis -lm
```

✅ 编译通过，零错误零警告。
