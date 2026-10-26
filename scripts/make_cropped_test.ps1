# Create a test image whose face is cut by the frame edge (to verify incomplete detection)
Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Image]::FromFile("C:\_OpenCV\projects\ApexFace\samples\obama_portrait.jpg")
# obama face box is roughly x=786..1514 in a 1916-wide image; crop from x=1054 keeps ~37% of the face
$w = $src.Width - 1054
$bmp = New-Object System.Drawing.Bitmap($w, $src.Height)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dst = New-Object System.Drawing.Rectangle(0, 0, $w, $src.Height)
$srcRect = New-Object System.Drawing.Rectangle(1054, 0, $w, $src.Height)
$g.DrawImage($src, $dst, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
$bmp.Save("C:\_OpenCV\projects\ApexFace\_tmp\cropped_face.jpg", [System.Drawing.Imaging.ImageFormat]::Jpeg)
$g.Dispose(); $bmp.Dispose(); $src.Dispose()
Write-Output "written _tmp\cropped_face.jpg ($w x $($src.Height))"
