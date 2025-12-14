# Gets the build number looking at Git history
export def 'get-build-number' []: nothing -> int { git rev-list --count master | into int }

# Creates a release version string
export def 'get-release-version' [version: string, build_number : int ]: nothing -> string { $'($version).($build_number)' }

# Clean all build artifacts
export def clean []: nothing -> nothing {
    rm --recursive --force temp/build temp/install android-project/.gradle android-project/build android-project/drunken_lobster_driving/build
}

# Build the game for Android
export def 'build android' [version: string]: nothing -> nothing {
    let build_number = get-build-number
    let release_version = get-release-version $version $build_number

    git restore android-project/drunken_lobster_driving/build.gradle
    open android-project/drunken_lobster_driving/build.gradle |
        str replace 'versionCode 1' $'versionCode ($build_number)' |
        str replace 'versionName "1.0"' $'versionName "($release_version)"' |
        save --force android-project/drunken_lobster_driving/build.gradle

    cmake --preset android-dev -D CMAKE_PROJECT_VERSION=$'($release_version)'
    cmake --build --preset android-dev --target install

    cmake --preset android-release -D CMAKE_PROJECT_VERSION=$'($release_version)'
    cmake --build --preset android-release --target install
}

# Build the game for Windows
export def 'build windows' [version: string]: nothing -> nothing {
    let build_number = get-build-number
    let release_version = get-release-version $version $build_number

    cmake --preset windows-dev -D CMAKE_PROJECT_VERSION=$'($release_version)'
    cmake --build --preset windows-dev --target install

    cmake --preset windows-release -D CMAKE_PROJECT_VERSION=$'($release_version)'
    cmake --build --preset windows-release --target install
}

# Build the game for HTML5
export def 'build html5' [version: string]: nothing -> nothing {
    let build_number = get-build-number
    let release_version = get-release-version $version $build_number

    cmake --preset html5-dev -D CMAKE_PROJECT_VERSION=$'($release_version)'
    cmake --build --preset html5-dev --target install

    cmake --preset html5-release -D CMAKE_PROJECT_VERSION=$'($release_version)'
    cmake --build --preset html5-release --target install
}

# Upload the Windows build to Itch
export def 'upload windows' [version: string]: nothing -> nothing {
    let build_number = get-build-number
    let release_version = get-release-version $version $build_number

    temp/butler-windows-amd64/butler.exe push temp/install/windows-release cinghycreations/drunken-lobster-driving:windows --userversion $release_version
}

# Upload the Android build to Itch
export def 'upload android' [version: string]: nothing -> nothing {
    let build_number = get-build-number
    let release_version = get-release-version $version $build_number

    temp/butler-windows-amd64/butler.exe push temp/install/android-release/drunken_lobster_driving-release.apk cinghycreations/drunken-lobster-driving:android --userversion $release_version
}

# Upload the HTML5 build to Itch
export def 'upload html5' [version: string]: nothing -> nothing {
    let build_number = get-build-number
    let release_version = get-release-version $version $build_number

    temp/butler-windows-amd64/butler.exe push temp/install/html5-release cinghycreations/drunken-lobster-driving:html5 --userversion $release_version
}

# Build the game for all platforms
export def build [version: string]: nothing -> nothing {
    build windows $version
    build android $version
    build html5 $version
}

# Upload the game for all platforms to Itch
export def upload [version: string]: nothing -> nothing {
    upload windows $version
    upload android $version
    upload html5 $version
}

# Tags the current commit on Git with the build string
export def tag [version: string]: nothing -> nothing {
    let build_number = get-build-number
    let release_version = get-release-version $version $build_number

    git tag -f $release_version    
}

# Clean, Build, Upload and Tag
export def release [version: string]: nothing -> nothing {
    clean
    build $version
    upload $version
    tag $version
}
