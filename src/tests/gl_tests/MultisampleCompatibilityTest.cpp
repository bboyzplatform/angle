//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// StateChangeTest:
//   Specifically designed for an ANGLE implementation of GL, these tests validate that
//   ANGLE's dirty bits systems don't get confused by certain sequences of state changes.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "shader_utils.h"

using namespace angle;

namespace
{

const GLint kWidth = 64;
const GLint kHeight = 64;

// test drawing with GL_MULTISAMPLE_EXT enabled/disabled.
class EXTMultisampleCompatibilityTest : public ANGLETest
{

protected:
    EXTMultisampleCompatibilityTest()
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        static const char* v_shader_str =
            "attribute vec4 a_Position;\n"
            "void main()\n"
            "{ gl_Position = a_Position; }";

        static const char* f_shader_str =
            "precision mediump float;\n"
            "uniform vec4 color;"
            "void main() { gl_FragColor = color; }";

        mProgram = CompileProgram(v_shader_str, f_shader_str);

        GLuint position_loc = glGetAttribLocation(mProgram, "a_Position");
        mColorLoc = glGetUniformLocation(mProgram, "color");

        glGenBuffers(1, &mVBO);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        static float vertices[] = {
            1.0f,  1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
            -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f, 1.0f,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(position_loc);
        glVertexAttribPointer(position_loc, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }

    void TearDown() override
    {
        glDeleteBuffers(1, &mVBO);
        glDeleteProgram(mProgram);

        ANGLETest::TearDown();
    }

    void prepareForDraw()
    {
        // Create a sample buffer.
        GLsizei num_samples = 4, max_samples = 0;
        glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
        num_samples = std::min(num_samples, max_samples);

        glGenRenderbuffers(1, &mSampleRB);
        glBindRenderbuffer(GL_RENDERBUFFER, mSampleRB);
        glRenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, num_samples,
                                             GL_RGBA8_OES, kWidth, kHeight);
        GLint param = 0;
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES,
                                 &param);
        EXPECT_GE(param, num_samples);

        glGenFramebuffers(1, &mSampleFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mSampleFBO);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, mSampleRB);
        EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
          glCheckFramebufferStatus(GL_FRAMEBUFFER));
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Create another FBO to resolve the multisample buffer into.
        glGenTextures(1, &mResolveTex);
        glBindTexture(GL_TEXTURE_2D, mResolveTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
           GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &mResolveFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mResolveFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           mResolveTex, 0);
        EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
          glCheckFramebufferStatus(GL_FRAMEBUFFER));

        glUseProgram(mProgram);
        glViewport(0, 0, kWidth, kHeight);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, mSampleFBO);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void prepareForVerify()
    {
        // Resolve.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, mSampleFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mResolveFBO);
        glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBlitFramebufferANGLE(0, 0, kWidth, kHeight, 0, 0, kWidth, kHeight,
          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, mResolveFBO);

        ASSERT_GL_NO_ERROR();
    }

    void cleanup()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &mResolveFBO);
        glDeleteFramebuffers(1, &mSampleFBO);
        glDeleteTextures(1, &mResolveTex);
        glDeleteRenderbuffers(1, &mSampleRB);

        ASSERT_GL_NO_ERROR();

    }

    bool isApplicable() const
    {
        return extensionEnabled("GL_EXT_multisample_compatibility") &&
             extensionEnabled("GL_ANGLE_framebuffer_multisample") &&
             extensionEnabled("GL_OES_rgb8_rgba8") &&
             !IsAMD();
    }
    GLuint mSampleFBO;
    GLuint mResolveFBO;
    GLuint mSampleRB;
    GLuint mResolveTex;

    GLuint mColorLoc;
    GLuint mProgram;
    GLuint mVBO;
};

} //

// Test simple state tracking
TEST_P(EXTMultisampleCompatibilityTest, TestStateTracking)
{
    if (!isApplicable())
        return;

    EXPECT_TRUE(glIsEnabled(GL_MULTISAMPLE_EXT));
    glDisable(GL_MULTISAMPLE_EXT);
    EXPECT_FALSE(glIsEnabled(GL_MULTISAMPLE_EXT));
    glEnable(GL_MULTISAMPLE_EXT);
    EXPECT_TRUE(glIsEnabled(GL_MULTISAMPLE_EXT));

    EXPECT_FALSE(glIsEnabled(GL_SAMPLE_ALPHA_TO_ONE_EXT));
    glEnable(GL_SAMPLE_ALPHA_TO_ONE_EXT);
    EXPECT_TRUE(glIsEnabled(GL_SAMPLE_ALPHA_TO_ONE_EXT));
    glDisable(GL_SAMPLE_ALPHA_TO_ONE_EXT);
    EXPECT_FALSE(glIsEnabled(GL_SAMPLE_ALPHA_TO_ONE_EXT));

    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

// Test that disabling GL_MULTISAMPLE_EXT is handled correctly.
TEST_P(EXTMultisampleCompatibilityTest, DrawAndResolve)
{
    if (!isApplicable())
        return;

    static const float kBlue[] = {0.0f, 0.0f, 1.0f, 1.0f};
    static const float kGreen[] = {0.0f, 1.0f, 0.0f, 1.0f};
    static const float kRed[] = {1.0f, 0.0f, 0.0f, 1.0f};

    // Different drivers seem to behave differently with respect to resulting
    // values. These might be due to different MSAA sample counts causing
    // different samples to hit.  Other option is driver bugs. Just test that
    // disabling multisample causes a difference.
    std::unique_ptr<uint8_t[]> results[3];
    const GLint kResultSize = kWidth * kHeight * 4;
    for (int pass = 0; pass < 3; pass++)
    {
        prepareForDraw();
        // Green: from top right to bottom left.
        glUniform4fv(mColorLoc, 1, kGreen);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Blue: from top left to bottom right.
        glUniform4fv(mColorLoc, 1, kBlue);
        glDrawArrays(GL_TRIANGLES, 3, 3);

        // Red, with and without MSAA: from bottom left to top right.
        if (pass == 1)
        {
            glDisable(GL_MULTISAMPLE_EXT);
        }
        glUniform4fv(mColorLoc, 1, kRed);
        glDrawArrays(GL_TRIANGLES, 6, 3);
        if (pass == 1)
        {
            glEnable(GL_MULTISAMPLE_EXT);
        }
        prepareForVerify();
        results[pass].reset(new uint8_t[kResultSize]);
        memset(results[pass].get(), 123u, kResultSize);
        glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                   results[pass].get());

        cleanup();
    }
    EXPECT_NE(0, memcmp(results[0].get(), results[1].get(), kResultSize));
    // Verify that rendering is deterministic, so that the pass above does not
    // come from non-deterministic rendering.
    EXPECT_EQ(0, memcmp(results[0].get(), results[2].get(), kResultSize));
}

// Test that enabling GL_SAMPLE_ALPHA_TO_ONE_EXT affects rendering.
TEST_P(EXTMultisampleCompatibilityTest, DrawAlphaOneAndResolve)
{
    if (!isApplicable())
        return;

    // SAMPLE_ALPHA_TO_ONE is specified to transform alpha values of
    // covered samples to 1.0. In order to detect it, we use non-1.0
    // alpha.
    static const float kBlue[] = {0.0f, 0.0f, 1.0f, 0.5f};
    static const float kGreen[] = {0.0f, 1.0f, 0.0f, 0.5f};
    static const float kRed[] = {1.0f, 0.0f, 0.0f, 0.5f};

    // Different drivers seem to behave differently with respect to resulting
    // alpha value. These might be due to different MSAA sample counts causing
    // different samples to hit.  Other option is driver bugs. Testing exact or
    // even approximate sample values is not that easy.  Thus, just test
    // representative positions which have fractional pixels, inspecting that
    // normal rendering is different to SAMPLE_ALPHA_TO_ONE rendering.
    std::unique_ptr<uint8_t[]> results[3];
    const GLint kResultSize = kWidth * kHeight * 4;

    for (int pass = 0; pass < 3; ++pass)
    {
        prepareForDraw();
        if (pass == 1)
        {
            glEnable(GL_SAMPLE_ALPHA_TO_ONE_EXT);
        }
        glEnable(GL_MULTISAMPLE_EXT);
        glUniform4fv(mColorLoc, 1, kGreen);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glUniform4fv(mColorLoc, 1, kBlue);
        glDrawArrays(GL_TRIANGLES, 3, 3);

        glDisable(GL_MULTISAMPLE_EXT);
        glUniform4fv(mColorLoc, 1, kRed);
        glDrawArrays(GL_TRIANGLES, 6, 3);

        prepareForVerify();
        results[pass].reset(new uint8_t[kResultSize]);
        memset(results[pass].get(), 123u, kResultSize);
        glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
            results[pass].get());
        if (pass == 1)
        {
            glDisable(GL_SAMPLE_ALPHA_TO_ONE_EXT);
        }

        cleanup();
    }
    EXPECT_NE(0, memcmp(results[0].get(), results[1].get(), kResultSize));
    // Verify that rendering is deterministic, so that the pass above does not
    // come from non-deterministic rendering.
    EXPECT_EQ(0, memcmp(results[0].get(), results[2].get(), kResultSize));
}

ANGLE_INSTANTIATE_TEST(EXTMultisampleCompatibilityTest, ES2_OPENGL(), ES2_OPENGLES(), ES3_OPENGL());

class MultisampleCompatibilityTest : public ANGLETest
{

  protected:
    MultisampleCompatibilityTest()
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void prepareForDraw(GLsizei numSamples)
    {
        // Create a sample buffer.
        glGenRenderbuffers(1, &mSampleRB);
        glBindRenderbuffer(GL_RENDERBUFFER, mSampleRB);
        glRenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, numSamples, GL_RGBA8, kWidth,
                                              kHeight);
        GLint param = 0;
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &param);
        EXPECT_GE(param, numSamples);
        glGenFramebuffers(1, &mSampleFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mSampleFBO);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mSampleRB);
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // Create another FBO to resolve the multisample buffer into.
        glGenTextures(1, &mResolveTex);
        glBindTexture(GL_TEXTURE_2D, mResolveTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &mResolveFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mResolveFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mResolveTex, 0);
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        glViewport(0, 0, kWidth, kHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, mSampleFBO);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ASSERT_GL_NO_ERROR();
    }

    void prepareForVerify()
    {
        // Resolve.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, mSampleFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mResolveFBO);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBlitFramebufferANGLE(0, 0, kWidth, kHeight, 0, 0, kWidth, kHeight, GL_COLOR_BUFFER_BIT,
                               GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, mResolveFBO);

        ASSERT_GL_NO_ERROR();
    }

    void cleanup()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &mResolveFBO);
        glDeleteFramebuffers(1, &mSampleFBO);
        glDeleteTextures(1, &mResolveTex);
        glDeleteRenderbuffers(1, &mSampleRB);

        ASSERT_GL_NO_ERROR();
    }

    bool isApplicable() const
    {
        return extensionEnabled("GL_ANGLE_framebuffer_multisample") &&
               extensionEnabled("GL_OES_rgb8_rgba8");
    }

    GLuint mSampleFBO;
    GLuint mResolveFBO;
    GLuint mSampleRB;
    GLuint mResolveTex;
};

// Test that enabling GL_SAMPLE_COVERAGE affects rendering.
TEST_P(MultisampleCompatibilityTest, DrawCoverageAndResolve)
{
    if (!isApplicable())
        return;

    // TODO: Figure out why this fails on Android.
    if (IsAndroid())
    {
        std::cout << "Test skipped on Android." << std::endl;
        return;
    }

    const std::string &vertex =
        "attribute vec4 position;\n"
        "void main()\n"
        "{ gl_Position = position; }";
    const std::string &fragment =
        "void main()\n"
        "{ gl_FragColor =  vec4(1.0, 0.0, 0.0, 1.0); }";

    ANGLE_GL_PROGRAM(drawRed, vertex, fragment);

    GLsizei maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    int iterationCount = maxSamples + 1;
    for (int samples = 1; samples < iterationCount; samples++)
    {
        prepareForDraw(samples);
        glEnable(GL_SAMPLE_COVERAGE);
        glSampleCoverage(1.0, false);
        drawQuad(drawRed.get(), "position", 0.5f);

        prepareForVerify();
        GLsizei pixelCount = kWidth * kHeight;
        std::vector<GLColor> actual(pixelCount, GLColor::black);
        glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
        glDisable(GL_SAMPLE_COVERAGE);
        cleanup();

        std::vector<GLColor> expected(pixelCount, GLColor::red);
        EXPECT_EQ(expected, actual);
    }
}

ANGLE_INSTANTIATE_TEST(MultisampleCompatibilityTest,
                       ES2_D3D9(),
                       ES2_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_D3D11(),
                       ES3_OPENGL(),
                       ES3_OPENGLES());