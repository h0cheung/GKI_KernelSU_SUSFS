#!/system/bin/sh
# xpad LED init script - 每 50ms 设置所有 xpad LED
# 用法: sh xpad_led_init.sh
# 按 Ctrl+C 停止

echo "Starting xpad LED init loop (50ms interval)..."
echo "Press Ctrl+C to stop"

while true; do
    # 找到所有 xpad LED 设备并设置 brightness
    for led in /sys/class/leds/xpad*; do
        if [ -d "$led" ] && [ -f "$led/brightness" ]; then
            # 设置 LED 值为 2 (对应 01 03 02 包)
            echo 2 > "$led/brightness" 2>/dev/null && \
                echo "$(date '+%H:%M:%S.%3N') Set $led to 2"
        fi
    done
    
    # 等待 50ms (0.05秒)
    # 安卓的 sleep 可能不支持小数，用 usleep 或其他方式
    if command -v usleep >/dev/null 2>&1; then
        usleep 50000
    else
        # fallback: 用 busybox 或者最小 sleep
        sleep 0.05 2>/dev/null || sleep 1
    fi
done
