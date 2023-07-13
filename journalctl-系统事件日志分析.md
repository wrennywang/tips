# 1. journalctl简介
journalctl，主要作用，管理系统的事件日志记录
可以查看所有的系统日志文件，提供了各种参数帮助用户更快定位到日志信息。
# 2. 参数
## 2.1 反序输出
```
journalctl -r
```
## 2.2 跟踪日志文件
```
journalctl -f 
```
## 2.3 显示指定时间的事件日志
```
// 时间段
journalctl --since "2023-07-12 15:00:00" --until "2023-07-12 18:00:00"
// 一段时间之前
journalctl --since 1 hour ago
```

## 2.4 指定服务日志
```
// 查看web服务的日志
journalctl -u httpd.service
// 多个服务
journalctl -u httpd.service -u crond.service
```

## 2.5 系统日志
```
// 命令 “journalctl -k” 和 “journalctl --dmesg” 用来显示系统的内核日志信息。
```

## 2.6 组合
```
journalctl -u docker.socket -u systemd-logind.service --since '2023-06-25' --until '2023-06-26'
```
