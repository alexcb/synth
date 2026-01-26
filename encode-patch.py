#!/usr/bin/env python3
lines = open("patch").readlines()
print('char patch_contents[] = ""')
for l in lines:
    l = l.strip()
    print(f'"{l}\\n"')
print(';')
