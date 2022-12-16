$file = 'networks.dat'

$offset = 0
$maxPassLen = 32
$maxSsidLen = 32

# Offsets
$ssidOffset = 0
$ssidLenOffset = $ssidOffset + $maxSsidLen + 1
$bssidOffset = $ssidLenOffset + 1
$passOffset = $bssidOffset + 6
$passLenOffset = $passOffset + $maxPassLen + 1
$secOffset = $passLenOffset + 1
$secOffset = $secOffset + ($secOffset % 4) # Apply DWORD alignment

# Size of one WiFi Entry
$entrySize = $secOffset + 4

$more = 'y'
$enc = [System.Text.Encoding]::ASCII

[byte[]]$dat = New-Object byte[] $entrySize
[byte[]]$bssid = @(0,0,0,0,0,0)

if (Test-Path $file) {
    Write-Output 'WiFi profile ($file) already exists.'

    $reconfig = Read-Host -Prompt 'Reconfigure? y/[n]'

    if ($reconfig.ToLower() -ne 'y') {
        exit 0
    }
}

while ($true) {
    $ssid = Read-Host -Prompt 'Enter the WiFi network SSID'
    if ($ssid.Length -gt $maxSsidLen) {
        $ssid.Remove($maxSsidLen)
    }

    $passSecure = Read-Host -AsSecureString -Prompt 'Enter the WiFi network password'
    #$passEncrypted = ConvertFrom-SecureString $passSecure
    $passPlainText = ConvertFrom-SecureString $passSecure -AsPlainText
    if ($passPlainText.Length -gt $maxPassLen) {
        $passPlainText.Remove($maxPassLen)
    }

    do {
        $security = [Int32](Read-Host -Prompt 'Enter the security (0=open, 1=WEP, 2=WPA)')
    } while ($security -lt 0 || $security -gt 2)

    # Copy data into byte array

    $enc.GetBytes($ssid).CopyTo($dat, $offset + $ssidOffset)
    [BitConverter]::GetBytes(([byte]$ssid.Length)).CopyTo($dat, $offset + $ssidLenOffset)
    $bssid.CopyTo($dat, $offset + $bssidOffset)
    $enc.GetBytes($passPlainText).CopyTo($dat, $offset + $passOffset)
    [BitConverter]::GetBytes([byte]$passPlainText.Length).CopyTo($dat, $offset + $passLenOffset)
    [BitConverter]::GetBytes($security).CopyTo($dat, $offset + $secOffset)

    $more = Read-Host -Prompt 'Add another WiFi network? y/[n]'

    if ($more.ToLower() -ne 'y') {
        break
    }

    $new_dat = New-Object byte[] ($dat.Length + $entrySize)
    $dat.CopyTo($new_dat, 0)
    $dat = $new_dat
    $offset += $entrySize
}

[IO.File]::WriteAllBytes($file, $dat)