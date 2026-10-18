# Project technical debt

Entries about one library or component are in [debt](debt/), one file per library or component, under the same sections as this page. Entries that belong to none stay here.

### Build

- Building on Windows requires a specific version of mingw - MinGW-w64 GCC 15.2.0 (`winlibs-gcc15`, `x86_64-ucrt-posix-seh`)
- Even compiled prx libraries on Windows require nearby (static linking of these dependencies causes conflicts):
  - libgcc_s_seh-1.dll
  - libstdc++-6.dll
  - libwinpthread-1.dll

### Silent stubs

Throughout the project, every function at every stage either **does exactly what it's supposed to or throws an exception**. Everywhere... except:

### Unknown function info

- AudioIn and NpSessionSignaling exports added without an implementation have assumed signatures
- `sceNetCtlGetIfStat` signature and 56-byte layout come from PPSA09030's call site and SharpProspero's `SceNetCtlIfStat`; the device kind is always 0, the byte counters are the host's totals over its up non-loopback hardware interfaces (not "since cleared"), and the not-connected result is assumed to match `sceNetCtlGetState`
- Font, Http2, Net, Ssl, SystemService and libkernel exports imported by PPSA12544 and added without an implementation have unknown signatures
- The Share, NpTrophy2, NpEntitlementAccess and WebBrowserDialog exports imported by PPSA12544's Unity plugins and added without an implementation have unknown signatures
- Acm, GameLiveStreaming, Net, NpEntitlementAccess, VoiceQoS, WebBrowserDialog and libkernel exports imported by PPSA14632 and added without an implementation are declared without parameters; of the VoiceQoS ones only those still throwing remain so

### Functional

- Controller haptic PCM drives Xbox-style motor rumble from its per-channel RMS (large/small motor, scale 2x, below 0.01 ignored, 80 ms hold); the mapping approximates waveform energy and is not measured against console hardware
- [Cancellation coverage](ARCHITECTURE.md#thread-cancellation) checks deferred request handling and recovery, not full asynchronous support or console ABI compatibility. Cancellation during file/socket I/O, sleep interruption, cancellation versus exit/detach, condition signal conservation under cancellation, and safe rejection of non-null invalid or reclaimed thread handles remain unverified. No console title or homebrew establishes the cleanup NIDs or buffer layout. Accepted asynchronous mode currently has no diagnostic for its deferred behavior.
- `--to-intel` guest module trampolines are covered only by a synthetic relinker test; no game title has been verified with them on Linux or Windows.

### macOS port validation

The following GPU results were reported by the macOS port author; they have not been reproduced on this integration build.

- On macOS (MoltenVK on an Apple GPU), 16 AGC tests fail on hardware limits:
  - Metal has no 64-bit floats (the 7 agc_driver_f64_* tests and agc_driver_division_result_modifiers);
  - it has no 64-bit atomics (agc_driver_buffer_atomics*, agc_driver_global_atomics64, agc_driver_global_atomics_float, agc_driver_global_atomics_lanes);
  - MoltenVK offers no VK_KHR_shader_clock (agc_driver_shader_clock).
- agc_driver_sdwa_float_selectors fails on macOS: an f16 multiply that should give -0.0 gives +0.0. In isolation, Metal keeps the sign through each step of the same chain (half to float, multiply, the software float-to-half conversion). The cause, found on an M1 Pro: SPIRV-Cross's MSL backend computes a precise multiplication as `fma(l, r, 0)`, which turns a -0.0 product into +0.0. A fix is submitted as KhronosGroup/SPIRV-Cross#2704; it reaches this project when MoltenVK takes that SPIRV-Cross.
- The [SPIR-V backend](../../core/shader/recompiler/SpirvBackend) works around SPIRV-Cross, which MoltenVK uses to make Metal shaders:
  - ordered not-equal is emitted as an unordered not-equal and two NaN tests on the bits, because SPIRV-Cross writes OpFOrdNotEqual as `!=`;
  - NaN tests read the bits;
  - an image atomic comes after an image-memory barrier, because Metal shows an invocation's earlier image stores to its atomics only after a texture fence;
  - subgroup results pass through a Private variable, so that SPIRV-Cross does not move them into `?:`, `&&` or `||`, where only some lanes would run them.
  - on MoltenVK the [barycentric tessellation stages](../../core/shader/recompiler/SpirvBackend/src/BarycentricGeometryShader.cpp) carry the clip distances as the second member of their `gl_PerVertex` blocks, because SPIRV-Cross cannot translate stand-alone `ClipDistance` variables in tessellation stages (`no member named 'gl_ClipDistance' in 'main0_out'`); other drivers get the stand-alone variables the vertex shader writes, which the interface matching rules require.

- On macOS every guest image keeps its own TLS block and thread pointer ([MacOsImage.cpp](../../core/relinker/elfpatcher/src/macos/MacOsImage.cpp)). Another module can reach a module's exported TLS variable only through general-dynamic access (DTPMOD64/DTPOFF64). The image initializer fills those slots from the TLS index the defining module exports. As on Windows, initial-exec access (TPOFF64) to another module's TLS fails the relink, and so do TLS relocations in the executable.
