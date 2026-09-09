#!/system/bin/sh

MODULE=/data/local/tmp/arm64_kernel_executor_test_module.ko
MODULE_NAME=arm64_kernel_executor_test_module
RUNNER=/data/local/tmp/executor_test_runner
INSTRUCTIONS=/data/local/tmp/instruction.txt
DEVICE=/dev/arm64_executor_test

cleanup()
{
    cleanup_status=0
    rm -f "$DEVICE" || cleanup_status=$?
    if grep -q "^$MODULE_NAME " /proc/modules; then
        rmmod "$MODULE_NAME" 2>/dev/null || cleanup_status=$?
    fi
    echo "cleanup_status=$cleanup_status"
}

trap cleanup EXIT

if grep -q "^$MODULE_NAME " /proc/modules; then
    rmmod "$MODULE_NAME" || exit $?
fi
rm -f "$DEVICE" || exit $?

test -f "$MODULE" || {
    echo "missing module: $MODULE"
    exit 1
}
test -f "$RUNNER" || {
    echo "missing runner: $RUNNER"
    exit 1
}
chmod 0755 "$RUNNER" || {
    status=$?
    echo "runner chmod failed: $RUNNER status=$status"
    exit "$status"
}
test -f "$INSTRUCTIONS" || {
    echo "missing instructions: $INSTRUCTIONS"
    exit 1
}

id
insmod "$MODULE" || {
    status=$?
    echo "module load failed: status=$status"
    dmesg | tail -40
    exit "$status"
}

device_wait=0
while test ! -c "$DEVICE" && test "$device_wait" -lt 50; do
    sleep 0.1
    device_wait=$((device_wait + 1))
done
test -c "$DEVICE" || {
    echo "kernel device was not created: $DEVICE"
    exit 1
}
chmod 0600 "$DEVICE"
"$RUNNER" "$INSTRUCTIONS" "$DEVICE"
runner_status=$?
echo "runner_status=$runner_status"
exit "$runner_status"