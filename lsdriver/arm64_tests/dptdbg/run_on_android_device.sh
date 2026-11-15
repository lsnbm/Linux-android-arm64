#!/system/bin/sh

set -eu

cd "$(dirname "$0")"
MODULE_NAME=arm64_dptdbg_test_module
test -f "$MODULE_NAME.ko"
test -x ./dptdbg_runner
test "$(id -u)" = 0
test "$(getconf PAGE_SIZE)" = 4096

if grep -q "^$MODULE_NAME " /proc/modules; then
    echo DPTDBG_TEST_MODULE_ALREADY_LOADED
    exit 1
fi

cleanup()
{
    status=$?
    cleanup_status=0
    if grep -q "^$MODULE_NAME " /proc/modules; then
        rmmod "$MODULE_NAME" || cleanup_status=$?
    fi
    echo "cleanup_status=$cleanup_status"
    chmod 0644 runner.log kernel.log 2>/dev/null || true
    if test "$cleanup_status" -ne 0; then
        status=$cleanup_status
    fi
    exit "$status"
}
trap cleanup EXIT

uname -a
boot_id=$(cat /proc/sys/kernel/random/boot_id)
taint_before=$(cat /proc/sys/kernel/tainted)
marker="DPTDBG_DEVICE_BEGIN_$$"
printf '<6>%s\n' "$marker" > /dev/kmsg

set +e
timeout -s KILL 60 ./dptdbg_runner > runner.log 2>&1
runner_status=$?
set -e

cat runner.log
echo "runner_status=$runner_status"
dmesg | sed -n "/$marker/,\$p" > kernel.log
grep -E 'DPTDBG_|WARNING:|BUG:|Internal error:|Kernel panic|Call trace:' kernel.log || true
echo "taint_before=$taint_before taint_after=$(cat /proc/sys/kernel/tainted)"

test "$runner_status" = 0
test "$(cat /proc/sys/kernel/random/boot_id)" = "$boot_id"
grep -q '^DPTDBG_TEST_LOAD_OK$' runner.log
grep -q '^DPTDBG_TEST_RESULT_OK$' runner.log
grep -q '^DPTDBG_TEST_UNLOAD_OK$' runner.log
grep -q '^DPTDBG_TEST_PASS$' runner.log
grep -q "$marker" kernel.log
grep -q 'DPTDBG_TEST_MONITOR_STARTED' kernel.log
grep -q 'DPTDBG_TEST_FUNCTION_PASS hits=6$' kernel.log
grep -q 'DPTDBG_TEST_UNLOAD_OK' kernel.log
if grep -Eq 'DPTDBG_TEST_.*FAILED|WARNING:|BUG:|Internal error:|Kernel panic|Call trace:' kernel.log; then
    echo DPTDBG_TEST_KERNEL_FAILED
    exit 1
fi
if grep -q "^$MODULE_NAME " /proc/modules; then
    echo DPTDBG_TEST_MODULE_STILL_LOADED
    exit 1
fi

echo DPTDBG_DEVICE_TEST_PASS