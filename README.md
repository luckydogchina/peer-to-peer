# peer-to-peer
这时一个用于实现tcp打洞的工程，用于在处在NAPT规则网关后面的节点进行peer-to-peer通信:
整个工程按照角色划分为：
  + 引导服务器（server）
  + 链接发起方 (sender)
  + 链接接受方 (recver)

流程如下:
  + 初始化：alice与bob链接到hole_server，hole_server获取到双方 外网地址 和 id 并保存;
  + alice 发送list请求，hole_server返回当前在线的peer id;
  + alice 绑定固定端口 alice_hole_port, 发送引导与bob链接的请求到hole_server,同时监听 alice_hole_port的connect请求（此时要保持与hole_server的链接）;
  + hole_server 转发链接请求 + alice的外网地址到bob;
  + bob 绑定固定端口 bob_hole_port,向 alice 的外网映射地址（发送connect请求），如果链接成功，则打洞结束， 否则执行下一步;
  + bob 绑定固定端口 bob_hole_port，向hole_server发送 connect alice的请求，hole_server 如上转发请求（注意，此时bob已经在alice的网关上打了一个洞了）;
  + alice收到connect message后仍然绑定alice_hole_port，向bob 的外网地址发起链接 。
 
 # 注意：
  如果alice和bob处在同一个网关后面无法成功。

