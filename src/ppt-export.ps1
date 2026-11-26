# PowerPoint-bestand omzetten naar PNG-dia's (1920x1080) met de geinstalleerde PowerPoint.
# Exit 0 = gelukt, 2 = PowerPoint niet beschikbaar, 3 = omzetten mislukt.
param([Parameter(Mandatory)] [string]$Source, [Parameter(Mandatory)] [string]$OutDir)
$ErrorActionPreference = 'Stop'
try { $app = New-Object -ComObject PowerPoint.Application } catch { Write-Output "Geen PowerPoint: $($_.Exception.Message)"; exit 2 }
try {
    # Open(FileName, ReadOnly, Untitled, WithWindow)
    $pres = $app.Presentations.Open($Source, -1, 0, 0)
    New-Item -ItemType Directory -Force $OutDir | Out-Null
    $pres.Export($OutDir, 'PNG', 1920, 1080)
    $pres.Close()
    Write-Output "Klaar: $((Get-ChildItem $OutDir -Filter *.png).Count) dia's"
    exit 0
} catch {
    Write-Output "Omzetten mislukt: $($_.Exception.Message)"
    exit 3
} finally {
    if ($app -and $app.Presentations.Count -eq 0) { $app.Quit() }
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($app) | Out-Null
}
