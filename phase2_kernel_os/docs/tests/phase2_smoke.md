# MiniOS Phase2 Smoke Test

这份清单用于每次修改后做一轮最小验证，目标是尽快确认核心链路没有明显损坏。

## 1. 构建

```text
make clean
make
make run
```

预期：

1. 编译通过
2. QEMU 正常启动
3. shell 出现
4. 没有立即 panic

## 2. RAMFS

```text
touch /smoke.txt
writefile /smoke.txt hello
append /smoke.txt world
cat /smoke.txt
```

预期：

1. RAMFS 可创建、写入、追加、读取

## 3. redirect

```text
run grep MiniOS < /readme.txt > /smoke_grep.txt
cat /smoke_grep.txt
```

预期：

1. `<` 和 `>` 正常
2. `grep` 结果能写入文件并再读出

## 4. Shell 多级 pipe

```text
run cat /readme.txt | run grep MiniOS | run wc
```

预期：

1. 多级 pipe 正常
2. 不 panic

## 5. pipe object

```text
run pipe_multi_test
```

预期：

1. 多个 pipe object 正常隔离

## 6. exec argv

```text
run exec_args_test
```

预期：

1. exec 后 argv 传递正常

## 7. pipe close

```text
run pipe_close_test
```

预期：

1. pipe close / EOF 路径正常
