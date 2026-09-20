# Build GASAIR 1.0.3

Directory layout after extracting the corresponding source ZIP:

```
GASAIR/source, resource, installer, CMakeLists.txt
deps/vst3sdk/...
Build-GASAIR.cmd
```

Requirements: Windows x64, Visual Studio 2022 C++ build tools with Windows SDK, CMake >=3.25, Ninja, and clang-cl. The CMD detects Visual Studio using vswhere; `GASAIR_VS_ROOT` can override the detected path. This release used clang-cl 19.1.5 and a static C runtime. No separately installed VC runtime is required by this build.

Run `Build-GASAIR.cmd`. Output: `build-gasair/VST3/Release/GASAIR.vst3`.

The CMake defaults disable validator execution, examples, automatic installation links and module-info generation. The build does not install or run the plugin. Both audio processor and controller use identifiers unique to GASAIR. Audio parameters have stable IDs 0–4 and a versioned binary state format. GUI output meter parameters use IDs 100 and 101.

Source files:

- `Processor.cpp`: bus arrangements, DSP, parameter changes, meter output and audio state.
- `Controller.cpp`: host parameters, editor creation and zoom state.
- `Panel.cpp`: fixed-coordinate artwork, pointer/progress rendering and interaction.
- `resource/gasair.png`: approved production panel including foreground elbow.

The artwork is a separate supplied/generated creative asset. No Fresh Air binary, source code, branding, graphics or licensed presets are included.
