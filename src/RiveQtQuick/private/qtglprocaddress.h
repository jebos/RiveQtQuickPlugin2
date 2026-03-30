#pragma once

struct QtOpenGLContextInfo {
  bool valid { false };
  bool isOpenGLES { false };
  int majorVersion { 0 };
  int minorVersion { 0 };
};

void* currentQtOpenGLContext();
bool isQtOpenGLContextCurrent(void* context);
void* resolveQtOpenGLProcAddress(void* context, const char* name);
QtOpenGLContextInfo queryQtOpenGLContextInfo(void* context);
