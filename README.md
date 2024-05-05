# chatserver
下载代码后需要配置相关设置
## nginx配置tcp负载均衡
* nginx编译安装需要先安装pcre、openssl、zlib等库，也可以直接编译执行下面的configure命令，根
* 据错误提示信息，安装相应缺少的库。
* 下面的make命令会向系统路径拷贝文件，需要在root用户下执行 
* zd@zd-virtual-machine:~/package/nginx-1.12.2# ./configure --with-stream 
* zd@zd-virtual-machine:~/package/nginx-1.12.2# make && make install
* 编译完成后，默认安装在了/usr/local/nginx目录。
* zd@zd-virtual-machine:~/package/nginx-1.12.2$ cd /usr/local/nginx/ 
* zd@zd-virtual-machine:/usr/local/nginx$ ls 
conf  html  logs  sbin
可执行文件在sbin目录里面，配置文件在conf目录里面。 
nginx -s reload   重新加载配置文件启动 
nginx -s stop  停止nginx服务
## redis环境安装和配置
* sudo apt-get install redis-server   # ubuntu命令安装redis服务
* 下载C++对应的hiredis
* git clone https://github.com/redis/hiredis
* cd hiredis、
* make、
* sudo make install、
  ```
  zd@zd-virtual-machine:~/github/hiredis$ sudo make install
  [sudo] zd 的密码： 
  mkdir -p /usr/local/include/hiredis /usr/local/include/hiredis/adapters 
  /usr/local/lib
  cp -pPR hiredis.h async.h read.h sds.h sslio.h /usr/local/include/hiredis
  cp -pPR adapters/*.h /usr/local/include/hiredis/adapters
  cp -pPR libhiredis.so /usr/local/lib/libhiredis.so.0.14
  cd /usr/local/lib && ln -sf libhiredis.so.0.14 libhiredis.so
  cp -pPR libhiredis.a /usr/local/lib
  mkdir -p /usr/local/lib/pkgconfig
  cp -pPR hiredis.pc /usr/local/lib/pkgconfig
  ```
* 拷贝生成的动态库到/usr/local/lib目录下：sudo ldconfig /usr/local/lib
## MySQL数据库配置修改etc中的mysql.cof
## 编译和运行
### 编译
* ./autobuild.sh
### 运行
* cd bin
* ./ChatServer 192.168.117.132 6000 （自己主机的ip地址，和绑定的端口号）
## 整体结构
* 头文件在include文件夹中，.cpp文件在src中
* dbPool是实现了一个MySQL数据库连接池
* model是封装了数据库中各个表的实际操作
* net是封装了查看muduo库实现的一个网络库
* redis封装了redis的连接、订阅发布的基本操作
* msg.h 是聊天协议的约定
# 客户端在Windows环境下利用qt编写。
* 地址： 
