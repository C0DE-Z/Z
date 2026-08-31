const GITHUB_REPO = 'C0DE-Z/Z';
const LATEST_RELEASE_URL = `https://api.github.com/repos/${GITHUB_REPO}/releases/latest`;
const RELEASES_URL = `https://api.github.com/repos/${GITHUB_REPO}/releases`;
const FALLBACK_DOWNLOAD_URL = `https://github.com/${GITHUB_REPO}/releases`;

function detectOS() {
    const ua = navigator.userAgent.toLowerCase();
    const platform = (navigator.userAgentData?.platform || navigator.platform || '').toLowerCase();
    if (platform.includes('win') || ua.includes('windows')) return 'windows';
    if (platform.includes('mac') || ua.includes('mac os')) return 'macos';
    if (platform.includes('linux') || ua.includes('linux')) return 'linux';
    return 'windows';
}

function pickAssetForOS(assets, os) {
    if (!Array.isArray(assets)) return null;
    const name = asset => asset.name.toLowerCase();
    const preferences = {
        windows: [asset => name(asset).includes('setup-windows') && name(asset).endsWith('.exe'), asset => name(asset).includes('windows') && name(asset).endsWith('.zip')],
        macos: [asset => name(asset).includes('macos') && name(asset).endsWith('.zip')],
        linux: [asset => name(asset).includes('linux') && name(asset).endsWith('.tar.gz')],
    };
    return (preferences[os] || preferences.windows).map(matches => assets.find(matches)).find(Boolean) || null;
}

function setReleaseUI(release) {
    const os = detectOS();
    const tagName = release?.tag_name || 'Latest';
    const downloadUrl = pickAssetForOS(release?.assets, os)?.browser_download_url || release?.html_url || FALLBACK_DOWNLOAD_URL;
    const platform = { windows: 'Windows', macos: 'macOS', linux: 'Linux' }[os] || 'your platform';
    document.querySelectorAll('.brand-tag').forEach(el => { el.textContent = tagName; });
    document.querySelectorAll('.download-version').forEach(el => { el.textContent = `Z VIDEO EDITOR — ${tagName.toUpperCase()}`; });
    document.querySelectorAll('.btn-download-nav').forEach(el => { el.href = '#download'; });
    document.querySelectorAll('.btn-download-main').forEach(el => {
        el.href = downloadUrl;
        el.textContent = `Download ${tagName} for ${platform}`;
        el.setAttribute('aria-label', `Download Z ${tagName} for ${platform}`);
    });
}

async function fetchLatestRelease() {
    try {
        let response = await fetch(LATEST_RELEASE_URL, { headers: { Accept: 'application/vnd.github+json' } });
        if (!response.ok) {
            response = await fetch(RELEASES_URL, { headers: { Accept: 'application/vnd.github+json' } });
            const releases = response.ok ? await response.json() : [];
            setReleaseUI(releases[0]);
            return;
        }
        setReleaseUI(await response.json());
    } catch (error) {
        console.warn('Could not fetch the latest Z release:', error);
        setReleaseUI(null);
    }
}

function setupNavigation() {
    const toggle = document.querySelector('.nav-toggle');
    const menu = document.querySelector('#nav-menu');
    toggle?.addEventListener('click', () => {
        const open = menu.classList.toggle('is-open');
        toggle.setAttribute('aria-expanded', String(open));
    });
    menu?.querySelectorAll('a').forEach(link => link.addEventListener('click', () => {
        menu.classList.remove('is-open');
        toggle?.setAttribute('aria-expanded', 'false');
    }));
}

document.addEventListener('DOMContentLoaded', () => { setupNavigation(); fetchLatestRelease(); });
