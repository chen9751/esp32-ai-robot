# Hello 测试语音

`hello.wav` 是 Hey Buddy 唤醒后的英文语音反馈。将此文件复制到 FAT32 TF 卡的 `audio/hello.wav`（挂载路径：`/sdcard/audio/hello.wav`）。

**格式：** WAV / PCM signed 16-bit / 24000 Hz / stereo，对应目前音频服务的 I2S 输出格式。

源码生成脚本在 `tools/generate_hello.sh`，依赖 Linux 的 espeak + ffmpeg（macOS 可使用 say + ffmpeg）。在已配置 GitHub Actions 写权限的仓库中，工作流可自动生成此文件并提交。启动时找不到文件仅记录日志，不中断唤醒、UI 或锁屏。
