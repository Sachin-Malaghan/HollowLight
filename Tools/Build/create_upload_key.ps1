# Creates the Google Play UPLOAD KEY for HOLLOWLIGHT. Run it once, yourself, in a PowerShell window:
#   powershell -ExecutionPolicy Bypass -File Tools\Build\create_upload_key.ps1
#
# You choose the password; it is never shown or sent anywhere. The script writes:
#   Build/Android/hollowlight-upload.keystore   the key (git-ignored)
#   Config/Android/AndroidEngine.ini            tells Unreal to sign releases with it (git-ignored)
#
# BACK BOTH UP (e.g. to a USB stick and a private cloud folder) and remember the password.
# If you lose them, Google can reset the upload key, but it takes days and a support request.
$ErrorActionPreference = 'Stop'
$root = Resolve-Path "$PSScriptRoot\..\.."
$keystore = Join-Path $root 'Build\Android\hollowlight-upload.keystore'
$ini = Join-Path $root 'Config\Android\AndroidEngine.ini'
$alias = 'hollowlight-upload'

if (Test-Path $keystore) {
    Write-Host "An upload key already exists: $keystore"
    Write-Host 'Delete it first only if you are sure it was never used for a Play upload.'
    exit 1
}

$jdk = Get-ChildItem 'C:\Program Files\Microsoft', 'C:\Program Files\Eclipse Adoptium', 'C:\Program Files\Android\Android Studio' -Directory -ErrorAction SilentlyContinue |
    Where-Object { Test-Path "$($_.FullName)\bin\keytool.exe" } | Select-Object -First 1
if (-not $jdk) { $jdk = Get-Item 'C:\Program Files\Android\Android Studio\jbr' }
$keytool = Join-Path $jdk.FullName 'bin\keytool.exe'

Write-Host ''
Write-Host 'Choose a password for the upload key (at least 8 characters). Write it down somewhere safe.'
$p1 = Read-Host 'Password' -AsSecureString
$p2 = Read-Host 'Type it again' -AsSecureString
$plain1 = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($p1))
$plain2 = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($p2))
if ($plain1 -ne $plain2) { throw 'The passwords do not match.' }
if ($plain1.Length -lt 8) { throw 'Use at least 8 characters.' }
if ($plain1 -match '["\r\n]') { throw 'Please avoid double quotes and line breaks in the password.' }

$name = Read-Host 'Your name or studio name (goes inside the certificate, e.g. Brainrot Interactive Studios)'
$country = Read-Host 'Two-letter country code (e.g. IN)'
$dname = "CN=$name, O=$name, C=$country"

New-Item -ItemType Directory -Force (Split-Path $keystore) | Out-Null
& $keytool -genkeypair -v -keystore $keystore -alias $alias -keyalg RSA -keysize 2048 -validity 10000 `
    -storepass $plain1 -keypass $plain1 -dname $dname
if ($LASTEXITCODE -ne 0) { throw 'keytool failed' }

New-Item -ItemType Directory -Force (Split-Path $ini) | Out-Null
Set-Content -Path $ini -Encoding ascii -Value @"
; Release signing for Google Play. Local only - git-ignored. Do not share or commit.
[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]
KeyStore=hollowlight-upload.keystore
KeyAlias=$alias
KeyStorePassword=$plain1
KeyPassword=
"@
$plain1 = $null; $plain2 = $null

Write-Host ''
Write-Host 'Upload key created.'
Write-Host "  Key file : $keystore"
Write-Host "  Settings : $ini"
Write-Host 'Now back up BOTH files and keep the password safe.'
