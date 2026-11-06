# CPU render portable test

`cpu_render_test` includes the production CPU raster, render wrapper, and DRM overlay backend. By default, DRM API calls use controlled test implementations; no real DRM device is opened and no hardware plane is changed.

## Coverage

The module runs three iterations of:

- clear
- line
- outline and filled rectangles
- outline and filled circles
- text rendering

It uses two guarded 640x640 vmalloc buffers. Transaction checks cover rejected candidates, fresh state after preflight checks, centered geometry on a 1264x2780 display, property-based z-order and 0..255 alpha, buffer flips, nested frame rejection, flush/commit/detach failures, modeset lock retries, inactive or undersized displays, occupied planes, and cleanup after another client takes the plane. Expected injected failures produce `lsdriver: cpu_render` error logs before the final pass marker.

The SDE fixture also covers idle physical planes labeled Primary, explicit `blend_op=premultiplied`, and exclusion of virtual planes with `multirect_mode` that share a hardware pipe.

These tests execute production selection and frame lifecycle code. They do not validate real DRM allocation, GEM mapping, locks, cache coherency, fences, SDE resource limits, or visible output.

## Build

Build the module with the target kernel's normal external-module command. For example, from the kernel build environment:

```sh
make -C <kernel-build-dir> -f <kernel-source>/Makefile \
    ARCH=arm64 LLVM=1 LLVM_IAS=1 \
    KBUILD_EXTRA_SYMBOLS=<kernel-build-dir>/Module.symvers \
    M=<workspace>/lsdriver/arm64_tests/cpu_render_test modules
```

The output module is `cpu_render_test.ko`.

## Real Android device

Build for the target kernel and load the module with the device's normal module-loading method. Tests run synchronously during module initialization and return success or an error; unload after completion. The same simulated DRM calls are used on Android, so this is not a hardware display test.

For an explicitly authorized display test, `insmod cpu_render_test.ko hardware=1` uses the actual backend to draw the centered green rectangle and text for five seconds, then detaches the plane and releases its buffers. It does not include the main driver's unrelated initialization. Inspect `hardware submissions=... ret=0 detached=1` in the kernel log and observe the display; a successful submission alone does not prove visibility. Unload with `rmmod cpu_render_test` after successful detach. If detach fails, the module pins itself until reboot to prevent unloading live resources; do not force-unload it. HWC may take the plane or reject resource allocation during the test.

`drm_probe.c` is a separate read-only userspace query for plane occupancy, formats, and SDE properties. Build with the Android NDK using the target kernel's `include/uapi` as an `-idirafter` path. It requires access to `/dev/dri/card0` and does not allocate framebuffers or submit display state.

## QEMU

`run_qemu.sh` is only an optional QEMU execution adapter. It builds the small init helper, boots the prepared test kernel, loads `cpu_render_test.ko`, waits for the result, and unloads the module.

Expected markers include:

```text
cpu_render_test: PASS iterations=3
CPU_RENDER_MODULE_LOAD_OK
CPU_RENDER_MODULE_UNLOAD_OK
CPU_RENDER_TEST_PASS
```

Real DRM/MSM allocation and display submission still require a separate Android device test. Rebuild the test module before running; checked-in binary or log files may describe an older implementation.
