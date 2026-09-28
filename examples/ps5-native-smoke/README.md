# Native application preflight

This separate OpenProspero C++ application checks that a PS5 **application**
actually reaches `main` and reads the running firmware. It is not a Vulkan or
OpenGL driver, and performs no AGC submission or VideoOut operation. The
console-qualified GPU bridge currently runs only as a **payload**.

From the OpenProspero SDK checkout on Windows, build with:

```powershell
& ..\OpenAGC\examples\ps5-native-smoke\build.ps1 -SdkRoot .
```

The script uses the SDK's application compiler and linker, then relinks with
`--no-companion`: the normal `prospero build` C++ application profile includes
companion autostart, which would confound this startup-only test. Neither the
companion nor the optional AGC loader object is part of this preflight.

The app writes
`/data/prosperoai/openagc-app-preflight-fw940.log` with the observed firmware,
its FW9.40 qualification bit, and explicit `gpu=not-attempted` and
`videoout=not-attempted` fields. A missing or unwritable log directory is a
failure, not a passing result. A successful build or ELF link does **not** prove
app launch, PKG installation, GPU execution or presentation on the console.
Keep any raw console logs outside this repository.
