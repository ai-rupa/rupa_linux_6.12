# NOTE:
#   Do NOT rely on shebang.
#   Android does not provide /bin/sh; use POSIX sh and
#   invoke this script explicitly with "sh".

CPU_POLICY_PATH="/sys/devices/system/cpu/cpufreq/policy0"
DDR_PATH="/sys/class/devfreq/dmc"
RK_TEST_BIN="librkcrypto_test -t=3"

echo "=================== CPU0 (Big Core) Frequency Setup ================="

# Step 1: set CPU0 governor
if [ -w "${CPU_POLICY_PATH}/scaling_governor" ]; then
    echo "Setting CPU0 (Big Core) governor to userspace..."
    echo userspace > "${CPU_POLICY_PATH}/scaling_governor"
else
    echo "Error: CPU0 (Big Core) governor path not found or not writable!"
    exit 1
fi

# Step 2: set CPU0 to max frequencies
if [ -f "${CPU_POLICY_PATH}/scaling_available_frequencies" ]; then
    freq_list=$(cat "${CPU_POLICY_PATH}/scaling_available_frequencies")
    max_freq=$(echo $freq_list | tr ' ' '\n' | sort -n | tail -1)
    echo "Available CPU frequencies: $freq_list"
    echo "Setting CPU frequency to max: $max_freq"
    echo $max_freq > "${CPU_POLICY_PATH}/scaling_setspeed"
else
    echo "Error: Cannot read available CPU frequencies!"
    exit 1
fi

# Step 3: set DDR to max frequencies
echo ""
echo "======================== DDR Frequency Setup ========================"
if [ -d "${DDR_PATH}" ]; then
    echo "DDR node found: ${DDR_PATH}"
    if [ -w "${DDR_PATH}/governor" ]; then
        echo "Setting DDR governor to userspace..."
        echo userspace > "${DDR_PATH}/governor"
    fi

    if [ -f "${DDR_PATH}/available_frequencies" ]; then
        ddr_freq_list=$(cat "${DDR_PATH}/available_frequencies")
        ddr_max_freq=$(echo $ddr_freq_list | tr ' ' '\n' | sort -n | tail -1)
        echo "Available DDR frequencies: $ddr_freq_list"
        echo "Setting DDR frequency to max: $ddr_max_freq"
        if [ -w "${DDR_PATH}/userspace/set_freq" ]; then
            echo $ddr_max_freq > "${DDR_PATH}/userspace/set_freq"
        else
            echo "Warning: DDR userspace/set_freq not writable!"
        fi
    else
        echo "Warning: DDR available_frequencies not found!"
    fi
else
    echo "DDR devfreq not supported on this platform."
fi

sleep 0.2

# Step 4: dump current frequencies
echo ""
echo "======================== Current Frequencies ========================"
if [ -f "${CPU_POLICY_PATH}/scaling_cur_freq" ]; then
    cur_cpu=$(cat "${CPU_POLICY_PATH}/scaling_cur_freq")
    echo "CPU0 current frequency: ${cur_cpu} KHz"
else
    echo "CPU0 current frequency: N/A"
fi

if [ -f "${DDR_PATH}/cur_freq" ]; then
    cur_ddr=$(cat "${DDR_PATH}/cur_freq")
    echo "DDR  current frequency: ${cur_ddr} Hz"
else
    # 修复：文件名跟在 awk 后面，不换行，命令完整闭合
    freq=$(awk '/^[[:space:]]*scmi_clk_ddr/ {print $5; exit}
               /^[[:space:]]*sclk_ddr/ {print $5; exit}
               /^[[:space:]]*clk_ddrphy/ {print $5; exit}' /sys/kernel/debug/clk/clk_summary 2>/dev/null)
    
    [ -n "$freq" ] && echo "DDR current frequency: ${freq} Hz" || echo "DDR current frequency: N/A"
fi

# Step 5: dump crypto module frequencies
echo ""
echo "===================== Current CRYPTO Frequencies ===================="
cat /sys/kernel/debug/clk/clk_summary | grep crypto
echo ""
echo ""

# Step 6: run perf test
echo "=================== Run librkcrypto_test on CPU0 ===================="
if ! command -v ${RK_TEST_BIN%% *} >/dev/null 2>&1; then
    echo "Error: '$RK_TEST_BIN' command not found!"
else
    if command -v taskset >/dev/null 2>&1; then
        cmd="taskset 1 $RK_TEST_BIN"
        echo "Executing command: $cmd"
        $cmd
    else
        echo "Warning: taskset not found, running without CPU binding..."
        echo "Executing command: $RK_TEST_BIN"
        $RK_TEST_BIN
    fi
fi

echo ""
echo "============================== Done ================================="
