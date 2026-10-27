# Windows SDK 7.0 (VC9 SP1 compilers) and 7.1 (v90 MSBuild toolset) for the build_vp.yml jobs.
#   Download: setup job, on a cache miss. Downloads both ISOs, checks size and SHA-256, extracts only the
#             Setup folders of the MSIs below and checks that every MSI is there before the folders get cached.
#   Install:  build jobs, after restoring the cache. Installs the MSIs and fails on any msiexec error.
param(
    [Parameter(Mandatory)][ValidateSet('Download', 'Install')][string]$Action
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true # fail on a native command's exit code, not only the last one's

$Sdks = @(
    @{
        Name   = 'Windows SDK 7.0'
        Dir    = 'SDK7'
        Size   = 1552508928
        Sha256 = '65739FB0874CC17EA6962D8CE7915364C7161FA106ED1BF1C917924C18AC63CA'
        # Microsoft no longer hosts it; two Wayback Machine captures of the same file
        Urls   = @(
            'https://web.archive.org/web/20161230154527/http://download.microsoft.com/download/2/E/9/2E911956-F90F-4BFB-8231-E292A7B6F287/GRMSDK_EN_DVD.iso',
            'https://web.archive.org/web/20130601064541/http://download.microsoft.com/download/2/E/9/2E911956-F90F-4BFB-8231-E292A7B6F287/GRMSDK_EN_DVD.iso'
        )
        Msis   = @(
            'WinSDK\WinSDK_x86.msi',
            'WinSDKBuild\WinSDKBuild_x86.msi',
            'WinSDKInterop\WinSDKInterop_x86.msi',
            'WinSDKTools\WinSDKTools_x86.msi',
            'WinSDKWin32Tools\WinSDKWin32Tools_x86.msi',
            'vc_stdx86\vc_stdx86.msi',
            'vc_stdamd64\vc_stdamd64.msi',
            'winsdk_intellidocs\winsdk_intellidocs.msi',
            'WinSDKNetFxTools\WinSDK_nfxtoolsm_x86.msi'
        )
    },
    @{
        Name   = 'Windows SDK 7.1'
        Dir    = 'SDK7.1'
        Size   = 594841600
        Sha256 = '27CB38F76095C0ACB9B558109F8693B39CBCEB796856C15BD575F9E9D0B316C3'
        Urls   = @(
            'https://download.microsoft.com/download/F/1/0/F10113F5-B750-4969-A255-274341AC6BCE/GRMSDK_EN_DVD.iso',
            'https://web.archive.org/web/20180603165022/https://download.microsoft.com/download/F/1/0/F10113F5-B750-4969-A255-274341AC6BCE/GRMSDK_EN_DVD.iso'
        )
        Msis   = @(
            'WinSDK\WinSDK_x86.msi',
            'WinSDKBuild\WinSDKBuild_x86.msi',
            'WinSDKInterop\WinSDKInterop_x86.msi',
            'WinSDKTools\WinSDKTools_x86.msi',
            'WinSDKWin32Tools\WinSDKWin32Tools_x86.msi',
            'vc_stdx86\vc_stdx86.msi',
            'vc_stdamd64\vc_stdamd64.msi',
            'WinSDKNetFxTools\WinSDK_nfxtoolsm_x86.msi'
        )
    }
)

function Get-VerifiedIso($Sdk, $Path) {
    foreach ($url in $Sdk.Urls) {
        Write-Host "$($Sdk.Name): downloading $url"
        $start = Get-Date
        try {
            # curl has no read timeout: --speed-limit/--speed-time abort a stalled transfer, --retry retries it
            curl.exe -L --fail --retry 2 --retry-all-errors --connect-timeout 60 --speed-limit 1024 --speed-time 120 --no-progress-meter -o $Path $url
        } catch {
            Write-Host "::warning::$($Sdk.Name): download failed from $url ($_)"
            continue
        }
        $size = (Get-Item $Path).Length
        # --fail only catches HTTP errors; a page served with 200 has the wrong size and hash
        if ($size -ne $Sdk.Size) {
            Write-Host "::warning::$($Sdk.Name): $size bytes from $url, expected $($Sdk.Size)"
            continue
        }
        $hash = (Get-FileHash $Path -Algorithm SHA256).Hash
        if ($hash -ne $Sdk.Sha256) {
            Write-Host "::warning::$($Sdk.Name): SHA-256 $hash from $url, expected $($Sdk.Sha256)"
            continue
        }
        Write-Host "$($Sdk.Name): $size bytes, SHA-256 OK, $([int]((Get-Date) - $start).TotalSeconds) s"
        return
    }
    throw "$($Sdk.Name): no URL gave the expected file"
}

if ($Action -eq 'Download') {
    foreach ($sdk in $Sdks) {
        $iso = "$($sdk.Dir).iso"
        Get-VerifiedIso $sdk $iso
        # Only the Setup folders that get installed; the rest of the ISOs (docs, samples, ia64) is ~1.8 GB
        $folders = $sdk.Msis | ForEach-Object { 'Setup\' + (Split-Path $_ -Parent) }
        7z x $iso -y "-o$($sdk.Dir)" @folders
        foreach ($msi in $sdk.Msis) {
            if (-not (Test-Path "$($sdk.Dir)\Setup\$msi")) { throw "$($sdk.Name): $msi missing after extraction" }
        }
        Remove-Item $iso
        $mb = [int]((Get-ChildItem $sdk.Dir -Recurse -File | Measure-Object Length -Sum).Sum / 1MB)
        Write-Host "$($sdk.Name): extracted $($sdk.Msis.Count) MSIs with their files ($mb MB)"
    }
} else {
    $logDir = Join-Path $env:RUNNER_TEMP 'sdk-msi-logs'
    New-Item -ItemType Directory -Force $logDir | Out-Null
    foreach ($sdk in $Sdks) {
        foreach ($msi in $sdk.Msis) {
            $path = (Resolve-Path "$($sdk.Dir)\Setup\$msi").Path
            $log = Join-Path $logDir ("$($sdk.Dir)-" + (Split-Path $msi -Leaf) + '.log')
            $start = Get-Date
            # Through msiexec: Start-Process on the .msi itself returns no exit code
            $p = Start-Process msiexec.exe -ArgumentList "/i `"$path`" /qn /norestart /l*v `"$log`"" -Wait -PassThru
            $seconds = [math]::Round(((Get-Date) - $start).TotalSeconds, 1)
            Write-Host "$($sdk.Name) $msi : exit $($p.ExitCode), $seconds s"
            # 0 = success, 3010 = success, restart required
            if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 3010) {
                Get-Content $log -Tail 40 | ForEach-Object { Write-Host "  $_" }
                throw "$($sdk.Name): $msi failed with msiexec exit code $($p.ExitCode) (log: $log)"
            }
        }
    }
}
