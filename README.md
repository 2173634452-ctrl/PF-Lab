# 海水养殖水质数据分析系统

本项目根据《编程基础实践》任务书建立，目标是构建一个基于 C 语言的海水养殖水质数据分析系统。

## 目录结构

- `src/`：C 源文件
- `include/`：头文件
- `data/raw/`：原始 CSV 数据
- `data/processed/`：预处理后数据
- `backup/`：备份文件
- `reports/`：分析报告和预警报告
- `docs/`：任务书和设计说明

## 主要功能模块

1. 数据基础操作
2. 数据预处理
3. 统计分析
4. 预测模型
5. 系统集成与用户权限管理

## 编译运行

建议使用 GCC 编译：

```bash
gcc -Iinclude src/*.c -o seawater_analysis
```

运行：

```bash
./seawater_analysis
```
