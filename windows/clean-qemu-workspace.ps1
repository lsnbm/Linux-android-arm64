[CmdletBinding()]
param(
    [string]$QemuRoot = 'D:\QEMU',
    [switch]$Apply,
    [switch]$IncludeLegacyAcceptance
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $QemuRoot -PathType Container)) {
    throw "QEMU root does not exist: $QemuRoot"
}

$runningQemu = @(Get-Process -Name 'qemu-system-aarch64' -ErrorAction SilentlyContinue)
if ($runningQemu.Count -gt 0) {
    throw "QEMU is running (PID $($runningQemu.Id -join ', ')); stop it before cleaning."
}

$targets = @(
    'executor-test-serial.log',
    'executor-test-host-stdout.log',
    'executor-test-host-stderr.log',
    'executor-status4.txt',
    'status4-unique.bin',
    'bazel-system-dlkm-build.log',
    'kernel-build-6.1.23.log',
    'avd\Android14_ARM64_6_1.avd\qemu-custom-init-serial.log',
    'avd\Android14_ARM64_6_1.avd\qemu-host-stderr.log',
    'avd\Android14_ARM64_6_1.avd\qemu-host-stdout.log',
    'avd\Android14_ARM64_6_1.avd\qemu-test.stderr.log',
    'avd\Android14_ARM64_6_1.avd\qemu-test.stdout.log',
    'avd\Android14_ARM64_6_1.avd\scrcpy-launcher-error.log',
    'avd\Android14_ARM64_6_1.avd\scrcpy-launcher.log',
    'avd\Android14_ARM64_6_1.avd\guest-performance-optimizer-error.log',
    'avd\Android14_ARM64_6_1.avd\guest-performance-optimizer.log',
    'avd\Android14_ARM64_6_1.avd\last-windows-qemu-commandline.txt',
    'sdk\.sdk\arch\16fa3c441d29fde6c9eea2f766eecf77032d68b4.part',
    'sdk\.temp\PackageOperation01'
)

if ($IncludeLegacyAcceptance) {
    $targets += 'acceptance\kernel-6.1.23-driver-test'
}

$resolvedTargets = @(
    $targets |
    ForEach-Object { Join-Path $QemuRoot $_ } |
    Where-Object { Test-Path -LiteralPath $_ }
)

if ($resolvedTargets.Count -eq 0) {
    Write-Host 'No cleanup targets found.'
    exit 0
}

$totalBytes = 0L
foreach ($target in $resolvedTargets) {
    $item = Get-Item -LiteralPath $target -Force
    if ($item.PSIsContainer) {
        $size = (Get-ChildItem -LiteralPath $target -Recurse -File -Force -ErrorAction SilentlyContinue |
            Measure-Object -Property Length -Sum).Sum
        if ($null -eq $size) {
            $size = 0
        }
    } else {
        $size = $item.Length
    }

    $totalBytes += [int64]$size
    $relative = $target.Substring($QemuRoot.TrimEnd('\').Length + 1)
    Write-Host ("{0,10:N1} MiB  {1}" -f ($size / 1MB), $relative)

    if ($Apply) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

$verb = if ($Apply) { 'Removed' } else { 'Would remove' }
Write-Host ("$verb {0} target(s), {1:N1} MiB." -f $resolvedTargets.Count, ($totalBytes / 1MB))
if (-not $Apply) {
    Write-Host 'Preview only. Re-run with -Apply to remove these targets.'
}