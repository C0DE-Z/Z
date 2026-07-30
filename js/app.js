// Z Video Editor - Auto Fetch Latest Release from GitHub API
const GITHUB_REPO = 'C0DE-Z/Z';
const LATEST_RELEASE_URL = `https://api.github.com/repos/${GITHUB_REPO}/releases/latest`;
const RELEASES_URL = `https://api.github.com/repos/${GITHUB_REPO}/releases`;
const TAGS_URL = `https://api.github.com/repos/${GITHUB_REPO}/tags`;
const FALLBACK_DOWNLOAD_URL = `https://github.com/${GITHUB_REPO}/releases`;

// Detect the user's operating system
function detectOS() {
    const ua = navigator.userAgent.toLowerCase();
    const platform = (navigator.userAgentData?.platform || navigator.platform || '').toLowerCase();
    if (platform.includes('win') || ua.includes('windows')) return 'windows';
    if (platform.includes('mac') || ua.includes('mac os')) return 'macos';
    if (platform.includes('linux') || ua.includes('linux')) return 'linux';
    return 'windows';
}

// Pick asset by matching platform keywords IN THE FILENAME, then extension
function pickAssetForOS(assets, os) {
    if (!assets || assets.length === 0) return null;

    // Keywords that MUST appear in the filename for each platform
    const platformKeywords = {
        windows: ['windows', 'win64', 'win-x64', 'win_x64'],
        macos:   ['macos', 'mac-', 'osx', 'darwin'],
        linux:   ['linux', 'ubuntu', 'appimage'],
    };

    // Extension fallbacks if no keyword match
    const extensionPrefs = {
        windows: ['.exe', '-windows.zip', '-win.zip'],
        macos:   ['.dmg'],
        linux:   ['.tar.gz', '.appimage', '.deb'],
    };

    const keywords = platformKeywords[os] || platformKeywords.windows;
    const exts = extensionPrefs[os] || extensionPrefs.windows;

    const name = (a) => a.name.toLowerCase();

    // 1. Try keyword match first (most reliable)
    for (const kw of keywords) {
        const match = assets.find(a => name(a).includes(kw));
        if (match) return match.browser_download_url;
    }

    // 2. Try extension match
    for (const ext of exts) {
        const match = assets.find(a => name(a).endsWith(ext));
        if (match) return match.browser_download_url;
    }

    // 3. Last resort: releases page
    return null;
}

async function fetchLatestRelease() {
    let tagName = 'Beta';
    let downloadUrl = FALLBACK_DOWNLOAD_URL;
    const os = detectOS();

    try {
        let res = await fetch(LATEST_RELEASE_URL);

        if (res.ok) {
            const data = await res.json();
            tagName = data.tag_name || 'Beta';
            downloadUrl = pickAssetForOS(data.assets, os) || data.html_url || FALLBACK_DOWNLOAD_URL;
        } else {
            res = await fetch(RELEASES_URL);
            if (res.ok) {
                const releases = await res.json();
                if (releases && releases.length > 0) {
                    tagName = releases[0].tag_name || 'Beta';
                    downloadUrl = pickAssetForOS(releases[0].assets, os) || releases[0].html_url || FALLBACK_DOWNLOAD_URL;
                } else {
                    const tagRes = await fetch(TAGS_URL);
                    if (tagRes.ok) {
                        const tags = await tagRes.json();
                        if (tags && tags.length > 0) {
                            tagName = tags[0].name || 'Beta';
                            downloadUrl = `https://github.com/${GITHUB_REPO}/releases/tag/${tagName}`;
                        }
                    }
                }
            }
        }
    } catch (err) {
        console.warn('GitHub API fetch failed:', err);
    }

    const formattedTag = /^\d/.test(tagName) ? `v${tagName}` : tagName;
    const osLabels = { windows: 'Windows', macos: 'macOS', linux: 'Linux' };
    const osLabel = osLabels[os] || 'Windows';

    document.querySelectorAll('.brand-tag').forEach(el => {
        el.textContent = formattedTag;
    });

    document.querySelectorAll('.download-version').forEach(el => {
        el.textContent = `Z VIDEO EDITOR - RELEASE ${formattedTag.toUpperCase()}`;
    });

    document.querySelectorAll('.btn-download-nav').forEach(el => {
        el.innerHTML = `
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
              <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/>
              <polyline points="7 10 12 15 17 10"/>
              <line x1="12" y1="15" x2="12" y2="3"/>
            </svg>
            Download ${formattedTag} for ${osLabel}
        `;
        el.href = downloadUrl;
    });

    document.querySelectorAll('.btn-primary').forEach(el => {
        el.href = downloadUrl;
    });

    console.log(`Z Video Editor: OS=${os}, tag=${formattedTag}, url=${downloadUrl}`);
}

document.addEventListener('DOMContentLoaded', fetchLatestRelease);
