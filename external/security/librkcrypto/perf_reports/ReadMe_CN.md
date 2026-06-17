## 吞吐率测试方法

### CPU定频

-    cpu用户空间设置  ：`echo userspace > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor`
-    查看是否设置成功 ：`cat /sys/devices/system/cpu/cpufreq/policy0/scaling_governor`
-    cpu频率列表      ：`cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies`
-    cpu频率设置      ：`echo 408000 > /sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed`
-    cpu查看当前频率  ：`cat /sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq`

### DDR定频（某些平台不可设置）

- ddr用户空间设置 ：`echo userspace > /sys/class/devfreq/dmc/governor`
- 查看是否关成功  ：`cat /sys/class/devfreq/dmc/governor`
- ddr频率列表     ：`cat /sys/class/devfreq/dmc/available_frequencies`
- ddr当前频率设置 ：`echo 528000000 > /sys/class/devfreq/dmc/userspace/set_freq`
- ddr查看当前频率 ：`cat /sys/class/devfreq/dmc/cur_freq`

### 执行`librkcrypto_test -t=3`即可执行吞吐率测试。

### 测试原理：不断对1M Byte数据执行加密或解密操作，计算1秒内处理的数据量。

## 注意事项

关于**SM4**算法性能的测试说明，如果测试中发现**SM4**性能只有20~30MB/s的情况，该问题是因为kernel没有开启**SM4**软算法或者**CE**算法。
在硬件**CRYPTO**驱动中，为了处理某些硬件无法处理的case，需要分配**SM4**算法进行fallback处理。在kernel未开启**SM4**软算法的情况下，分配fallback的时间框架层会消耗40~50ms的时间，导致性能异常。(其他算法例如**DES/AES**都默认有开启软算法实现。)

这时候请开启`CRYPTO_SM4_GENERIC`配置后，重新编译kernel烧写进行测试。

## 自动化测试
以上步骤可以通过`sh run_perf.sh`自动完成，需要预先将`librkcrypto_test`和`librkcrypto.so`推送到板子上。

测试结果示例：

```shell
root@linaro-alip:/data# sh run_perf.sh
=================== CPU0 (Big Core) Frequency Setup =================
Setting CPU0 (Big Core) governor to userspace...
Available CPU frequencies: 408000 600000 816000 1008000 1200000 1416000 1608000 1800000 2016000
Setting CPU frequency to max: 2016000

======================== DDR Frequency Setup ========================
DDR node found: /sys/class/devfreq/dmc
Setting DDR governor to userspace...
Available DDR frequencies: 528000000 1068000000 1560000000 2112000000
Setting DDR frequency to max: 2112000000

======================== Current Frequencies ========================
CPU0 current frequency: 2016000 KHz
DDR  current frequency: 2112000000 Hz

===================== Current CRYPTO Frequencies ====================
    aclk_crypto_ns                   0       0        0        350000000   0          0     50000      N      crypto@2a400000                 no_connection_id
          hclk_crypto_ns             0       0        0        175500000   0          0     50000      N            crypto@2a400000                 no_connection_id
 clk_pka_crypto_ns                   0       0        0        0           0          0     50000      N   crypto@2a400000                 no_connection_id


=================== Run librkcrypto_test on CPU0 ====================
Executing command: taskset -c 0 librkcrypto_test -t=3
throughput_mode = 3
I rkcrypto: [rk_crypto_init, 398]: rkcrypto api version 1.3.0
dma_fd: otpkey  [AES-256]       ECB     ENCRYPT 719MB/s.
dma_fd: otpkey  [AES-256]       ECB     DECRYPT 719MB/s.
dma_fd: otpkey  [AES-256]       CBC     ENCRYPT 275MB/s.
dma_fd: otpkey  [AES-256]       CBC     DECRYPT 719MB/s.
dma_fd: otpkey  [AES-256]       CTR     ENCRYPT 719MB/s.
dma_fd: otpkey  [AES-256]       CTR     DECRYPT 719MB/s.
dma_fd: otpkey  [SM4-128]       ECB     ENCRYPT 450MB/s.
dma_fd: otpkey  [SM4-128]       ECB     DECRYPT 450MB/s.
dma_fd: otpkey  [SM4-128]       CBC     ENCRYPT 143MB/s.
dma_fd: otpkey  [SM4-128]       CBC     DECRYPT 450MB/s.
dma_fd: otpkey  [SM4-128]       CTR     ENCRYPT 450MB/s.
dma_fd: otpkey  [SM4-128]       CTR     DECRYPT 450MB/s.
dma_fd: test otp_key throughput SUCCESS.

virt:   otpkey  [AES-256]       ECB     ENCRYPT 170MB/s.
virt:   otpkey  [AES-256]       ECB     DECRYPT 171MB/s.
virt:   otpkey  [AES-256]       CBC     ENCRYPT 122MB/s.
virt:   otpkey  [AES-256]       CBC     DECRYPT 170MB/s.
virt:   otpkey  [AES-256]       CTR     ENCRYPT 163MB/s.
virt:   otpkey  [AES-256]       CTR     DECRYPT 163MB/s.
virt:   otpkey  [SM4-128]       ECB     ENCRYPT 146MB/s.
virt:   otpkey  [SM4-128]       ECB     DECRYPT 146MB/s.
virt:   otpkey  [SM4-128]       CBC     ENCRYPT 87MB/s.
virt:   otpkey  [SM4-128]       CBC     DECRYPT 145MB/s.
virt:   otpkey  [SM4-128]       CTR     ENCRYPT 140MB/s.
virt:   otpkey  [SM4-128]       CTR     DECRYPT 140MB/s.
virt:   test otp_key throughput SUCCESS.

I rkcrypto: [rk_crypto_init, 398]: rkcrypto api version 1.3.0
dma_fd: [DES-64]        ECB     ENCRYPT 500MB/s.
dma_fd: [DES-64]        ECB     DECRYPT 500MB/s.
dma_fd: [DES-64]        CBC     ENCRYPT 137MB/s.
dma_fd: [DES-64]        CBC     DECRYPT 500MB/s.
dma_fd: [TDES-192]      ECB     ENCRYPT 187MB/s.
dma_fd: [TDES-192]      ECB     DECRYPT 187MB/s.
dma_fd: [TDES-192]      CBC     ENCRYPT 49MB/s.
dma_fd: [TDES-192]      CBC     DECRYPT 187MB/s.
dma_fd: [AES-256]       ECB     ENCRYPT 1065MB/s.
dma_fd: [AES-256]       ECB     DECRYPT 1066MB/s.
dma_fd: [AES-256]       CBC     ENCRYPT 311MB/s.
dma_fd: [AES-256]       CBC     DECRYPT 1066MB/s.
dma_fd: [AES-256]       CTS     N/A
dma_fd: [AES-256]       CTS     N/A
dma_fd: [AES-256]       CTR     ENCRYPT 815MB/s.
dma_fd: [AES-256]       CTR     DECRYPT 815MB/s.
dma_fd: [SM4-128]       ECB     ENCRYPT 552MB/s.
dma_fd: [SM4-128]       ECB     DECRYPT 552MB/s.
dma_fd: [SM4-128]       CBC     ENCRYPT 152MB/s.
dma_fd: [SM4-128]       CBC     DECRYPT 552MB/s.
dma_fd: [SM4-128]       CTS     N/A
dma_fd: [SM4-128]       CTS     N/A
dma_fd: [SM4-128]       CTR     ENCRYPT 475MB/s.
dma_fd: [SM4-128]       CTR     DECRYPT 476MB/s.
dma_fd: test cipher throughput SUCCESS.

virt:   [DES-64]        ECB     ENCRYPT 279MB/s.
virt:   [DES-64]        ECB     DECRYPT 279MB/s.
virt:   [DES-64]        CBC     ENCRYPT 108MB/s.
virt:   [DES-64]        CBC     DECRYPT 279MB/s.
virt:   [TDES-192]      ECB     ENCRYPT 141MB/s.
virt:   [TDES-192]      ECB     DECRYPT 141MB/s.
virt:   [TDES-192]      CBC     ENCRYPT 44MB/s.
virt:   [TDES-192]      CBC     DECRYPT 142MB/s.
virt:   [AES-256]       ECB     ENCRYPT 391MB/s.
virt:   [AES-256]       ECB     DECRYPT 391MB/s.
virt:   [AES-256]       CBC     ENCRYPT 202MB/s.
virt:   [AES-256]       CBC     DECRYPT 391MB/s.
virt:   [AES-256]       CTS     N/A
virt:   [AES-256]       CTS     N/A
virt:   [AES-256]       CTR     ENCRYPT 350MB/s.
virt:   [AES-256]       CTR     DECRYPT 351MB/s.
virt:   [SM4-128]       ECB     ENCRYPT 295MB/s.
virt:   [SM4-128]       ECB     DECRYPT 294MB/s.
virt:   [SM4-128]       CBC     ENCRYPT 118MB/s.
virt:   [SM4-128]       CBC     DECRYPT 294MB/s.
virt:   [SM4-128]       CTS     N/A
virt:   [SM4-128]       CTS     N/A
virt:   [SM4-128]       CTR     ENCRYPT 271MB/s.
virt:   [SM4-128]       CTR     DECRYPT 271MB/s.
virt:   test cipher throughput SUCCESS.

I rkcrypto: [rk_crypto_init, 398]: rkcrypto api version 1.3.0
dma_fd: [AES-256]       GCM     ENCRYPT 310MB/s.
dma_fd: [AES-256]       GCM     DECRYPT 310MB/s.
dma_fd: [SM4-128]       GCM     ENCRYPT 152MB/s.
dma_fd: [SM4-128]       GCM     DECRYPT 152MB/s.
dma_fd: test aead throughput SUCCESS.

virt:   [AES-256]       GCM     N/A
virt:   [SM4-128]       GCM     N/A
virt:   test aead throughput SUCCESS.

I rkcrypto: [rk_crypto_init, 398]: rkcrypto api version 1.3.0
virt:   [         MD5]  248MB/s.
virt:   [        SHA1]  208MB/s.
virt:   [      SHA256]  250MB/s.
virt:   [      SHA224]  247MB/s.
virt:   [      SHA512]  359MB/s.
virt:   [      SHA384]  359MB/s.
virt:   [  SHA512_224]  N/A
virt:   [  SHA512_256]  N/A
virt:   [         SM3]  248MB/s.
virt:   test hash throughput SUCCESS.

dma_fd: [         MD5]  315MB/s.
dma_fd: [        SHA1]  256MB/s.
dma_fd: [      SHA256]  315MB/s.
dma_fd: [      SHA224]  315MB/s.
dma_fd: [      SHA512]  493MB/s.
dma_fd: [      SHA384]  493MB/s.
dma_fd: [  SHA512_224]  N/A
dma_fd: [  SHA512_256]  N/A
dma_fd: [         SM3]  315MB/s.
dma_fd: test hash throughput SUCCESS.

virt:   [    HMAC_MD5]  248MB/s.
virt:   [   HMAC_SHA1]  208MB/s.
virt:   [ HMAC_SHA256]  249MB/s.
virt:   [ HMAC_SHA512]  359MB/s.
virt:   [    HMAC_SM3]  249MB/s.
virt:   test hmac throughput SUCCESS.

dma_fd: [    HMAC_MD5]  315MB/s.
dma_fd: [   HMAC_SHA1]  256MB/s.
dma_fd: [ HMAC_SHA256]  315MB/s.
dma_fd: [ HMAC_SHA512]  493MB/s.
dma_fd: [    HMAC_SM3]  315MB/s.
dma_fd: test hmac throughput SUCCESS.

I rkcrypto: [rk_crypto_init, 398]: rkcrypto api version 1.3.0
virt:   [RSA-1024]      PRIV    ENCRYPT 25ms.
virt:   [RSA-1024]      PUB     DECRYPT 5ms.
virt:   [RSA-2048]      PRIV    ENCRYPT 95ms.
virt:   [RSA-2048]      PUB     DECRYPT 16ms.
virt:   [RSA-3072]      PRIV    ENCRYPT 312ms.
virt:   [RSA-3072]      PUB     DECRYPT 53ms.
virt:   [RSA-4096]      PRIV    ENCRYPT 682ms.
virt:   [RSA-4096]      PUB     DECRYPT 85ms.
test rsa throughput SUCCESS.

Test throughput SUCCESS.

######## Test done ########

============================== Done =================================
root@linaro-alip:/data#
```

