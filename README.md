# audioplayer

运用了ffmpeg解码输入音频流

将解码后的音频通过swr重采样得到PCM

再利用OpenAL播放PCM音频

本项目运行于macOS平台

需使用homebrew下载好ffmpeg

若下载路径非默认需在Xcode的中修改searchpath到对应的include文件夹

并在framework中添加对于OpenAL框架的链接

使用方法 ./audioplayer+视频文件路径

如 ./audioplayer /Users/mac/Desktop/2.mp4
