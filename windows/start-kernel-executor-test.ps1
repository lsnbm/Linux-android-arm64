[CmdletBinding()]
param(
    [string]$KernelPath = 'D:\QEMU\acceptance\kernel-6.1-Android14-script\Image',
    [string]$InitrdPath = 'E:\1.CodeRepository\Android\Kernel\lsdriver\arm64_tests\executor\executor-test-initramfs.cpio.gz',
    [string]$CpuModel = 'max'
)

$ErrorActionPreference = 'Stop'

$qemuPath = 'D:\QEMU\qemu-modern\qemu-system-aarch64.exe'
$logPath = 'D:\QEMU\executor-test-serial.log'
$stdoutPath = 'D:\QEMU\executor-test-host-stdout.log'
$stderrPath = 'D:\QEMU\executor-test-host-stderr.log'
$requiredPaths = @($qemuPath, $KernelPath, $InitrdPath)
$missingPaths = @($requiredPaths | Where-Object { -not (Test-Path -LiteralPath $_) })
if ($missingPaths.Count -gt 0) {
    throw "Required test files are missing:`n$($missingPaths -join "`n")"
}

$runningQemu = @(Get-Process -Name 'qemu-system-aarch64' -ErrorAction SilentlyContinue)
if ($runningQemu.Count -gt 0) {
    throw "QEMU is already running (PID $($runningQemu.Id -join ', '))."
}

Remove-Item -LiteralPath $logPath, $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
$arguments = @(
    '-name', '"ARM64 executor kernel test"',
    '-machine', 'virt,gic-version=2',
    '-cpu', $CpuModel,
    '-accel', 'tcg,thread=multi',
    '-smp', '4',
    '-m', '2048',
    '-nodefaults',
    '-kernel', ([IO.Path]::GetFullPath($KernelPath)),
    '-initrd', ([IO.Path]::GetFullPath($InitrdPath)),
    '-append', '"console=ttyAMA0,115200n8 rdinit=/init panic=-1"',
    '-chardev', "file,id=serial0,path=$logPath",
    '-serial', 'chardev:serial0',
    '-display', 'none',
    '-no-reboot'
)

$process = Start-Process -FilePath $qemuPath -ArgumentList $arguments -WorkingDirectory (Split-Path $qemuPath) `
    -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -PassThru
if ($process.WaitForExit(1000)) {
    $stderrText = Get-Content -LiteralPath $stderrPath -Raw -ErrorAction SilentlyContinue
    throw "QEMU exited during startup (exit code $($process.ExitCode)):`n$stderrText"
}
Write-Host "ARM64 executor test started. PID: $($process.Id)"
Write-Host "Serial log: $logPath"