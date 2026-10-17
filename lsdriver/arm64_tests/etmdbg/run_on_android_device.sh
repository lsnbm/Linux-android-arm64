#!/system/bin/sh

MODULE=/data/local/tmp/arm64_etmdbg_test_module.ko
MODULE_NAME=arm64_etmdbg_test_module
DEVICE=/dev/arm64_etmdbg_test
TRACE=/data/local/tmp/arm64_etmdbg_trace.bin

cleanup_status=0
cleanup()
{
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
rm -f "$DEVICE" "$TRACE" || exit $?
test -f "$MODULE" || {
    echo "ETMDBG_TEST_MISSING_MODULE"
    exit 1
}

insmod "$MODULE" || {
    status=$?
    echo "ETMDBG_TEST_MODULE_LOAD_FAILED status=$status"
    dmesg | tail -40
    exit "$status"
}

device_wait=0
while test ! -c "$DEVICE" && test "$device_wait" -lt 50; do
    sleep 0.1
    device_wait=$((device_wait + 1))
done
test -c "$DEVICE" || {
    echo "ETMDBG_TEST_DEVICE_MISSING"
    exit 1
}

cat "$DEVICE" > "$TRACE" || {
    status=$?
    echo "ETMDBG_TEST_TRACE_READ_FAILED status=$status"
    exit "$status"
}

trace_bytes=$(wc -c < "$TRACE")
trace_hash=$(sha256sum "$TRACE" | cut -d ' ' -f 1)
echo "ETMDBG_TEST_TRACE_FILE=$TRACE bytes=$trace_bytes sha256=$trace_hash"
test "$trace_bytes" -gt 72 || {
    echo "ETMDBG_TEST_TRACE_TOO_SHORT bytes=$trace_bytes"
    exit 1
}