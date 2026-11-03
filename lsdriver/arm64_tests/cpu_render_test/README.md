# CPU render portable test

`cpu_render_test` is an isolated kernel-module test for the CPU raster and render wrapper code. The test core is shared by real Android devices and QEMU; only the loader or platform command line differs.

## Coverage

The module runs three iterations of:

- clear
- line
- outline and filled rectangles
- outline and filled circles
- text rendering

It uses a guarded 640x640 vmalloc buffer and a fake render backend. This validates the CPU raster algorithms and wrapper behavior without requiring DRM master ownership or a real display framebuffer.

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

Push the module to the device and load it with the device's normal module-loading method. The module logs a pass or failure result and then exits its test thread; it does not require a QEMU-specific runtime.

## QEMU

`run_qemu.sh` is only an optional QEMU execution adapter. It builds the small init helper, boots the prepared test kernel, loads `cpu_render_test.ko`, waits for the result, and unloads the module.

Expected markers include:

```text
cpu_render_test: PASS iterations=3
CPU_RENDER_MODULE_LOAD_OK
CPU_RENDER_MODULE_UNLOAD_OK
CPU_RENDER_TEST_PASS
```

The fake backend intentionally does not validate the real DRM/MSM framebuffer mapping path. That path still requires a separate Android device test.
