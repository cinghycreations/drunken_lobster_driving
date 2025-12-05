def 'main clean' [] {
    rm --recursive --force temp/build temp/install
}

def 'main windows dev' [version: string] {
    let commit = (git log --format=%H -1)
    cmake --preset windows-dev -D CC_BUILD_VERSION=$'($version)' -D CC_BUILD_PLATFORM=windows -D CC_BUILD_CONFIG=dev -D CC_BUILD_COMMIT=$'($commit)'
    cmake --build --preset windows-dev --target install
}

def 'main windows release' [version: string] {
    let commit = (git log --format=%H -1)
    cmake --preset windows-release -D CC_BUILD_VERSION=$'($version)' -D CC_BUILD_PLATFORM=windows -D CC_BUILD_CONFIG=release -D CC_BUILD_COMMIT=$'($commit)'
    cmake --build --preset windows-release --target install
}

def 'main android dev' [version: string] {
    let commit = (git log --format=%H -1)
    cmake --preset android-dev -D CC_BUILD_VERSION=$'($version)' -D CC_BUILD_PLATFORM=android -D CC_BUILD_CONFIG=dev -D CC_BUILD_COMMIT=$'($commit)'
    cmake --build --preset android-dev --target install
}

def 'main android release' [version: string] {
    let commit = (git log --format=%H -1)
    cmake --preset android-release -D CC_BUILD_VERSION=$'($version)' -D CC_BUILD_PLATFORM=android -D CC_BUILD_CONFIG=release -D CC_BUILD_COMMIT=$'($commit)'
    cmake --build --preset android-release --target install
}

def 'main upload windows' [version: string] {
    temp/butler-windows-amd64/butler.exe push temp/install/windows-release cinghycreations/drunken-lobster-driving:windows --userversion $version
}

def 'main upload android' [version: string] {
    temp/butler-windows-amd64/butler.exe push temp/install/android-release cinghycreations/drunken-lobster-driving:android --userversion $version
}

def 'main build' [version: string] {
    main windows dev $version
    main windows release $version
    main android dev $version
    main android release $version
}

def 'main' [] {}