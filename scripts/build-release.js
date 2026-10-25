/// <reference path="../docs/txiki-api.d.ts" />

// Configures, builds and installs, from clean build and install folders, the
// dev and release configurations of the game for a platform, stamping the
// version from the release tag on HEAD.
//
// Usage: tjs run scripts/build-release.js [--help] <platform>

import path from 'tjs:path';

const PLATFORMS = [ 'windows', 'html5' ];
const CONFIGS = [ 'dev', 'release' ];

// Release tag as created by create-release.js: X.Y.Z[-W]+B.
const RELEASE_TAG_RE = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?\+(\d+)$/;

const USAGE = `Usage: tjs run scripts/build-release.js [--help] <${PLATFORMS.join('|')}>`;

const HELP = `${USAGE}

Configures, builds and installs the game for the given platform, in both the
dev and release configurations, using the CMake presets <platform>-dev and
<platform>-release. Output goes to temp/build/<preset> and the installed game
to temp/install/<preset>; both folders are deleted first, so every build
starts from scratch.

The version is read from the release tag on the current commit, which must
have the form X.Y.Z[-W]+B (see scripts/create-release.js). X, Y, Z and B are
passed to CMake as the project major, minor, patch and tweak version; the
prerelease tag W is not passed, as CMake versions are numeric only.

Platforms:
  windows   Visual Studio build.
  html5     Emscripten build. Requires the EMSDK_HOME environment variable.

Options:
  -h, --help  Print this help and exit.

Example:
  tjs run scripts/create-release.js 1.2.0
  tjs run scripts/build-release.js windows`;

const repoDir = path.join(import.meta.dirname, '..');

async function run(args, { capture = false } = {}) {
    const stdio = capture ? 'pipe' : 'inherit';
    const proc = tjs.spawn(args, { cwd: repoDir, stdout: stdio, stderr: stdio });
    const [ out, err, status ] = await Promise.all([ proc.stdout?.text(), proc.stderr?.text(), proc.wait() ]);

    if (status.exit_status !== 0) {
        throw new Error(`${args.join(' ')} failed with exit code ${status.exit_status}${err ? `: ${err.trim()}` : ''}`);
    }

    return out;
}

function fail(message) {
    console.error(message);
    console.error(USAGE);
    console.error('Run with --help for details.');
    tjs.exit(1);
}

// tjs.args is [ exe, 'run', script, ...scriptArgs ].
const args = tjs.args.slice(tjs.args.indexOf('run') + 2);

if (args.includes('--help') || args.includes('-h')) {
    console.log(HELP);
    tjs.exit(0);
}

if (args.length !== 1) {
    fail('Expected exactly one argument: the platform.');
}

const platform = args[0];

if (!PLATFORMS.includes(platform)) {
    fail(`Invalid platform "${platform}": expected one of ${PLATFORMS.join(', ')}.`);
}

if (platform === 'html5' && !tjs.env.EMSDK_HOME) {
    console.error('The EMSDK_HOME environment variable must point to the Emscripten SDK.');
    tjs.exit(1);
}

try {
    const tags = (await run([ 'git', 'tag', '--points-at', 'HEAD' ], { capture: true }))
        .split('\n')
        .map(t => t.trim())
        .filter(t => RELEASE_TAG_RE.test(t));

    if (tags.length === 0) {
        throw new Error('The current commit has no release tag. Create one with scripts/create-release.js.');
    }

    if (tags.length > 1) {
        throw new Error(`The current commit has more than one release tag: ${tags.join(', ')}.`);
    }

    const [ , major, minor, patch, , build ] = RELEASE_TAG_RE.exec(tags[0]);

    const presets = CONFIGS.map(config => `${platform}-${config}`);

    // Same folders as binaryDir and installDir in CMakePresets.json.
    for (const preset of presets) {
        for (const dir of [ path.join(repoDir, 'temp', 'build', preset), path.join(repoDir, 'temp', 'install', preset) ]) {
            console.log(`Removing ${dir}`);

            try {
                await tjs.remove(dir);
            } catch (e) {
                throw new Error(`Could not remove ${dir}: ${e.message}. Close any program using it (e.g. Visual Studio) and retry.`);
            }
        }
    }

    console.log(`Building ${platform} version ${tags[0]}.`);

    for (const preset of presets) {
        await run([
            'cmake', '--preset', preset,
            '-D', `CMAKE_PROJECT_VERSION_MAJOR=${major}`,
            '-D', `CMAKE_PROJECT_VERSION_MINOR=${minor}`,
            '-D', `CMAKE_PROJECT_VERSION_PATCH=${patch}`,
            '-D', `CMAKE_PROJECT_VERSION_TWEAK=${build}`,
        ]);
        await run([ 'cmake', '--build', '--preset', preset, '--target', 'install' ]);
    }

    console.log(`Built ${platform} version ${tags[0]} in temp/install/${platform}-{${CONFIGS.join(',')}}.`);
} catch (e) {
    console.error(e.message);
    tjs.exit(1);
}
