#include "Clearwing.h"

#include <java/lang/String.h>
#include <java/nio/ByteBuffer.h>
#include <java/nio/CharBuffer.h>
#include <java/nio/ShortBuffer.h>
#include <java/nio/FloatBuffer.h>
#include <java/nio/DoubleBuffer.h>
#include <java/nio/IntBuffer.h>
#include <java/nio/LongBuffer.h>
#include <java/io/IOException.h>

#include <arc/backend/switchgdx/SwitchApplication.h>
#include <arc/backend/switchgdx/SwitchGraphics.h>
#include <arc/backend/switchgdx/SwitchGL.h>
#include <arc/backend/switchgdx/SwitchInput.h>
#include <arc/backend/switchgdx/SwitchFiles.h>
#include <arc/util/ArcRuntimeException.h>
#include <arc/util/Buffers.h>
#include <arc/graphics/Pixmap.h>
#include <arc/Input_TextInput.h>

#include <fcntl.h>
#include <csignal>
#include <vector>
#include <string>
#include <mutex>
#include "switchgdx/tinyfiledialogs.h"
#include <curl/curl.h>
#include <chrono>
#include <SDL.h>
#include <SDL_gamecontroller.h>
#include "SDL_messagebox.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#define STBI_NO_STDIO

#include <arc/func/Cons.h>
#include <java/lang/Runnable.h>

#include "stb_image.h"

#if !defined(__WIN32__) && !defined(__WINRT__)
# include <sys/socket.h>
# include <arpa/inet.h>
# include <netinet/in.h>
# include <netdb.h>
#else
# include <winsock2.h>
# include <ws2tcpip.h>
typedef int socklen_t;
#endif

#ifdef __WINRT__
#include "winrt/base.h"
namespace winrt::impl {
    template <typename Async>
    auto wait_for(Async const& async, Windows::Foundation::TimeSpan const& timeout);
}
#include <Windows.h>
#include <winrt/base.h>
#include <winrt/Windows.Storage.h>
std::string getLocalPathUWP() {
    auto path = winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
    return std::string(path.begin(), path.end());
}

#define main main
extern "C" extern int main(int argc, char* args[]);
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    AllocConsole();
    FILE* fpstdin = stdin, * fpstdout = stdout, * fpstderr = stderr;
    freopen_s(&fpstdin, "CONIN$", "r", stdin);
    freopen_s(&fpstdout, "CONOUT$", "w", stdout);
    freopen_s(&fpstderr, "CONOUT$", "w", stderr);

    SDL_WinRTRunApp(main, nullptr);
}
#endif

#ifdef __SWITCH__
# include <switch.h>
# include <EGL/egl.h>
# include <EGL/eglext.h>
# include <glad/glad.h>
# include <stdio.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <sys/errno.h>
# include <unistd.h>
#else
#include "glad/gles2.h"
#endif

static int touches[16 * 3];

#ifdef __SWITCH__
static EGLDisplay display;
static EGLContext context;
static EGLSurface surface;

static PadState combinedPad;
static PadState pads[8];

static int nxlinkSock = -1;
static bool socketInit;
#else
static SDL_Window *window;
static int buttons;
static float joysticks[4];
#endif

#ifdef __SWITCH__
extern "C" void userAppInit() {
    socketInitializeDefault();
    nxlinkStdio();
}

extern "C" void userAppExit() {
    socketExit();
}
#endif

void *getBufferAddress(jcontext ctx, java_nio_Buffer *buffer) {
    if (!buffer)
        return nullptr;
    int typeSize = 1;
    if (isInstance(ctx, (jobject) buffer, &class_java_nio_ByteBuffer))
        typeSize = sizeof(jbyte);
    else if (isInstance(ctx, (jobject) buffer, &class_java_nio_FloatBuffer))
        typeSize = sizeof(jfloat);
    else if (isInstance(ctx, (jobject) buffer, &class_java_nio_IntBuffer))
        typeSize = sizeof(jint);
    else if (isInstance(ctx, (jobject) buffer, &class_java_nio_ShortBuffer))
        typeSize = sizeof(jshort);
    else if (isInstance(ctx, (jobject) buffer, &class_java_nio_CharBuffer))
        typeSize = sizeof(jchar);
    else if (isInstance(ctx, (jobject) buffer, &class_java_nio_LongBuffer))
        typeSize = sizeof(jlong);
    else if (isInstance(ctx, (jobject) buffer, &class_java_nio_DoubleBuffer))
        typeSize = sizeof(jdouble);
    return (char*)buffer->F_address + typeSize * buffer->F_position;
}

void *getBufferAddress(java_nio_ByteBuffer *buffer) {
    return (char*)buffer->parent.F_address + sizeof(jbyte) * buffer->parent.F_position;
}

void *getBufferAddress(java_nio_ShortBuffer *buffer) {
    return (char*)buffer->parent.F_address + sizeof(jshort) * buffer->parent.F_position;
}

void *getBufferAddress(java_nio_CharBuffer *buffer) {
    return (char*)buffer->parent.F_address + sizeof(jchar) * buffer->parent.F_position;
}

void *getBufferAddress(java_nio_IntBuffer *buffer) {
    return (char*)buffer->parent.F_address + sizeof(jint) * buffer->parent.F_position;
}

void *getBufferAddress(java_nio_LongBuffer *buffer) {
    return (char*)buffer->parent.F_address + sizeof(jlong) * buffer->parent.F_position;
}

void *getBufferAddress(java_nio_FloatBuffer *buffer) {
    return (char*)buffer->parent.F_address + sizeof(jfloat) * buffer->parent.F_position;
}

void *getBufferAddress(java_nio_DoubleBuffer *buffer) {
    return (char*)buffer->parent.F_address + sizeof(jdouble) * buffer->parent.F_position;
}

extern "C" {

void SM_arc_backend_switchgdx_SwitchApplication_init_boolean(jcontext ctx, jbool vsync) {
    for (int i = 0; i < 16; i++)
        touches[i * 3] = -1;

#if defined(__WIN32__) || defined(__WINRT__)
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data))
        throwIOException(ctx, "WSAStartup exception");
#endif

#ifdef __SWITCH__
    padConfigureInput(8, HidNpadStyleSet_NpadStandard);
    padInitializeAny(&combinedPad);

    padInitializeDefault(&pads[0]);
    for (int i = 1; i < 8; i++)
        padInitialize(&pads[i], static_cast<HidNpadIdType>(HidNpadIdType_No1 + i));

    setInitialize();

    hidInitializeTouchScreen();

    Result result = romfsInit();
    if (R_FAILED(result))
        ;//Todo: Error handling/logging
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, NULL, NULL);
    eglBindAPI(EGL_OPENGL_API);
    EGLConfig config;
    EGLint numConfigs;
    static const EGLint framebufferAttributeList[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE,     8,
        EGL_GREEN_SIZE,   8,
        EGL_BLUE_SIZE,    8,
        EGL_ALPHA_SIZE,   8,
        EGL_DEPTH_SIZE,   24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    eglChooseConfig(display, framebufferAttributeList, &config, 1, &numConfigs);
    surface = eglCreateWindowSurface(display, config, nwindowGetDefault(), NULL);
    static const EGLint contextAttributeList[] =
    {
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_CONTEXT_MAJOR_VERSION_KHR, 2,
        EGL_CONTEXT_MINOR_VERSION_KHR, 0,
        EGL_NONE
    };
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributeList);
    eglMakeCurrent(display, surface, surface, context);
    gladLoadGL();

    SDL_Init(SDL_INIT_AUDIO);
#else
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

    window = SDL_CreateWindow("SwitchGDX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);

#ifdef __WINRT__
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
    SDL_SetHint("SDL_WINRT_HANDLE_BACK_BUTTON", "1");
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif

    auto context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(vsync ? 1 : 0);

    gladLoadGLES2((GLADloadfunc) SDL_GL_GetProcAddress);
#endif
    
    curl_global_init(CURL_GLOBAL_ALL);
}

void SM_arc_backend_switchgdx_SwitchApplication_dispose0(jcontext ctx) {
#if defined(__WIN32__) || defined(__WINRT__)
    WSACleanup();
#endif

#ifdef __SWITCH__
    if (display) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context)
            eglDestroyContext(display, context);
        if (surface)
            eglDestroySurface(display, surface);
        eglTerminate(display);
    }

    SDL_Quit();

    romfsExit();
#endif

    curl_global_cleanup();
}

#ifndef __SWITCH__
static int keyToButton(int key) {
    switch (key) {
        case SDL_SCANCODE_Z:
            return 1 << 3; // Y
        case SDL_SCANCODE_X:
            return 1 << 1; // B
        case SDL_SCANCODE_C:
            return 1 << 0; // A
        case SDL_SCANCODE_V:
            return 1 << 2; // X
        case SDL_SCANCODE_F:
            return 1 << 4; // Left stick
        case SDL_SCANCODE_G:
            return 1 << 5; // Right stick
        case SDL_SCANCODE_Q:
            return 1 << 6; // L
        case SDL_SCANCODE_E:
            return 1 << 7; // R
        case SDL_SCANCODE_R:
            return 1 << 8; // ZL
        case SDL_SCANCODE_T:
            return 1 << 9; // ZR
        case SDL_SCANCODE_N:
            return 1 << 10; // Plus
        case SDL_SCANCODE_M:
            return 1 << 11; // Minus
        case SDL_SCANCODE_UP:
            return 1 << 13; // D-up
        case SDL_SCANCODE_DOWN:
            return 1 << 15; // D-down
        case SDL_SCANCODE_LEFT:
            return 1 << 12; // D-left
        case SDL_SCANCODE_RIGHT:
            return 1 << 14; // D-right
        default:
            return 0;
    }
}

static int keyToAxis(int scancode) {
    switch (scancode) {
        case SDL_SCANCODE_W:
            return 0x4 + 1;
        case SDL_SCANCODE_S:
            return 1;
        case SDL_SCANCODE_A:
            return 0x4 + 0;
        case SDL_SCANCODE_D:
            return 0;
        case SDL_SCANCODE_I:
            return 0x4 + 3;
        case SDL_SCANCODE_K:
            return 3;
        case SDL_SCANCODE_J:
            return 0x4 + 2;
        case SDL_SCANCODE_L:
            return 2;
        default:
            return -1;
    }
}

static int mapButtonSDL(int button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_A:
            return 1 << 0;
        case SDL_CONTROLLER_BUTTON_B:
            return 1 << 1;
        case SDL_CONTROLLER_BUTTON_X:
            return 1 << 2;
        case SDL_CONTROLLER_BUTTON_Y:
            return 1 << 3;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:
            return 1 << 4;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
            return 1 << 5;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            return 1 << 6;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            return 1 << 7;
        case SDL_CONTROLLER_BUTTON_START:
            return 1 << 10;
        case SDL_CONTROLLER_BUTTON_BACK:
            return 1 << 11;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            return 1 << 12;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            return 1 << 13;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            return 1 << 14;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            return 1 << 15;
        default:
            return 0;
    }
}
#else
static u64 remapPadButtons(u64 buttons, u32 style) {
    u64 mapped = buttons;

    if (style & HidNpadStyleTag_NpadJoyLeft) {
        mapped &= ~(
            HidNpadButton_Left | HidNpadButton_Right | HidNpadButton_Up | HidNpadButton_Down |
            HidNpadButton_StickLLeft | HidNpadButton_StickLRight | HidNpadButton_StickLUp | HidNpadButton_StickLDown |
            HidNpadButton_LeftSL | HidNpadButton_LeftSR
        );

        if (buttons & HidNpadButton_Left)
            mapped |= HidNpadButton_B;
        if (buttons & HidNpadButton_Down)
            mapped |= HidNpadButton_A;
        if (buttons & HidNpadButton_Up)
            mapped |= HidNpadButton_Y;
        if (buttons & HidNpadButton_Right)
            mapped |= HidNpadButton_X;

        if (buttons & HidNpadButton_StickLLeft)
            mapped |= HidNpadButton_StickLDown;
        if (buttons & HidNpadButton_StickLDown)
            mapped |= HidNpadButton_StickLRight;
        if (buttons & HidNpadButton_StickLRight)
            mapped |= HidNpadButton_StickLUp;
        if (buttons & HidNpadButton_StickLUp)
            mapped |= HidNpadButton_StickLLeft;

        if (buttons & HidNpadButton_LeftSL)
            mapped |= HidNpadButton_L;
        if (buttons & HidNpadButton_LeftSR)
            mapped |= HidNpadButton_R;
    } else if (style & HidNpadStyleTag_NpadJoyRight) {
        mapped &= ~(
            HidNpadButton_A | HidNpadButton_B | HidNpadButton_X | HidNpadButton_Y |
            HidNpadButton_StickLLeft | HidNpadButton_StickLRight | HidNpadButton_StickLUp | HidNpadButton_StickLDown |
            HidNpadButton_LeftSL | HidNpadButton_LeftSR
        );

        if (buttons & HidNpadButton_A)
            mapped |= HidNpadButton_B;
        if (buttons & HidNpadButton_X)
            mapped |= HidNpadButton_A;
        if (buttons & HidNpadButton_B)
            mapped |= HidNpadButton_Y;
        if (buttons & HidNpadButton_Y)
            mapped |= HidNpadButton_X;

        if (buttons & HidNpadButton_StickRLeft)
            mapped |= HidNpadButton_StickRUp;
        if (buttons & HidNpadButton_StickRDown)
            mapped |= HidNpadButton_StickRLeft;
        if (buttons & HidNpadButton_StickRRight)
            mapped |= HidNpadButton_StickRDown;
        if (buttons & HidNpadButton_StickRUp)
            mapped |= HidNpadButton_StickRRight;

        if (buttons & HidNpadButton_RightSL)
            mapped |= HidNpadButton_L;
        if (buttons & HidNpadButton_RightSR)
            mapped |= HidNpadButton_R;
    }

    return mapped;
}

static void remapPadAxes(float *axes, u32 style) {
    if (style & HidNpadStyleTag_NpadJoyLeft) {
        float temp = axes[0];
        axes[0] = -axes[1];
        axes[1] = temp;
    } else if(style & HidNpadStyleTag_NpadJoyRight) {
        axes[0] = axes[3];
        axes[1] = -axes[2];
        axes[2] = 0;
        axes[3] = 0;
    }
}
#endif

jbool SM_arc_backend_switchgdx_SwitchApplication_update_R_boolean(jcontext ctx) {
#ifdef __SWITCH__
    padUpdate(&combinedPad);
    u64 kDown = padGetButtonsDown(&combinedPad);
    if (kDown & HidNpadButton_Plus)
        return false;

    for (int i = 0; i < 8; i++)
        padUpdate(&pads[i]);

    HidTouchScreenState touchState;
    if (hidGetTouchScreenStates(&touchState, 1)) {
        for (int i = 0; i < 16; i++)
            if (i < touchState.count) {
                touches[i * 3 + 0] = touchState.touches[i].finger_id;
                touches[i * 3 + 1] = touchState.touches[i].x;
                touches[i * 3 + 2] = 720 - 1 - touchState.touches[i].y;
            } else {
                touches[i * 3 + 0] = -1;
                touches[i * 3 + 1] = 0;
                touches[i * 3 + 2] = 0;
            }
    }

    eglSwapBuffers(display, surface);
    return appletMainLoop();
#else
    SDL_Event event;
    int axis;
    int height;
    SDL_GetWindowSize(window, nullptr, &height);
    while (SDL_PollEvent(&event))
        switch (event.type) {
            case SDL_QUIT:
                return false;
            case SDL_MOUSEMOTION:
                touches[1] = event.motion.x;
                touches[2] = height - 1 - event.motion.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
                touches[0] = 0;
                touches[1] = event.button.x;
                touches[2] = height - 1 - event.button.y;
                break;
            case SDL_MOUSEBUTTONUP:
                touches[0] = -1;
                break;
            case SDL_KEYDOWN:
                buttons |= keyToButton(event.key.keysym.scancode);
                axis = keyToAxis(event.key.keysym.scancode);
                if (axis > -1 and !event.key.repeat)
                    joysticks[axis & 0x3] += axis & 0x4 ? -1 : 1;
                break;
            case SDL_KEYUP:
                buttons &= ~keyToButton(event.key.keysym.scancode);
                axis = keyToAxis(event.key.keysym.scancode);
                if (axis > -1 and !event.key.repeat)
                    joysticks[axis & 0x3] = 0;
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                buttons |= mapButtonSDL(event.cbutton.button);
                break;
            case SDL_CONTROLLERBUTTONUP:
                buttons &= ~mapButtonSDL(event.cbutton.button);
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (event.caxis.axis >= 0 && event.caxis.axis < 4)
                    joysticks[event.caxis.axis] = (float)event.caxis.value / 32768.f;
                for (int i = 0; i < 2; i++)
                    if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT + i) {
                        if (event.caxis.value > 512)
                            buttons |= 1 << (8 + i);
                        else
                            buttons &= ~(1 << (8 + i));
                    }
                break;
            case SDL_CONTROLLERDEVICEADDED:
                SDL_GameControllerOpen(event.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                SDL_GameControllerClose(SDL_GameControllerFromPlayerIndex(event.cdevice.which));
                break;
        }

    SDL_GL_SwapWindow(window);
    return true;
#endif
}

jobject M_arc_backend_switchgdx_SwitchFiles_getLocalStoragePath_R_java_lang_String(jcontext ctx, jobject self) {
#ifdef __WINRT__
    auto path = getLocalPathUWP();
    return (jobject) stringFromNative(ctx, path.c_str());
#else
    return (jobject) stringFromNative(ctx, "data");
#endif
}

jbool M_arc_backend_switchgdx_SwitchApplication_openURI_java_lang_String_R_boolean(jcontext ctx, jobject self, jobject urlObj) {
#ifdef __SWITCH__
    WebCommonConfig config;
    WebCommonReply reply;
    return !webPageCreate(&config, stringToNative(ctx, (jstring)urlObj)) and !webConfigSetWhitelist(&config, "^http*") and !webConfigShow(&config, &reply);
#else
    std::string url(stringToNative(ctx, (jstring) urlObj));
# if defined(__WIN32__) || defined(__WINRT__)
    return !system(("start " + url).c_str());
# elif __APPLE__
    return !system(("open " + url).c_str());
# else
    return !system(("xdg-open " + url).c_str());
# endif
#endif
}

jobject SM_arc_filedialogs_FileDialogs_saveFileDialog_java_lang_String_java_lang_String_Array1_java_lang_String_java_lang_String_R_java_lang_String(jcontext ctx, jobject param0, jobject param1, jobject param2, jobject param3) {
    return nullptr; // Todo
}

jobject SM_arc_filedialogs_FileDialogs_openFileDialog_java_lang_String_java_lang_String_Array1_java_lang_String_java_lang_String_boolean_R_java_lang_String(jcontext ctx, jobject param0, jobject param1, jobject param2, jobject param3, jbool param4) {
    return nullptr; // Todo
}

void SM_arc_util_Buffers_freeMemory_java_nio_ByteBuffer(jcontext ctx, jobject buffer) {
    auto memory = (char *) ((java_nio_Buffer *) buffer)->F_address;
    if (memory) {
        delete[] memory;
        ((java_nio_Buffer *) buffer)->F_address = 0;
    }
}

jobject SM_arc_util_Buffers_newDisposableByteBuffer_int_R_java_nio_ByteBuffer(jcontext ctx, jint size) {
    return SM_java_nio_ByteBuffer_allocateDirect_int_R_java_nio_ByteBuffer(ctx, size); // Todo: This should be non-owning
}

jlong SM_arc_util_Buffers_getBufferAddress_java_nio_Buffer_R_long(jcontext ctx, jobject buffer) {
    return ((java_nio_Buffer *) buffer)->F_address;
}

void SM_arc_util_Buffers_clear_java_nio_ByteBuffer_int(jcontext ctx, jobject buffer, jint numBytes) {
    memset((void *) ((java_nio_Buffer *) buffer)->F_address, 0, numBytes);
}

void SM_arc_util_Buffers_copyJni_Array1_float_java_nio_Buffer_int_int(jcontext ctx, jobject src, jobject dst, jint numFloats, jint offset) {
    memcpy((float *) ((java_nio_Buffer *) dst)->F_address, (float *) ((jarray) src)->data + offset, numFloats << 2);
}

void SM_arc_util_Buffers_copyJni_Array1_byte_int_java_nio_Buffer_int_int(jcontext ctx, jobject src, jint srcOffset, jobject dst, jint dstOffset, jint numBytes) {
    memcpy((char *) ((java_nio_Buffer *) dst)->F_address + dstOffset, (jbyte *) ((jarray) src)->data + srcOffset, numBytes);
}

void SM_arc_util_Buffers_copyJni_Array1_short_int_java_nio_Buffer_int_int(jcontext ctx, jobject src, jint srcOffset, jobject dst, jint dstOffset, jint numBytes) {
    memcpy((char *) ((java_nio_Buffer *) dst)->F_address + dstOffset, (jshort *) ((jarray) src)->data + srcOffset, numBytes);
}

void SM_arc_util_Buffers_copyJni_Array1_int_int_java_nio_Buffer_int_int(jcontext ctx, jobject src, jint srcOffset, jobject dst, jint dstOffset, jint numBytes) {
    memcpy((char *) ((java_nio_Buffer *) dst)->F_address + dstOffset, (jint *) ((jarray) src)->data + srcOffset, numBytes);
}

void SM_arc_util_Buffers_copyJni_Array1_float_int_java_nio_Buffer_int_int(jcontext ctx, jobject src, jint srcOffset, jobject dst, jint dstOffset, jint numBytes) {
    memcpy((char *) ((java_nio_Buffer *) dst)->F_address + dstOffset, (jfloat *) ((jarray) src)->data + srcOffset, numBytes);
}

void SM_arc_util_Buffers_copyJni_java_nio_Buffer_int_java_nio_Buffer_int_int(jcontext ctx, jobject src, jint srcOffset, jobject dst, jint dstOffset, jint numBytes) {
    memcpy((char *) ((java_nio_Buffer *) dst)->F_address + dstOffset, (char *) ((java_nio_Buffer *) src)->F_address + srcOffset, numBytes);
}

jobject SM_arc_graphics_Pixmap_loadJni_Array1_long_Array1_byte_int_int_R_java_nio_ByteBuffer(jcontext ctx, jobject nativeData, jobject buffer, jint offset, jint len) {
    int width, height, format;
    auto pixels = stbi_load_from_memory((unsigned char *) ((jarray) buffer)->data + offset, len, &width, &height, &format, STBI_rgb_alpha);
    if (!pixels)
        return nullptr;
    auto pixelBuffer = gcAllocProtected(ctx, &class_java_nio_ByteBuffer);
    init_java_nio_ByteBuffer_long_int_boolean(ctx, pixelBuffer, (jlong) pixels, (jint) (width * height * 4), false);
    unprotectObject(pixelBuffer);
    auto nativeDataPtr = (jlong *) ((jarray) nativeData)->data;
    nativeDataPtr[0] = (jlong) pixels;
    nativeDataPtr[1] = width;
    nativeDataPtr[2] = height;
    return pixelBuffer;
}

jobject SM_arc_graphics_Pixmap_createJni_Array1_long_int_int_R_java_nio_ByteBuffer(jcontext ctx, jobject nativeData, jint width, jint height) {
    auto pixels = new char[width * height * 4]{};
    auto pixelBuffer = gcAllocProtected(ctx, &class_java_nio_ByteBuffer);
    init_java_nio_ByteBuffer_long_int_boolean(ctx, pixelBuffer, (jlong) pixels, width * height * 4, false);
    unprotectObject(pixelBuffer);
    auto nativeDataPtr = (jlong *) ((jarray) nativeData)->data;
    nativeDataPtr[0] = (jlong) pixels;
    nativeDataPtr[1] = width;
    nativeDataPtr[2] = height;
    return pixelBuffer;
}

void SM_arc_graphics_Pixmap_free_long(jcontext ctx, jlong pixmap) {
    delete[] (char *) pixmap;
}

jobject SM_arc_graphics_Pixmap_getFailureReason_R_java_lang_String(jcontext ctx) {
    return (jobject) stringFromNative(ctx, stbi_failure_reason());
}

jint M_arc_backend_switchgdx_SwitchGraphics_getWidth_R_int(jcontext ctx, jobject self) {
    int width;
#ifdef __SWITCH__
//    eglQuerySurface(display, surface, EGL_WIDTH, &width);
    return 1280;
#else
    SDL_GetWindowSize(window, &width, nullptr);
#endif
    return width;
}

jint M_arc_backend_switchgdx_SwitchGraphics_getHeight_R_int(jcontext ctx, jobject self) {
    int height;
#ifdef __SWITCH__
//    eglQuerySurface(display, surface, EGL_HEIGHT, &height);
    return 720;
#else
    SDL_GetWindowSize(window, nullptr, &height);
#endif
    return height;
}

jint SM_arc_backend_switchgdx_SwitchInput_getButtons_int_R_int(jcontext ctx, jint controller) {
#ifdef __SWITCH__
    auto &pad = controller == -1 ? combinedPad : pads[controller];
    return remapPadButtons(padGetButtons(&pad), padGetStyleSet(&pad));
#else
    return buttons;
#endif
}

void SM_arc_backend_switchgdx_SwitchInput_getAxes_int_Array1_float(jcontext ctx, jint controller, jobject axes) {
    auto array = (float *) ((jarray) axes)->data;
#ifdef __SWITCH__
    const auto &pad = controller == -1 ? combinedPad : pads[controller];
    auto stickLeft = padGetStickPos(&pad, 0);
    auto stickRight = padGetStickPos(&pad, 1);
    array[0] = (float)stickLeft.x / JOYSTICK_MAX;
    array[1] = (float)stickLeft.y / JOYSTICK_MAX;
    array[2] = (float)stickRight.x / JOYSTICK_MAX;
    array[3] = (float)stickRight.y / JOYSTICK_MAX;
    remapPadAxes(array, padGetStyleSet(&pad));
    // Todo: Is inversion needed?
    array[1] *= -1;
    array[3] *= -1;
#else
    memcpy(array, joysticks, sizeof(joysticks));
#endif
}

jbool SM_arc_backend_switchgdx_SwitchInput_isConnected_int_R_boolean(jcontext ctx, jint controller) {
#ifdef __SWITCH__
    return pads[controller].active_handheld or pads[controller].active_id_mask;
#else
    return controller == 0;
#endif
}

void SM_arc_backend_switchgdx_SwitchInput_remapControllers_int_int_boolean_boolean(jcontext ctx, jint min, jint max, jbool dualJoy, jbool singleMode) {
#ifdef __SWITCH__
    HidLaControllerSupportArg arg;
    hidLaCreateControllerSupportArg(&arg);
    arg.hdr.player_count_min = min;
    arg.hdr.player_count_max = max;
    arg.hdr.enable_permit_joy_dual = dualJoy;
    arg.hdr.enable_single_mode = singleMode;
    hidLaShowControllerSupportForSystem(nullptr, &arg, false);
#endif
}

void SM_arc_backend_switchgdx_SwitchInput_getTouchData_Array1_int(jcontext ctx, jobject touchData) {
    memcpy((void *) ((jarray) touchData)->data, touches, sizeof(touches));
}

void M_arc_backend_switchgdx_SwitchInput_getTextInput_arc_Input$TextInput(jcontext ctx, jobject self, jobject inputObj) {
    auto input = (arc_Input$TextInput *) inputObj;
    auto title = stringToNative(ctx, (jstring)input->F_title);
    auto message = stringToNative(ctx, (jstring)input->F_message);
    auto text = stringToNative(ctx, (jstring)input->F_text);

#ifdef __SWITCH__
    Result rc;
    SwkbdConfig kbd;
    char result[256];
    rc = swkbdCreate(&kbd, 0);
    if (rc)
        goto failed;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, title);
    swkbdConfigSetGuideText(&kbd, message);
    swkbdConfigSetInitialText(&kbd, text);
    swkbdConfigSetStringLenMax(&kbd, std::min(input->F_maxLength, (int)sizeof(result) - 1));
    rc = swkbdShow(&kbd, result, sizeof(result));
    if (rc)
        goto failed;
#elif defined(__WINRT__)
    goto failed;
#else
    auto result = tinyfd_inputBox(title, message, text);
    if (!result)
        goto failed;
#endif

    if (input->F_allowEmpty or strlen(result) > 0) {
        auto resultObj = (jobject) stringFromNativeProtected(ctx, result);
        INVOKE_INTERFACE(arc_func_Cons, get_java_lang_Object, (jobject)input->F_accepted, resultObj);
        unprotectObject(resultObj);
        return;
    }

    failed:
    INVOKE_INTERFACE(java_lang_Runnable, run, (jobject)input->F_canceled);
}

void M_arc_backend_switchgdx_SwitchGL_glActiveTexture_int(jcontext ctx, jobject self, jint texture) {
    glActiveTexture(texture);
}

void M_arc_backend_switchgdx_SwitchGL_glBindTexture_int_int(jcontext ctx, jobject self, jint target, jint texture) {
    glBindTexture(target, texture);
}

void M_arc_backend_switchgdx_SwitchGL_glBlendFunc_int_int(jcontext ctx, jobject self, jint sfactor, jint dfactor) {
    glBlendFunc(sfactor, dfactor);
}

void M_arc_backend_switchgdx_SwitchGL_glClear_int(jcontext ctx, jobject self, jint mask) {
    glClear(mask);
}

void M_arc_backend_switchgdx_SwitchGL_glClearColor_float_float_float_float(jcontext ctx, jobject self, jfloat red, jfloat green, jfloat blue, jfloat alpha) {
    glClearColor(red, green, blue, alpha);
}

void M_arc_backend_switchgdx_SwitchGL_glClearDepthf_float(jcontext ctx, jobject self, jfloat depth) {
    glClearDepthf(depth);
}

void M_arc_backend_switchgdx_SwitchGL_glClearStencil_int(jcontext ctx, jobject self, jint s) {
    glClearStencil(s);
}

void M_arc_backend_switchgdx_SwitchGL_glColorMask_boolean_boolean_boolean_boolean(jcontext ctx, jobject self, jbool red, jbool green, jbool blue, jbool alpha) {
    glColorMask(red, green, blue, alpha);
}

void M_arc_backend_switchgdx_SwitchGL_glCompressedTexImage2D_int_int_int_int_int_int_int_java_nio_Buffer(jcontext ctx, jobject self, jint target, jint level, jint internalformat, jint width, jint height, jint border, jint imageSize, jobject data) {
    glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, (void *) getBufferAddress(ctx, (java_nio_Buffer *) data));
}

void M_arc_backend_switchgdx_SwitchGL_glCompressedTexSubImage2D_int_int_int_int_int_int_int_int_java_nio_Buffer(jcontext ctx, jobject self, jint target, jint level, jint xoffset, jint yoffset, jint width, jint height, jint format, jint imageSize, jobject data) {
    glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, (void *) getBufferAddress(ctx, (java_nio_Buffer *) data));
}

void M_arc_backend_switchgdx_SwitchGL_glCopyTexImage2D_int_int_int_int_int_int_int_int(jcontext ctx, jobject self, jint target, jint level, jint internalformat, jint x, jint y, jint width, jint height, jint border) {
    glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void M_arc_backend_switchgdx_SwitchGL_glCopyTexSubImage2D_int_int_int_int_int_int_int_int(jcontext ctx, jobject self, jint target, jint level, jint xoffset, jint yoffset, jint x, jint y, jint width, jint height) {
    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void M_arc_backend_switchgdx_SwitchGL_glCullFace_int(jcontext ctx, jobject self, jint mode) {
    glCullFace(mode);
}

void M_arc_backend_switchgdx_SwitchGL_glDeleteTexture_int(jcontext ctx, jobject self, jint texture) {
    glDeleteTextures(1, (GLuint *) &texture);
}

void M_arc_backend_switchgdx_SwitchGL_glDepthFunc_int(jcontext ctx, jobject self, jint func) {
    glDepthFunc(func);
}

void M_arc_backend_switchgdx_SwitchGL_glDepthMask_boolean(jcontext ctx, jobject self, jbool flag) {
    glDepthMask(flag);
}

void M_arc_backend_switchgdx_SwitchGL_glDepthRangef_float_float(jcontext ctx, jobject self, jfloat zNear, jfloat zFar) {
    glDepthRangef(zNear, zFar);
}

void M_arc_backend_switchgdx_SwitchGL_glDisable_int(jcontext ctx, jobject self, jint cap) {
    glDisable(cap);
}

void M_arc_backend_switchgdx_SwitchGL_glDrawArrays_int_int_int(jcontext ctx, jobject self, jint mode, jint first, jint count) {
    glDrawArrays(mode, first, count);
}

void M_arc_backend_switchgdx_SwitchGL_glDrawElements_int_int_int_java_nio_Buffer(jcontext ctx, jobject self, jint mode, jint count, jint type, jobject indices) {
    glDrawElements(mode, count, type, (void *) getBufferAddress(ctx, (java_nio_Buffer *) indices));
}

void M_arc_backend_switchgdx_SwitchGL_glEnable_int(jcontext ctx, jobject self, jint cap) {
    glEnable(cap);
}

void M_arc_backend_switchgdx_SwitchGL_glFinish(jcontext ctx, jobject self) {
    glFinish();
}

void M_arc_backend_switchgdx_SwitchGL_glFlush(jcontext ctx, jobject self) {
    glFlush();
}

void M_arc_backend_switchgdx_SwitchGL_glFrontFace_int(jcontext ctx, jobject self, jint mode) {
    glFrontFace(mode);
}

jint M_arc_backend_switchgdx_SwitchGL_glGenTexture_R_int(jcontext ctx, jobject self) {
    GLuint texture;
    glGenTextures(1, &texture);
    return (jint) texture;
}

jint M_arc_backend_switchgdx_SwitchGL_glGetError_R_int(jcontext ctx, jobject self) {
    return (jint) glGetError();
}

void M_arc_backend_switchgdx_SwitchGL_glGetIntegerv_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint pname, jobject params) {
    glGetIntegerv(pname, (GLint *) getBufferAddress((java_nio_IntBuffer *) params));
}

jobject M_arc_backend_switchgdx_SwitchGL_glGetString_int_R_java_lang_String(jcontext ctx, jobject self, jint name) {
    return (jobject) stringFromNative(ctx, (char *) glGetString(name));
}

void M_arc_backend_switchgdx_SwitchGL_glHint_int_int(jcontext ctx, jobject self, jint target, jint mode) {
    glHint(target, mode);
}

void M_arc_backend_switchgdx_SwitchGL_glLineWidth_float(jcontext ctx, jobject self, jfloat width) {
    glLineWidth(width);
}

void M_arc_backend_switchgdx_SwitchGL_glPixelStorei_int_int(jcontext ctx, jobject self, jint pname, jint param) {
    glPixelStorei(pname, param);
}

void M_arc_backend_switchgdx_SwitchGL_glPolygonOffset_float_float(jcontext ctx, jobject self, jfloat factor, jfloat units) {
    glPolygonOffset(factor, units);
}

void M_arc_backend_switchgdx_SwitchGL_glReadPixels_int_int_int_int_int_int_java_nio_Buffer(jcontext ctx, jobject self, jint x, jint y, jint width, jint height, jint format, jint type, jobject pixels) {
    glReadPixels(x, y, width, height, format, type, (void *) getBufferAddress(ctx, (java_nio_Buffer *) pixels));
}

void M_arc_backend_switchgdx_SwitchGL_glScissor_int_int_int_int(jcontext ctx, jobject self, jint x, jint y, jint width, jint height) {
    glScissor(x, y, width, height);
}

void M_arc_backend_switchgdx_SwitchGL_glStencilFunc_int_int_int(jcontext ctx, jobject self, jint func, jint ref, jint mask) {
    glStencilFunc(func, ref, mask);
}

void M_arc_backend_switchgdx_SwitchGL_glStencilMask_int(jcontext ctx, jobject self, jint mask) {
    glStencilMask(mask);
}

void M_arc_backend_switchgdx_SwitchGL_glStencilOp_int_int_int(jcontext ctx, jobject self, jint fail, jint zfail, jint zpass) {
    glStencilOp(fail, zfail, zpass);
}

void M_arc_backend_switchgdx_SwitchGL_glTexImage2D_int_int_int_int_int_int_int_int_java_nio_Buffer(jcontext ctx, jobject self, jint target, jint level, jint internalformat, jint width, jint height, jint border, jint format, jint type, jobject pixels) {
    glTexImage2D(target, level, internalformat, width, height, border, format, type, (void *) getBufferAddress(ctx, (java_nio_Buffer *) pixels));
}

void M_arc_backend_switchgdx_SwitchGL_glTexParameterf_int_int_float(jcontext ctx, jobject self, jint target, jint pname, jfloat param) {
    glTexParameterf(target, pname, param);
}

void M_arc_backend_switchgdx_SwitchGL_glTexSubImage2D_int_int_int_int_int_int_int_int_java_nio_Buffer(jcontext ctx, jobject self, jint target, jint level, jint xoffset, jint yoffset, jint width, jint height, jint format, jint type, jobject pixels) {
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, (void *) getBufferAddress(ctx, (java_nio_Buffer *) pixels));
}

void M_arc_backend_switchgdx_SwitchGL_glViewport_int_int_int_int(jcontext ctx, jobject self, jint x, jint y, jint width, jint height) {
    glViewport(x, y, width, height);
}

void M_arc_backend_switchgdx_SwitchGL_glAttachShader_int_int(jcontext ctx, jobject self, jint program, jint shader) {
    glAttachShader(program, shader);
}

void M_arc_backend_switchgdx_SwitchGL_glBindAttribLocation_int_int_java_lang_String(jcontext ctx, jobject self, jint program, jint index, jobject name) {
    glBindAttribLocation(program, index, stringToNative(ctx, (jstring) name));
}

void M_arc_backend_switchgdx_SwitchGL_glBindBuffer_int_int(jcontext ctx, jobject self, jint target, jint buffer) {
    glBindBuffer(target, buffer);
}

void M_arc_backend_switchgdx_SwitchGL_glBindFramebuffer_int_int(jcontext ctx, jobject self, jint target, jint framebuffer) {
    glBindFramebuffer(target, framebuffer);
}

void M_arc_backend_switchgdx_SwitchGL_glBindRenderbuffer_int_int(jcontext ctx, jobject self, jint target, jint renderbuffer) {
    glBindRenderbuffer(target, renderbuffer);
}

void M_arc_backend_switchgdx_SwitchGL_glBlendColor_float_float_float_float(jcontext ctx, jobject self, jfloat red, jfloat green, jfloat blue, jfloat alpha) {
    glBlendColor(red, green, blue, alpha);
}

void M_arc_backend_switchgdx_SwitchGL_glBlendEquation_int(jcontext ctx, jobject self, jint mode) {
    glBlendEquation(mode);
}

void M_arc_backend_switchgdx_SwitchGL_glBlendEquationSeparate_int_int(jcontext ctx, jobject self, jint modeRGB, jint modeAlpha) {
    glBlendEquationSeparate(modeRGB, modeAlpha);
}

void M_arc_backend_switchgdx_SwitchGL_glBlendFuncSeparate_int_int_int_int(jcontext ctx, jobject self, jint srcRGB, jint dstRGB, jint srcAlpha, jint dstAlpha) {
    glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void M_arc_backend_switchgdx_SwitchGL_glBufferData_int_int_java_nio_Buffer_int(jcontext ctx, jobject self, jint target, jint size, jobject buffer, jint usage) {
    glBufferData(target, size, (void *) getBufferAddress(ctx, (java_nio_Buffer *) buffer), usage);
}

void M_arc_backend_switchgdx_SwitchGL_glBufferSubData_int_int_int_java_nio_Buffer(jcontext ctx, jobject self, jint target, jint offset, jint size, jobject data) {
    glBufferSubData(target, offset, size, (void *) getBufferAddress(ctx, (java_nio_Buffer *) data));
}

jint M_arc_backend_switchgdx_SwitchGL_glCheckFramebufferStatus_int_R_int(jcontext ctx, jobject self, jint target) {
    return (jint) glCheckFramebufferStatus(target);
}

void M_arc_backend_switchgdx_SwitchGL_glCompileShader_int(jcontext ctx, jobject self, jint shader) {
    glCompileShader(shader);
}

jint M_arc_backend_switchgdx_SwitchGL_glCreateProgram_R_int(jcontext ctx, jobject self) {
    return (int) glCreateProgram();
}

jint M_arc_backend_switchgdx_SwitchGL_glCreateShader_int_R_int(jcontext ctx, jobject self, jint type) {
    return (int) glCreateShader(type);
}

void M_arc_backend_switchgdx_SwitchGL_glDeleteBuffer_int(jcontext ctx, jobject self, jint buffer) {
    glDeleteBuffers(1, (GLuint *) &buffer);
}

void M_arc_backend_switchgdx_SwitchGL_glDeleteFramebuffer_int(jcontext ctx, jobject self, jint framebuffer) {
    glDeleteFramebuffers(1, (GLuint *) &framebuffer);
}

void M_arc_backend_switchgdx_SwitchGL_glDeleteProgram_int(jcontext ctx, jobject self, jint program) {
    glDeleteProgram(program);
}

void M_arc_backend_switchgdx_SwitchGL_glDeleteRenderbuffer_int(jcontext ctx, jobject self, jint renderbuffer) {
    glDeleteRenderbuffers(1, (GLuint *) &renderbuffer);
}

void M_arc_backend_switchgdx_SwitchGL_glDeleteShader_int(jcontext ctx, jobject self, jint shader) {
    glDeleteShader(shader);
}

void M_arc_backend_switchgdx_SwitchGL_glDetachShader_int_int(jcontext ctx, jobject self, jint program, jint shader) {
    glDetachShader(program, shader);
}

void M_arc_backend_switchgdx_SwitchGL_glDisableVertexAttribArray_int(jcontext ctx, jobject self, jint index) {
    glDisableVertexAttribArray(index);
}

void M_arc_backend_switchgdx_SwitchGL_glDrawElements_int_int_int_int(jcontext ctx, jobject self, jint mode, jint count, jint type, jint indices) {
    glDrawElements(mode, count, type, (void *) (jlong) indices);
}

void M_arc_backend_switchgdx_SwitchGL_glEnableVertexAttribArray_int(jcontext ctx, jobject self, jint index) {
    glEnableVertexAttribArray(index);
}

void M_arc_backend_switchgdx_SwitchGL_glFramebufferRenderbuffer_int_int_int_int(jcontext ctx, jobject self, jint target, jint attachment, jint renderbuffertarget, jint renderbuffer) {
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void M_arc_backend_switchgdx_SwitchGL_glFramebufferTexture2D_int_int_int_int_int(jcontext ctx, jobject self, jint target, jint attachment, jint textarget, jint texture, jint level) {
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

jint M_arc_backend_switchgdx_SwitchGL_glGenBuffer_R_int(jcontext ctx, jobject self) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    return (jint) buffer;
}

void M_arc_backend_switchgdx_SwitchGL_glGenerateMipmap_int(jcontext ctx, jobject self, jint target) {
    glGenerateMipmap(target);
}

jint M_arc_backend_switchgdx_SwitchGL_glGenFramebuffer_R_int(jcontext ctx, jobject self) {
    GLuint buffer;
    glGenFramebuffers(1, &buffer);
    return (jint) buffer;
}

jint M_arc_backend_switchgdx_SwitchGL_glGenRenderbuffer_R_int(jcontext ctx, jobject self) {
    GLuint buffer;
    glGenRenderbuffers(1, &buffer);
    return (jint) buffer;
}

jobject M_arc_backend_switchgdx_SwitchGL_glGetActiveAttrib_int_int_java_nio_IntBuffer_java_nio_IntBuffer_R_java_lang_String(jcontext ctx, jobject self, jint program, jint index, jobject size, jobject type) {
    char buffer[64];
    glGetActiveAttrib(program, index, 63, nullptr, (GLint *) getBufferAddress((java_nio_IntBuffer *) size), (GLenum *) getBufferAddress((java_nio_IntBuffer *) type), buffer);
    return (jobject) stringFromNative(ctx, buffer);
}

jobject M_arc_backend_switchgdx_SwitchGL_glGetActiveUniform_int_int_java_nio_IntBuffer_java_nio_IntBuffer_R_java_lang_String(jcontext ctx, jobject self, jint program, jint index, jobject size, jobject type) {
    char buffer[64];
    glGetActiveUniform(program, index, 63, nullptr, (GLint *) getBufferAddress((java_nio_IntBuffer *) size), (GLenum *) getBufferAddress((java_nio_IntBuffer *) type), buffer);
    return (jobject) stringFromNative(ctx, buffer);
}

jint M_arc_backend_switchgdx_SwitchGL_glGetAttribLocation_int_java_lang_String_R_int(jcontext ctx, jobject self, jint program, jobject name) {
    return glGetAttribLocation(program, stringToNative(ctx, (jstring) name));
}

void M_arc_backend_switchgdx_SwitchGL_glGetBooleanv_int_java_nio_Buffer(jcontext ctx, jobject self, jint pname, jobject params) {
    glGetBooleanv(pname, (GLboolean *) getBufferAddress(ctx, (java_nio_Buffer *) params));
}

void M_arc_backend_switchgdx_SwitchGL_glGetBufferParameteriv_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint target, jint pname, jobject params) {
    glGetBufferParameteriv(target, pname, (GLint *) getBufferAddress((java_nio_IntBuffer *) params));
}

void M_arc_backend_switchgdx_SwitchGL_glGetFloatv_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint pname, jobject params) {
    glGetFloatv(pname, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) params));
}

void M_arc_backend_switchgdx_SwitchGL_glGetFramebufferAttachmentParameteriv_int_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint target, jint attachment, jint pname, jobject params) {
    glGetFramebufferAttachmentParameteriv(target, attachment, pname, (GLint *) getBufferAddress((java_nio_IntBuffer *) params));
}

void M_arc_backend_switchgdx_SwitchGL_glGetProgramiv_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint program, jint pname, jobject params) {
    glGetProgramiv(program, pname, (GLint *) getBufferAddress((java_nio_IntBuffer *) params));
}

jobject M_arc_backend_switchgdx_SwitchGL_glGetProgramInfoLog_int_R_java_lang_String(jcontext ctx, jobject self, jint program) {
    char buffer[128];
    glGetProgramInfoLog(program, 127, nullptr, buffer);
    return (jobject) stringFromNative(ctx, buffer);
}

void M_arc_backend_switchgdx_SwitchGL_glGetRenderbufferParameteriv_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint target, jint pname, jobject params) {
    glGetRenderbufferParameteriv(target, pname, (GLint *) getBufferAddress((java_nio_IntBuffer *) params));
}

void M_arc_backend_switchgdx_SwitchGL_glGetShaderiv_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint shader, jint pname, jobject params) {
    glGetShaderiv(shader, pname, (GLint *) getBufferAddress((java_nio_IntBuffer *) params));
}

jobject M_arc_backend_switchgdx_SwitchGL_glGetShaderInfoLog_int_R_java_lang_String(jcontext ctx, jobject self, jint program) {
    char buffer[128];
    glGetShaderInfoLog(program, 127, nullptr, buffer);
    return (jobject) stringFromNative(ctx, buffer);
}

void M_arc_backend_switchgdx_SwitchGL_glGetShaderPrecisionFormat_int_int_java_nio_IntBuffer_java_nio_IntBuffer(jcontext ctx, jobject self, jint shadertype, jint precisiontype, jobject range, jobject precision) {
    glGetShaderPrecisionFormat(shadertype, precisiontype, (GLint *) getBufferAddress((java_nio_IntBuffer *) range), (GLint *) getBufferAddress((java_nio_IntBuffer *) precision));
}

void M_arc_backend_switchgdx_SwitchGL_glGetTexParameterfv_int_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint target, jint pname, jobject params) {
    glGetTexParameterfv(target, pname, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) params));
}

void M_arc_backend_switchgdx_SwitchGL_glGetTexParameteriv_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint target, jint pname, jobject params) {
    glGetTexParameteriv(target, pname, (GLint *) getBufferAddress((java_nio_IntBuffer *) params));
}

void M_arc_backend_switchgdx_SwitchGL_glGetUniformfv_int_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint program, jint location, jobject params) {
    glGetUniformfv(program, location, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) params));
}

void M_arc_backend_switchgdx_SwitchGL_glGetUniformiv_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint program, jint location, jobject params) {
    glGetUniformiv(program, location, (GLint *) getBufferAddress((java_nio_IntBuffer *) params));
}

jint M_arc_backend_switchgdx_SwitchGL_glGetUniformLocation_int_java_lang_String_R_int(jcontext ctx, jobject self, jint program, jobject name) {
    return glGetUniformLocation(program, stringToNative(ctx, (jstring) name));
}

void M_arc_backend_switchgdx_SwitchGL_glGetVertexAttribfv_int_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint index, jint pname, jobject params) {
    glGetVertexAttribfv(index, pname, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) params));
}

void M_arc_backend_switchgdx_SwitchGL_glGetVertexAttribiv_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint index, jint pname, jobject params) {
    glGetVertexAttribiv(index, pname, (GLint *) getBufferAddress((java_nio_IntBuffer *) params));
}

jbool M_arc_backend_switchgdx_SwitchGL_glIsBuffer_int_R_boolean(jcontext ctx, jobject self, jint buffer) {
    return glIsBuffer(buffer);
}

jbool M_arc_backend_switchgdx_SwitchGL_glIsEnabled_int_R_boolean(jcontext ctx, jobject self, jint cap) {
    return glIsEnabled(cap);
}

jbool M_arc_backend_switchgdx_SwitchGL_glIsFramebuffer_int_R_boolean(jcontext ctx, jobject self, jint framebuffer) {
    return glIsFramebuffer(framebuffer);
}

jbool M_arc_backend_switchgdx_SwitchGL_glIsProgram_int_R_boolean(jcontext ctx, jobject self, jint program) {
    return glIsProgram(program);
}

jbool M_arc_backend_switchgdx_SwitchGL_glIsRenderbuffer_int_R_boolean(jcontext ctx, jobject self, jint renderbuffer) {
    return glIsRenderbuffer(renderbuffer);
}

jbool M_arc_backend_switchgdx_SwitchGL_glIsShader_int_R_boolean(jcontext ctx, jobject self, jint shader) {
    return glIsShader(shader);
}

jbool M_arc_backend_switchgdx_SwitchGL_glIsTexture_int_R_boolean(jcontext ctx, jobject self, jint texture) {
    return glIsTexture(texture);
}

void M_arc_backend_switchgdx_SwitchGL_glLinkProgram_int(jcontext ctx, jobject self, jint program) {
    glLinkProgram(program);
}

void M_arc_backend_switchgdx_SwitchGL_glReleaseShaderCompiler(jcontext ctx, jobject self) {
    glReleaseShaderCompiler();
}

void M_arc_backend_switchgdx_SwitchGL_glRenderbufferStorage_int_int_int_int(jcontext ctx, jobject self, jint target, jint internalformat, jint width, jint height) {
    glRenderbufferStorage(target, internalformat, width, height);
}

void M_arc_backend_switchgdx_SwitchGL_glSampleCoverage_float_boolean(jcontext ctx, jobject self, jfloat value, jbool invert) {
    glSampleCoverage(value, invert);
}

void M_arc_backend_switchgdx_SwitchGL_glShaderSource_int_java_lang_String(jcontext ctx, jobject self, jint shader, jobject sourceObject) {
    auto source = stringToNative(ctx, (jstring) sourceObject);
    glShaderSource(shader, 1, &source, nullptr);
}

void M_arc_backend_switchgdx_SwitchGL_glStencilFuncSeparate_int_int_int_int(jcontext ctx, jobject self, jint face, jint func, jint ref, jint mask) {
    glStencilFuncSeparate(face, func, ref, mask);
}

void M_arc_backend_switchgdx_SwitchGL_glStencilMaskSeparate_int_int(jcontext ctx, jobject self, jint face, jint mask) {
    glStencilMaskSeparate(face, mask);
}

void M_arc_backend_switchgdx_SwitchGL_glStencilOpSeparate_int_int_int_int(jcontext ctx, jobject self, jint face, jint fail, jint zfail, jint zpass) {
    glStencilOpSeparate(face, fail, zfail, zpass);
}

void M_arc_backend_switchgdx_SwitchGL_glTexParameterfv_int_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint target, jint pname, jobject params) {
    glTexParameterfv(target, pname, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) params));
}

void M_arc_backend_switchgdx_SwitchGL_glTexParameteri_int_int_int(jcontext ctx, jobject self, jint target, jint pname, jint param) {
    glTexParameteri(target, pname, param);
}

void M_arc_backend_switchgdx_SwitchGL_glTexParameteriv_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint target, jint pname, jobject params) {
    glTexParameteriv(target, pname, (GLint *) getBufferAddress((java_nio_IntBuffer *) params));
}

void M_arc_backend_switchgdx_SwitchGL_glUniform1f_int_float(jcontext ctx, jobject self, jint location, jfloat x) {
    glUniform1f(location, x);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform1fv_int_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint location, jint count, jobject v) {
    glUniform1fv(location, count, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) v));
}

void M_arc_backend_switchgdx_SwitchGL_glUniform1fv_int_int_Array1_float_int(jcontext ctx, jobject self, jint location, jint count, jobject v, jint offset) {
    glUniform1fv(location, count, (GLfloat *) ((jarray) v)->data + offset);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform1i_int_int(jcontext ctx, jobject self, jint location, jint x) {
    glUniform1i(location, x);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform1iv_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint location, jint count, jobject v) {
    glUniform1iv(location, count, (GLint *) getBufferAddress((java_nio_IntBuffer *) v));
}

void M_arc_backend_switchgdx_SwitchGL_glUniform1iv_int_int_Array1_int_int(jcontext ctx, jobject self, jint location, jint count, jobject v, jint offset) {
    glUniform1iv(location, count, (GLint *) ((jarray) v)->data + offset);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform2f_int_float_float(jcontext ctx, jobject self, jint location, jfloat x, jfloat y) {
    glUniform2f(location, x, y);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform2fv_int_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint location, jint count, jobject v) {
    glUniform2fv(location, count, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) v));
}

void M_arc_backend_switchgdx_SwitchGL_glUniform2fv_int_int_Array1_float_int(jcontext ctx, jobject self, jint location, jint count, jobject v, jint offset) {
    glUniform2fv(location, count, (GLfloat *) ((jarray) v)->data + offset);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform2i_int_int_int(jcontext ctx, jobject self, jint location, jint x, jint y) {
    glUniform2i(location, x, y);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform2iv_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint location, jint count, jobject v) {
    glUniform2iv(location, count, (GLint *) getBufferAddress((java_nio_IntBuffer *) v));
}

void M_arc_backend_switchgdx_SwitchGL_glUniform2iv_int_int_Array1_int_int(jcontext ctx, jobject self, jint location, jint count, jobject v, jint offset) {
    glUniform2iv(location, count, (GLint *) ((jarray) v)->data + offset);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform3f_int_float_float_float(jcontext ctx, jobject self, jint location, jfloat x, jfloat y, jfloat z) {
    glUniform3f(location, x, y, z);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform3fv_int_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint location, jint count, jobject v) {
    glUniform3fv(location, count, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) v));
}

void M_arc_backend_switchgdx_SwitchGL_glUniform3fv_int_int_Array1_float_int(jcontext ctx, jobject self, jint location, jint count, jobject v, jint offset) {
    glUniform3fv(location, count, (GLfloat *) ((jarray) v)->data + offset);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform3i_int_int_int_int(jcontext ctx, jobject self, jint location, jint x, jint y, jint z) {
    glUniform3i(location, x, y, z);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform3iv_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint location, jint count, jobject v) {
    glUniform3iv(location, count, (GLint *) getBufferAddress((java_nio_IntBuffer *) v));
}

void M_arc_backend_switchgdx_SwitchGL_glUniform3iv_int_int_Array1_int_int(jcontext ctx, jobject self, jint location, jint count, jobject v, jint offset) {
    glUniform3iv(location, count, (GLint *) ((jarray) v)->data + offset);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform4f_int_float_float_float_float(jcontext ctx, jobject self, jint location, jfloat x, jfloat y, jfloat z, jfloat w) {
    glUniform4f(location, x, y, z, w);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform4fv_int_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint location, jint count, jobject v) {
    glUniform4fv(location, count, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) v));
}

void M_arc_backend_switchgdx_SwitchGL_glUniform4fv_int_int_Array1_float_int(jcontext ctx, jobject self, jint location, jint count, jobject v, jint offset) {
    glUniform4fv(location, count, (GLfloat *) ((jarray) v)->data + offset);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform4i_int_int_int_int_int(jcontext ctx, jobject self, jint location, jint x, jint y, jint z, jint w) {
    glUniform4i(location, x, y, z, w);
}

void M_arc_backend_switchgdx_SwitchGL_glUniform4iv_int_int_java_nio_IntBuffer(jcontext ctx, jobject self, jint location, jint count, jobject v) {
    glUniform4iv(location, count, (GLint *) getBufferAddress((java_nio_IntBuffer *) v));
}

void M_arc_backend_switchgdx_SwitchGL_glUniform4iv_int_int_Array1_int_int(jcontext ctx, jobject self, jint location, jint count, jobject v, jint offset) {
    glUniform4iv(location, count, (GLint *) ((jarray) v)->data + offset);
}

void M_arc_backend_switchgdx_SwitchGL_glUniformMatrix2fv_int_int_boolean_java_nio_FloatBuffer(jcontext ctx, jobject self, jint location, jint count, jbool transpose, jobject value) {
    glUniformMatrix2fv(location, count, transpose, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) value));
}

void M_arc_backend_switchgdx_SwitchGL_glUniformMatrix2fv_int_int_boolean_Array1_float_int(jcontext ctx, jobject self, jint location, jint count, jbool transpose, jobject value, jint offset) {
    glUniformMatrix2fv(location, count, transpose, (GLfloat *) ((jarray) value)->data + offset);
}

void M_arc_backend_switchgdx_SwitchGL_glUniformMatrix3fv_int_int_boolean_java_nio_FloatBuffer(jcontext ctx, jobject self, jint location, jint count, jbool transpose, jobject value) {
    glUniformMatrix3fv(location, count, transpose, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) value));
}

void M_arc_backend_switchgdx_SwitchGL_glUniformMatrix3fv_int_int_boolean_Array1_float_int(jcontext ctx, jobject self, jint location, jint count, jbool transpose, jobject value, jint offset) {
    glUniformMatrix3fv(location, count, transpose, (GLfloat *) ((jarray) value)->data + offset);
}

void M_arc_backend_switchgdx_SwitchGL_glUniformMatrix4fv_int_int_boolean_java_nio_FloatBuffer(jcontext ctx, jobject self, jint location, jint count, jbool transpose, jobject value) {
    glUniformMatrix4fv(location, count, transpose, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) value));
}

void M_arc_backend_switchgdx_SwitchGL_glUniformMatrix4fv_int_int_boolean_Array1_float_int(jcontext ctx, jobject self, jint location, jint count, jbool transpose, jobject value, jint offset) {
    glUniformMatrix4fv(location, count, transpose, (GLfloat *) ((jarray) value)->data + offset);
}

void M_arc_backend_switchgdx_SwitchGL_glUseProgram_int(jcontext ctx, jobject self, jint program) {
    glUseProgram(program);
}

void M_arc_backend_switchgdx_SwitchGL_glValidateProgram_int(jcontext ctx, jobject self, jint program) {
    glValidateProgram(program);
}

void M_arc_backend_switchgdx_SwitchGL_glVertexAttrib1f_int_float(jcontext ctx, jobject self, jint indx, jfloat x) {
    glVertexAttrib1f(indx, x);
}

void M_arc_backend_switchgdx_SwitchGL_glVertexAttrib1fv_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint indx, jobject values) {
    glVertexAttrib1fv(indx, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) values));
}

void M_arc_backend_switchgdx_SwitchGL_glVertexAttrib2f_int_float_float(jcontext ctx, jobject self, jint indx, jfloat x, jfloat y) {
    glVertexAttrib2f(indx, x, y);
}

void M_arc_backend_switchgdx_SwitchGL_glVertexAttrib2fv_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint indx, jobject values) {
    glVertexAttrib2fv(indx, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) values));
}

void M_arc_backend_switchgdx_SwitchGL_glVertexAttrib3f_int_float_float_float(jcontext ctx, jobject self, jint indx, jfloat x, jfloat y, jfloat z) {
    glVertexAttrib3f(indx, x, y, z);
}

void M_arc_backend_switchgdx_SwitchGL_glVertexAttrib3fv_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint indx, jobject values) {
    glVertexAttrib3fv(indx, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) values));
}

void M_arc_backend_switchgdx_SwitchGL_glVertexAttrib4f_int_float_float_float_float(jcontext ctx, jobject self, jint indx, jfloat x, jfloat y, jfloat z, jfloat w) {
    glVertexAttrib4f(indx, x, y, z, w);
}

void M_arc_backend_switchgdx_SwitchGL_glVertexAttrib4fv_int_java_nio_FloatBuffer(jcontext ctx, jobject self, jint indx, jobject values) {
    glVertexAttrib4fv(indx, (GLfloat *) getBufferAddress((java_nio_FloatBuffer *) values));
}

void M_arc_backend_switchgdx_SwitchGL_glVertexAttribPointer_int_int_int_boolean_int_java_nio_Buffer(jcontext ctx, jobject self, jint index, jint size, jint type, jbool normalized, jint stride, jobject ptr) {
    glVertexAttribPointer(index, size, type, normalized, stride, getBufferAddress(ctx, (java_nio_Buffer *) ptr));
}

void M_arc_backend_switchgdx_SwitchGL_glVertexAttribPointer_int_int_int_boolean_int_int(jcontext ctx, jobject self, jint index, jint size, jint type, jbool normalized, jint stride, jint ptr) {
    glVertexAttribPointer(index, size, type, normalized, stride, (void *) (jlong) ptr);
}

}
