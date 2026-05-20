# SB03 客户示例工程

这个工程是给客户调用静态库的 ESP32-P4 示例。工程本身只包含板级和业务胶水代码：

- ES8311 + I2S 音频采集
- GPIO/TTL 输出
- NVS 通信模式选择
- license 串口写入
- Dreame 日志输出
- RS485 协议

工程不包含模型文件、模型推理实现、TensorFlow Lite Micro resolver 实现、后处理算法实现。算法只通过下面两个文件接入：

```text
components/sb03_snore_prebuilt/lib/libsb03_snore_lib.a
components/sb03_snore_prebuilt/include/sb03_snore_lib.h
```

## 编译

```powershell
cd "E:\claude src\esp32-p4-sb03\examples\sb03_snore_customer_app"
. "C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1" | Out-Null
idf.py build
```

烧录：

```powershell
idf.py -p COM4 flash monitor
```

## 算法接口

应用层只调用两个算法函数：

```c
sb03_snore_verify_license(license);
sb03_snore_infer(samples, sample_count, timestamp_ms, &result);
```

### sb03_snore_verify_license

函数原型：

```c
sb03_snore_status_t sb03_snore_verify_license(const char *license);
```

输入：

- `license`：以 `\0` 结尾的 license 字符串，格式为 `SB03-xxxxxxxxxxxxxxxx`。
- license 需要和当前 ESP32-P4 的 eFuse MAC 绑定。设备 ID 可通过串口命令 `device_id get` 获取。
- 建议在开机后、NVS 初始化完成后调用一次。示例工程会从 NVS namespace `sb03_lic`、key `license` 读取已保存 license 后调用。

输出：

- 函数返回 `SB03_SNORE_OK`：license 验证通过，后续推理无限制，不再消耗试用次数。
- 函数返回错误码：license 无效或参数错误，后续推理进入未激活试用模式。
- 这个函数不返回识别结果，只负责解锁库内部授权状态。

常见返回值：

```c
SB03_SNORE_OK             // 验证成功
SB03_SNORE_ERR_ARG        // license 参数为空
SB03_SNORE_ERR_INIT       // 读取芯片 ID 失败
SB03_SNORE_ERR_NO_LICENSE // license 不匹配
```

### sb03_snore_infer

函数原型：

```c
sb03_snore_status_t sb03_snore_infer(const int16_t *samples,
                                     int sample_count,
                                     uint32_t timestamp_ms,
                                     sb03_snore_result_t *result);
```

输入：

- `samples`：16 kHz、单声道、16-bit PCM 音频数据指针。
- `sample_count`：输入采样点数量。推荐传 `16000`，库内部会按模型输入窗口取前 `15600` 点，不足部分补 0。
- `timestamp_ms`：当前音频帧时间戳，单位 ms，用于 60 秒滑动窗口状态机。
- `result`：输出结构体指针，调用方分配，函数内部写入识别结果。

返回值：

```c
SB03_SNORE_OK                // 推理成功，result 有效
SB03_SNORE_ERR_ARG           // 输入参数错误
SB03_SNORE_ERR_INIT          // 初始化失败，例如内存分配失败
SB03_SNORE_ERR_MODEL         // 模型或 tensor 初始化失败
SB03_SNORE_ERR_INVOKE        // 推理执行失败
SB03_SNORE_ERR_TRIAL_EXPIRED // 未激活且 2000 次试用已用完，result 会被清零
```

输出 `result` 关键字段：

```c
result.snoresig           // 当前帧是否识别为鼾声，1=是，0=否
result.snore_score        // 当前帧鼾声分数，0-100
result.snore_count        // 60 秒窗口内正例累计
result.iferr_count        // 60 秒窗口内干扰扣分累计
result.output_p           // Dreame 核心输出，snore_count - iferr_count，小于等于 1 时清零
result.dreame_snore_index // Dreame 输出强度，等于 output_p * 10
result.audio_volume       // 当前帧声音大小
result.audio_peak         // 当前帧峰值
result.audio_rms          // 当前帧 RMS
result.infer_ms           // 单帧推理耗时，单位 ms
result.licensed           // 当前是否已激活
result.trial_count        // 未激活模式下已经使用的推理次数
result.trial_limit        // 未激活试用上限，当前为 2000
result.top[5]             // 分数最高的 5 个分类 index/score，用于调试
```

调用示例：

```c
sb03_snore_result_t result = {};
sb03_snore_status_t status = sb03_snore_infer(samples, sample_count, timestamp_ms, &result);

if (status == SB03_SNORE_OK) {
    // result 有效，可以更新 TTL、Dreame、RS485 输出
} else if (status == SB03_SNORE_ERR_TRIAL_EXPIRED) {
    // 未激活试用次数用完，result 已清零，不应继续输出有效识别结果
} else {
    // 参数、初始化、模型或推理错误
}
```

未激活时，库允许 2000 次成功推理。每次成功推理后会更新 NVS 里的试用计数。串口日志会显示：

```text
licensed=0 trial=17/2000 remaining=1983
```

激活成功后：

```text
licensed=1
```

## 串口命令

固件 console 支持：

```text
device_id get
license set <SB03-...> reboot
comm_mode get
comm_mode set dreame reboot
comm_mode set rs485 reboot
```

说明：

- `device_id get`：读取当前芯片 eFuse MAC，作为云端生成 license 的设备 ID。
- `license set <SB03-...> reboot`：保存 license 到 NVS，并重启。
- `comm_mode set dreame reboot`：切到默认 Dreame 模式。
- `comm_mode set rs485 reboot`：切到 RS485 模式。

## 日志检查

打开 monitor：

```powershell
idf.py -p COM4 monitor
```

正常运行会看到：

```text
audio_capture: record took ...
snore_engine: **************** DREAME CORE OUTPUT ****************
snore_engine: output_p=... snore_index=... sound_level=...
snore_engine: sn=... raw=... sig=... infer=... licensed=0 trial=... remaining=...
```

重点字段：

- `sound_level`：声音大小。
- `sig=1`：当前 1 秒帧识别为鼾声。
- `output_p`：Dreame 核心输出。
- `snore_index`：Dreame 输出强度。
- `remaining`：未激活剩余推理次数。
