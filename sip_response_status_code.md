# SIP消息状态码  
阅读osip代码，看到  
MSG_IS_STATUS_1XX(msg)  
MSG_IS_STATUS_2XX(msg)  
MSG_IS_STATUS_3XX(msg)  
MSG_IS_STATUS_4XX(msg)  
MSG_IS_STATUS_5XX(msg)  
MSG_IS_STATUS_6XX(msg)  

翻rfc文档了解了一下，见[rfc3261](https://tools.ietf.org/html/rfc3261) 7.2 Responses  
## 1.状态码为3位整数  
## 2.第一位整数代表类型  
## 3.分6大类  
  ### 3.1 1xx: 临时应答，收到请求，继续处理的请求  
  ### 3.2 2xx: 成功
  ### 3.3 3xx: 重定向
  ### 3.4 4xx: 客户端错误
  ### 3.5 5xx: 服务端错误
  ### 3.6 6xx: 全局性错误
