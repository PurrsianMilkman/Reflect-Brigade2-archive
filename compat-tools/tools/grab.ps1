param(
  [string]$TitleMatch = "Brigade",
  [string]$Out = "shot.png"
)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern IntPtr FindWindowA(string c, string n);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern int GetWindowTextA(IntPtr h, System.Text.StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
}
"@
$found = [IntPtr]::Zero
$foundTitle = ""
$cb = [W+EnumProc]{
  param($h,$p)
  if (-not [W]::IsWindowVisible($h)) { return $true }
  $sb = New-Object System.Text.StringBuilder 512
  [void][W]::GetWindowTextA($h,$sb,512)
  $t = $sb.ToString()
  if ($t -and $t -like "*$TitleMatch*") {
    $script:found = $h; $script:foundTitle = $t; return $false
  }
  return $true
}
[void][W]::EnumWindows($cb,[IntPtr]::Zero)
if ($found -eq [IntPtr]::Zero) { Write-Output "window not found"; exit 1 }
Write-Output "window: '$foundTitle'"
[void][W]::ShowWindow($found,9)      # SW_RESTORE
[void][W]::SetForegroundWindow($found)
Start-Sleep -Milliseconds 900
$cr = New-Object W+RECT
[void][W]::GetClientRect($found,[ref]$cr)
$tl = New-Object W+POINT
$tl.X = 0; $tl.Y = 0
[void][W]::ClientToScreen($found,[ref]$tl)
$w = $cr.R - $cr.L; $h = $cr.B - $cr.T
if ($w -le 0 -or $h -le 0) { Write-Output "bad client rect"; exit 1 }
Write-Output ("client {0}x{1} at screen ({2},{3})" -f $w,$h,$tl.X,$tl.Y)
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($tl.X, $tl.Y, 0, 0, (New-Object System.Drawing.Size $w, $h))
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
# quick content summary so we can tell black from not-black
$sw = [Math]::Min(64,$w); $sh = [Math]::Min(64,$h)
$sum=0; $n=0; $mn=255; $mx=0
for ($y=0; $y -lt $sh; $y++) {
  for ($x=0; $x -lt $sw; $x++) {
    $c = $bmp.GetPixel([int]($x*$w/$sw), [int]($y*$h/$sh))
    $v = ($c.R+$c.G+$c.B)/3
    $sum += $v; $n++
    if ($v -lt $mn) { $mn = $v }
    if ($v -gt $mx) { $mx = $v }
  }
}
Write-Output ("saved {0}  mean={1:N1} min={2} max={3}" -f $Out, ($sum/$n), $mn, $mx)
$bmp.Dispose()
