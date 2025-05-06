# 程序介绍
把图片风格化，c++实现
图片处理部分用了这个库https://github.com/nothings/stb/tree/master

# 编译
在MSYS2 MinGW64中
1安装libharu库，用于生成 PDF 文件
pacman -S mingw-w64-x86_64-libharu

2安装安装 inih，用于配置文件
pacman -S mingw-w64-x86_64-libinih

3进行编译
g++ ascii_to_image.cpp -o ascii_to_image -std=c++23 -lstdc++fs -linih -O3 -flto -pthread
g++ ascii_to_pdf.cpp -o ascii_to_pdf -std=c++23 -lstdc++fs -linih -lhpdf -O3 -flto -pthread

# 程序输出格式
可以输出png图片
可以输出pdf,但是如果设置字符宽度过大，会导致超出pdf最大的限制，导致pdf无法创建

# 图片示例
![4](https://github.com/user-attachments/assets/72ce2168-044b-41ff-9d16-304f636d8115)
![4_BlackOnWhite](https://github.com/user-attachments/assets/17ac514f-c82b-4ad1-beb1-5325d8ee940b)



