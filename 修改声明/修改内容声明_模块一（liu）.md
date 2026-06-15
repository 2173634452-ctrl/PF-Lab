# 修改内容声明 — 模块一（liu）

> **日期**：2026-05-31  
> **修改人**：liu  
> **分支**：liu  
> **模块**：模块一：数据基础操作（1.1 ~ 1.7）

---

## 一、修改概览

本次提交完成了《编程基础实践》任务书中**模块一：数据基础操作**的全部7个子任务（1.1~1.7）的代码实现与文档编写。

---

## 二、修改文件清单

### 修改的已有文件（6个）

| 文件 | 修改内容 |
|------|---------|
| `include/common.h` | 新增 `#include <time.h>`、`#include <stdbool.h>`；新增6组参数合理范围常量（`VALID_TEMP_MIN/MAX` 等）；`WaterDataset` 新增 `bool preprocessed` 字段 |
| `include/data_io.h` | 新增10个函数声明：数据概览、存储性能对比、分页浏览、筛选、排序、修改、单条删除、批量删除 |
| `include/backup.h` | 新增 `list_backup_files`、`validate_backup_file` 函数声明；调整 `list_backup_files` 缓冲区大小 |
| `include/ui.h` | 新增 `show_data_submenu`、`handle_data_submenu` 函数声明；`show_data_submenu` 增加参数 |
| `src/data_io.c` | **重写全部内容**（原为5个空壳函数）。实现：CSV读取（动态扩容+缺失值处理+错误处理+**6/7列兼容解析+时间戳自动生成**）、CSV/二进制读写、性能对比、分页浏览、条件筛选、参数排序、单条修改（含范围验证+自动备份+预处理标记重置）、单条删除+批量删除（含确认+自动备份+预处理标记重置） |
| `src/backup.c` | **重写全部内容**（原为2个空壳函数）。实现：带时间戳备份、备份文件列表、格式验证、数据恢复 |
| `src/ui.c` | 模块一子菜单（12个选项）的实现与串联；主菜单中数据备份/恢复快捷入口；数据概览显示 |

### 新建文件（2个）

| 文件 | 说明 |
|------|------|
| `docs/module1_design.md` | 1.1 设计论述文档：动态数组选择依据、CSV vs 二进制对比分析、实际存储大小差异原因 |
| `修改声明/修改内容声明_模块一（liu）.md` | 本文件，修改内容声明 |

---

## 三、功能覆盖情况（对照任务书）

### 1.1 数据结构设计 ✅
- `WaterRecord` 结构体定义（`include/common.h`）
- `WaterDataset` 动态数组结构体
- 论述文档说明动态数组选择原因（`docs/module1_design.md`）

### 1.2 文件读取 ✅
- `load_csv_data()` — 自动跳过第一行表头
- 动态内存分配：初始容量1000，满时翻倍（`realloc`）
- 错误处理：文件不存在/格式错误/内存不足 → 释放已分配内存并返回NULL
- 空值处理：空字段(`,,`)、`NaN`/`nan`/`NAN`、`-999`/`-9999` → 标记 `valid=false`
- 数据概览文件写入 `reports/data_overview.txt`

### 1.3 数据存储 ✅
- `save_csv_data()` — 文本格式写入
- `save_binary_data()` — 二进制格式写入
- `load_binary_data()` — 二进制格式读取
- `compare_storage_performance()` — 性能对比表（`clock()` 计时）
- CSV vs 二进制论述（`docs/module1_design.md`）

### 1.4 数据查询 ✅
- `view_data_paginated()` — 分页浏览（15条/页，N/P/J/Q操作）
- `filter_data_by_range()` — 按参数范围筛选
- `sort_and_display_data()` — 按任意参数升序/降序（`qsort`）

### 1.5 数据修改 ✅
- `modify_single_record()` — 按记录号定位修改
- 范围验证：新值必须在合理范围 [min, max] 内
- 保存确认：`confirm_action()` 询问是否保存
- 自动备份：修改前调用 `backup_dataset()`

### 1.6 数据删除 ✅
- `delete_single_record()` — 单条删除 + 确认提示
- `batch_delete_records()` — 按条件批量删除 + 确认提示
- 自动备份：删除前调用 `backup_dataset()`

### 1.7 数据备份与恢复 ✅
- `backup_dataset()` — 带时间戳备份（`backup_YYYYMMDD_HHMMSS.csv`）
- `list_backup_files()` — 列出可用备份文件
- `validate_backup_file()` — 验证备份文件格式（列数检查）
- `restore_dataset()` — 从备份恢复数据

---

## 四、编译验证

```
gcc -Iinclude src/*.c -o seawater_analysis -Wall -Wextra
```
✅ 编译通过，零错误零警告。

---

## 五、BUG 修复记录（2026-06-15 追加）

> 以下 BUG 在模块三集成测试时发现，根因均在模块一的 `src/data_io.c`。

### BUG-1：CSV 列数不匹配导致数据显示全为零 🔴

| 项目 | 说明 |
|------|------|
| **严重程度** | 🔴 严重 — 数据读取全部失效 |
| **根因** | CSV 文件实际为 **6 列**（无时间戳列），但 `load_csv_data` 解析条件为 `field_count < 7`，导致全部 23,203 条记录被跳过 |
| **修复** | `field_count < 7` → `< 6`；增加 `offset` 变量（6 列 =0，7 列 =1）统一数值字段索引；6 列时自动生成时间戳 |
| **影响文件** | `src/data_io.c` 第 191-216 行 |

### BUG-2：时间戳自动生成溢出（非法日期 `2024-01-81`） 🟡

| 项目 | 说明 |
|------|------|
| **严重程度** | 🟡 中等 — 后期记录的日期显示异常，按天分组混乱 |
| **根因** | 初版用 `sprintf(ts, "2024-01-%02d 12:%02d:%02d", day_index, ...)` 硬编码拼凑。当记录数 > 31×288 时，`day_index` 超过 31，`%02d` 不会自动进位，产生 `2024-01-81` 等非法日期 |
| **修复** | 改用 `mktime()` + `localtime()` + `strftime()` 标准日历函数，`mktime` 自动处理月/年进位和闰年；时间基准从 2024-01-01 改为任务书要求的 **2025-01-01 12:00** |
| **影响文件** | `src/data_io.c` 第 203-216 行 |

### 关联影响

BUG-1 和 BUG-2 的修复还涉及以下连锁改动：

| 改动 | 说明 |
|------|------|
| `include/common.h` | `WaterDataset` 新增 `bool preprocessed` 字段 |
| `src/data_io.c` | `modify_single_record`、`delete_single_record`、`batch_delete_records` 执行后将 `preprocessed` 重置为 `false` |
| `src/ui.c` | 分析子菜单新增预处理状态显示 + Y/N 提醒确认 |

---

## 六、待完成依赖

- 模块一正常运行需要 `data/raw/data.csv` 数据文件（用户需自行放入）
- 模块二 ~ 模块五尚未实现（函数仍为空壳）
