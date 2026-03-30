#include "qtglprocaddress.h"

#ifdef RIVEQT_ENABLE_OPENGL

#include <QOpenGLContext>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
void* fallbackDesktopGLProcAddress(const char* name)
{
#ifdef Q_OS_WIN
  static HMODULE s_openGLLibrary = LoadLibraryA("opengl32.dll");
  return s_openGLLibrary
    ? reinterpret_cast<void*>(GetProcAddress(s_openGLLibrary, name))
    : nullptr;
#elif defined(Q_OS_MACOS)
  if (void* symbol = dlsym(RTLD_DEFAULT, name)) {
    return symbol;
  }
  static void* s_openGLLibrary = dlopen(
    "/System/Library/Frameworks/OpenGL.framework/OpenGL",
    RTLD_LAZY | RTLD_LOCAL);
  return s_openGLLibrary ? dlsym(s_openGLLibrary, name) : nullptr;
#else
  static void* s_openGLLibrary = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
  if (s_openGLLibrary) {
    if (void* symbol = dlsym(s_openGLLibrary, name)) {
      return symbol;
    }
  }
  return dlsym(RTLD_DEFAULT, name);
#endif
}
} // namespace

void* currentQtOpenGLContext()
{
  return QOpenGLContext::currentContext();
}

bool isQtOpenGLContextCurrent(void* context)
{
  return QOpenGLContext::currentContext() == context;
}

void* resolveQtOpenGLProcAddress(void* context, const char* name)
{
  auto* glContext = static_cast<QOpenGLContext*>(context);
  void* procAddress = glContext
    ? reinterpret_cast<void*>(glContext->getProcAddress(name))
    : nullptr;
  return procAddress ? procAddress : fallbackDesktopGLProcAddress(name);
}

QtOpenGLContextInfo queryQtOpenGLContextInfo(void* context)
{
  auto* glContext = static_cast<QOpenGLContext*>(context);
  if (!glContext) {
    return {};
  }

  const QSurfaceFormat format = glContext->format();
  return {
    .valid = true,
    .isOpenGLES = glContext->isOpenGLES(),
    .majorVersion = format.majorVersion(),
    .minorVersion = format.minorVersion(),
  };
}

#else

void* currentQtOpenGLContext()
{
  return nullptr;
}

bool isQtOpenGLContextCurrent(void*)
{
  return false;
}

void* resolveQtOpenGLProcAddress(void*, const char*)
{
  return nullptr;
}

QtOpenGLContextInfo queryQtOpenGLContextInfo(void*)
{
  return {};
}

#endif
