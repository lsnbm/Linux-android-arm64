param(
    [string]$Root = $PSScriptRoot,
    [string]$NdkBuild = "E:\\android-ndk-r29\\ndk-build.cmd",
    [int]$Jobs = 12
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = (Get-Location).Path
}

function Zh([string]$escaped) {
    return [regex]::Unescape($escaped)
}

# Force UTF-8 console output.
try { chcp 65001 > $null } catch {}
$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $Utf8NoBom
[Console]::OutputEncoding = $Utf8NoBom
$OutputEncoding = $Utf8NoBom

$CloudflaredVersion = "2026.7.1"
$CloudflaredSha256 = "18f2c9bfc7a67a971bd96f1a5a1935def3c1e52aa386626f1566f04e9b5478d6"
$CloudflaredPath = Join-Path $Root "android\jni\assets\cloudflared-linux-arm64"
$CloudflaredUrl = "https://github.com/cloudflare/cloudflared/releases/download/$CloudflaredVersion/cloudflared-linux-arm64"

function Ensure-CloudflaredPayload {
    $payloadDirectory = Split-Path -Parent $CloudflaredPath
    $downloadPath = "$CloudflaredPath.download"

    if (-not (Test-Path -LiteralPath $CloudflaredPath)) {
        New-Item -ItemType Directory -Path $payloadDirectory -Force | Out-Null
        Write-Host ((Zh '\u6b63\u5728\u4e0b\u8f7d cloudflared {0} ARM64 ...') -f $CloudflaredVersion)
        try {
            Invoke-WebRequest -Uri $CloudflaredUrl -OutFile $downloadPath
            Move-Item -LiteralPath $downloadPath -Destination $CloudflaredPath -Force
        }
        finally {
            Remove-Item -LiteralPath $downloadPath -Force -ErrorAction SilentlyContinue
        }
    }

    $actualHash = (Get-FileHash -LiteralPath $CloudflaredPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne $CloudflaredSha256) {
        throw ((Zh 'cloudflared SHA256 \u6821\u9a8c\u5931\u8d25: expected={0}, actual={1}, path={2}') -f $CloudflaredSha256, $actualHash, $CloudflaredPath)
    }

    Write-Host ((Zh 'cloudflared {0} \u8d44\u6e90\u6821\u9a8c\u901a\u8fc7') -f $CloudflaredVersion)
}

if (-not (Test-Path -LiteralPath $NdkBuild)) {
    throw ((Zh '\u672a\u627e\u5230 ndk-build: {0}') -f $NdkBuild)
}

Ensure-CloudflaredPayload

$mkFiles = @(Get-ChildItem -Path $Root -Recurse -File -Filter "Android.mk" |
    Where-Object { $_.Directory.Name -ieq "jni" } |
    Sort-Object -Property FullName)

if (-not $mkFiles) {
    Write-Host ((Zh '\u5728 Root={0} \u4e0b\u672a\u627e\u5230 jni/Android.mk') -f $Root)
    exit 1
}

Write-Host ((Zh '\u5f00\u59cb\u7f16\u8bd1, Root={0}') -f $Root)
Write-Host ((Zh '\u5171\u627e\u5230 {0} \u4e2a Android.mk') -f $mkFiles.Count)

$results = @()

foreach ($mk in $mkFiles) {
    $projectDir = $mk.Directory.Parent.FullName
    $buildScript = Join-Path "jni" $mk.Name

    Write-Host ""
    Write-Host "============================================================"
    Write-Host ((Zh '\u9879\u76ee\u76ee\u5f55: {0}') -f $projectDir)
    Write-Host ((Zh '\u6784\u5efa\u811a\u672c: {0}') -f $buildScript)

    $ok = $false
    $exitCode = -1
    $errorText = ""

    Push-Location $projectDir
    try {
        $args = @("-j$Jobs")

        & $NdkBuild @args
        $exitCode = $LASTEXITCODE
        $ok = ($exitCode -eq 0)
    }
    catch {
        $errorText = $_.Exception.Message
        $ok = $false
    }
    finally {
        Pop-Location
    }

    $results += [PSCustomObject]@{
        Project  = $projectDir
        MkFile   = $mk.FullName
        Success  = $ok
        ExitCode = $exitCode
        Error    = $errorText
    }
}

Write-Host ""
Write-Host (Zh '\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d \u7f16\u8bd1\u6c47\u603b \u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d\u003d')
$results |
    Select-Object Project, Success, ExitCode |
    Format-Table -AutoSize

$failed = @($results | Where-Object { -not $_.Success })

if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host (Zh '\u5931\u8d25\u9879\u76ee\u5217\u8868:')
    foreach ($item in $failed) {
        Write-Host ((Zh '- {0}') -f $item.Project)
        if ($item.Error) {
            Write-Host ((Zh '  \u9519\u8bef: {0}') -f $item.Error)
        }
        else {
            Write-Host ((Zh '  \u9000\u51fa\u7801: {0}') -f $item.ExitCode)
        }
    }
    exit 1
}

Write-Host ""
Write-Host (Zh '\u5168\u90e8\u9879\u76ee\u7f16\u8bd1\u5b8c\u6210')
exit 0
