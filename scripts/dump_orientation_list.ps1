# Verify EXIF orientation via GDI+ for files listed in a text file (one name per line)
param([string]$ListFile = "C:\_OpenCV\projects\ApexFace\_tmp\portrait_files.txt")
Add-Type -AssemblyName System.Drawing
Get-Content $ListFile | ForEach-Object {
    $p = Join-Path "C:\_OpenCV\projects\ApexFace\_photo" $_
    try {
        $img = [System.Drawing.Image]::FromFile($p)
        $ori = 0
        foreach ($pi in $img.PropertyItems) { if ($pi.Id -eq 274) { $ori = $pi.Value[0] } }
        Write-Output ("{0}:{1}" -f $_, $ori)
        $img.Dispose()
    } catch { Write-Output ("{0}:ERR" -f $_) }
}
