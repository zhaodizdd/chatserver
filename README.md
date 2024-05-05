# chatserver
下载代码后需要配置相关设置
## nginx配置tcp负载均衡
* nginx编译安装需要先安装pcre、openssl、zlib等库，也可以直接编译执行下面的configure命令，根
* 据错误提示信息，安装相应缺少的库。
* 下面的make命令会向系统路径拷贝文件，需要在root用户下执行 
* tony@tony-virtual-machine:~/package/nginx-1.12.2# ./configure --with-stream 
* tony@tony-virtual-machine:~/package/nginx-1.12.2# make && make install
* 编译完成后，默认安装在了/usr/local/nginx目录。
* tony@tony-virtual-machine:~/package/nginx-1.12.2$ cd /usr/local/nginx/ 
* tony@tony-virtual-machine:/usr/local/nginx$ ls 
