# Dump EXIF orientation (tag 274) for a sample of photos using GDI+ (ground truth)
param([int]$Count = 300)
Add-Type -AssemblyName System.Drawing
$files = Get-ChildItem "C:\_OpenCV\projects\ApexFace\_photo\*.JPG" | Select-Object -First $Count
foreach ($f in $files) {
    try {
        $img = [System.Drawing.Image]::FromFile($f.FullName)
        $ori = 0
        foreach ($p in $img.PropertyItems) { if ($p.Id -eq 274) { $ori = $p.Value[0] } }
        Write-Output ("{0}:{1}" -f $f.Name, $ori)
        $img.Dispose()
    } catch { Write-Output ("{0}:ERR" -f $f.Name) }
}
