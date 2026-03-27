#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glad_custom.h"

int GLAD_GL_version_es = 1;
int GLAD_GL_version_major = 1;
int GLAD_GL_version_minor = 0;

static void load_Desktop_GL(GLADloadfunc load) {
    if (GLAD_GL_version_es) return;

    GLAD_GL_ANGLE_polygon_mode = 1;
    glad_glPolygonModeANGLE = (PFNGLPOLYGONMODEANGLEPROC)load("glPolygonMode");

    if (GLAD_IS_GL_VERSION_AT_LEAST(4, 2)) {
        GLAD_GL_EXT_base_instance = 1;
        glad_glDrawArraysInstancedBaseInstanceEXT = (PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC)load("glDrawArraysInstancedBaseInstance");
        glad_glDrawElementsInstancedBaseInstanceEXT = (PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC)load("glDrawElementsInstancedBaseInstance");
        glad_glDrawElementsInstancedBaseVertexBaseInstanceEXT = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC)load("glDrawElementsInstancedBaseVertexBaseInstance");
    }

    if (GLAD_IS_GL_VERSION_AT_LEAST(4, 6))
    {
        GLAD_GL_ANGLE_base_vertex_base_instance_shader_builtin = 1;
    }
}

PFNGLFRAMEBUFFERMEMORYLESSPIXELLOCALSTORAGEANGLEPROC glad_glFramebufferMemorylessPixelLocalStorageANGLE = NULL;
PFNGLFRAMEBUFFERTEXTUREPIXELLOCALSTORAGEANGLEPROC glad_glFramebufferTexturePixelLocalStorageANGLE = NULL;
PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEFVANGLEPROC glad_glFramebufferPixelLocalClearValuefvANGLE = NULL;
PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEIVANGLEPROC glad_glFramebufferPixelLocalClearValueivANGLE = NULL;
PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEUIVANGLEPROC glad_glFramebufferPixelLocalClearValueuivANGLE = NULL;
PFNGLBEGINPIXELLOCALSTORAGEANGLEPROC glad_glBeginPixelLocalStorageANGLE = NULL;
PFNGLENDPIXELLOCALSTORAGEANGLEPROC glad_glEndPixelLocalStorageANGLE = NULL;
PFNGLPIXELLOCALSTORAGEBARRIERANGLEPROC glad_glPixelLocalStorageBarrierANGLE = NULL;
PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERFVANGLEPROC glad_glGetFramebufferPixelLocalStorageParameterfvANGLE = NULL;
PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERIVANGLEPROC glad_glGetFramebufferPixelLocalStorageParameterivANGLE = NULL;
PFNGLPOLYGONMODEANGLEPROC glad_glPolygonModeANGLE = NULL;
PFNGLPROVOKINGVERTEXANGLEPROC glad_glProvokingVertexANGLE = NULL;
/* #ifdef RIVE_DESKTOP_GL */
/* #endif */
int GLAD_GL_ANGLE_base_vertex_base_instance_shader_builtin = 0;
int GLAD_GL_ANGLE_shader_pixel_local_storage = 0;
int GLAD_GL_ANGLE_shader_pixel_local_storage_coherent = 0;
int GLAD_GL_ANGLE_polygon_mode = 0;
int GLAD_GL_ANGLE_provoking_vertex = 0;

static int has_extension_name(const char* extensions, const char* ext) {
    const char* location = NULL;
    const char* terminator = NULL;

    if (extensions == NULL || ext == NULL) {
        return 0;
    }

    while ((location = strstr(extensions, ext)) != NULL) {
        terminator = location + strlen(ext);
        if ((location == extensions || *(location - 1) == ' ') &&
            (*terminator == ' ' || *terminator == '\0')) {
            return 1;
        }
        extensions = terminator;
    }

    return 0;
}

static void load_GL_ANGLE_shader_pixel_local_storage(GLADloadfunc load) {
    if(!GLAD_GL_ANGLE_shader_pixel_local_storage) return;
    glad_glFramebufferMemorylessPixelLocalStorageANGLE = (PFNGLFRAMEBUFFERMEMORYLESSPIXELLOCALSTORAGEANGLEPROC)load("glFramebufferMemorylessPixelLocalStorageANGLE");
    glad_glFramebufferTexturePixelLocalStorageANGLE = (PFNGLFRAMEBUFFERTEXTUREPIXELLOCALSTORAGEANGLEPROC)load("glFramebufferTexturePixelLocalStorageANGLE");
    glad_glFramebufferPixelLocalClearValuefvANGLE = (PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEFVANGLEPROC)load("glFramebufferPixelLocalClearValuefvANGLE");
    glad_glFramebufferPixelLocalClearValueivANGLE = (PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEIVANGLEPROC)load("glFramebufferPixelLocalClearValueivANGLE");
    glad_glFramebufferPixelLocalClearValueuivANGLE = (PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEUIVANGLEPROC)load("glFramebufferPixelLocalClearValueuivANGLE");
    glad_glBeginPixelLocalStorageANGLE = (PFNGLBEGINPIXELLOCALSTORAGEANGLEPROC)load("glBeginPixelLocalStorageANGLE");
    glad_glEndPixelLocalStorageANGLE = (PFNGLENDPIXELLOCALSTORAGEANGLEPROC)load("glEndPixelLocalStorageANGLE");
    glad_glPixelLocalStorageBarrierANGLE = (PFNGLPIXELLOCALSTORAGEBARRIERANGLEPROC)load("glPixelLocalStorageBarrierANGLE");
    glad_glGetFramebufferPixelLocalStorageParameterfvANGLE = (PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERFVANGLEPROC)load("glGetFramebufferPixelLocalStorageParameterfvANGLE");
    glad_glGetFramebufferPixelLocalStorageParameterivANGLE = (PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERIVANGLEPROC)load("glGetFramebufferPixelLocalStorageParameterivANGLE");
}
static void load_GL_ANGLE_polygon_mode(GLADloadfunc load) {
    if(!GLAD_GL_ANGLE_polygon_mode) return;
    glad_glPolygonModeANGLE = (PFNGLPOLYGONMODEANGLEPROC)load("glPolygonModeANGLE");
}
static void load_GL_ANGLE_provoking_vertex(GLADloadfunc load) {
    if(!GLAD_GL_ANGLE_provoking_vertex) return;
    glad_glProvokingVertexANGLE = (PFNGLPROVOKINGVERTEXANGLEPROC)load("glProvokingVertexANGLE");
}
int gladLoadCustomLoader(GLADloadfunc load) {
    int ret = gladLoadGLES2(load);
    if (ret == 0) {
        return 0;
    }

    const char* version = (const char*)glGetString(GL_VERSION);
    GLAD_GL_version_es = strstr(version, "OpenGL ES") != NULL;
    if (GLAD_GL_version_es)
    {
#ifdef _MSC_VER
        sscanf_s(version, "OpenGL ES %d.%d", &GLAD_GL_version_major, &GLAD_GL_version_minor);
#else
        sscanf(version, "OpenGL ES %d.%d", &GLAD_GL_version_major, &GLAD_GL_version_minor);
#endif
    }
    else
    {
#ifdef _MSC_VER
        sscanf_s(version, "%d.%d", &GLAD_GL_version_major, &GLAD_GL_version_minor);
#else
        sscanf(version, "%d.%d", &GLAD_GL_version_major, &GLAD_GL_version_minor);
#endif
    }

    if ((GLAD_GL_version_es || GLAD_IS_GL_VERSION_AT_LEAST(3, 0)) &&
        glad_glGetIntegerv != NULL &&
        glad_glGetStringi != NULL)
    {
        GLint extensionCount = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
        for (int i = 0; i < extensionCount; ++i)
        {
            const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
            if (strcmp(ext, "GL_ANGLE_base_vertex_base_instance_shader_builtin") == 0)
            {
                GLAD_GL_ANGLE_base_vertex_base_instance_shader_builtin = 1;
            }
            if (strcmp(ext, "GL_ANGLE_shader_pixel_local_storage") == 0)
            {
                GLAD_GL_ANGLE_shader_pixel_local_storage = 1;
            }
            else if (strcmp(ext, "GL_ANGLE_shader_pixel_local_storage_coherent") == 0)
            {
                GLAD_GL_ANGLE_shader_pixel_local_storage_coherent = 1;
            }
            else if (strcmp(ext, "GL_ANGLE_polygon_mode") == 0)
            {
                GLAD_GL_ANGLE_polygon_mode = 1;
            }
            else if (strcmp(ext, "GL_ANGLE_provoking_vertex") == 0)
            {
                GLAD_GL_ANGLE_provoking_vertex = 1;
            }
        }
    }
    else
    {
        const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
        GLAD_GL_ANGLE_base_vertex_base_instance_shader_builtin =
            has_extension_name(extensions, "GL_ANGLE_base_vertex_base_instance_shader_builtin");
        GLAD_GL_ANGLE_shader_pixel_local_storage =
            has_extension_name(extensions, "GL_ANGLE_shader_pixel_local_storage");
        GLAD_GL_ANGLE_shader_pixel_local_storage_coherent =
            has_extension_name(extensions, "GL_ANGLE_shader_pixel_local_storage_coherent");
        GLAD_GL_ANGLE_polygon_mode =
            has_extension_name(extensions, "GL_ANGLE_polygon_mode");
        GLAD_GL_ANGLE_provoking_vertex =
            has_extension_name(extensions, "GL_ANGLE_provoking_vertex");
    }
    load_GL_ANGLE_shader_pixel_local_storage(load);
    load_GL_ANGLE_polygon_mode(load);
    load_GL_ANGLE_provoking_vertex(load);
    load_Desktop_GL(load);
    return ret;
}
