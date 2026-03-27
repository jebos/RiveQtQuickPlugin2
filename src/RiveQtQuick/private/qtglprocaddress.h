#pragma once

void* currentQtOpenGLContext();
bool isQtOpenGLContextCurrent(void* context);
void* resolveQtOpenGLProcAddress(void* context, const char* name);
