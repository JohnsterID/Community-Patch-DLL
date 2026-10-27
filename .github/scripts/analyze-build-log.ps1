# Counts and annotates the warnings and errors in MSBuild and Clang build logs, and fails if there are errors.
# usage: analyze-build-log.ps1 "MSVC Debug=msvc_debug_msbuild.log" "MSVC Release=msvc_release_msbuild.log"
# Matches MSBuild's message format "origin : [fatal ]error|warning [CODE] : text" (origin = file(line[,col]), LINK,
# EXEC, lib(obj), ...) and Clang's "file:line:col: warning: text" / "lld-link: warning: text".
param(
    [Parameter(Mandatory, ValueFromRemainingArguments)][string[]]$Logs
)

$ErrorActionPreference = 'Stop'

$message = '^\s*(?<origin>.*?)\s*:\s*(?<cat>fatal error|error|warning)(?:\s+(?<code>[A-Z]+\d+))?\s*:\s*(?<text>.*?)\s*$'
$location = '^(?<file>.+?)(?:\((?<line>\d+)(?:,(?<col>\d+))?\)|:(?<line>\d+)(?::(?<col>\d+))?)$'
$workspace = if ($env:GITHUB_WORKSPACE) { $env:GITHUB_WORKSPACE.TrimEnd('\', '/') + '\' } else { '' }

function Format-Property($Value) {
    # workflow command property escaping
    $Value -replace '%', '%25' -replace "`r", '%0D' -replace "`n", '%0A' -replace ':', '%3A' -replace ',', '%2C'
}

$rows = @()
$totalErrors = 0
foreach ($entry in $Logs) {
    $label, $path = $entry -split '=', 2
    if (-not (Test-Path $path)) {
        Write-Host "[$label] no log ($path): the build did not run"
        $rows += "| $label | - | - | not run |"
        continue
    }
    $seen = @{}
    $warnings = 0
    $errors = 0
    foreach ($raw in Get-Content $path) {
        if ($raw -notmatch $message) { continue }
        $m = $Matches
        # MSBuild repeats every warning and error in the summary at the end, with the project in brackets
        $text = $m.text -replace '\s*\[[^\]]+\.(?:vcx?proj|sln)\]$', ''
        $key = "$($m.origin)|$($m.cat)|$($m.code)|$text"
        if ($seen.ContainsKey($key)) { continue }
        $seen[$key] = $true
        if ($text -match 'unknown warning option.*-Wno-enum-constexpr-conversion') { continue }

        $isError = $m.cat -ne 'warning'
        if ($isError) { $errors++ } else { $warnings++ }
        $props = @()
        if ($m.origin -match $location) {
            $file = $Matches.file
            if ($workspace -and $file.StartsWith($workspace, [StringComparison]::OrdinalIgnoreCase)) { $file = $file.Substring($workspace.Length) }
            $props += "file=$(Format-Property ($file -replace '\\', '/'))", "line=$($Matches.line)"
            if ($Matches.col) { $props += "col=$($Matches.col)" }
        }
        $props += "title=$(Format-Property "$label $($m.cat) $($m.code)".Trim())"
        $origin = if ($props.Count -gt 1) { '' } else { "$($m.origin): " }
        Write-Host "::$(if ($isError) { 'error' } else { 'warning' }) $($props -join ',')::$origin$text"
    }
    # MSBuild's own count, to compare with
    $summary = (Select-String -Path $path -Pattern '^\s*\d+ (Warning|Error)\(s\)' | ForEach-Object { $_.Line.Trim() }) -join ', '
    Write-Host "[$label] $warnings warnings, $errors errors$(if ($summary) { " (MSBuild: $summary)" })"
    $rows += "| $label | $warnings | $errors | $summary |"
    $totalErrors += $errors
}

if ($env:GITHUB_STEP_SUMMARY) {
    @('| Build | Warnings | Errors | MSBuild summary |', '|---|---|---|---|') + $rows | Add-Content $env:GITHUB_STEP_SUMMARY
}
if ($totalErrors -gt 0) {
    Write-Host "::error::$totalErrors error(s) in the build logs, see the annotations above"
    exit 1
}
