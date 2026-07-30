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

    if (ua.includes('win') || platform.includes('win')) return 'windows';
    if (ua.includes('mac') || platform.includes('mac')) return 'macos';
    if (ua.includes('linux') || platform.includes('linux')) return 'linux';
    return 'windows'; // default fallback
}

// Pick the best asset for the user's OS from the release assets list
function pickAssetForOS(assets, os) {
    if (!assets || assets.length === 0) return null;

    const preferences = {
        windows: ['.zip', '.exe', '.msi'],
        macos:   ['.dmg', '-macos', '-mac'],
        linux:   ['.tar.gz', '.AppImage', '.deb', '.rpm'],
    };

    const prefs = preferences[os] || preferences.windows;

    // Try each preferred extension in order
    for (const ext of prefs) {
        const match = assets.find(a => a.name.toLowerCase().includes(ext));
        if (match) return match.browser_download_url;
    }

    // Last resort: first asset
    return assets[0].browser_download_url;
}

async function fetchLatestRelease() {
    let tagName = 'Beta';
    let downloadUrl = FALLBACK_DOWNLOAD_URL;
    const os = detectOS();

    try {
        // 1. Try latest official release
        let res = await fetch(LATEST_RELEASE_URL);

        if (res.ok) {
            const data = await res.json();
            tagName = data.tag_name || 'Beta';
            const picked = pickAssetForOS(data.assets, os);
            downloadUrl = picked || data.html_url || FALLBACK_DOWNLOAD_URL;
        } else {
            // 2. Fall back to all releases
            res = await fetch(RELEASES_URL);
            if (res.ok) {
                const releases = await res.json();
                if (releases && releases.length > 0) {
                    tagName = releases[0].tag_name || 'Beta';
                    const picked = pickAssetForOS(releases[0].assets, os);
                    downloadUrl = picked || releases[0].html_url || FALLBACK_DOWNLOAD_URL;
                } else {
                    // 3. Fall back to git tags
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
        console.warn('GitHub API fetch failed, using fallback:', err);
    }

    // Format tag: only prepend 'v' if it starts with a digit (e.g. 1.0.0 -> v1.0.0, Beta -> Beta)
    const formattedTag = /^\d/.test(tagName) ? `v${tagName}` : tagName;

    // Update brand version badge
    document.querySelectorAll('.brand-tag').forEach(el => {
        el.textContent = formattedTag;
    });

    // Update download version heading
    document.querySelectorAll('.download-version').forEach(el => {
        el.textContent = `Z VIDEO EDITOR - RELEASE ${formattedTag.toUpperCase()}`;
    });

    // Update nav download button
    const osLabels = { windows: 'Windows', macos: 'macOS', linux: 'Linux' };
    const osLabel = osLabels[os] || 'Windows';

    document.querySelectorAll('.btn-download-nav').forEach(el => {
        el.innerHTML = `
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
            Download ${formattedTag} for ${osLabel}
        `;
        el.href = downloadUrl;
    });

    // Update all primary download buttons
    document.querySelectorAll('.btn-primary').forEach(el => {
        el.href = downloadUrl;
    });

    console.log(`Z Video Editor: detected OS=${os}, tag=${formattedTag}, download=${downloadUrl}`);
}

document.addEventListener('DOMContentLoaded', () => {
    fetchLatestRelease();
});
