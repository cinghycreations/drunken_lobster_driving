/// <reference path="../docs/txiki-api.d.ts" />

// Creates a local release tag of the form X.Y.Z[-W]+B, where B is one more
// than the highest build number already tagged for X.Y.Z[-W].
// The tag is not pushed.
//
// Usage: tjs run scripts/create-release.js [--help] X.Y.Z[-W]

import path from 'tjs:path';

// Semantic version without build metadata (https://semver.org).
const SEMVER_RE = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?$/;

const repoDir = path.join(import.meta.dirname, '..');

async function git(...args) {
    const proc = tjs.spawn([ 'git', ...args ], { cwd: repoDir, stdout: 'pipe', stderr: 'pipe' });
    const [ out, err, status ] = await Promise.all([ proc.stdout.text(), proc.stderr.text(), proc.wait() ]);

    if (status.exit_status !== 0) {
        throw new Error(`git ${args.join(' ')} failed: ${err.trim()}`);
    }

    return out;
}

const USAGE = 'Usage: tjs run scripts/create-release.js [--help] X.Y.Z[-W]';

const HELP = `${USAGE}

Creates a local Git tag X.Y.Z[-W]+B for the current commit, where B is one more
than the highest build number already tagged for X.Y.Z[-W] (1 if there is none).
The tag is not pushed.

Version format (semantic versioning, without build metadata):
  X, Y, Z   Major, minor and patch numbers. Required. Non-negative integers
            without leading zeros (e.g. 1.0.0, 0.12.3).
  -W        Prerelease tag. Optional. One or more dot-separated identifiers
            made of [0-9A-Za-z-]; numeric identifiers must not have leading
            zeros (e.g. 1.0.0-beta, 1.0.0-rc.2).

Options:
  -h, --help  Print this help and exit.

Examples:
  tjs run scripts/create-release.js 1.2.0         -> 1.2.0+1, 1.2.0+2, ...
  tjs run scripts/create-release.js 1.2.0-beta.1  -> 1.2.0-beta.1+1, ...`;

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
    fail('Expected exactly one argument: the version.');
}

const version = args[0];

if (!SEMVER_RE.test(version)) {
    fail(`Invalid version "${version}": expected X.Y.Z or X.Y.Z-W.`);
}

try {
    const tags = (await git('tag', '--list')).split('\n').map(t => t.trim());
    const prefix = `${version}+`;
    const builds = tags
        .filter(t => t.startsWith(prefix) && /^\d+$/.test(t.slice(prefix.length)))
        .map(t => parseInt(t.slice(prefix.length), 10));
    const lastBuild = builds.length > 0 ? Math.max(...builds) : 0;
    const newTag = `${version}+${lastBuild + 1}`;

    console.log(`Found ${builds.length} existing tag(s) for ${version}${builds.length > 0 ? `, last build ${lastBuild}` : ''}.`);

    await git('tag', newTag);

    console.log(`Created tag ${newTag}. Push it with: git push origin ${newTag}`);
} catch (e) {
    console.error(e.message);
    tjs.exit(1);
}
