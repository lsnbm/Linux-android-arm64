[CmdletBinding()]
param(
    [string]$Serial = '',
    [string]$Version = '',
    [string]$ExpectedManufacturer = '',
    [string]$ExpectedModel = '',
    [string]$LogPath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$testsRoot = $PSScriptRoot
$testDir = Join-Path $testsRoot 'executor'
$runnerPath = Join-Path $testDir 'executor_test_runner'
$instructionPath = Join-Path $testsRoot 'instruction.txt'
$deviceScriptPath = Join-Path $testDir 'run_on_android_device.sh'

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
    return [pscustomobject]@{
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
    return ($result.Output -join "`n").Trim()
}

$deviceListResult = Invoke-AdbRaw -Arguments @('devices')
if ($deviceListResult.Status -ne 0) {
    throw "adb devices failed with status $($deviceListResult.Status)`n$($deviceListResult.Output -join "`n")"
}
$deviceListOutput = $deviceListResult.Output
$onlineSerials = @($deviceListOutput | ForEach-Object {
    if ($_ -match '^(\S+)\s+device\s*$') { $Matches[1] }
})
if ([string]::IsNullOrWhiteSpace($Serial)) {
    if ($onlineSerials.Count -eq 0) {
        throw 'no online ADB device found'
    }
    if ($onlineSerials.Count -ne 1) {
        throw "multiple ADB devices found; use -Serial: $($onlineSerials -join ', ')"
    }
    $Serial = $onlineSerials[0]
}
$adbPrefix = @('-s', $Serial)

if ((Invoke-AdbText @('get-state')) -ne 'device') {
    throw 'ADB target is not ready'
}

$machine = Invoke-AdbText @('shell', 'uname', '-m')
if ($machine -ne 'aarch64') {
    throw "ADB target is not AArch64 hardware: $machine"
}

$rootIdentity = Invoke-AdbText @('shell', 'su', '-c', 'id')
if ($rootIdentity -notmatch 'uid=0') {
    throw "root shell is unavailable: $rootIdentity"
}

$manufacturer = Invoke-AdbText @('shell', 'getprop', 'ro.product.manufacturer')
$model = Invoke-AdbText @('shell', 'getprop', 'ro.product.model')
if ((-not [string]::IsNullOrWhiteSpace($ExpectedManufacturer) -and
     $manufacturer -ne $ExpectedManufacturer) -or
    (-not [string]::IsNullOrWhiteSpace($ExpectedModel) -and
     $model -ne $ExpectedModel)) {
    throw "unexpected physical device: manufacturer=$manufacturer model=$model"
}
$kernel = Invoke-AdbText @('shell', 'uname', '-r')
$androidRelease = if ($kernel -match '^5\.10(?:\.|-|$)') {
    Invoke-AdbText @('shell', 'getprop', 'ro.build.version.release')
} else {
    ''
}
if (-not [string]::IsNullOrWhiteSpace($Version)) {
    $overrideFileName = if ($Version.EndsWith('.ko', [StringComparison]::OrdinalIgnoreCase)) {
        $Version
    } else {
        "$Version.ko"
    }
    $overridePath = Join-Path $testDir $overrideFileName
    if (-not (Test-Path -LiteralPath $overridePath -PathType Leaf)) {
        throw "module version override does not exist: $overridePath"
    }
    $module = Get-Item -LiteralPath $overridePath
} else {
    $releaseMatch = [regex]::Match($kernel, '^(?<series>\d+\.\d+)')
    if (-not $releaseMatch.Success) {
        throw "cannot parse kernel release: $kernel"
    }
    $kernelSeries = $releaseMatch.Groups['series'].Value
    $seriesPattern = '^' + [regex]::Escape($kernelSeries) + '(?:$|[-_])'
    $candidates = @(Get-ChildItem -LiteralPath $testDir -Filter '*.ko' -File |
        Where-Object { $_.BaseName -match $seriesPattern } |
        Sort-Object -Property Name)
    $exactCandidates = @($candidates | Where-Object { $_.BaseName -ceq $kernelSeries })
    if ($exactCandidates.Count -eq 1) {
        $module = $exactCandidates[0]
    } elseif ($candidates.Count -eq 0) {
        throw "no .ko matches kernel series $kernelSeries (reported kernel: $kernel)"
    } elseif ($kernelSeries -ceq '5.10' -and $candidates.Count -gt 1) {
        $androidMatch = [regex]::Match($androidRelease, '^(?<major>\d+)')
        if (-not $androidMatch.Success) {
            throw "cannot parse Android release for kernel series 5.10: $androidRelease"
        }
        $androidMajor = $androidMatch.Groups['major'].Value
        $androidPattern = 'Android' + [regex]::Escape($androidMajor) + '$'
        $androidCandidates = @($candidates | Where-Object { $_.BaseName -match $androidPattern })
        if ($androidCandidates.Count -ne 1) {
            throw "no unique .ko matches kernel series 5.10 and Android $androidMajor`: $($candidates.Name -join ', ')"
        }
        $module = $androidCandidates[0]
    } elseif ($candidates.Count -ne 1) {
        throw "multiple .ko files match kernel series $kernelSeries; keep one or add $kernelSeries.ko: $($candidates.Name -join ', ')"
    } else {
        $module = $candidates[0]
    }
}
$modulePath = $module.FullName
$Version = $module.BaseName

if ([string]::IsNullOrWhiteSpace($LogPath)) {
    $desktop = [Environment]::GetFolderPath([Environment+SpecialFolder]::DesktopDirectory)
    if ([string]::IsNullOrWhiteSpace($desktop)) {
        $desktop = Join-Path $HOME 'Desktop'
    }
    $safeDeviceName = "$manufacturer-$model" -replace '[^A-Za-z0-9._-]', '_'
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $LogPath = Join-Path $desktop "arm64-executor-$safeDeviceName-$Version-$timestamp.log"
}
$logDirectory = Split-Path -Parent ([IO.Path]::GetFullPath($LogPath))
if (-not (Test-Path -LiteralPath $logDirectory -PathType Container)) {
    [void](New-Item -ItemType Directory -Path $logDirectory -Force)
}
$LogPath = [IO.Path]::GetFullPath($LogPath)

$requiredPaths = @($modulePath, $runnerPath, $instructionPath, $deviceScriptPath)
$missingPaths = @($requiredPaths | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
if ($missingPaths.Count -ne 0) {
    throw "missing real-device test artifacts:`n$($missingPaths -join "`n")"
}

$instructions = @(Get-Content -LiteralPath $instructionPath | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
$invalidInstructions = @($instructions | Where-Object { $_ -notmatch '^[0-9A-Fa-f]{8}$' })
if ($invalidInstructions.Count -ne 0) {
    throw 'instruction corpus contains invalid rows'
}
$expectedCount = $instructions.Count
$runnerBuildIdMatch = [regex]::Match(
    [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($runnerPath)),
    'arm64_executor_runner_build_id=([0-9a-f]{64})'
)
if (-not $runnerBuildIdMatch.Success) {
    throw 'runner build identity is missing'
}
$runnerBuildIdMarker = "runner_build_id=arm64_executor_runner_build_id=$($runnerBuildIdMatch.Groups[1].Value)"

$selinuxBefore = Invoke-AdbText @('shell', 'getenforce')
Write-Host "adb_serial=$Serial"
Write-Host "physical_device=$manufacturer $model machine=$machine kernel=$kernel android=$androidRelease selinux=$selinuxBefore"
Write-Host "selected_module=$($module.Name)"

$remoteModule = '/data/local/tmp/arm64_kernel_executor_test_module.ko'
$remoteRunner = '/data/local/tmp/executor_test_runner'
$remoteInstructions = '/data/local/tmp/instruction.txt'
$remoteScript = '/data/local/tmp/run_executor_test.sh'
$remoteLog = '/data/local/tmp/arm64_executor_test.log'
$remoteDevice = '/dev/arm64_executor_test'
$runOutput = @()
$runStatus = -1

try {
    Write-Host (Invoke-AdbText @('push', $modulePath, $remoteModule))
    Write-Host (Invoke-AdbText @('push', $runnerPath, $remoteRunner))
    Write-Host (Invoke-AdbText @('push', $instructionPath, $remoteInstructions))
    Write-Host (Invoke-AdbText @('push', $deviceScriptPath, $remoteScript))
    $transfers = @(
        @($modulePath, $remoteModule),
        @($runnerPath, $remoteRunner),
        @($instructionPath, $remoteInstructions),
        @($deviceScriptPath, $remoteScript)
    )
    foreach ($transfer in $transfers) {
        $localHash = (Get-FileHash -LiteralPath $transfer[0] -Algorithm SHA256).Hash.ToLowerInvariant()
        $remoteHashOutput = Invoke-AdbText @('shell', 'sha256sum', $transfer[1])
        $remoteHash = ($remoteHashOutput -split '\s+')[0].ToLowerInvariant()
        if ($remoteHash -cne $localHash) {
            throw "ADB transfer hash mismatch: $($transfer[1])"
        }
    }
    Write-Host (Invoke-AdbText @('shell', 'chmod', '0755', $remoteRunner, $remoteScript))

    $remoteRunCommand = "$remoteScript > $remoteLog 2>&1; run_status=`$?; chmod 0644 $remoteLog; exit `$run_status"
    $runResult = Invoke-AdbRaw -Arguments @('shell', 'su', '-c', $remoteRunCommand)
    $runStatus = $runResult.Status

    $pullResult = Invoke-AdbRaw -Arguments @('pull', $remoteLog, $LogPath)
    $pullOutput = $pullResult.Output
    $pullStatus = $pullResult.Status
    $pullOutput | ForEach-Object { Write-Host $_ }
    if ($pullStatus -ne 0) {
        throw "adb pull test log failed with status $pullStatus`: $remoteLog"
    }
    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf) -or
        (Get-Item -LiteralPath $LogPath).Length -eq 0) {
        throw "pulled test log is missing or empty: $LogPath"
    }
    $runOutput = @(Get-Content -LiteralPath $LogPath)
    $runOutput | ForEach-Object { Write-Host $_ }
}
finally {
    Invoke-AdbText @(
        'shell', 'su', '-c',
        "rm -f $remoteDevice; rmmod arm64_kernel_executor_test_module 2>/dev/null || true; rm -f $remoteModule $remoteRunner $remoteInstructions $remoteScript $remoteLog"
    ) | Out-Null
}

$resultRows = @($runOutput | Where-Object { $_ -match '^index=\d+ ' })
$protocolMarker = 'protocol_version=6'
$instructionCountMarker = "instruction_count=$expectedCount"
$passMarker = "continuous test passed cases=$expectedCount"
$cleanupMarker = 'cleanup_status=0'

if ($runStatus -ne 0) {
    throw "real-device executor test failed with status $runStatus; log=$LogPath"
}
if ($resultRows.Count -ne $expectedCount) {
    throw "real-device result row count mismatch: rows=$($resultRows.Count) expected=$expectedCount; log=$LogPath"
}
$comparedCount = 0
$skippedCount = 0
for ($index = 0; $index -lt $expectedCount; $index++) {
    $expectedRaw = $instructions[$index].ToLowerInvariant()
    $passRow = "^index=$index raw=0x$expectedRaw status=2 mismatch=none$"
    $skipRow = "^index=$index raw=0x$expectedRaw status=4 action=skip emulator=not_called$"
    if ($resultRows[$index] -match $passRow) {
        $comparedCount++
    }
    elseif ($resultRows[$index] -match $skipRow) {
        $skippedCount++
    }
    else {
        throw "real-device result mismatch at index ${index}: $($resultRows[$index]); log=$LogPath"
    }
}
$requiredMarkers = @($protocolMarker, $runnerBuildIdMarker, $instructionCountMarker, $passMarker, 'runner_status=0', $cleanupMarker)
$requiredMarkers += @(
    'input_owner=runner',
    'verdict_owner=runner',
    'raw_compare=x0-x30,sp,pc,pstate,q0-q31,fpcr,fpsr,tpidr_el0,memory4096',
    'input_profile=regs.txt',
    "case_counts=compared=$comparedCount skipped=$skippedCount"
)
foreach ($marker in $requiredMarkers) {
    if (@($runOutput | Where-Object { $_ -ceq $marker }).Count -ne 1) {
        throw "real-device result marker mismatch: $marker; log=$LogPath"
    }
}

$loadedModules = Invoke-AdbText @('shell', 'su', '-c', 'cat /proc/modules')
$moduleState = if ($loadedModules -match '(?m)^arm64_kernel_executor_test_module ') { 'loaded' } else { 'unloaded' }
$deviceListingResult = Invoke-AdbRaw -Arguments @('shell', 'ls', $remoteDevice)
$deviceState = if ($deviceListingResult.Status -eq 0) { 'present' } else { 'absent' }
$selinuxAfter = Invoke-AdbText @('shell', 'getenforce')
if ($moduleState -ne 'unloaded' -or $deviceState -ne 'absent' -or $selinuxAfter -ne $selinuxBefore) {
    throw "post-test state mismatch: module=$moduleState device=$deviceState selinux=$selinuxAfter"
}

Write-Host "real-device executor test passed cases=$expectedCount compared=$comparedCount skipped=$skippedCount"
Write-Host "cleanup module=$moduleState device=$deviceState selinux=$selinuxAfter"
Write-Host "log=$LogPath"