# python虚拟环境导出包安装到另一台电脑

## 1 当前虚拟环境

### 1.1 进入当前虚拟环境

```
  >source swagger/bin/active
```

### 1.2 输出已安装包名称及版本并记录到requirements.txt文件中

```
  >pip freeze > requirements.txt
```

### 1.3 将安装的包保存到文件夹
```
  >pip download -r requirements.txt -d packages 
```
如有错误提示根据提示处理，如加“--trusted-host mirrors.aliyun.com”

## 2 另一台电脑
### 2.1 新建虚拟环境
```
  >python -m venv swagger
```

### 2.2 将requirements.txt和packages复制到虚拟环境里，激活虚拟环境后安装
```
  >source swagger/bin/active
  >pip install --no-index --find-links=packages -r requirements.txt
```
