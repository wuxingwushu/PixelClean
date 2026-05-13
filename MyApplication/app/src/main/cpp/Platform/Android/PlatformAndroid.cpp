#include "PlatformAndroid.h"
#include "DebugLog.h"
#include "GlobalVariable.h"
#include <android_native_app_glue.h>
#include <android/configuration.h>

PlatformAndroid& PlatformAndroid::Instance()
{
    static PlatformAndroid instance;
    return instance;
}

bool PlatformAndroid::Initialize(void* androidAppState)
{
    mApp = androidAppState;
    auto* app = static_cast<android_app*>(mApp);

    app->onAppCmd = HandleAppCmd;
    app->onInputEvent = HandleInputEvent;
    app->userData = this;

    LOGD("PlatformAndroid initialized");

    while (!mHasWindow && !mQuit) {
        int events;
        android_poll_source* source;

        while (ALooper_pollAll(0, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
            if (source) {
                source->process(app, source);
            }
            if (mQuit) {
                return false;
            }
        }
    }

    mWidth = ANativeWindow_getWidth(app->window);
    mHeight = ANativeWindow_getHeight(app->window);
    LOGD("Window created: %dx%d", mWidth, mHeight);

    return true;
}

void PlatformAndroid::Shutdown()
{
    LOGD("PlatformAndroid shutdown");
}

void PlatformAndroid::PollEvents()
{
    auto* app = static_cast<android_app*>(mApp);

    int events;
    android_poll_source* source;

    while (ALooper_pollAll(mPaused ? -1 : 0, nullptr, &events,
                           reinterpret_cast<void**>(&source)) >= 0) {
        if (source) {
            source->process(app, source);
        }
        if (mQuit) {
            break;
        }
    }
}

bool PlatformAndroid::ShouldClose() const
{
    return mQuit;
}

bool PlatformAndroid::IsPaused() const
{
    return mPaused;
}

void PlatformAndroid::WaitForResume()
{
    auto* app = static_cast<android_app*>(mApp);

    int events;
    android_poll_source* source;

    while (mPaused && !mQuit) {
        if (ALooper_pollAll(-1, nullptr, &events,
                            reinterpret_cast<void**>(&source)) >= 0) {
            if (source) {
                source->process(app, source);
            }
        }
    }
}

float PlatformAndroid::GetDPIScale() const
{
    auto* app = static_cast<android_app*>(mApp);
    if (!app || !app->activity) return 1.0f;

    AConfiguration* config = AConfiguration_new();
    AConfiguration_fromAssetManager(config, app->activity->assetManager);
    int32_t density = AConfiguration_getDensity(config);
    AConfiguration_delete(config);

    return static_cast<float>(density) / 160.0f;
}

void PlatformAndroid::GetFramebufferSize(int32_t* width, int32_t* height) const
{
    *width = mWidth;
    *height = mHeight;
}

void PlatformAndroid::HandleAppCmd(struct android_app* app, int32_t cmd)
{
    auto* self = static_cast<PlatformAndroid*>(app->userData);
    self->OnAppCmd(cmd);
}

void PlatformAndroid::OnAppCmd(int32_t cmd)
{
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        mHasWindow = true;
        LOGD("APP_CMD_INIT_WINDOW");
        break;
    case APP_CMD_TERM_WINDOW:
        mHasWindow = false;
        LOGD("APP_CMD_TERM_WINDOW");
        break;
    case APP_CMD_GAINED_FOCUS:
        mHasFocus = true;
        LOGD("APP_CMD_GAINED_FOCUS");
        break;
    case APP_CMD_LOST_FOCUS:
        mHasFocus = false;
        LOGD("APP_CMD_LOST_FOCUS");
        break;
    case APP_CMD_PAUSE:
        mPaused = true;
        LOGD("APP_CMD_PAUSE");
        break;
    case APP_CMD_RESUME:
        mPaused = false;
        LOGD("APP_CMD_RESUME");
        break;
    case APP_CMD_DESTROY:
        mQuit = true;
        LOGD("APP_CMD_DESTROY");
        break;
    case APP_CMD_CONFIG_CHANGED:
        LOGD("APP_CMD_CONFIG_CHANGED");
        break;
    default:
        break;
    }
}

int32_t PlatformAndroid::HandleInputEvent(struct android_app* app, struct AInputEvent* inputEvent)
{
    auto* self = static_cast<PlatformAndroid*>(app->userData);
    (void)self;
    int32_t type = AInputEvent_getType(inputEvent);

    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(inputEvent);
        int32_t actionCode = action & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(inputEvent, 0);
        float y = AMotionEvent_getY(inputEvent, 0);

        switch (actionCode) {
        case AMOTION_EVENT_ACTION_DOWN:
            LOGD("Touch down: (%.1f, %.1f)", x, y);
            return 1;
        case AMOTION_EVENT_ACTION_UP:
            LOGD("Touch up: (%.1f, %.1f)", x, y);
            return 1;
        case AMOTION_EVENT_ACTION_MOVE:
            return 1;
        default:
            break;
        }
    } else if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t keyCode = AKeyEvent_getKeyCode(inputEvent);
        int32_t keyAction = AKeyEvent_getAction(inputEvent);

        if (keyCode == AKEYCODE_BACK && keyAction == AKEY_EVENT_ACTION_UP) {
            LOGD("Back key pressed, requesting ESC");
            Global::AndroidRequestESC = true;
            return 1;
        }
    }

    return 0;
}