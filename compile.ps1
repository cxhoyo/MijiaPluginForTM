param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('Win32', 'x64')]
    [string]$Platform = 'x64',

    [switch]$Pause
)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $scriptDir

$msbuildExe = $null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path -LiteralPath $vswhere) {
    $candidate = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' |
        Select-Object -First 1
    if ($candidate -and (Test-Path -LiteralPath $candidate)) {
        $msbuildExe = $candidate
    }
}

if (-not $msbuildExe) {
    $knownPaths = @(
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe'
    )
    $msbuildExe = $knownPaths | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

if (-not $msbuildExe) {
    Write-Error '未找到 MSBuild。请安装 Visual Studio 2022 Build Tools，并勾选“使用 C++ 的桌面开发”。'
}

Write-Host "MSBuild: $msbuildExe" -ForegroundColor Cyan
Write-Host "构建: $Configuration | $Platform" -ForegroundColor Cyan
& $msbuildExe '.\MijiaPowerPlugin.sln' "/p:Configuration=$Configuration" "/p:Platform=$Platform" '/m' '/v:minimal'
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild 失败，退出码 $LASTEXITCODE"
}

$dllPath = Join-Path $scriptDir "bin\$Configuration\$Platform\MijiaPower.dll"
if (-not (Test-Path -LiteralPath $dllPath)) {
    throw "构建成功但未找到 DLL：$dllPath"
}

$dll = Get-Item -LiteralPath $dllPath
Write-Host "DLL: $($dll.FullName)" -ForegroundColor Green
Write-Host "大小: $($dll.Length) 字节" -ForegroundColor Green

if ($Pause) {
    Read-Host '按 Enter 退出'
}
