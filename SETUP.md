# EMBERHOME — setup for building and releasing

Windows builds work on this machine today. Android and iOS need a few one-time installs and accounts
that only you can do (logins, licence agreements, payments). This is the list, in order.

## 0. Once: add the mobile target platforms to Unreal

The installed UE 5.8 currently has **no Android or iOS platform support**.

1. Open the **Epic Games Launcher** → **Unreal Engine** → **Library**.
2. On the 5.8 tile, click the arrow → **Options**.
3. Under **Target Platforms**, tick **Android** and **iOS**, then **Apply** (a few GB download).

## 1. Android

### Tools (once)
1. Install **Android Studio** (the version Epic lists for 5.8 on "Set Up Android SDK, NDK and Android Studio").
   Open it once, finish the setup wizard, and in *SDK Manager* install the SDK Platform for Android 15 (API 35)
   and "Android SDK Command-line Tools".
2. Close Android Studio and run, from a normal Command Prompt:
   ```
   "C:\Program Files\Epic Games\UE_5.8\Engine\Extras\Android\SetupAndroid.bat"
   ```
   It installs the exact NDK and build tools this engine version expects and sets `ANDROID_HOME`/`NDKROOT`.
3. Sign out and back in (or reboot) so the new environment variables are seen.

### Two fixes this PC needed (do the same on a new machine if Gradle fails)
- **"Unsupported class file major version 69"**: recent Android Studio ships Java 25, which Gradle 8
  (used by UE 5.8) can't run on. Install JDK 21 (`winget install --id Microsoft.OpenJDK.21 -e --source winget`)
  and add this line to `%USERPROFILE%\.gradle\gradle.properties`:
  `org.gradle.java.home=C:/Program Files/Microsoft/jdk-21.0.12.101-hotspot` (adjust to the installed version).
- **"PKIX path building failed"** while Gradle downloads: antivirus HTTPS scanning (Avast/AVG Web Shield).
  `Tools\Build\package.ps1` handles this automatically by giving Java a trust store that includes the antivirus root.

### Build a test APK
```
powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Android -Config Development
```
The APK lands in `Packaged\Android\`. Install it with the generated `Install_Hollowlight-arm64.bat`
(phone connected by USB, developer mode + USB debugging on).

### Rollout day (everything below is already prepared)
1. Once, create the upload key yourself (you type the password; nothing is stored in git):
   ```
   powershell -ExecutionPolicy Bypass -File Tools\Build\create_upload_key.ps1
   ```
   Back up `Build\Android\emberhome-upload.keystore` and `Config\Android\AndroidEngine.ini`.
2. Build the signed bundle:
   ```
   powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Android -Release
   ```
   Upload `Packaged\Android\Hollowlight-Android-Shipping.aab` (the `_universal.apk` next to it is for your own phone).
3. Store graphics are in `Build\Android\PlayStore\` (icon 512, feature graphic 1024x500, 8 screenshots);
   regenerate with `Tools\Build\store_graphics.ps1` after a new capture. Text: `STORE_LISTING.md`.
4. For every update after the first, raise `StoreVersion` (and `VersionDisplayName`) in `Config/DefaultEngine.ini`.

### Release build for Google Play (reference)
1. Create an upload key (keep it and the passwords safe and **out of git**):
   ```
   keytool -genkey -v -keystore emberhome-upload.keystore -alias emberhome-upload -keyalg RSA -keysize 2048 -validity 10000
   ```
   Put the keystore in `Build\Android\` and set, in the editor under *Project Settings → Android →
   Distribution Signing*, the keystore file, alias and passwords. (These end up in `Config/DefaultEngine.ini`;
   if the repo is ever public, move the passwords into a local, git-ignored config instead.)
2. In *Project Settings → Android*, tick **Generate bundle (AAB)**.
3. Build:
   ```
   powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Android -Release
   ```
4. In the **Google Play Console** (one-time $25 developer registration): create the app, upload the `.aab`
   to an internal-testing track first, fill in the listing from `STORE_LISTING.md`, the privacy policy URL
   (`https://<your site>/privacy.html`), Data safety (**no data collected or shared**), content rating
   questionnaire (no violence against people, mild peril), target audience, and ads (**no ads**).
   Before each update, raise `StoreVersion` in `Config/DefaultEngine.ini`.

## 2. iOS / iPadOS

iOS apps can only be compiled, signed and uploaded with **Xcode on a Mac**. Two ways:

- **Build on the Mac (simplest):** install UE 5.8 (with iOS) and Xcode there, copy or clone this repo,
  open `Hollowlight.uproject`, and package for iOS from *Platforms → iOS → Package Project*.
- **Build from this PC with the Mac as a remote builder:** in *Project Settings → iOS → Build*, set
  *Remote Server Name* to the Mac's address and your Mac user name, then *Generate SSH Key*. After that
  `Tools\Build\package.ps1 -Platform IOS` compiles remotely.

Either way you need an **Apple Developer Program** membership ($99/year). In
*Project Settings → iOS* choose **Automatic Signing** with your team ID (bundle id `com.brainrotinteractive.emberhome`
is already set). For the App Store: `package.ps1 -Platform IOS -Release`, upload the `.ipa` with
Xcode's Organizer or Transporter, and in **App Store Connect** fill in the listing from `STORE_LISTING.md`,
the privacy policy URL, and the privacy "nutrition label" (**Data Not Collected**). Test with TestFlight first.

## 3. Website

`Website/` is a finished static site. To put it online with GitHub Pages:

1. Create a repository on GitHub and push this project to it (`main` branch).
2. On GitHub: *Settings → Pages → Build and deployment → Source: **GitHub Actions***.
3. The workflow in `.github/workflows/pages.yml` publishes `Website/` on every push that changes it.
   The site appears at `https://<user>.github.io/<repo>/`. For your own domain, add it under
   *Settings → Pages → Custom domain* and point a CNAME record at `<user>.github.io`.

When the store pages exist, replace the two "Coming soon" buttons in `Website/index.html` with the
real Google Play and App Store links, and link the Windows download.

## 4. Useful checks before any release

```
powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1         # every level + checkpoint winnable
powershell -ExecutionPolicy Bypass -File Tools\Validation\run_tests.ps1   # automation tests
powershell -ExecutionPolicy Bypass -File Tools\Validation\capture.ps1     # screenshots of every screen
powershell -ExecutionPolicy Bypass -File Tools\Validation\capture.ps1 -Phone
```
