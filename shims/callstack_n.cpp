/*
 * android::CallStack for libaudiocomponentengine.so (MTK audio bit
 * converter/SRC/limiter, dlopened by audio.primary.mt6755.so). The N blob
 * imports CallStack from libutils; since P it lives in libutilscallstack, so
 * the dlopen() fails, the HAL gets no bit converter and asserts in
 * AudioALSAPlaybackHandlerBase::initBitConverter(). The blob only builds
 * debug stacks with it: these do nothing and touch no object memory.
 */
#include <sys/types.h>
#include <utils/String8.h>

namespace android {

class CallStack {
public:
    CallStack();
    ~CallStack();
    void update(int32_t ignoreDepth, pid_t tid);
    String8 toString(const char* prefix) const;
};

CallStack::CallStack() {}
CallStack::~CallStack() {}
void CallStack::update(int32_t, pid_t) {}
String8 CallStack::toString(const char*) const { return String8(); }

}  // namespace android
