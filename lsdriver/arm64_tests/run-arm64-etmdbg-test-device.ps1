[CmdletBinding()]
param(
    [string]$Serial = '2912b4a6',
    [string]$TracePath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$testsRoot = $PSScriptRoot
$testDir = Join-Path $testsRoot 'etmdbg'
$modulePath = Join-Path $testDir 'arm64_etmdbg_test_module.ko'
$deviceScriptPath = Join-Path $testDir 'run_on_android_device.sh'
$adbCommand = Get-Command adb -ErrorAction Stop

function Invoke-AdbRaw {
    param([string[]]$Arguments)

    $output = @(& $adbCommand.Source '-s' $Serial @Arguments 2>&1)
    $status = $LASTEXITCODE
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

if (-not (Test-Path -LiteralPath $modulePath -PathType Leaf)) {
    throw "missing module: $modulePath; build it with make -C lsdriver/arm64_tests/etmdbg module"
}
if (-not (Test-Path -LiteralPath $deviceScriptPath -PathType Leaf)) {
    throw "missing device script: $deviceScriptPath"
}
if ([string]::IsNullOrWhiteSpace($TracePath)) {
    $TracePath = Join-Path $testDir 'arm64_etmdbg_trace.bin'
}
$traceDirectory = Split-Path -Parent ([IO.Path]::GetFullPath($TracePath))
if (-not (Test-Path -LiteralPath $traceDirectory -PathType Container)) {
    [void](New-Item -ItemType Directory -Path $traceDirectory -Force)
}
$TracePath = [IO.Path]::GetFullPath($TracePath)

$remoteModule = '/data/local/tmp/arm64_etmdbg_test_module.ko'
$remoteScript = '/data/local/tmp/run_arm64_etmdbg_test.sh'
$remoteTrace = '/data/local/tmp/arm64_etmdbg_trace.bin'
$remoteLog = '/data/local/tmp/arm64_etmdbg_test.log'
$localLog = Join-Path $traceDirectory 'arm64_etmdbg_test.log'

try {
    Write-Host (Invoke-AdbText @('push', $modulePath, $remoteModule))
    Write-Host (Invoke-AdbText @('push', $deviceScriptPath, $remoteScript))
    Write-Host (Invoke-AdbText @('shell', 'chmod', '0755', $remoteScript))

    $run = Invoke-AdbRaw -Arguments @('shell', 'su', '-c', "$remoteScript > $remoteLog 2>&1; status=`$?; chmod 0644 $remoteLog; exit `$status")
    $logPull = Invoke-AdbRaw -Arguments @('pull', $remoteLog, $localLog)
    $logPull.Output | ForEach-Object { Write-Host $_ }
    if ($logPull.Status -eq 0 -and (Test-Path -LiteralPath $localLog -PathType Leaf)) {
        Get-Content -LiteralPath $localLog | ForEach-Object { Write-Host $_ }
    }
    if ($run.Status -ne 0) {
        throw "device ETM/TRBE test failed with status $($run.Status); see $localLog"
    }
    $pull = Invoke-AdbRaw -Arguments @('pull', $remoteTrace, $TracePath)
    $pull.Output | ForEach-Object { Write-Host $_ }
    if ($pull.Status -ne 0) {
        throw "trace pull failed: $remoteTrace"
    }
    $log = Invoke-AdbText @('shell', 'cat', $remoteLog)
    $log | Write-Host
}
finally {
    Invoke-AdbRaw -Arguments @('shell', 'su', '-c', "rm -f $remoteModule $remoteScript $remoteTrace $remoteLog; rmmod arm64_etmdbg_test_module 2>/dev/null || true") | Out-Null
}

$trace = [IO.File]::ReadAllBytes($TracePath)
if ($trace.Length -le 72) {
    throw "trace is too short: $($trace.Length) bytes"
}
$magic = [BitConverter]::ToUInt32($trace, 0)
$version = [BitConverter]::ToUInt32($trace, 4)
$status = [BitConverter]::ToInt32($trace, 8)
$traceStart = [BitConverter]::ToUInt64($trace, 16)
$traceStop = [BitConverter]::ToUInt64($trace, 24)
$traceSize = [BitConverter]::ToUInt64($trace, 32)
$traceStatus = [BitConverter]::ToUInt64($trace, 40)
$traceWrite = [BitConverter]::ToUInt64($trace, 48)

if ($magic -ne 0x45544d54 -or $version -ne 1 -or $status -ne 0) {
    throw "invalid ETMDBG header: magic=0x$('{0:x8}' -f $magic) version=$version status=$status"
}
if ($traceStart -ge $traceStop -or $traceSize -eq 0 -or $traceSize -ne ($trace.Length - 72)) {
    throw "invalid trace range/size: start=0x$('{0:x}' -f $traceStart) stop=0x$('{0:x}' -f $traceStop) size=$traceSize file=$($trace.Length)"
}
if (($traceStatus -band 0x40000) -ne 0) {
    throw "TRBE abort status: 0x$('{0:x}' -f $traceStatus)"
}
if ($traceWrite -eq 0) {
    throw 'TRBE write pointer is zero'
}

Write-Host "ETMDBG_TEST_PASS trace=$TracePath bytes=$($trace.Length) payload=$traceSize status=0x$('{0:x}' -f $traceStatus)"