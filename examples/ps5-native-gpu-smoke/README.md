# Application-profile GPU bridge diagnostic

This OpenProspero application links the existing FW9.40-only OpenAGC AGC
bridge, its firmware gate, and the same pinned one-draw probe that produced
exactly 64 pixels in a **native payload**. It calls the optional
`openprospero/udp_log.h` SDK transport to report to a PC instead of writing
into the application's possibly inaccessible `/data` directory. It does not
call Vulkan/OpenGL or present a frame.

Build from the OpenProspero SDK checkout on Windows:

```powershell
& ..\OpenAGC\examples\ps5-native-gpu-smoke\build.ps1 -SdkRoot . -LogHost <PC-IP>
```

`-LogHost` must be the PC's reachable IPv4 address, not the console IP.
Run `python .\tools\udp_log_receiver.py --console-ip
<PS5-IP> --bind <PC-IP> --port 9999 --output <private-log-path>` on the PC **before**
launching the application. Logs are sent in bounded numbered UDP datagrams;
UDP delivery is best effort, so no messages may mean the app stopped before
`main`, network imports failed, or the receiver/firewall rejected traffic.
The app refuses to run if it cannot initialize its UDP diagnostics.

The app explicitly opts into the SDK's bounded AGC loader using all four
`--wrap=dlopen|dlsym|dlclose|dlerror` arguments and does not autostart a
companion. Compile/link success is structural evidence only. An experimental
FW9.40 PKG reached the application's `EXEC` loader event but no UDP message
was received on the PC; this does not prove `main`, direct-memory allocation,
AGC submission, marker completion or display. A timeout or failed submission
may leave GPU work in flight: keep the mapped memory alive and do not repeat
the draw without an independently observed completion.
