# base85 [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

An implementation of base85 ascii encoding modeled after the gnu coreutils.

```
base85 - Wunkolo <wunkolo@gmail.com>
Usage: base85 [Options]... [File]
       base85 --decode [Options]... [File]
Options:
  -h, --help            Display this help/usage information
  -d, --decode          Decodes incoming binary ascii into bytes
  -i, --ignore-garbage  When decoding, ignores non-base85 characters
  -w, --wrap=Columns    Wrap encoded binary output within columns
                        Default is `76`. `0` Disables linewrapping
```
---
