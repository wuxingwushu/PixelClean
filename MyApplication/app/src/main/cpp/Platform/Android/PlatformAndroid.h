#include "Platform/PlatformDetection.h"

#include <cstdint>
#include <functional>

class PlatformAndroid {
public:
    static PlatformAndroid& Instance();

    bool Initialize(void* androidAppState);
    void Shutdown();

    void PollEvents();
    bool ShouldClose() const;
    bool IsPaused() const;
    void WaitForResume();

    float GetDPIScale() const;
    void GetFramebufferSize(int32_t* width, int32_t* height) const;

private:
    PlatformAndroid() = default;

    void OnAppCmd(int32_t cmd);
    static void HandleAppCmd(struct android_app* app, int32_t cmd);
    static int32_t HandleInputEvent(struct android_app* app, struct AInputEvent* event);

    void* mApp = nullptr;
    bool mHasWindow = false;
    bool mPaused = false;
    bool mHasFocus = false;
    bool mQuit = false;
    int32_t mWidth = 0;
    int32_t mHeight = 0;
};