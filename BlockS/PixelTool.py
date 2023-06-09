import os
from PIL import Image

f = open("PixelS.h","w")   #打开文件
f.write("#pragma once\n\n")

TextureNumber = 0
path = './Pixel图片/'
for file_name in os.listdir(path): # 第一次读取文件夹下的文件 获得图片数量，并且 把 图片对应的索引给 #define 定义好
    f.write("#define	" + file_name[:-4] + " " + str(TextureNumber) + "\n")
    TextureNumber += 1


f.write("\nconst unsigned int TextureNumber = " + str(TextureNumber) + ";\n\nconst unsigned char pixelS[" + str(TextureNumber) + "][1024]{\n")

shu = 0
for file_name in os.listdir(path): # 获得 path 路径下的所有图片 并且逐一循环
    print(file_name)
    im = Image.open(path + file_name) # 打开 file_name
    f.write("   {\n   ")
    # 图片 像素点提取
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            pix = im.getpixel((x, y))
            f.write(str(pix[0]) + ", ")
            f.write(str(pix[1]) + ", ")
            f.write(str(pix[2]) + ", ")
            f.write(str(255) + ", ")
            shu += 4
            if shu == 16: # 横向 最多 16 个
                f.write("\n")
                f.write("   ")
                shu = 0
    f.write("},\n")
    im.close()

f.write("};\n")
f.close()


'''
cd C:/Users/wuxingwushu/Desktop/CppPixel                        # 再这个文件夹下创建 virtualenv
virtualenv testvenv                                             # 创建 virtualenv
cd C:/Users/wuxingwushu/Desktop/CppPixel/testvenv/Scripts       # 来到这个文件路径下 可以打开 ./activate
./activate                                                      # 启动 activate

pip list                                                        # 查看 python 虚拟环境

pip install Image                                               # 安装 需要的 Image库

python ./PixelTool.py                                           # 尝试运行

这个没有黑窗口
pyinstaller -c -Fw -- ./PixelTool.py                            # 打包成为 EXE
'''