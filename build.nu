def 'main clean' [] {
    rm --recursive --force temp/build temp/install android-project/.gradle android-project/build android-project/drunken_lobster_driving/build
}

def 'main build android' [version: string] {
    let build_number = git rev-list --count --all | into int
    let commit = git log --format=%H -1
    let release_version = $'($version).($build_number)'

    git restore android-project/drunken_lobster_driving/build.gradle
    open android-project/drunken_lobster_driving/build.gradle |
        str replace 'versionCode 1' $'versionCode ($build_number)' |
        str replace 'versionName "1.0"' $'versionName "($release_version)"' |
        save --force android-project/drunken_lobster_driving/build.gradle

    cmake --preset android-dev -D CC_BUILD_VERSION=$'($version)' -D CC_BUILD_PLATFORM=android -D CC_BUILD_CONFIG=dev -D CC_BUILD_NUMBER=$'($build_number)' -D CC_BUILD_COMMIT=$'($commit)'
    cmake --build --preset android-dev --target install

    cmake --preset android-release -D CC_BUILD_VERSION=$'($version)' -D CC_BUILD_PLATFORM=android -D CC_BUILD_CONFIG=release -D CC_BUILD_NUMBER=$'($build_number)' -D CC_BUILD_COMMIT=$'($commit)'
    cmake --build --preset android-release --target install
}

def 'main build windows' [version: string] {
    let build_number = git rev-list --count --all | into int
    let commit = git log --format=%H -1
    let release_version = $'($version).($build_number)'

    cmake --preset windows-dev -D CC_BUILD_VERSION=$'($version)' -D CC_BUILD_PLATFORM=windows -D CC_BUILD_CONFIG=dev -D CC_BUILD_NUMBER=$'($build_number)' -D CC_BUILD_COMMIT=$'($commit)'
    cmake --build --preset windows-dev --target install

    cmake --preset windows-release -D CC_BUILD_VERSION=$'($version)' -D CC_BUILD_PLATFORM=windows -D CC_BUILD_CONFIG=release -D CC_BUILD_NUMBER=$'($build_number)' -D CC_BUILD_COMMIT=$'($commit)'
    cmake --build --preset windows-release --target install
}

def 'main upload windows' [version: string] {
    let build_number = git rev-list --count --all | into int
    let commit = git log --format=%H -1
    let release_version = $'($version).($build_number)'

    temp/butler-windows-amd64/butler.exe push temp/install/windows-release cinghycreations/drunken-lobster-driving:windows --userversion $release_version
}

def 'main upload android' [version: string] {
    let build_number = git rev-list --count --all | into int
    let commit = git log --format=%H -1
    let release_version = $'($version).($build_number)'

    temp/butler-windows-amd64/butler.exe push temp/install/android-release/drunken_lobster_driving-release.apk cinghycreations/drunken-lobster-driving:android --userversion $release_version
}

def 'main' [version: string] {
    main clean
    main build windows $version
    main build android $version
    main upload windows $version
    main upload android $version
}
