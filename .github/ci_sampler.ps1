param([string]$Csv, [string]$DumpDir, [string]$ProcDump, [string]$StopFile, [int]$IntervalS = 10, [int]$FlatS = 90)
# CI sampler for the Release build: CPU, private bytes and thread states of the build processes every
# $IntervalS seconds. When link CPU time stops advancing for $FlatS seconds, write one set of thread-stack
# minidumps (procdump -mm) of that process, every MSBuild/Tracker and mspdbsrv, so a stall can be inspected.
$names = 'cl','link','mspdbsrv','MSBuild','Tracker','cvtres','mt'
"wall,pid,name,parent,priv_mb,virt_mb,cpu_s,threads,running,wait_reasons" | Out-File -Encoding ascii $Csv
$parents = @{}; $last = @{}; $dumped = $false
New-Item -ItemType Directory -Force $DumpDir | Out-Null
while (-not (Test-Path $StopFile)) {
    $now = Get-Date
    foreach ($p in (Get-Process -Name $names -ErrorAction SilentlyContinue)) {
        if (-not $parents.ContainsKey($p.Id)) {
            $parents[$p.Id] = -1
            try { $parents[$p.Id] = (Get-CimInstance Win32_Process -Filter "ProcessId=$($p.Id)" -ErrorAction Stop).ParentProcessId } catch {}
        }
        $cpu = 0; try { $cpu = [math]::Round($p.TotalProcessorTime.TotalSeconds, 1) } catch {}
        $running = 0; $waits = @{}
        foreach ($t in $p.Threads) {
            if ("$($t.ThreadState)" -eq 'Running') { $running++ }
            elseif ("$($t.ThreadState)" -eq 'Wait') { $k = "$($t.WaitReason)"; $waits[$k] = 1 + [int]$waits[$k] }
        }
        $w = ($waits.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" }) -join ' '
        "{0},{1},{2},{3},{4},{5},{6},{7},{8},{9}" -f $now.ToString('HH:mm:ss'), $p.Id, $p.Name, $parents[$p.Id],
            [math]::Round($p.PrivateMemorySize64/1MB,1), [math]::Round($p.VirtualMemorySize64/1MB,1), $cpu,
            $p.Threads.Count, $running, $w | Out-File -Append -Encoding ascii $Csv
        if ($p.Name -eq 'link') {
            if (-not $last.ContainsKey($p.Id) -or $cpu -ne $last[$p.Id].cpu) { $last[$p.Id] = @{ cpu = $cpu; since = $now } }
            elseif (-not $dumped -and $ProcDump -and ($now - $last[$p.Id].since).TotalSeconds -ge $FlatS) {
                $dumped = $true
                "{0},STALL,{1} pid {2} cpu flat at {3} s for {4} s; dumping" -f $now.ToString('HH:mm:ss'), $p.Name, $p.Id, $cpu, $FlatS | Out-File -Append -Encoding ascii $Csv
                $targets = @($p) + @(Get-Process -Name 'MSBuild','Tracker','mspdbsrv' -ErrorAction SilentlyContinue)
                foreach ($d in $targets) {
                    & $ProcDump -accepteula -mm $d.Id (Join-Path $DumpDir ("{0}_{1}.dmp" -f $d.Name, $d.Id)) 2>&1 | Out-Null
                }
            }
        }
    }
    Start-Sleep -Seconds $IntervalS
}
