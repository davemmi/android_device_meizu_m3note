// Nougat android::AudioSystem symbols for the MTK audio blobs (both ABIs).
//
// Android 13 moved AudioSystem to libaudioclient with other signatures, and
// the vendor namespace cannot reach it anyway; the blobs got these from the
// Nougat MTK libmedia.  The declarations reproduce the imports exactly.
//
// audio.primary.mt6755.so:
//   _ZN7android11AudioSystem24getDeviceConnectionStateEjPKc
//   Return AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE (0): the HAL then only trusts
//   the devices the policy announces through set_parameters.
//
// libaudio_param_parser.so (AppOps, dlopened by the HAL's SpeechParamParser
// and GainTableParamParser):
//   _ZN7android11AudioSystem13getParametersEiRKNS_7String8E
//   _ZN7android11AudioSystem13setParametersEiRKNS_7String8E
//   Without them the dlopen fails ("cannot locate symbol ...getParameters"),
//   appHandle stays NULL and SpeechParamParser::InitSpeechNetwork()
//   dereferences it: android.hardware.audio.service crashes every 5 s and
//   system_server's AudioService waits for the audio policy until the
//   watchdog kills it (boot loop at the boot animation).  The parser only
//   uses them to push tuning changes to a running HAL: an empty reply and
//   NO_ERROR keep it on its XML defaults.
#include <stdint.h>
#include <utils/Errors.h>
#include <utils/String8.h>

namespace android {

class AudioSystem {
  public:
    static int getDeviceConnectionState(uint32_t device, const char* address);
    static String8 getParameters(int ioHandle, const String8& keys);
    static status_t setParameters(int ioHandle, const String8& keyValuePairs);
};

int AudioSystem::getDeviceConnectionState(uint32_t, const char*) { return 0; }

String8 AudioSystem::getParameters(int, const String8&) { return String8(); }

status_t AudioSystem::setParameters(int, const String8&) { return NO_ERROR; }

}  // namespace android
