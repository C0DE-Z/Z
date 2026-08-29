function copyCode(elementId) {
    const codeElement = document.getElementById(elementId);
    if (!codeElement) return;

    const textToCopy = codeElement.innerText;
    navigator.clipboard.writeText(textToCopy).then(() => {
        alert('Code snippet copied to clipboard!');
    }).catch(err => {
        console.error('Failed to copy code snippet:', err);
    });
}
