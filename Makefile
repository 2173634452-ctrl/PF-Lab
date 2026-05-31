CC       = gcc
CFLAGS   = -std=c11 -Wall -Iinclude
LDFLAGS  =
LDLIBS   = -lm              # 如需数学库，按需保留

SRCDIR   = src
OBJDIR   = build
TARGET   = seawater_analysis

SRC      = $(wildcard $(SRCDIR)/*.c)
OBJ      = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC))
# 自动生成的依赖文件（.d）
DEP      = $(OBJ:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# 同时生成 .o 和 .d 文件
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

# 引入依赖文件（-include 表示文件不存在时不报错）
-include $(DEP)

clean:
	rm -rf $(OBJDIR) $(TARGET)