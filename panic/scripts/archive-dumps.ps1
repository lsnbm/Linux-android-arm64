[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Pack', 'Test', 'Restore')]
    [string]$Mode,
    [Parameter(Mandatory = $true)]
    [string]$ArchivePath,
    [Parameter(Mandatory = $true)]
    [string]$SevenZip,
    [Parameter(Mandatory = $true)]
    [string]$WorkPath,
    [string]$SourcePath = (Join-Path $PSScriptRoot '..\dump'),
    [string]$DestinationPath,
    [ValidateRange(16, 1024)]
    [int]$DictionaryMiB = 128
)

$ErrorActionPreference = 'Stop'
$SevenZip = (Resolve-Path -LiteralPath $SevenZip).Path
$ArchivePath = [IO.Path]::GetFullPath($ArchivePath)
$WorkPath = [IO.Path]::GetFullPath($WorkPath)
$stagePath = Join-Path $WorkPath ([guid]::NewGuid().ToString('N'))

function Get-ContainedPath([string]$Root, [string]$Relative) {
    if ([string]::IsNullOrEmpty($Relative) -or [IO.Path]::IsPathRooted($Relative)) {
        throw 'Invalid relative archive path.'
    }
    $rootPrefix = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $fullPath = [IO.Path]::GetFullPath((Join-Path $rootPrefix $Relative))
    if (-not $fullPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw ('Archive path escapes its root: ' + $Relative)
    }
    return $fullPath
}

try {
    if ($Mode -eq 'Pack') {
        $SourcePath = (Resolve-Path -LiteralPath $SourcePath).Path.TrimEnd('\', '/')
        $sourcePrefix = $SourcePath + [IO.Path]::DirectorySeparatorChar
        if ($stagePath.StartsWith($sourcePrefix, [StringComparison]::OrdinalIgnoreCase) -or
            $ArchivePath.StartsWith($sourcePrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Archive and temporary files must be outside the source directory.'
        }
        if (Test-Path -LiteralPath $ArchivePath) {
            throw 'Archive already exists.'
        }
        $archiveDirectory = Split-Path -Parent $ArchivePath
        [IO.Directory]::CreateDirectory($archiveDirectory) | Out-Null
        $archiveName = [IO.Path]::GetFileName($ArchivePath)
        if (@(Get-ChildItem -LiteralPath $archiveDirectory -File -Force | Where-Object {
            $_.Name.StartsWith($archiveName + '.', [StringComparison]::OrdinalIgnoreCase)
        }).Count -gt 0) {
            throw 'Archive volumes already exist.'
        }
        $sourceEntries = @(Get-ChildItem -LiteralPath $SourcePath -Recurse -Force)
        if (@($sourceEntries | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count -gt 0) {
            throw 'Reparse points require a separate backup policy.'
        }
        $sourceFiles = @($sourceEntries | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
        if ($sourceFiles.Count -eq 0) {
            throw 'Source directory contains no files.'
        }
        [IO.Directory]::CreateDirectory($stagePath) | Out-Null
        $payloadRoot = Join-Path $stagePath 'payload'
        $uniqueContents = @{}
        $fileRecords = [Collections.Generic.List[object]]::new()
        [long]$sourceBytes = 0
        [long]$payloadBytes = 0
        foreach ($sourceFile in $sourceFiles) {
            $relativePath = $sourceFile.FullName.Substring($sourcePrefix.Length).Replace('\', '/')
            $contentHash = (Get-FileHash -LiteralPath $sourceFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            if (-not $uniqueContents.ContainsKey($contentHash)) {
                $payloadPath = Get-ContainedPath $payloadRoot $relativePath
                [IO.Directory]::CreateDirectory((Split-Path -Parent $payloadPath)) | Out-Null
                Copy-Item -LiteralPath $sourceFile.FullName -Destination $payloadPath
                if ((Get-FileHash -LiteralPath $payloadPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $contentHash) {
                    throw ('Source changed while archiving: ' + $relativePath)
                }
                $uniqueContents[$contentHash] = $relativePath
                $payloadBytes += $sourceFile.Length
            }
            $fileRecords.Add([ordered]@{
                Path = $relativePath
                Length = $sourceFile.Length
                SHA256 = $contentHash
                Payload = $uniqueContents[$contentHash]
                LastWriteTimeUtc = $sourceFile.LastWriteTimeUtc.ToString('o')
            })
            $sourceBytes += $sourceFile.Length
            if (($fileRecords.Count % 2000) -eq 0) {
                Write-Output ('Indexed files: ' + $fileRecords.Count)
            }
        }
        $manifest = [ordered]@{
            Format = 'panic-dump-archive-v1'
            CreatedUtc = [DateTime]::UtcNow.ToString('o')
            SourceBytes = $sourceBytes
            UniqueFiles = $uniqueContents.Count
            PayloadBytes = $payloadBytes
            Directories = @($sourceEntries | Where-Object { $_.PSIsContainer } | ForEach-Object {
                $_.FullName.Substring($sourcePrefix.Length).Replace('\', '/')
            })
            Files = @($fileRecords.ToArray())
        }
        $manifestJson = $manifest | ConvertTo-Json -Depth 6
        [IO.File]::WriteAllText((Join-Path $stagePath 'manifest.json'), $manifestJson, [Text.UTF8Encoding]::new($false))
        [pscustomobject]@{ Files = $sourceFiles.Count; UniqueFiles = $uniqueContents.Count; SourceBytes = $sourceBytes; PayloadBytes = $payloadBytes } | ConvertTo-Json -Compress | Write-Output
        Push-Location -LiteralPath $stagePath
        try {
            & $SevenZip a -t7z $ArchivePath 'manifest.json' 'payload' '-mx=9' '-m0=LZMA2' ("-md=${DictionaryMiB}m") '-mfb=273' '-ms=on' '-mqs=on' '-mmt=2' '-v96m' '-bsp0'
            if ($LASTEXITCODE -ne 0) { throw '7-Zip archive creation failed.' }
        } finally {
            Pop-Location
        }
        & $SevenZip t ($ArchivePath + '.001') '-bsp0'
        if ($LASTEXITCODE -ne 0) { throw '7-Zip archive integrity check failed.' }
        Write-Output ('PACK_OK: ' + $ArchivePath + '.001')
    } else {
        if (-not (Test-Path -LiteralPath $ArchivePath -PathType Leaf)) {
            throw 'The archive or its first volume does not exist.'
        }
        if ($Mode -eq 'Restore') {
            if ([string]::IsNullOrWhiteSpace($DestinationPath)) {
                throw 'Restore requires DestinationPath.'
            }
            $DestinationPath = [IO.Path]::GetFullPath($DestinationPath)
            if (Test-Path -LiteralPath $DestinationPath) {
                throw 'Restore destination must not exist.'
            }
        }
        [IO.Directory]::CreateDirectory($stagePath) | Out-Null
        & $SevenZip x $ArchivePath ('-o' + $stagePath) '-y' '-bsp0'
        if ($LASTEXITCODE -ne 0) { throw '7-Zip archive extraction failed.' }
        $manifest = [IO.File]::ReadAllText((Join-Path $stagePath 'manifest.json'), [Text.Encoding]::UTF8) | ConvertFrom-Json
        if ($manifest.Format -ne 'panic-dump-archive-v1' -or $manifest.Files.Count -eq 0) {
            throw 'Unsupported or empty archive manifest.'
        }
        $payloadRoot = Join-Path $stagePath 'payload'
        $verifiedPayloads = @{}
        $restoredPaths = @{}
        [long]$verifiedBytes = 0
        foreach ($fileRecord in $manifest.Files) {
            $null = Get-ContainedPath (Join-Path $stagePath 'restored') $fileRecord.Path
            if ($restoredPaths.ContainsKey($fileRecord.Path)) {
                throw ('Duplicate manifest path: ' + $fileRecord.Path)
            }
            $restoredPaths[$fileRecord.Path] = $true
            $payloadPath = Get-ContainedPath $payloadRoot $fileRecord.Payload
            if (-not $verifiedPayloads.ContainsKey($fileRecord.Payload)) {
                $payloadFile = Get-Item -LiteralPath $payloadPath
                $verifiedPayloads[$fileRecord.Payload] = @{
                    Hash = (Get-FileHash -LiteralPath $payloadPath -Algorithm SHA256).Hash.ToLowerInvariant()
                    Length = $payloadFile.Length
                }
            }
            $verifiedPayload = $verifiedPayloads[$fileRecord.Payload]
            if ($verifiedPayload.Hash -ne $fileRecord.SHA256 -or $verifiedPayload.Length -ne $fileRecord.Length) {
                throw ('Payload hash or size mismatch: ' + $fileRecord.Path)
            }
            $verifiedBytes += $fileRecord.Length
        }
        foreach ($relativeDirectory in $manifest.Directories) {
            $null = Get-ContainedPath (Join-Path $stagePath 'restored') $relativeDirectory
        }
        if ($verifiedBytes -ne $manifest.SourceBytes -or $verifiedPayloads.Count -ne $manifest.UniqueFiles) {
            throw 'Archive manifest totals do not match its contents.'
        }
        if ($Mode -eq 'Restore') {
            [IO.Directory]::CreateDirectory($DestinationPath) | Out-Null
            foreach ($relativeDirectory in $manifest.Directories) {
                [IO.Directory]::CreateDirectory((Get-ContainedPath $DestinationPath $relativeDirectory)) | Out-Null
            }
            foreach ($fileRecord in $manifest.Files) {
                $destinationFile = Get-ContainedPath $DestinationPath $fileRecord.Path
                $payloadPath = Get-ContainedPath $payloadRoot $fileRecord.Payload
                [IO.Directory]::CreateDirectory((Split-Path -Parent $destinationFile)) | Out-Null
                Copy-Item -LiteralPath $payloadPath -Destination $destinationFile
                $fileTimestamp = [DateTime]$fileRecord.LastWriteTimeUtc
                [IO.File]::SetLastWriteTimeUtc($destinationFile, $fileTimestamp.ToUniversalTime())
            }
        }
        [pscustomobject]@{ Result = ($Mode.ToUpperInvariant() + '_OK'); Files = $manifest.Files.Count; UniqueFiles = $verifiedPayloads.Count; SourceBytes = $verifiedBytes } | ConvertTo-Json -Compress | Write-Output
    }
} finally {
    if (Test-Path -LiteralPath $stagePath) {
        Remove-Item -LiteralPath $stagePath -Recurse -Force
    }
}