const GITHUB_REPO = 'C0DE-Z/Z';
const LATEST_RELEASE_URL = `https://api.github.com/repos/${GITHUB_REPO}/releases/latest`;
const RELEASES_URL = `https://api.github.com/repos/${GITHUB_REPO}/releases`;
const TAGS_URL = `https://api.github.com/repos/${GITHUB_REPO}/tags`;
const FALLBACK_DOWNLOAD_URL = `https://github.com/${GITHUB_REPO}/releases`;

async function fetchLatestRelease() {
    let tagName = 'Beta';
    let downloadUrl = FALLBACK_DOWNLOAD_URL;

    try {
        let res = await fetch(LATEST_RELEASE_URL);
        
        if (res.ok) {
            const data = await res.json();
            tagName = data.tag_name || 'Beta';
            if (data.assets && data.assets.length > 0) {
                const asset = data.assets.find(a => 
                    a.name.endsWith('.zip') || 
                    a.name.endsWith('.exe') || 
                    a.name.endsWith('.AppImage') || 
                    a.name.endsWith('.dmg')
                ) || data.assets[0];
                downloadUrl = asset.browser_download_url;
            } else if (data.html_url) {
                downloadUrl = data.html_url;
            }
        } else {
            res = await fetch(RELEASES_URL);
            if (res.ok) {
                const releases = await res.json();
                if (releases && releases.length > 0) {
                    tagName = releases[0].tag_name || 'Beta';
                    downloadUrl = releases[0].html_url || FALLBACK_DOWNLOAD_URL;
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
        console.warn('GitHub API fetch fallback:', err);
    }
    const formattedTag = /^\d/.test(tagName) ? `v${tagName}` : tagName;
    document.querySelectorAll('.brand-tag').forEach(el => {
        el.textContent = formattedTag;
    });

    document.querySelectorAll('.download-version').forEach(el => {
        el.textContent = `Z VIDEO EDITOR - RELEASE ${formattedTag.toUpperCase()}`;
    });

    document.querySelectorAll('.btn-download-nav').forEach(el => {
        el.innerHTML = `
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
            Download ${formattedTag}
        `;
        el.href = downloadUrl;
    });

    document.querySelectorAll('.btn-primary').forEach(el => {
        el.href = downloadUrl;
    });
}

document.addEventListener('DOMContentLoaded', () => {
    fetchLatestRelease();
});
