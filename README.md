# stardict

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C](https://img.shields.io/badge/C-11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard))
[![CI](https://github.com/kejinlu/stardict/actions/workflows/ci.yml/badge.svg)](https://github.com/kejinlu/stardict/actions/workflows/ci.yml)

**stardict** — A C library for parsing **StarDict** and **dictd** dictionary files, with support for lookup, prefix search, synonym expansion, and random-access compressed data reading.

## Features

- Parse StarDict dictionaries (.ifo, .idx, .dict / .dict.dz, .syn)
- Parse dictd dictionaries (.index, .dict / .dict.dz)
- Support gzip-compressed dictzip format (.dict.dz, .ridx.gz) with random access
- Synonym file support for StarDict (.syn)
- Resource storage support (res.rifo / res.ridx / res.rdic database and res/ file directory)
- Prefix search (word suggestions)
- Index traversal API for entry iteration
- Cross-platform: Linux, macOS, Windows

## Building

### Prerequisites

- C compiler with C11 support
- zlib
- CMake 3.14+

### Install Dependencies

**macOS:**
```bash
brew install cmake zlib
```

**Ubuntu/Debian:**
```bash
sudo apt-get install cmake zlib1g-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install cmake zlib-devel
```

**Windows:**
Install dependencies using [vcpkg](https://vcpkg.io/):
```cmd
vcpkg install zlib:x64-windows
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Build from Source

```bash
git clone https://github.com/kejinlu/stardict.git
cd stardict

mkdir build && cd build
cmake ..
cmake --build .

# Run tests
ctest --output-on-failure

# Install (optional)
sudo cmake --install .
```

#### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `STARDICT_BUILD_TESTS` | `ON` | Build test suite |
| `BUILD_SHARED_LIBS` | `OFF` | Build shared library instead of static |

## Quick Start

### Looking Up a Word (StarDict)

```c
#include "sd_stardict.h"
#include <stdio.h>

int main() {
    sd_stardict *dict = sd_stardict_open("dictionary.ifo");
    if (!dict) {
        fprintf(stderr, "Failed to open dictionary\n");
        return 1;
    }

    // Get dictionary info
    const sd_stardict_ifo *info = stardict_get_info(dict);
    if (info) {
        printf("Book: %s\n", info->bookname ? info->bookname : "N/A");
        printf("Words: %u\n", info->wordcount);
    }

    // Look up a word
    sd_lookup_result *result = stardict_lookup(dict, "hello");
    if (result) {
        for (size_t i = 0; i < result->count; i++) {
            printf("Definition: %s\n", result->definitions[i]);
        }
        sd_lookup_result_free(result);
    }

    sd_stardict_close(dict);
    return 0;
}
```

### Word Suggestions

```c
// Prefix search
sd_suggestion_result *sg = stardict_suggest(dict, "hel", 10);
if (sg) {
    for (size_t i = 0; i < sg->count; i++) {
        printf("  %s\n", sg->suggestions[i]->word);
    }
    sd_suggestion_result_free(sg);
}
```

### Looking Up a Word (dictd)

```c
#include "sd_dictd.h"

sd_dictd *dict = sd_dictd_open("dictionary.index");
if (!dict) return 1;

sd_lookup_result *result = sd_dictd_lookup(dict, "hello");
if (result) {
    for (size_t i = 0; i < result->count; i++) {
        printf("Definition: %s\n", result->definitions[i]);
    }
    sd_lookup_result_free(result);
}

sd_dictd_close(dict);
```

## Architecture

```
src/
  sd_stardict.h/c        - StarDict dictionary (open, lookup, suggest)
  sd_dictd.h/c           - dictd dictionary (open, lookup, suggest)
  sd_dictfile.h/c        - Dict data reader (plain / dictzip)
  sd_dictzip.h/c         - Dictzip compressed format parser
  sd_dictfile_index.h/c  - Generic word index (WordList / Offset page mode)
  sd_stardict_ifo.h/c    - StarDict .ifo metadata parser
  sd_stardict_syn.h/c    - StarDict .syn synonym file parser
  sd_stardict_res.h/c    - StarDict resource storage (database / file)
  sd_array.h             - Dynamic array (header-only)
  sd_port.h              - Platform abstraction layer (header-only)
  sd_types.h/c           - Generic result types
```

## Platform Support

- **Linux** (tested)
- **macOS** (tested)
- **Windows** (MSVC)

## License

MIT License

Copyright (c) 2026 kejinlu

## Acknowledgments

stardict incorporates the following third-party components:

- **[Unity](https://github.com/ThrowTheSwitch/Unity)** by ThrowTheSwitch (MIT License) — Test framework
- **[zlib](https://zlib.net/)** by Jean-loup Gailly and Mark Adler (zlib License) — Gzip / dictzip decompression
