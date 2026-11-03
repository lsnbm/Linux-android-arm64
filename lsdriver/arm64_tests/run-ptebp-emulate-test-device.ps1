[CmdletBinding()]
param(
    [string]$Serial = '',
    [string]$LogPath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$testRoot = Join-Path $PSScriptRoot 'ptebp_emulate'
$modulePath = Join-Path $testRoot 'ptebp_emulate_test_module.ko'
$runnerPath = Join-Path $testRoot 'ptebp_emulate_test_runner'
$instructionPath = Join-Path $testRoot 'instruction.txt'
$deviceScriptPath = Join-Path $testRoot 'run_on_android_device.sh'
$adbCommand = Get-Command adb -ErrorAction Stop
$adbPrefix = @()

function Invoke-AdbRaw {
    param([string[]]$Arguments)

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $adbCommand.Source @adbPrefix @Arguments 2>&1)
        $status = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    [pscustomobject]@{
        Output = @($output | ForEach-Object { $_.ToString() })
        Status = $status
    }
}

function Invoke-AdbText {
    param([string[]]$Arguments)

    $result = Invoke-AdbRaw -Arguments $Arguments
    if ($result.Status -ne 0) {
        throw "adb $($Arguments -join ' ') failed with status $($result.Status)`n$($result.Output -join "`n")"
    }
    ($result.Output -join "`n").Trim()
}

$deviceList = Invoke-AdbRaw -Arguments @('devices')
if ($deviceList.Status -ne 0) {
    throw "adb devices failed: $($deviceList.Output -join "`n")"
}
$onlineSerials = @($deviceList.Output | ForEach-Object {
    if ($_ -match '^([^\s]+)\s+device\s*$') { $Matches[1] }
})
if ([string]::IsNullOrWhiteSpace($Serial)) {
    if ($onlineSerials.Count -ne 1) {
        throw "expected exactly one online device, found: $($onlineSerials -join ', ')"
    }
    $Serial = $onlineSerials[0]
}
$adbPrefix = @('-s', $Serial)
if ((Invoke-AdbText @('get-state')) -ne 'device') {
    throw "ADB target is not ready: $Serial"
}
if ((Invoke-AdbText @('shell', 'uname', '-m')) -ne 'aarch64') {
    throw 'ADB target is not AArch64'
}
$rootIdentity = Invoke-AdbText @('shell', 'su', '-c', 'id')
if ($rootIdentity -notmatch 'uid=0') {
    throw "root shell is unavailable: $rootIdentity"
}

$required = @($modulePath, $runnerPath, $instructionPath, $deviceScriptPath)
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
if ($missing.Count -ne 0) {
    throw "missing test artifacts:`n$($missing -join "`n")"
}

if ([string]::IsNullOrWhiteSpace($LogPath)) {
    $LogPath = Join-Path (Get-Location) 'ptebp_emulate_device.log'
}
$LogPath = [IO.Path]::GetFullPath($LogPath)
$logDirectory = Split-Path -Parent $LogPath
if (-not (Test-Path -LiteralPath $logDirectory -PathType Container)) {
    [void](New-Item -ItemType Directory -Path $logDirectory -Force)
}

$remoteModule = '/data/local/tmp/ptebp_emulate_test_module.ko'
$remoteRunner = '/data/local/tmp/ptebp_emulate_test_runner'
$remoteInstructions = '/data/local/tmp/ptebp_emulate_instruction.txt'
$remoteScript = '/data/local/tmp/run_ptebp_emulate_test.sh'
$remoteLog = '/data/local/tmp/ptebp_emulate_test.log'
$remoteProc = '/proc/ptebp_emulate_test'
$beforeSelinux = Invoke-AdbText @('shell', 'getenforce')
$runStatus = -1

try {
    foreach ($transfer in @(
        @($modulePath, $remoteModule),
        @($runnerPath, $remoteRunner),
        @($instructionPath, $remoteInstructions),
        @($deviceScriptPath, $remoteScript)
    )) {
        Write-Host (Invoke-AdbText @('push', $transfer[0], $transfer[1]))
        $localHash = (Get-FileHash -LiteralPath $transfer[0] -Algorithm SHA256).Hash.ToLowerInvariant()
        $remoteHash = (Invoke-AdbText @('shell', 'sha256sum', $transfer[1])).Split()[0].ToLowerInvariant()
        if ($localHash -ne $remoteHash) {
            throw "hash mismatch after push: $($transfer[1])"
        }
    }
    Write-Host (Invoke-AdbText @('shell', 'chmod', '0755', $remoteRunner, $remoteScript))
    $remoteCommand = "$remoteScript > $remoteLog 2>&1; status=`$?; chmod 0644 $remoteLog; exit `$status"
    $runResult = Invoke-AdbRaw -Arguments @('shell', 'su', '-c', $remoteCommand)
    $runStatus = $runResult.Status

    $pullResult = Invoke-AdbRaw -Arguments @('pull', $remoteLog, $LogPath)
    if ($pullResult.Status -ne 0) {
        throw "failed to pull test log: $($pullResult.Output -join "`n")"
    }
    $runOutput = @(Get-Content -LiteralPath $LogPath)
    $runOutput | ForEach-Object { Write-Host $_ }
}
finally {
    Invoke-AdbRaw -Arguments @('shell', 'su', '-c',
        "rmmod ptebp_emulate_test_module 2>/dev/null || true; rm -f $remoteModule $remoteRunner $remoteInstructions $remoteScript $remoteLog") | Out-Null
}

$instructionCount = @(Get-Content -LiteralPath $instructionPath | Where-Object { $_ -match '\S' }).Count
if ($runStatus -ne 0) {
    throw "PTEBP emulator device test failed with status $runStatus; log=$LogPath"
}
if (@($runOutput | Where-Object { $_ -ceq "ptebp emulator test passed cases=$instructionCount" }).Count -ne 1) {
    throw "missing pass marker; log=$LogPath"
}
if (@($runOutput | Where-Object { $_ -ceq "case_counts=compared=$instructionCount skipped=0" }).Count -ne 1) {
    throw "case count mismatch; log=$LogPath"
}
if (@($runOutput | Where-Object { $_ -ceq 'runner_status=0' }).Count -ne 1 -or
    @($runOutput | Where-Object { $_ -ceq 'cleanup_status=0' }).Count -ne 1) {
    throw "runner or cleanup status failed; log=$LogPath"
}

$moduleLoaded = Invoke-AdbText @('shell', 'su', '-c', 'grep -q "^ptebp_emulate_test_module " /proc/modules; echo $?')
$procPresent = Invoke-AdbRaw -Arguments @('shell', 'su', '-c', "test -e $remoteProc")
$afterSelinux = Invoke-AdbText @('shell', 'getenforce')
if ($moduleLoaded -ne '1' -or $procPresent.Status -eq 0 -or $afterSelinux -ne $beforeSelinux) {
    throw "post-test cleanup mismatch: module=$moduleLoaded proc_status=$($procPresent.Status) selinux=$afterSelinux"
}

Write-Host "PTEBP emulator device test passed cases=$instructionCount serial=$Serial"
Write-Host "log=$LogPath"
