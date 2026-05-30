# Task74：用户态 grep 程序 / pipe 文本过滤验证

## 1. 本轮目标

本轮目标是新增一个最小用户态 `grep` 程序，用于验证 stdin / pipe / stdout redirect 下的数据过滤。

## 2. 为什么需要本任务

`cat` 更像复制数据，`wc` 更像统计数据；而 `grep` 会真正筛选文本内容，更能体现 pipe 的价值：

```text
输入数据 -> grep keyword -> 匹配行
```

## 3. grep 当前语义

当前 `grep` 为教学版最小实现：

1. 用法为 `grep <keyword>`
2. 第一个参数作为关键字
3. 统一从 stdin 读取
4. 逐行判断是否包含关键字
5. 如果包含，则输出整行
6. 当前采用教学版 ASCII 大小写无关匹配
7. 不支持正则表达式

## 4. 依赖 syscall

`grep` 当前主要依赖：

1. `sys_get_argc()`
2. `sys_get_arg()`
3. `sys_read(0, buf, size)`
4. `sys_write(text)`
5. `sys_exit(status)`

## 5. 验证方式

本轮主要验证：

```text
run grep MiniOS
run grep MiniOS < /readme.txt
run cat /readme.txt | run grep MiniOS
run cat < /readme.txt | run grep MiniOS > /grep.txt
cat /grep.txt
touch /input.txt
writefile /input.txt hello
run grep hello < /input.txt
run cat /input.txt | run grep hello
append /input.txt world
run grep world < /input.txt
```

## 6. 当前限制

1. 暂不支持正则表达式
2. 暂不支持 `-i` / `-n` / `-v`
3. 暂不支持多文件
4. 暂无交互式 tty stdin
5. 不支持直接 `run grep keyword /file`，当前统一通过 stdin 读取
6. 行长度有限制，超长行会报错并丢弃该行
7. 后续可以扩展更多文本工具或 grep 参数
