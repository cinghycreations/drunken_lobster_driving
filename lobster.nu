# Gets the build number looking at Git history
export def 'get-version-build' []: nothing -> int {
    git rev-list --count master | into int
}

# Creates a version struct
export def 'get-version' [release: string]: nothing -> record<major: int, minor: int, patch: int, build: int> {
    $release | parse '{major}.{minor}.{patch}' | into int major minor patch | get 0 | insert 'build' (get-version-build)
}

# Creates a version string
export def 'get-version-string' [release: string]: nothing -> string {
    get-version $release | $'($in.major).($in.minor).($in.patch)+($in.build)'
}

# Clean all build artifacts
export def clean []: nothing -> nothing {
    rm --recursive --force temp/build temp/install android-project/.gradle android-project/build android-project/drunken_lobster_driving/build
}

# Build the game for Android
export def 'build android' [release: string]: nothing -> nothing {
    let version = get-version $release
    let version_string = get-version-string $release

    git restore android-project/drunken_lobster_driving/build.gradle
    open android-project/drunken_lobster_driving/build.gradle |
        str replace 'versionCode 1' $'versionCode ($version.build)' |
        str replace 'versionName "1.0"' $'versionName "($version_string)"' |
        save --force android-project/drunken_lobster_driving/build.gradle

    cmake --preset android-dev -D CMAKE_PROJECT_VERSION_MAJOR=$'($version.major)' -D CMAKE_PROJECT_VERSION_MINOR=$'($version.minor)' -D CMAKE_PROJECT_VERSION_PATCH=$'($version.patch)' -D CMAKE_PROJECT_VERSION_TWEAK=$'($version.build)'
    cmake --build --preset android-dev --target install

    cmake --preset android-release -D CMAKE_PROJECT_VERSION_MAJOR=$'($version.major)' -D CMAKE_PROJECT_VERSION_MINOR=$'($version.minor)' -D CMAKE_PROJECT_VERSION_PATCH=$'($version.patch)' -D CMAKE_PROJECT_VERSION_TWEAK=$'($version.build)'
    cmake --build --preset android-release --target install
}

# Build the game for Windows
export def 'build windows' [release: string]: nothing -> nothing {
        let version = get-version $release
    let version_string = get-version-string $release

    cmake --preset windows-dev -D CMAKE_PROJECT_VERSION_MAJOR=$'($version.major)' -D CMAKE_PROJECT_VERSION_MINOR=$'($version.minor)' -D CMAKE_PROJECT_VERSION_PATCH=$'($version.patch)' -D CMAKE_PROJECT_VERSION_TWEAK=$'($version.build)'
    cmake --build --preset windows-dev --target install

    cmake --preset windows-release -D CMAKE_PROJECT_VERSION_MAJOR=$'($version.major)' -D CMAKE_PROJECT_VERSION_MINOR=$'($version.minor)' -D CMAKE_PROJECT_VERSION_PATCH=$'($version.patch)' -D CMAKE_PROJECT_VERSION_TWEAK=$'($version.build)'
    cmake --build --preset windows-release --target install
}

# Build the game for HTML5
export def 'build html5' [release: string]: nothing -> nothing {
        let version = get-version $release
    let version_string = get-version-string $release

    cmake --preset html5-dev -D CMAKE_PROJECT_VERSION_MAJOR=$'($version.major)' -D CMAKE_PROJECT_VERSION_MINOR=$'($version.minor)' -D CMAKE_PROJECT_VERSION_PATCH=$'($version.patch)' -D CMAKE_PROJECT_VERSION_TWEAK=$'($version.build)'
    cmake --build --preset html5-dev --target install

    cmake --preset html5-release -D CMAKE_PROJECT_VERSION_MAJOR=$'($version.major)' -D CMAKE_PROJECT_VERSION_MINOR=$'($version.minor)' -D CMAKE_PROJECT_VERSION_PATCH=$'($version.patch)' -D CMAKE_PROJECT_VERSION_TWEAK=$'($version.build)'
    cmake --build --preset html5-release --target install
}

# Upload the Windows build to Itch
export def 'upload windows' [release: string]: nothing -> nothing {
        let version = get-version $release
    let version_string = get-version-string $release

    temp/butler-windows-amd64/butler.exe push temp/install/windows-release cinghycreations/drunken-lobster-driving:windows --userversion $version_string
}

# Upload the Android build to Itch
export def 'upload android' [release: string]: nothing -> nothing {
    let version = get-version $release
    let version_string = get-version-string $release

    temp/butler-windows-amd64/butler.exe push temp/install/android-release/drunken_lobster_driving-release.apk cinghycreations/drunken-lobster-driving:android --userversion $version_string
}

# Upload the HTML5 build to Itch
export def 'upload html5' [release: string]: nothing -> nothing {
    let version = get-version $release
    let version_string = get-version-string $release

    temp/butler-windows-amd64/butler.exe push temp/install/html5-release cinghycreations/drunken-lobster-driving:html5 --userversion $version_string
}

# Build the game for all platforms
export def build [release: string]: nothing -> nothing {
    build windows $release
    build android $release
    build html5 $release
}

# Upload the game for all platforms to Itch
export def upload [release: string]: nothing -> nothing {
    upload windows $release
    upload android $release
    upload html5 $release
}

# Tags the current commit on Git with the build string
export def tag [release: string]: nothing -> nothing {
    let version = get-version $release
    let version_string = get-version-string $release

    git tag -f $version_string
}

# Clean, Build, Upload and Tag
export def release [release: string]: nothing -> nothing {
    clean
    build $release
    upload $release
    tag $release
}
