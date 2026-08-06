# Builds LuckyStar.exe for Windows (GUI, no console window).
# Requires Go 1.24+ and a MinGW-w64 GCC toolchain on PATH (CGO is needed by Fyne).

$ErrorActionPreference = "Stop"

if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    Write-Host "gcc not found on PATH. Install WinLibs: winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT" -ForegroundColor Yellow
}

go mod tidy
go vet ./...
go test ./internal/bridge/
go build -ldflags "-H windowsgui -s -w" -o LuckyStar.exe .
Write-Host "Built LuckyStar.exe" -ForegroundColor Green
