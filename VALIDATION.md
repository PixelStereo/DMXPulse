# Validation — 2026-10-27

Environment: macOS Apple Silicon, AppleClang 21, Qt 6.12.0, C++17.

## Builds

- Release: `-Wall -Wextra -Wpedantic`, no compiler warnings reported.
- Debug: AddressSanitizer and UndefinedBehaviorSanitizer.
- Both builds pass the `protocol` and `window` QtTest suites.

Sanitizer invocation:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build-sanitize
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitize --output-on-failure
```

Leak detection is disabled for this run; the sanitizer results do not establish the absence of memory leaks or defects outside the exercised paths.

## Covered behavior

- Packet round trips and all truncation lengths for ArtDmx and sACN.
- Universe / priority bounds and malformed length / protocol fields.
- A deterministic corpus of 20,000 arbitrary byte buffers, plus byte-by-byte mutations of valid packets.
- Local UDP reception for both protocols and actual Art-Net output loopback.
- A competing local Art-Net listener.
- Slider edits leave received values and grayscale unchanged until a corresponding UDP packet is received; further unsent edits leave the grid unchanged again.
- Sequence-disabled Art-Net followed by a nonzero sequence; old packet rejection.
- sACN large forward sequence jumps, rollover and recent duplicate rejection.
- Selecting an already received universe without waiting for another frame.
- A 400-packet local burst reaches its final received value.
- An unchanged input causes zero grid-model data changes during a 150 ms observation interval.
- Settings round trips, automatic listening restoration, optional transmission restoration and missing-adapter handling.

Tests use temporary settings files and isolated UDP ports. Local burst coverage is a regression check, not a guaranteed network throughput benchmark.

## Reviewed implementation changes

- The grid renders receive data only. Output edits never populate its values.
- Only changed channel values / output tooltips update the grid model.
- Source lookup during rendering uses a hash table instead of a nested search.
- Incoming UDP batches yield to the event loop and schedule continued draining if packets remain.
- Receive buffers are requested at 1 MiB; the OS may enforce a different size.
- Sequence handling distinguishes Art-Net's disabled-sequence value from sACN rollover rules.
- sACN source identity follows CID + universe and the application's CID persists across launches.
- Shutdown saves the workspace before stopping network activity and avoids overwriting the saved session state afterward.
- Invalid or unavailable adapters prevent automatic restart. Transmission error handling avoids recursive shutdown attempts.

Windows/Linux compilation, extended soak testing, mobile platforms, and validation on physical lighting equipment are still outstanding. These tests cannot guarantee that the software is free of defects.
