[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Serial,
    [Parameter(Mandatory = $true)][string]$TargetBinary,
    [string]$LogPrefix = '',
    [switch]$Baseline
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$adbPath = (Get-Command adb -ErrorAction Stop).Source
$testRoot = Join-Path $PSScriptRoot 'ptebp_emulate'
$modulePath = Join-Path $testRoot 'ptebp_emulate_test_module.ko'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$remoteRoot = "/data/local/tmp/ptebp-target-$stamp"
if ($Serial -notmatch '^[a-zA-Z0-9._:-]+$') { throw 'Invalid ADB serial' }
if (-not $LogPrefix) { $LogPrefix = Join-Path $testRoot "target-inline-$stamp" }
$LogPrefix = [IO.Path]::GetFullPath($LogPrefix)
[void][IO.Directory]::CreateDirectory((Split-Path -Parent $LogPrefix))

function Invoke-TargetAdb {
    param([string[]]$Arguments)
    $output = @(& $adbPath -s $Serial @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "adb failed ($LASTEXITCODE): $($output -join "`n")"
    }
    ($output -join "`n").Trim()
}

function Invoke-TargetRoot {
    param([string]$Command)
    Invoke-TargetAdb @('shell', 'su', '-c', "'$Command'")
}

$process = $null
$moduleLoaded = $false
$targetPid = 0
$address = ''
$writer = [IO.StreamWriter]::new("$LogPrefix-program.log", $false, [Text.UTF8Encoding]::new($false))
$summary = [Collections.Generic.List[string]]::new()
try {
    Invoke-TargetRoot 'test ! -d /sys/module/ptebp_emulate_test_module && test ! -d /sys/module/arm64_dptdbg_test_module' | Out-Null
    $kernelBefore = Invoke-TargetRoot 'dmesg | tail -n 1'
    $selinuxBefore = Invoke-TargetAdb @('shell', 'getenforce')
    Invoke-TargetAdb @('shell', "mkdir $remoteRoot") | Out-Null
    foreach ($transfer in @(@($TargetBinary, 'hwbreak_test'), @($modulePath, 'ptebp_emulate_test_module.ko'))) {
        if ($Baseline -and $transfer[1] -eq 'ptebp_emulate_test_module.ko') { continue }
        Invoke-TargetAdb @('push', $transfer[0], "$remoteRoot/$($transfer[1])") | Out-Null
        $localHash = (Get-FileHash -LiteralPath $transfer[0] -Algorithm SHA256).Hash.ToLowerInvariant()
        $deviceHash = (Invoke-TargetAdb @('shell', 'sha256sum', "$remoteRoot/$($transfer[1])")).Split()[0]
        if ($localHash -ne $deviceHash) { throw 'Transferred artifact hash mismatch' }
        $summary.Add("SHA256 $($transfer[1])=$localHash")
    }
    Invoke-TargetAdb @('shell', "chmod 0755 $remoteRoot/hwbreak_test") | Out-Null

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $adbPath
    $startInfo.Arguments = "-s $Serial shell -T su -c '$remoteRoot/hwbreak_test --inline-hook-only'"
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.StandardOutputEncoding = [Text.Encoding]::UTF8
    $startInfo.StandardErrorEncoding = [Text.Encoding]::UTF8
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    while ($true) {
        $lineTask = $process.StandardOutput.ReadLineAsync()
        if (-not $lineTask.Wait(15000)) { throw 'Target startup timed out' }
        $line = $lineTask.Result
        if ($null -eq $line) { throw 'Target exited before reporting its breakpoint address' }
        $writer.WriteLine($line)
        if ($line -match 'PID=(\d+)') { $targetPid = [int]$Matches[1] }
        if ($line -match '^Inline hook.*timing_probe_1 .*?(0x[0-9a-fA-F]+)') {
            $address = $Matches[1]
            break
        }
    }
    if (-not $targetPid -or -not $address) { throw 'Target PID or address missing' }
    $summary.Add("TARGET pid=$targetPid address=$address baseline=$Baseline")
    if (-not $Baseline) {
        Invoke-TargetRoot "insmod $remoteRoot/ptebp_emulate_test_module.ko target_tgid=$targetPid target_address=$address" | Out-Null
        $moduleLoaded = $true
        $state = Invoke-TargetRoot 'cat /sys/module/ptebp_emulate_test_module/parameters/target_active /sys/module/ptebp_emulate_test_module/parameters/target_hits'
        $summary.Add("BEFORE active,hits=$($state -replace "`n", ',')")
        if ($state.Split("`n")[0].Trim() -ne '1') { throw 'PTE monitor is inactive before testing' }
    }

    $remainingTask = $process.StandardOutput.ReadToEndAsync()
    $process.StandardInput.WriteLine('y')
    $process.StandardInput.Close()
    if (-not $process.WaitForExit(45000)) { throw 'Target inline-hook checks timed out' }
    $writer.Write($remainingTask.GetAwaiter().GetResult())
    $writer.Write($stderrTask.GetAwaiter().GetResult())
    $writer.Flush()
    $summary.Add("PROGRAM_EXIT=$($process.ExitCode)")
    if ($moduleLoaded) {
        $state = Invoke-TargetRoot 'cat /sys/module/ptebp_emulate_test_module/parameters/target_active /sys/module/ptebp_emulate_test_module/parameters/target_hits'
        $summary.Add("AFTER active,hits=$($state -replace "`n", ',')")
    }
}
finally {
    if ($process -and -not $process.HasExited) {
        if ($targetPid) { Invoke-TargetRoot "kill -TERM $targetPid" | Out-Null }
        if (-not $process.WaitForExit(5000)) { $process.Kill() }
    }
    if ($moduleLoaded) {
        Invoke-TargetRoot 'rmmod ptebp_emulate_test_module' | Out-Null
        $summary.Add('MODULE_UNLOADED')
    }
    $kernelLog = Invoke-TargetRoot 'dmesg'
    [IO.File]::WriteAllText("$LogPrefix-kernel.log", $kernelLog, [Text.UTF8Encoding]::new($false))
    Invoke-TargetRoot "test ! -d /sys/module/ptebp_emulate_test_module && rm -rf $remoteRoot" | Out-Null
    if ((Invoke-TargetAdb @('shell', 'getenforce')) -ne $selinuxBefore) { throw 'SELinux state changed' }
    $summary.Add("KERNEL_BEFORE=$kernelBefore")
    $summary.Add('DEVICE_CLEANED')
    [IO.File]::WriteAllLines("$LogPrefix-summary.log", $summary, [Text.UTF8Encoding]::new($false))
    $writer.Dispose()
    if ($process) { $process.Dispose() }
}
$summary | ForEach-Object { Write-Host $_ }
Write-Host "program_log=$LogPrefix-program.log"