#!/usr/bin/env python3
lines = open("default_patch").readlines()
print('char patch_contents[] = ""')
for l in lines:
    l = l.strip()
    if l.startswith('#'):
        continue
    print(f'"{l}\\n"')
print(';')
