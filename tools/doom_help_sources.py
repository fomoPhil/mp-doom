"""Generate MPC help text from approved copy and patch only build copies."""
import json
import re


def replace_function(source, name, body):
    match = re.search(r'^void ' + name + r'\(.*?\)\s*\{', source, re.M)
    if not match:
        raise ValueError('Missing pinned help function: ' + name)
    end = match.end()
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[:match.start()] + 'void ' + name + '(void)\n{\n' + body + '\n}' + source[end:]


def patch_help(root, build, source):
    pages = json.loads((root / 'doom/help_pages.json').read_text())
    if len(pages) != 3:
        raise ValueError('Expected three approved help pages')
    text = []
    for page in pages:
        fields = ['.title=' + json.dumps(page['title'])]
        for key, ckey, limit in [('rows', 'rows', 10), ('notes', 'notes', 3), ('saveRows', 'save_rows', 4)]:
            values = page.get(key, [])
            if len(values) > limit:
                raise ValueError('Help content exceeds its display layout')
            encoded = [('{' + ','.join(json.dumps(s) for s in row) + '}') if isinstance(row, list)
                       else json.dumps(row) for row in values]
            fields += ['.' + ckey + '={' + ','.join(encoded) + '}', '.' + ckey + '_count=' + str(len(values))]
        text.append('{' + ','.join(fields) + '}')
    (build / 'mpc_help_data.inc').write_text(',\n'.join(text) + '\n')
    for name in ('M_DrawReadThis1', 'M_DrawReadThis2'):
        source = replace_function(source, name, '    MPC_HelpDraw();')
    # These functions have an int argument in the pinned source.
    # Retain it because native menu callbacks use that signature.
    for name, body in [('M_ReadThis', '    (void)choice;\n    MPC_HelpBegin();\n    M_SetupNextMenu(&ReadDef1);'),
                       ('M_ReadThis2', '    (void)choice;\n    MPC_HelpNext();')]:
        source = replace_function(source, name, body).replace('void ' + name + '(void)\n{', 'void ' + name + '(int choice)\n{')
    old = '    if ( gamemode == retail )\n\t      currentMenu = &ReadDef2;\n\t    else\n\t      currentMenu = &ReadDef1;'
    if source.count(old) != 1:
        raise ValueError('Unexpected pinned help shortcut')
    return source.replace(old, '    MPC_HelpBegin();\n            currentMenu = &ReadDef1;')
