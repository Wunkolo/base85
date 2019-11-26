# base85 [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

An implementation of base85 ascii encoding modeled after the gnu coreutils.

```
base85 - Wunkolo <wunkolo@gmail.com>
Usage: base85 [Options]... [File]
       base85 --decode [Options]... [File]
Options:
  -h, --help            Display this help/usage information
  -d, --decode          Decodes incoming base85 into binary bytes
  -i, --ignore-garbage  When decoding, ignores non-base85 characters
  -w, --wrap=Columns    Wrap encoded base85 output within columns
                        Default is `76`. `0` Disables linewrapping
```

```
% inxi -C
CPU:       Topology: Dual Core model: Intel Core i3-6100 bits: 64 type: MT MCP L2 cache: 3072 KiB 
           Speed: 3700 MHz min/max: 800/3700 MHz Core speeds (MHz): 1: 3700 2: 3700 3: 3700 4: 3700 

% cat /dev/zero | ./base85 --wrap=0 | pv > /dev/null
4.38GiB 0:00:10 [ 455MiB/s]
```

---

# Todo:

- [ ] Implement all base85 variant options
  - [ ] bota
  - [ ] Adobe
  - [ ] Ascii85
  - [ ] RFC-1924
- [ ] Add SIMD-vectorizations for each variant
  - [ ] x86(`SSE` to `AVX512`)
  - [ ] ARM(`NEON`)
