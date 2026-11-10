// Boxshelf PS5 - PC preview: renders the real scene code through the kit's GL renderer on Mesa's surfaceless
// EGL (llvmpipe) into a 1920x1080 PPM. Adapted from Estante's tools/preview-estante.cpp.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// usage: preview-boxshelf-gl <out.ppm> <mode>
#include "app/scene.hpp"
#include "core/save_file.hpp"
#include "gfx/gl_program.hpp"
#include "gfx/renderer.hpp"
#include "gl_main.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr int kWidth = 1920, kHeight = 1080;

struct GlHost
{
    hui::gfx::Renderer renderer;
    hui::gfx::Font font;
    boxshelf::GlCanvas canvas;
    std::uint32_t font_texture = 0;
    GLuint fbo = 0, color = 0;

    bool init()
    {
        auto get_display = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
            eglGetProcAddress("eglGetPlatformDisplayEXT"));
        EGLDisplay display = get_display ? get_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
                                         : eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint major = 0, minor = 0;
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor) || !eglBindAPI(EGL_OPENGL_API))
            return false;
        const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 5,
                                     EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
        EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
        if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context))
            return false;
        hui::gfx::set_glsl_prefix("#version 450 core\n");
        if (!renderer.init())
            return false;
        std::string bytes;
        if (!hui::save::read_file("assets/fonts/inter.huifont", &bytes) || !font.load(bytes))
            return false;
        font_texture = renderer.batch().create_font_texture(font);
        glGenTextures(1, &color);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }

    void render(const boxshelf::app::SceneInput &input)
    {
        canvas.begin(&renderer.batch(), &font, font_texture);
        boxshelf::app::draw_scene(canvas, input);
        renderer.begin();
        canvas.submit(renderer);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        renderer.present(fbo, kWidth, kHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }

    bool write_ppm(const char *path)
    {
        FILE *f = std::fopen(path, "wb");
        if (!f)
            return false;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(kWidth) * kHeight * 4u);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        std::fprintf(f, "P6\n%d %d\n255\n", kWidth, kHeight);
        for (int y = kHeight - 1; y >= 0; y--)
            for (int x = 0; x < kWidth; x++)
                std::fwrite(&pixels[(static_cast<std::size_t>(y) * kWidth + x) * 4], 1, 3, f);
        return std::fclose(f) == 0;
    }
};
} // namespace

namespace boxshelf
{
[[noreturn]] void run_gl(GlDrawScene) noexcept
{
    std::abort(); // the preview draws frames itself
}
} // namespace boxshelf

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::fprintf(stderr, "usage: %s <out.ppm> <mode>\n", argv[0]);
        return 2;
    }
    const std::string mode = argv[2];
    boxshelf::app::SceneInput input;
    if (mode == "empty")
        input.time = 0.0;
    else
    {
        std::fprintf(stderr, "unknown preview mode: %s\n", mode.c_str());
        return 2;
    }
    static GlHost host;
    if (!host.init())
    {
        std::fprintf(stderr, "no GL context or renderer (is Mesa EGL installed?)\n");
        return 4;
    }
    host.render(input);
    return host.write_ppm(argv[1]) ? 0 : 3;
}
