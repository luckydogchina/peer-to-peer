# peer-to-peer
这时一个用于实现tcp打洞的工程，用于在处在NAPT规则网关后面的节点进行peer-to-peer通信:
整个工程按照角色划分为：
  + 引导服务器（server）
  + 链接发起方 (sender)
  + 链接接受方 (recver)

流程如下:
（1） 初始化：sender与recver链接到server，server获取到双方 外网地址 和 id 并保存;
（2）sender 发送list请求，server返回当前在线的peer id;
（3）sender 发送链接请求 + recver id 到server;
（4）server 转发链接请求 + sender id 到 recver;
（5）recver 向 sender 9798端口打洞（发送connect请求），启动本地 9798端口监听服务，发送 ok 通知 + sender id ;
（6）server 转发 ok 通知 + recver id 到 sender;
（7）sender 向 recver 9798 端口 发送connect请求， 如果正常将链接成功，如果不正常将启动 本地 9798 端口监听服务 ，发送 ok 通知 + recver id 到 server，跳转到第（5）步（此时双方身份互换）。

