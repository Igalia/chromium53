/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrYUVEffect.h"

#include "GrCoordTransform.h"
#include "GrFragmentProcessor.h"
#include "GrInvariantOutput.h"
#include "GrProcessor.h"
#include "glsl/GrGLSLFragmentProcessor.h"
#include "glsl/GrGLSLFragmentShaderBuilder.h"
#include "glsl/GrGLSLProgramDataManager.h"
#include "glsl/GrGLSLUniformHandler.h"

namespace {

static const float kJPEGConversionMatrix[16] = {
    1.0f,  0.0f,      1.402f,  -0.701f,
    1.0f, -0.34414f, -0.71414f, 0.529f,
    1.0f,  1.772f,    0.0f,    -0.886f,
    0.0f,  0.0f,      0.0f,     1.0
};

static const float kRec601ConversionMatrix[16] = {
    1.164f,  0.0f,    1.596f, -0.87075f,
    1.164f, -0.391f, -0.813f,  0.52925f,
    1.164f,  2.018f,  0.0f,   -1.08175f,
    0.0f,    0.0f,    0.0f,    1.0}
;

static const float kRec709ConversionMatrix[16] = {
    1.164f,  0.0f,    1.793f, -0.96925f,
    1.164f, -0.213f, -0.533f,  0.30025f,
    1.164f,  2.112f,  0.0f,   -1.12875f,
    0.0f,    0.0f,    0.0f,    1.0f}
;

static const float kJPEGInverseConversionMatrix[16] = {
     0.299001f,  0.586998f,  0.114001f,  0.0000821798f,
    -0.168736f, -0.331263f,  0.499999f,  0.499954f,
     0.499999f, -0.418686f, -0.0813131f, 0.499941f,
     0.f,        0.f,        0.f,        1.f
};

static const float kRec601InverseConversionMatrix[16] = {
     0.256951f,  0.504421f,  0.0977346f, 0.0625f,
    -0.148212f, -0.290954f,  0.439166f,  0.5f,
     0.439166f, -0.367886f, -0.0712802f, 0.5f,
     0.f,        0.f,        0.f,        1.f
};

static const float kRec709InverseConversionMatrix[16] = {
     0.182663f,  0.614473f, 0.061971f, 0.0625f,
    -0.100672f, -0.338658f, 0.43933f,  0.5f,
     0.439142f, -0.39891f, -0.040231f, 0.5f,
     0.f,        0.f,       0.f,       1.
};

class YUVtoRGBEffect : public GrFragmentProcessor {
public:
    static sk_sp<GrFragmentProcessor> Make(GrTexture* yTexture, GrTexture* uTexture,
                                           GrTexture* vTexture, const SkISize sizes[3],
                                           SkYUVColorSpace colorSpace, bool nv12) {
        SkScalar w[3], h[3];
        w[0] = SkIntToScalar(sizes[0].fWidth)  / SkIntToScalar(yTexture->width());
        h[0] = SkIntToScalar(sizes[0].fHeight) / SkIntToScalar(yTexture->height());
        w[1] = SkIntToScalar(sizes[1].fWidth)  / SkIntToScalar(uTexture->width());
        h[1] = SkIntToScalar(sizes[1].fHeight) / SkIntToScalar(uTexture->height());
        w[2] = SkIntToScalar(sizes[2].fWidth)  / SkIntToScalar(vTexture->width());
        h[2] = SkIntToScalar(sizes[2].fHeight) / SkIntToScalar(vTexture->height());
        SkMatrix yuvMatrix[3];
        yuvMatrix[0] = GrCoordTransform::MakeDivByTextureWHMatrix(yTexture);
        yuvMatrix[1] = yuvMatrix[0];
        yuvMatrix[1].preScale(w[1] / w[0], h[1] / h[0]);
        yuvMatrix[2] = yuvMatrix[0];
        yuvMatrix[2].preScale(w[2] / w[0], h[2] / h[0]);
        GrTextureParams::FilterMode uvFilterMode =
            ((sizes[1].fWidth  != sizes[0].fWidth) ||
             (sizes[1].fHeight != sizes[0].fHeight) ||
             (sizes[2].fWidth  != sizes[0].fWidth) ||
             (sizes[2].fHeight != sizes[0].fHeight)) ?
            GrTextureParams::kBilerp_FilterMode :
            GrTextureParams::kNone_FilterMode;
        return sk_sp<GrFragmentProcessor>(new YUVtoRGBEffect(
            yTexture, uTexture, vTexture, yuvMatrix, uvFilterMode, colorSpace, nv12));
    }

    const char* name() const override { return "YUV to RGB"; }

    SkYUVColorSpace getColorSpace() const { return fColorSpace; }

    bool isNV12() const {
        return fNV12;
    }

    class GLSLProcessor : public GrGLSLFragmentProcessor {
    public:
        void emitCode(EmitArgs& args) override {
            GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
            const YUVtoRGBEffect& effect = args.fFp.cast<YUVtoRGBEffect>();

            const char* colorSpaceMatrix = nullptr;
            fMatrixUni = args.fUniformHandler->addUniform(kFragment_GrShaderFlag,
                                                          kMat44f_GrSLType, kDefault_GrSLPrecision,
                                                          "ColorSpaceMatrix", &colorSpaceMatrix);
            fragBuilder->codeAppendf("%s = vec4(", args.fOutputColor);
            fragBuilder->appendTextureLookup(args.fTexSamplers[0], args.fCoords[0].c_str(),
                                             args.fCoords[0].getType());
            fragBuilder->codeAppend(".r,");
            fragBuilder->appendTextureLookup(args.fTexSamplers[1], args.fCoords[1].c_str(),
                                             args.fCoords[1].getType());
            if (effect.fNV12) {
                fragBuilder->codeAppendf(".rg,");
            } else {
                fragBuilder->codeAppend(".r,");
                fragBuilder->appendTextureLookup(args.fTexSamplers[2], args.fCoords[2].c_str(),
                                                 args.fCoords[2].getType());
                fragBuilder->codeAppendf(".g,");
            }
            fragBuilder->codeAppendf("1.0) * %s;", colorSpaceMatrix);
        }

    protected:
        void onSetData(const GrGLSLProgramDataManager& pdman,
                       const GrProcessor& processor) override {
            const YUVtoRGBEffect& yuvEffect = processor.cast<YUVtoRGBEffect>();
            switch (yuvEffect.getColorSpace()) {
                case kJPEG_SkYUVColorSpace:
                    pdman.setMatrix4f(fMatrixUni, kJPEGConversionMatrix);
                    break;
                case kRec601_SkYUVColorSpace:
                    pdman.setMatrix4f(fMatrixUni, kRec601ConversionMatrix);
                    break;
                case kRec709_SkYUVColorSpace:
                    pdman.setMatrix4f(fMatrixUni, kRec709ConversionMatrix);
                    break;
            }
        }

    private:
        GrGLSLProgramDataManager::UniformHandle fMatrixUni;

        typedef GrGLSLFragmentProcessor INHERITED;
    };

private:
    YUVtoRGBEffect(GrTexture* yTexture, GrTexture* uTexture, GrTexture* vTexture,
                   const SkMatrix yuvMatrix[3], GrTextureParams::FilterMode uvFilterMode,
                   SkYUVColorSpace colorSpace, bool nv12)
        : fYTransform(kLocal_GrCoordSet, yuvMatrix[0], yTexture)
        , fYAccess(yTexture)
        , fUTransform(kLocal_GrCoordSet, yuvMatrix[1], uTexture)
        , fUAccess(uTexture, uvFilterMode)
        , fVAccess(vTexture, uvFilterMode)
        , fColorSpace(colorSpace)
        , fNV12(nv12) {
        this->initClassID<YUVtoRGBEffect>();
        this->addCoordTransform(&fYTransform);
        this->addTextureAccess(&fYAccess);
        this->addCoordTransform(&fUTransform);
        this->addTextureAccess(&fUAccess);
        if (!fNV12) {
            fVTransform = GrCoordTransform(kLocal_GrCoordSet, yuvMatrix[2], vTexture);
            this->addCoordTransform(&fVTransform);
            this->addTextureAccess(&fVAccess);
        }
    }

    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override {
        return new GLSLProcessor;
    }

    void onGetGLSLProcessorKey(const GrGLSLCaps& caps, GrProcessorKeyBuilder* b) const override {
        b->add32(fNV12);
    }

    bool onIsEqual(const GrFragmentProcessor& sBase) const override {
        const YUVtoRGBEffect& s = sBase.cast<YUVtoRGBEffect>();
        return (fColorSpace == s.getColorSpace()) && (fNV12 == s.isNV12());
    }

    void onComputeInvariantOutput(GrInvariantOutput* inout) const override {
        // YUV is opaque
        inout->setToOther(kA_GrColorComponentFlag, 0xFF << GrColor_SHIFT_A,
                          GrInvariantOutput::kWillNot_ReadInput);
    }

    GrCoordTransform fYTransform;
    GrTextureAccess fYAccess;
    GrCoordTransform fUTransform;
    GrTextureAccess fUAccess;
    GrCoordTransform fVTransform;
    GrTextureAccess fVAccess;
    SkYUVColorSpace fColorSpace;
    bool fNV12;

    typedef GrFragmentProcessor INHERITED;
};


class RGBToYUVEffect : public GrFragmentProcessor {
public:
    enum OutputChannels {
        // output color r = y, g = u, b = v, a = a
        kYUV_OutputChannels,
        // output color rgba = y
        kY_OutputChannels,
        // output color r = u, g = v, b = 0, a = a
        kUV_OutputChannels,
        // output color rgba = u
        kU_OutputChannels,
        // output color rgba = v
        kV_OutputChannels
    };

    RGBToYUVEffect(sk_sp<GrFragmentProcessor> rgbFP, SkYUVColorSpace colorSpace,
                   OutputChannels output)
        : fColorSpace(colorSpace)
        , fOutputChannels(output) {
        this->initClassID<RGBToYUVEffect>();
        this->registerChildProcessor(std::move(rgbFP));
    }

    const char* name() const override { return "RGBToYUV"; }

    SkYUVColorSpace getColorSpace() const { return fColorSpace; }

    OutputChannels outputChannels() const { return fOutputChannels; }

    class GLSLProcessor : public GrGLSLFragmentProcessor {
    public:
        GLSLProcessor() : fLastColorSpace(-1), fLastOutputChannels(-1) {}

        void emitCode(EmitArgs& args) override {
            GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
            OutputChannels oc = args.fFp.cast<RGBToYUVEffect>().outputChannels();

            SkString outputColor("rgbColor");
            this->emitChild(0, args.fInputColor, &outputColor, args);

            const char* uniName;
            switch (oc) {
                case kYUV_OutputChannels:
                    fRGBToYUVUni = args.fUniformHandler->addUniformArray(
                        kFragment_GrShaderFlag,
                        kVec4f_GrSLType, kDefault_GrSLPrecision,
                        "RGBToYUV", 3, &uniName);
                    fragBuilder->codeAppendf("%s = vec4(dot(rgbColor.rgb, %s[0].rgb) + %s[0].a,"
                                                       "dot(rgbColor.rgb, %s[1].rgb) + %s[1].a,"
                                                       "dot(rgbColor.rgb, %s[2].rgb) + %s[2].a,"
                                                       "rgbColor.a);",
                                             args.fOutputColor, uniName, uniName, uniName, uniName,
                                             uniName, uniName);
                    break;
                case kUV_OutputChannels:
                    fRGBToYUVUni = args.fUniformHandler->addUniformArray(
                        kFragment_GrShaderFlag,
                        kVec4f_GrSLType, kDefault_GrSLPrecision,
                        "RGBToUV", 2, &uniName);
                    fragBuilder->codeAppendf("%s = vec4(dot(rgbColor.rgb, %s[0].rgb) + %s[0].a,"
                                                       "dot(rgbColor.rgb, %s[1].rgb) + %s[1].a,"
                                                       "0.0,"
                                                       "rgbColor.a);",
                                             args.fOutputColor, uniName, uniName, uniName, uniName);
                    break;
                case kY_OutputChannels:
                case kU_OutputChannels:
                case kV_OutputChannels:
                    fRGBToYUVUni = args.fUniformHandler->addUniform(
                        kFragment_GrShaderFlag,
                        kVec4f_GrSLType, kDefault_GrSLPrecision,
                        "RGBToYUorV", &uniName);
                    fragBuilder->codeAppendf("%s = vec4(dot(rgbColor.rgb, %s.rgb) + %s.a);\n",
                                             args.fOutputColor, uniName, uniName);
                    break;
            }
        }

    private:
        void onSetData(const GrGLSLProgramDataManager& pdman,
                       const GrProcessor& processor) override {
            const RGBToYUVEffect& effect = processor.cast<RGBToYUVEffect>();
            OutputChannels oc = effect.outputChannels();
            if (effect.getColorSpace() != fLastColorSpace || oc != fLastOutputChannels) {

                const float* matrix = nullptr;
                switch (effect.getColorSpace()) {
                    case kJPEG_SkYUVColorSpace:
                        matrix = kJPEGInverseConversionMatrix;
                        break;
                    case kRec601_SkYUVColorSpace:
                        matrix = kRec601InverseConversionMatrix;
                        break;
                    case kRec709_SkYUVColorSpace:
                        matrix = kRec709InverseConversionMatrix;
                        break;
                }
                switch (oc) {
                    case kYUV_OutputChannels:
                        pdman.set4fv(fRGBToYUVUni, 3, matrix);
                        break;
                    case kUV_OutputChannels:
                        pdman.set4fv(fRGBToYUVUni, 2, matrix + 4);
                        break;
                    case kY_OutputChannels:
                        pdman.set4fv(fRGBToYUVUni, 1, matrix);
                        break;
                    case kU_OutputChannels:
                        pdman.set4fv(fRGBToYUVUni, 1, matrix + 4);
                        break;
                    case kV_OutputChannels:
                        pdman.set4fv(fRGBToYUVUni, 1, matrix + 8);
                        break;
                }
                fLastColorSpace = effect.getColorSpace();
            }
        }
        GrGLSLProgramDataManager::UniformHandle fRGBToYUVUni;
        int                                     fLastColorSpace;
        int                                     fLastOutputChannels;

        typedef GrGLSLFragmentProcessor INHERITED;
    };

private:
    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override {
        return new GLSLProcessor;
    }

    void onGetGLSLProcessorKey(const GrGLSLCaps& caps, GrProcessorKeyBuilder* b) const override {
        // kY, kU, and kV all generate the same code, just upload different coefficients.
        if (kU_OutputChannels == fOutputChannels || kV_OutputChannels == fOutputChannels) {
            b->add32(kY_OutputChannels);
        } else {
            b->add32(fOutputChannels);
        }
    }

    bool onIsEqual(const GrFragmentProcessor& sBase) const override {
        const RGBToYUVEffect& s = sBase.cast<RGBToYUVEffect>();
        return fColorSpace == s.getColorSpace() && fOutputChannels == s.outputChannels();
    }

    void onComputeInvariantOutput(GrInvariantOutput* inout) const override {
        inout->setToUnknown(GrInvariantOutput::kWillNot_ReadInput);
    }

    GrCoordTransform    fTransform;
    GrTextureAccess     fAccess;
    SkYUVColorSpace     fColorSpace;
    OutputChannels      fOutputChannels;

    typedef GrFragmentProcessor INHERITED;
};

}

//////////////////////////////////////////////////////////////////////////////

sk_sp<GrFragmentProcessor> GrYUVEffect::MakeYUVToRGB(GrTexture* yTexture, GrTexture* uTexture,
                                                     GrTexture* vTexture, const SkISize sizes[3],
                                                     SkYUVColorSpace colorSpace, bool nv12) {
    SkASSERT(yTexture && uTexture && vTexture && sizes);
    return YUVtoRGBEffect::Make(yTexture, uTexture, vTexture, sizes, colorSpace, nv12);
}

sk_sp<GrFragmentProcessor>
GrYUVEffect::MakeRGBToYUV(sk_sp<GrFragmentProcessor> rgbFP, SkYUVColorSpace colorSpace) {
    SkASSERT(rgbFP);
    return sk_sp<GrFragmentProcessor>(
        new RGBToYUVEffect(std::move(rgbFP), colorSpace, RGBToYUVEffect::kYUV_OutputChannels));
}

sk_sp<GrFragmentProcessor>
GrYUVEffect::MakeRGBToY(sk_sp<GrFragmentProcessor> rgbFP, SkYUVColorSpace colorSpace) {
    SkASSERT(rgbFP);
    return sk_sp<GrFragmentProcessor>(
        new RGBToYUVEffect(std::move(rgbFP), colorSpace, RGBToYUVEffect::kY_OutputChannels));
}

sk_sp<GrFragmentProcessor>
GrYUVEffect::MakeRGBToUV(sk_sp<GrFragmentProcessor> rgbFP, SkYUVColorSpace colorSpace) {
    SkASSERT(rgbFP);
    return sk_sp<GrFragmentProcessor>(
        new RGBToYUVEffect(std::move(rgbFP), colorSpace, RGBToYUVEffect::kUV_OutputChannels));
}

sk_sp<GrFragmentProcessor>
GrYUVEffect::MakeRGBToU(sk_sp<GrFragmentProcessor> rgbFP, SkYUVColorSpace colorSpace) {
    SkASSERT(rgbFP);
    return sk_sp<GrFragmentProcessor>(
        new RGBToYUVEffect(std::move(rgbFP), colorSpace, RGBToYUVEffect::kU_OutputChannels));
}

sk_sp<GrFragmentProcessor>
GrYUVEffect::MakeRGBToV(sk_sp<GrFragmentProcessor> rgbFP, SkYUVColorSpace colorSpace) {
    SkASSERT(rgbFP);
    return sk_sp<GrFragmentProcessor>(
        new RGBToYUVEffect(std::move(rgbFP), colorSpace, RGBToYUVEffect::kV_OutputChannels));
}
