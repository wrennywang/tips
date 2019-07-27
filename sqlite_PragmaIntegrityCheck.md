# sqlite数据库运行一段时间后异常 

* **现象:**  
存储服务器启不来，跟踪代码发现启动时校验数据库文件,  
调用"PRAGMA integrity_check"  
执行获取返回"row 9857 missing from index sqlite_autoindex_tb_store_log_20190723_1"  

* **解决办法:**  
   1. 下载[sqlite工具](https://www.sqlite.org/2019/sqlite-tools-win32-x86-3290000.zip)  
   2. 将数据导出  
   ```
   >sqlite3.exe xxx.db .dump > new.sql
   ```
   3. 导入新库  
   ```
   >sqlite3.exe new.db < new.sql
   ```
   4. 校验
   ```
   >sqlite3.exe new.db "pragma integrity_check"
   ```
   返回'ok'，处理完毕
