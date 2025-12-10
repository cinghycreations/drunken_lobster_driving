def get_build_number [] { git rev-list --count master | into int }
def get_release_version [version: string, build_number : int ] { $'($version).($build_number)' }

def 'main clean' [] {
    rm --recursive --force temp/build temp/install android-project/.gradle android-project/build android-project/drunken_lobster_driving/build
}

def 'main build android' [version: string] {
    let build_number = get_build_number
    let release_version = get_release_version $version $build_number

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

def 'main build windows' [version: string] {
    let build_number = get_build_number
    let release_version = get_release_version $version $build_number

    cmake --preset windows-dev -D CMAKE_PROJECT_VERSION=$'($release_version)'
    cmake --build --preset windows-dev --target install

    cmake --preset windows-release -D CMAKE_PROJECT_VERSION=$'($release_version)'
    cmake --build --preset windows-release --target install
}

def 'main upload windows' [version: string] {
    let build_number = get_build_number
    let release_version = get_release_version $version $build_number

    temp/butler-windows-amd64/butler.exe push temp/install/windows-release cinghycreations/drunken-lobster-driving:windows --userversion $release_version
}

def 'main upload android' [version: string] {
    let build_number = get_build_number
    let release_version = get_release_version $version $build_number

    temp/butler-windows-amd64/butler.exe push temp/install/android-release/drunken_lobster_driving-release.apk cinghycreations/drunken-lobster-driving:android --userversion $release_version
}

def 'main build' [version: string] {
    main build windows $version
    main build android $version
}

def 'main upload' [version: string] {
    main upload windows $version
    main upload android $version
}

def 'main' [version: string] {
    main clean
    main build $version
    main upload $version
}
