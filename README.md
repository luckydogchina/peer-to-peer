# peer-to-peer
这时一个用于实现p2p 通讯的工程，用于在处在NAPT规则网关后面的节点进行peer-to-peer通信:
整个工程按照角色划分为：
  + 打洞服务器（hole_server）
  + 链接发起方 (alice)
  + 链接接受方 (bob)

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
 # 进展
   目前在实现tcp打洞技术，但由于tcp限制颇多，后续会拓展udp打洞技术：
   + setup1 tcp 打洞
      + 1.1 明文传输
      + 1.2 拓展tls协议
      + 1.3 用户注册管理
   + setup2 udp 打洞
      + 暂无
