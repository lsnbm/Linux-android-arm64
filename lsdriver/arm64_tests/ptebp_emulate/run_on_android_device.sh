#!/system/bin/sh

MODULE=/data/local/tmp/ptebp_emulate_test_module.ko
MODULE_NAME=ptebp_emulate_test_module
RUNNER=/data/local/tmp/ptebp_emulate_test_runner
INSTRUCTIONS=/data/local/tmp/ptebp_emulate_instruction.txt
DEVICE=/proc/ptebp_emulate_test

cleanup_status=0
cleanup()
{
    if grep -q "^$MODULE_NAME " /proc/modules; then
        rmmod "$MODULE_NAME" || cleanup_status=$?
    fi
    rm -f "$MODULE" "$RUNNER" "$INSTRUCTIONS"
    echo "cleanup_status=$cleanup_status"
}
trap cleanup EXIT

if grep -q "^$MODULE_NAME " /proc/modules; then
    rmmod "$MODULE_NAME" || exit $?
fi
test -f "$MODULE" || { echo "missing module: $MODULE"; exit 1; }
test -f "$RUNNER" || { echo "missing runner: $RUNNER"; exit 1; }
test -f "$INSTRUCTIONS" || { echo "missing instructions: $INSTRUCTIONS"; exit 1; }
chmod 0755 "$RUNNER" || exit $?

insmod "$MODULE" || {
    status=$?
    echo "module load failed: status=$status"
    dmesg | tail -60
    exit "$status"
}

device_wait=0
while test ! -e "$DEVICE" && test "$device_wait" -lt 50; do
    sleep 0.1
    device_wait=$((device_wait + 1))
done
test -e "$DEVICE" || {
    echo "kernel proc entry was not created: $DEVICE"
    exit 1
}
chmod 0600 "$DEVICE"
"$RUNNER" "$INSTRUCTIONS" "$DEVICE"
runner_status=$?
echo "runner_status=$runner_status"
exit "$runner_status"
