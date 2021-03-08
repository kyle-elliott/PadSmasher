# PadSmasher
Simple PE file mangler

Currently, this example only creates Nop sleds with the function padding, however this project can be extended to however you wish. This program accepts valid PE files in either 64bit or 32bit and produces a mangled, but valid output that still executes while making it harder for static analyzers to understand.

# Usage
    PadSmasher.exe -file example.dll
