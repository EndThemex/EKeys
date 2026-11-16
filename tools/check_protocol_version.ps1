# check_protocol_version.ps1
# Verifies that firmware, Web and desktop App declare the same protocol MAJOR/MINOR.
# Exit code 0 = OK, 1 = mismatch or missing declaration.

$ErrorActionPreference = 'Stop'

$firmwareRoot = Split-Path -Parent $PSScriptRoot
$siblingRoot = Split-Path -Parent $firmwareRoot
$webRoot = Join-Path $siblingRoot 'EKeysWeb'
$appRoot = Join-Path $siblingRoot 'EKeysApp'

function Read-Version {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Name
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "missing file: $Path"
    }
    $match = Select-String -LiteralPath $Path -Pattern $Pattern |
        Select-Object -First 1
    if (-not $match) {
        throw "cannot find $Name in $Path"
    }
    return [int]$match.Matches[0].Groups[1].Value
}

$versions = [ordered]@{
    'firmware' = Read-Version `
        (Join-Path $firmwareRoot 'src\protocol\DeviceIdentity.h') `
        'kProtocolVersion\s*=\s*(\d+)' 'kProtocolVersion'
    'web' = Read-Version `
        (Join-Path $webRoot 'src\protocol\index.ts') `
        'export const PROTOCOL_VERSION\s*=\s*(\d+)' 'PROTOCOL_VERSION'
    'app' = Read-Version `
        (Join-Path $appRoot 'src\protocol.rs') `
        'pub const PROTOCOL_VERSION:\s*u32\s*=\s*(\d+)' 'PROTOCOL_VERSION'
}

$minors = [ordered]@{
    'firmware' = Read-Version `
        (Join-Path $firmwareRoot 'src\protocol\DeviceIdentity.h') `
        'kProtocolMinor\s*=\s*(\d+)' 'kProtocolMinor'
    'web' = Read-Version `
        (Join-Path $webRoot 'src\protocol\index.ts') `
        'export const PROTOCOL_MINOR\s*=\s*(\d+)' 'PROTOCOL_MINOR'
    'app' = Read-Version `
        (Join-Path $appRoot 'src\protocol.rs') `
        'pub const PROTOCOL_MINOR:\s*u32\s*=\s*(\d+)' 'PROTOCOL_MINOR'
}

$majorValues = @($versions.Values | Select-Object -Unique)
$minorVersions = @($minors.Values | Select-Object -Unique)

foreach ($name in $versions.Keys) {
    Write-Host ("{0,-8} v{1}.{2}" -f $name, $versions[$name], $minors[$name])
}

if ($majorValues.Count -ne 1 -or $minorVersions.Count -ne 1) {
    Write-Host 'FAIL: protocol versions are not aligned.' -ForegroundColor Red
    exit 1
}

Write-Host 'OK: protocol versions are aligned.' -ForegroundColor Green
exit 0
