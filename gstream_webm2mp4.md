**收到一个小测试，一个webm文件**

1. 获取文件音视频编码格式
2. 使用gst-lauch命令，将该文件转码为mp4文件
3. c语言实现2

**1.文件编码**

准备在centos虚拟机上完成测试，下载完文件后，直接在文件属性中看到音视频编码格式；

在windows下用vlc打开进一步确认；

**2.查资料了解gstreamer转码**

粗略翻了一下gstreamer[官方文档](https://gstreamer.freedesktop.org/documentation/installing/on-windows.html?gi-language=c)

百度查gstreamer webm，看了一下[视频转webm](https://stackoverflow.com/questions/4649925/convert-video-to-webm-using-gstreamer)

通过该连接中示例：
> gst-launch-0.10 filesrc location=oldfile.ext ! decodebin name=demux ! queue ! ffmpegcolorspace ! vp8enc ! webmmux name=mux ! filesink location=newfile.webm demux. ! queue ! progressreport ! audioconvert ! audioresample ! vorbisenc ! mux.

翻阅mp4相关文档[mp4mux](https://gstreamer.freedesktop.org/documentation/isomp4/mp4mux.html?gi-language=c)
> gst-launch-1.0 gst-launch-1.0 v4l2src num-buffers=50 ! queue ! x264enc ! mp4mux ! filesink location=video.mp4

在app做录像时存为mp4文件，音频编码使用fdk aac，翻阅[fdkaac](https://gstreamer.freedesktop.org/documentation/fdkaac/fdkaacenc.html?gi-language=c#fdkaacenc)相关文档

推测出webm转mp4大概为：
> gst-launch-0.10 filesrc location=oldfile.ext ! decodebin name=demux ! queue ! x264enc ! mp4mux name=mux ! filesink location=newfile.mp4 demux. ! queue ! progressreport ! audioconvert ! audioresample ! fdkaacenc ! mux.

**3.搭环境验证**

在centos7下：
> yum install gstreamer*

安装完毕后，运行脚本验证，提示" no element 'x264enc'"
> gst-inspect-1.0 | grep aac

> gst-inspect-1.0 | grep 264

均未找到音视频相应编码器

安装Ubuntu18.04.1虚拟机，已集成gstreamer，未找到音视频编码器

mac os安装 gstreamer
> brew install gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-libav

安装速度太慢

下载windows安装包，查询
> gst-inspect-1.0.exe x264enc

**找到！**

无fdkaacenc

找aac编码，找到avenc_aac
> gst-launch-1.0.exe filesrc location=e:\\sample.webm ! decodebin name=demux ! queue ! x264enc ! mp4mux name=mux ! filesink location=e:\\new.mp4 demux. ! queue ! progressreport ! audioconvert ! audioresample ! avenc_aac ! mux.

转码成功，播放无音频；播放.webm原始文件，无音频；

找有音频文件验证；

未找到webm文件，尝试将有音频mp4文件转webm，再webm转mp4，进行验证；
> gst-launch-1.0.exe filesrc location=e:\\1.mp4 ! decodebin name=demux ! queue ! vp8enc ! webmmux name=mux ! filesink location=e:\\2.webm demux. ! queue ! progressreport ! audioconvert ! audioresample ! vorbisenc ! mux.

> gst-launch-1.0.exe filesrc location=e:\\2.webm ! decodebin name=demux ! queue ! x264enc ! mp4mux name=mux ! filesink location=e:\\3.mp4 demux. ! queue ! audioconvert ! audioresample ! avenc_aac ! mux.

**搞定！**
---------------------------------------------------
**补充**

`mac os安装完毕`

H.264编码器：x264enc/vtenc_h264/vtenc_h264_hw

aac编码器：faac/avenc_aac

`Ubuntu`
> sudo apt-get install gstreamer1.0-libav

找到avenc_h264_omx / avenc_aac

使用avenc_h264_omx
> gst-launch-1.0 filesrc location=../Desktop/2.webm ! decodebin name=demux ! queue ! avenc_h264_omx ! mp4mux name=mux ! filesink location=1.mp4 demux. ! queue ! audioconvert ! audioresample ! avenc_aac ! mux.

会提示'ERROR:xxx matroskademux0: Internal data stream error.'
> sudo apt-get install gstreamer1.0-plugins-ugly

增加x264enc
> gst-launch-1.0 filesrc location=../Desktop/2.webm ! decodebin name=demux ! queue ! x264enc ! mp4mux name=mux ! filesink location=1.mp4 demux. ! queue ! audioconvert ! audioresample ! avenc_aac ! mux.
*OK*

`CentOS 7`
    
> sudo yum -y install gstreamer1-libav
    
libav无法直接安装; 提示没有可用软件包gstreamer1-libav
> sudo yum -y install http://li.nux.ro/download/nux/dextop/el7/x86_64/nux-dextop-release-0-5.el7.nux.noarch.rpm <br>
> sudo yum -y install libdvdcss gstreamer{,1}-plugins-ugly gstreamer-plugins-bad-nonfree gstreamer1-plugins-bad-freeworld libde265 x265

从输出看,相关插件从0.10.4.x更新至0.10.19-17<br>
x264-libs, x265-libs安装<br>
faac安装
> gst-launch-1.0 filesrc location=sample.webm ! decodebin name=demux ! queue ! x264enc ! mp4mux name=mux ! filesink location=1.mp4 demux. ! queue ! audioconvert ! audioresample ! faac ! mux.

提示错误管道：无组件'faac'<br>
移除音频处理OK
> sudo yum -y install gstreamer1-libav

安装完毕，似乎没有用<br>
...原来是gst-inspect的问题<br>
使用
> gst-inspect-1.0 | grep aac

找到avenc_aac,
> gst-launch-1.0 filesrc location=sample.webm ! decodebin name=demux ! queue ! x264enc ! mp4mux name=mux ! filesink location=1.mp4 demux. ! queue ! audioconvert ! audioresample ! avenc_aac ! mux.

*centos7 ok*
---------------------------------------------------
**C代码实现脚本**
