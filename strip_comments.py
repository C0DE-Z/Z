import os
import re

def process_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Pattern to match strings, single line comments, and multi-line comments
    pattern = re.compile(
        r'(\".*?(?<!\\)\"|\'.*?(?<!\\)\')|(/\*.*?\*/)|(//.*?$)',
        re.MULTILINE | re.DOTALL
    )

    def replacer(match):
        if match.group(1) is not None:
            # It's a string or char literal, return as is
            return match.group(1)
        elif match.group(2) is not None:
            # It's a block comment, return empty
            return ''
        elif match.group(3) is not None:
            # It's a line comment
            text = match.group(3)
            # Preserve special plugin metadata
            if text.startswith('// @name') or text.startswith('// @desc') or text.startswith('// @param'):
                return text
            return ''
        return match.group(0)

    new_content = pattern.sub(replacer, content)

    # Clean up empty lines that might have been left behind
    # But only if they are completely empty (just spaces/tabs)
    lines = new_content.split('\n')
    cleaned_lines = []
    for line in lines:
        if line.strip() == '' and line != '':
            # It was a line with only spaces (probably indentation of a removed comment)
            pass
        else:
            cleaned_lines.append(line)

    with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(cleaned_lines))

src_dir = os.path.join(os.getcwd(), 'src')
for root, dirs, files in os.walk(src_dir):
    for file in files:
        if file.endswith(('.cpp', '.h', '.c')):
            process_file(os.path.join(root, file))

print('Comments stripped.')
