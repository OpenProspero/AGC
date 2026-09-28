#include <openprospero/firmware.h>

#include <stdint.h>
#include <stdio.h>

namespace {
constexpr uint32_t kQualifiedFirmware = 0x09400008u;
constexpr char kReportPath[] = "/data/prosperoai/openagc-app-preflight-fw940.log";
}

extern "C" int main() {
    const uint32_t firmware = op_ps5_system_firmware_version();
    const bool qualified = firmware == kQualifiedFirmware;
    fprintf(stderr, "openagc-app-preflight: firmware=%08x qualified=%u\n",
            firmware, qualified ? 1u : 0u);

    FILE *report = fopen(kReportPath, "w");
    if (report == nullptr) {
        fprintf(stderr, "openagc-app-preflight: cannot open %s\n", kReportPath);
        return 2;
    }
    const int written = fprintf(report,
                                "openagc-app-preflight: firmware=%08x qualified=%u gpu=not-attempted videoout=not-attempted\n",
                                firmware, qualified ? 1u : 0u);
    const int closed = fclose(report);
    if (written < 0 || closed != 0) {
        fprintf(stderr, "openagc-app-preflight: cannot write %s\n", kReportPath);
        return 3;
    }
    return qualified ? 0 : 1;
}
