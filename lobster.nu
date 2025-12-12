export def 'get_build_number' [] { git rev-list --count master | into int }
export def 'get_release_version' [version: string, build_number : int ] { $'($version).($build_number)' }

export def clean [] {
    rm --recursive --force temp/build temp/install android-project/.gradle android-project/build android-project/drunken_lobster_driving/build
}

export def 'build android' [version: string] {
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

export def 'build windows' [version: string] {
    let build_number = get_build_number
    let release_version = get_release_version $version $build_number

    cmake --preset windows-dev -D CMAKE_PROJECT_VERSION=$'($release_version)'
    cmake --build --preset windows-dev --target install

    cmake --preset windows-release -D CMAKE_PROJECT_VERSION=$'($release_version)'
    cmake --build --preset windows-release --target install
}

export def 'build html5' [version: string] {
    let build_number = get_build_number
    let release_version = get_release_version $version $build_number

    cmake --preset html5-dev -D CMAKE_PROJECT_VERSION=$'($release_version)'
    cmake --build --preset html5-dev --target install

    cmake --preset html5-release -D CMAKE_PROJECT_VERSION=$'($release_version)'
    cmake --build --preset html5-release --target install
}

export def 'upload windows' [version: string] {
    let build_number = get_build_number
    let release_version = get_release_version $version $build_number

    temp/butler-windows-amd64/butler.exe push temp/install/windows-release cinghycreations/drunken-main-driving:windows --userversion $release_version
}

export def 'upload android' [version: string] {
    let build_number = get_build_number
    let release_version = get_release_version $version $build_number

    temp/butler-windows-amd64/butler.exe push temp/install/android-release/drunken_lobster_driving-release.apk cinghycreations/drunken-main-driving:android --userversion $release_version
}

export def 'upload html5' [version: string] {
    let build_number = get_build_number
    let release_version = get_release_version $version $build_number

    temp/butler-windows-amd64/butler.exe push temp/install/html5-release cinghycreations/drunken-main-driving:html5 --userversion $release_version
}

export def build [version: string] {
    build windows $version
    build android $version
    build html5 $version
}

export def upload [version: string] {
    upload windows $version
    upload android $version
    upload html5 $version
}

export def tag [version: string] {
    let build_number = get_build_number
    let release_version = get_release_version $version $build_number

    git tag -f $release_version    
}

export def release [version: string] {
    clean
    build $version
    upload $version
    tag $version
}
