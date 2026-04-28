# MiniOS 学习者专属：Linux 文件系统地图（最实用版）

## 一、核心概念

Linux 的所有内容都从根目录开始：

```text
/
```

这叫 **根目录（root directory）**。

注意：

```text
/root
```

这是 **root 用户的家目录**，不是根目录本身。

---

## 二、目录总览

```text
/
├── bin
├── sbin
├── etc
├── home
├── root
├── usr
├── var
├── tmp
├── dev
├── proc
├── lib
└── boot
```

---

## 三、重点目录说明

## 1. /bin

基础命令程序目录。

常见命令：

```bash
ls
cp
mv
rm
cat
echo
bash
pwd
```

输入 `ls` 时，系统可能实际执行：

```text
/bin/ls
```

---

## 2. /sbin

系统管理命令目录。

常见命令：

```bash
reboot
mount
fsck
shutdown
```

---

## 3. /etc

系统配置文件目录。

常见文件：

```text
/etc/passwd
/etc/hosts
/etc/ssh/sshd_config
/etc/fstab
```

---

## 4. /home

普通用户家目录。

例如：

```text
/home/wyh
```

---

## 5. /root

root 超级用户家目录。

你服务器里常见：

```bash
root@server:~#
```

其中 `~` 就是：

```text
/root
```

---

## 6. /usr

大型应用程序、库、头文件目录。

常见内容：

```text
/usr/bin/python3
/usr/bin/g++
/usr/include
/usr/lib
```

---

## 7. /var

经常变化的数据目录。

常见内容：

```text
/var/log
/var/cache
/var/lib
```

---

## 8. /tmp

临时文件目录。

程序运行时缓存文件常放这里。

---

## 9. /dev

设备文件目录。

常见设备：

```text
/dev/null
/dev/tty
/dev/sda
/dev/random
```

Linux 的理念之一：**万物皆文件**。

---

## 10. /proc

内核运行信息虚拟文件系统。

常见命令：

```bash
cat /proc/cpuinfo
cat /proc/meminfo
cat /proc/self/status
```

这些不是磁盘真实文件，而是内核动态生成。

---

## 11. /lib

系统基础动态库目录。

例如：

```text
libc.so
libstdc++.so
```

---

## 12. /boot

系统启动文件目录。

例如：

```text
Linux 内核镜像
GRUB 配置
initramfs
```

---

## 四、PATH 机制（非常重要）

输入：

```bash
ls
```

Shell 为什么知道去哪找？

查看：

```bash
echo $PATH
```

示例：

```text
/usr/local/bin:/usr/bin:/bin
```

Shell 会按顺序查找：

```text
/usr/local/bin/ls
/usr/bin/ls
/bin/ls
```

找到就执行。

---

## 五、为什么默认进入 /root

如果你登录的是 root 用户：

```bash
whoami
```

输出：

```text
root
```

则默认家目录为：

```text
/root
```

如果是普通用户，则通常进入：

```text
/home/用户名
```

---

## 六、与你当前 MiniOS 的关系

你执行：

```bash
run ls
```

可能调用：

```text
/bin/ls
```

你执行：

```bash
g++
```

可能调用：

```text
/usr/bin/g++
```

也就是说，你当前用户态 MiniOS 运行在 Linux 提供的文件系统和工具链之上。

---

## 七、建议立即实践命令

```bash
pwd
whoami
echo $HOME
ls /
which ls
which g++
echo $PATH
ls /bin | head
ls /etc | head
ls /proc | head
```

---

## 八、一句话总结

Linux 根目录 `/` 下的文件夹是按职责划分的系统结构：

- `/bin` 放命令
- `/etc` 放配置
- `/home` 放普通用户
- `/root` 放管理员用户
- `/usr` 放程序与库
- `/proc` 放内核信息
- `/dev` 放设备

理解这张地图后，你的 MiniOS Phase1 就完整了。

