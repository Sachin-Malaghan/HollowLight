# Packages EMBERHOME with Unreal's BuildCookRun.
#   powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Win64
#   powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Android            # needs Android Studio + SDK/NDK (SETUP.md)
#   powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Android -Release   # signed .aab for Google Play
#   powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform IOS                # needs a Mac with Xcode (remote build, SETUP.md)
# Output: Packaged/<Platform>/
param(
    [ValidateSet('Win64', 'Android', 'IOS')][string]$Platform = 'Win64',
    [ValidateSet('Shipping', 'Development')][string]$Config = 'Shipping',
    [switch]$Release
)
$ErrorActionPreference = 'Stop'
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.8' }
$root = Resolve-Path "$PSScriptRoot\..\.."
$project = Join-Path $root 'Hollowlight.uproject'
$out = Join-Path $root "Packaged\$Platform"

$args = @(
    'BuildCookRun', "-project=$project", '-noP4', "-platform=$Platform", "-clientconfig=$Config",
    '-build', '-cook', '-stage', '-package', '-pak', '-iostore', '-compressed', '-archive', "-archivedirectory=$out",
    '-nodebuginfo', '-utf8output', '-unattended'
)
if ($Platform -eq 'Android' -and $Release -and -not (Test-Path (Join-Path $root 'Config\Android\AndroidEngine.ini'))) {
    throw 'No upload key yet. Run Tools\Build\create_upload_key.ps1 first (once), then build the release again.'
}

if ($Platform -eq 'Android') {
    # Pick up the SDK/NDK variables SetupAndroid.bat wrote, even if this shell started before it ran.
    foreach ($v in 'ANDROID_HOME', 'NDKROOT', 'NDK_ROOT', 'JAVA_HOME') {
        $val = [Environment]::GetEnvironmentVariable($v, 'User')
        if ($val) { Set-Item "env:$v" $val }
    }
    # Antivirus HTTPS scanning (Avast/AVG Web Shield, corporate proxies) re-signs traffic with a root
    # that Windows trusts but Android Studio's Java does not, so Gradle downloads fail with
    # "PKIX path building failed". Give Java a copy of its trust store with those roots added.
    # Gradle for UE 5.8 runs on Java 17-21. Recent Android Studio bundles a newer Java (25 fails with
    # "Unsupported class file major version 69"), so prefer an installed JDK 21 or 17 for this build only.
    $jdk = Get-ChildItem 'C:\Program Files\Microsoft', 'C:\Program Files\Eclipse Adoptium', 'C:\Program Files\Java', 'C:\Program Files\Zulu' -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^(jdk-?|zulu-?)(21|17)' -and (Test-Path "$($_.FullName)\bin\java.exe") } |
        Sort-Object Name -Descending | Select-Object -First 1
    if ($jdk) { $env:JAVA_HOME = $jdk.FullName; Write-Host "Java for Gradle: $($jdk.FullName)" }
    else { Write-Warning 'No JDK 17/21 found; if Gradle fails with "Unsupported class file major version", install one (winget install Microsoft.OpenJDK.21).' }

    $scanRoots = Get-ChildItem Cert:\LocalMachine\Root, Cert:\CurrentUser\Root -ErrorAction SilentlyContinue |
        Where-Object { $_.Subject -match 'Avast|AVG|Kaspersky|ESET|Bitdefender|Zscaler' }
    $jbr = if ($env:JAVA_HOME) { $env:JAVA_HOME } else { 'C:\Program Files\Android\Android Studio\jbr' }
    if ($scanRoots -and (Test-Path "$jbr\bin\keytool.exe")) {
        $trustDir = Join-Path $root 'Intermediate\JavaTrust'
        New-Item -ItemType Directory -Force $trustDir | Out-Null
        $store = Join-Path $trustDir 'cacerts-with-scanners'
        Copy-Item "$jbr\lib\security\cacerts" $store -Force
        $i = 0
        $ErrorActionPreference = 'Continue'   # keytool reports success on stderr
        foreach ($c in $scanRoots) {
            $cer = Join-Path $trustDir "scanner-$i.cer"
            [IO.File]::WriteAllBytes($cer, $c.Export('Cert'))
            & "$jbr\bin\keytool.exe" -importcert -noprompt -alias "scanner-$i" -file $cer -keystore $store -storepass changeit 2>&1 | Out-Null
            $i++
        }
        $ErrorActionPreference = 'Stop'
        $env:JAVA_TOOL_OPTIONS = "-Djavax.net.ssl.trustStore=$store -Djavax.net.ssl.trustStorePassword=changeit"
        Write-Host "Java trust store: added $i HTTPS-scanning root(s) for Gradle downloads"
    }
}

switch ($Platform) {
    'Win64'   { $args += '-prereqs' }
    'Android' { $args += '-cookflavor=ASTC'; if ($Release) { $args += '-distribution' } }
    'IOS'     { if ($Release) { $args += '-distribution' } }
}
& "$engine\Engine\Build\BatchFiles\RunUAT.bat" @args
if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed ($LASTEXITCODE)" }
Get-ChildItem $out -Recurse -Include *.exe, *.apk, *.aab, *.ipa -ErrorAction SilentlyContinue | Select-Object FullName, Length
