/*
 * SPDX-FileCopyrightText: Copyright (c) 2020-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include "Rtxdi/GI/ReSTIRGI.h"

namespace rtxdi
{

RTXDI_GIBufferIndices GetDefaultReSTIRGIBufferIndices()
{
    RTXDI_GIBufferIndices bufferIndices = {};
    bufferIndices.secondarySurfaceReSTIRDIOutputBufferIndex = 0;
    bufferIndices.temporalResamplingInputBufferIndex = 0;
    bufferIndices.temporalResamplingOutputBufferIndex = 0;
    bufferIndices.spatialResamplingInputBufferIndex = 0;
    bufferIndices.spatialResamplingOutputBufferIndex = 0;
    return bufferIndices;
}

RTXDI_GITemporalResamplingParameters GetDefaultReSTIRGITemporalResamplingParams()
{
    RTXDI_GITemporalResamplingParameters params = {};
    params.depthThreshold = 0.1f;
    params.enableFallbackSampling = true;
    params.enablePermutationSampling = false;
    params.maxHistoryLength = 8;
    params.maxReservoirAge = 30;
    params.normalThreshold = 0.6f;
    params.biasCorrectionMode = RTXDI_GIBiasCorrectionMode::Basic;
    return params;
}

RTXDI_BoilingFilterParameters GetDefaultReSTIRGIBoilingFilterParams()
{
    RTXDI_BoilingFilterParameters params = {};
    params.enableBoilingFilter = static_cast<uint32_t>(true);
    params.boilingFilterStrength = 0.2f;
    return params;
}

RTXDI_GISpatialResamplingParameters GetDefaultReSTIRGISpatialResamplingParams()
{
    RTXDI_GISpatialResamplingParameters params = {};
    params.numSamples = 2;
    params.biasCorrectionMode = RTXDI_GIBiasCorrectionMode::Basic;
    params.depthThreshold = 0.1f;
    params.normalThreshold = 0.6f;
    params.samplingRadius = 32.0f;
    return params;
}

RTXDI_GISpatioTemporalResamplingParameters GetDefaultReSTIRGISpatioTemporalResamplingParams()
{
    RTXDI_GISpatioTemporalResamplingParameters params = {};
    params.depthThreshold = 0.1f;
    params.normalThreshold = 0.6f;
    params.enableFallbackSampling = true;
    params.enablePermutationSampling = false;
    params.maxHistoryLength = 8;
    params.maxReservoirAge = 30;
    params.numSamples = 2;
    params.biasCorrectionMode = RTXDI_GIBiasCorrectionMode::Basic;
    params.samplingRadius = 32.0f;
    return params;
}

RTXDI_GIFinalShadingParameters GetDefaultReSTIRGIFinalShadingParams()
{
    RTXDI_GIFinalShadingParameters params = {};
    params.enableFinalMIS = true;
    params.enableFinalVisibility = true;
    return params;
}

ReSTIRGIContext::ReSTIRGIContext(const ReSTIRGIStaticParameters& staticParams) :
    m_staticParams(staticParams),
    m_frameIndex(0),
    m_reservoirBufferParams(CalculateReservoirBufferParameters(staticParams.RenderWidth, staticParams.RenderHeight, staticParams.CheckerboardSamplingMode)),
    m_resamplingMode(rtxdi::ReSTIRGI_ResamplingMode::None),
    m_bufferIndices(GetDefaultReSTIRGIBufferIndices()),
    m_temporalResamplingParams(GetDefaultReSTIRGITemporalResamplingParams()),
    m_boilingFilterParams(GetDefaultReSTIRGIBoilingFilterParams()),
    m_spatialResamplingParams(GetDefaultReSTIRGISpatialResamplingParams()),
    m_spatioTemporalResamplingParams(GetDefaultReSTIRGISpatioTemporalResamplingParams()),
    m_finalShadingParams(GetDefaultReSTIRGIFinalShadingParams())
{
}

ReSTIRGIStaticParameters ReSTIRGIContext::GetStaticParams() const
{
    return m_staticParams;
}

uint32_t ReSTIRGIContext::GetFrameIndex() const
{
    return m_frameIndex;
}

RTXDI_ReservoirBufferParameters ReSTIRGIContext::GetReservoirBufferParameters() const
{
    return m_reservoirBufferParams;
}

ReSTIRGI_ResamplingMode ReSTIRGIContext::GetResamplingMode() const
{
    return m_resamplingMode;
}

RTXDI_GIBufferIndices ReSTIRGIContext::GetBufferIndices() const
{
    return m_bufferIndices;
}

RTXDI_GITemporalResamplingParameters ReSTIRGIContext::GetTemporalResamplingParameters() const
{
    return m_temporalResamplingParams;
}

RTXDI_BoilingFilterParameters ReSTIRGIContext::GetBoilingFilterParameters() const
{
    return m_boilingFilterParams;
}

RTXDI_GISpatialResamplingParameters ReSTIRGIContext::GetSpatialResamplingParameters() const
{
    return m_spatialResamplingParams;
}

RTXDI_GISpatioTemporalResamplingParameters ReSTIRGIContext::GetSpatioTemporalResamplingParameters() const
{
    return m_spatioTemporalResamplingParams;
}

RTXDI_GIFinalShadingParameters ReSTIRGIContext::GetFinalShadingParameters() const
{
    return m_finalShadingParams;
}

void ReSTIRGIContext::SetFrameIndex(uint32_t frameIndex)
{
    m_frameIndex = frameIndex;
    m_temporalResamplingParams.uniformRandomNumber = JenkinsHash(m_frameIndex);
    UpdateBufferIndices();
}

void ReSTIRGIContext::SetResamplingMode(ReSTIRGI_ResamplingMode resamplingMode)
{
    m_resamplingMode = resamplingMode;
    UpdateBufferIndices();
}

void ReSTIRGIContext::SetTemporalResamplingParameters(const RTXDI_GITemporalResamplingParameters& temporalResamplingParams)
{
    m_temporalResamplingParams = temporalResamplingParams;
    m_temporalResamplingParams.uniformRandomNumber = JenkinsHash(m_frameIndex);
}

void ReSTIRGIContext::SetBoilingFilterParameters(const RTXDI_BoilingFilterParameters& boilingFilterParams)
{
    m_boilingFilterParams = boilingFilterParams;
}

void ReSTIRGIContext::SetSpatialResamplingParameters(const RTXDI_GISpatialResamplingParameters& spatialResamplingParams)
{
    m_spatialResamplingParams = spatialResamplingParams;
}

void ReSTIRGIContext::SetSpatioTemporalResamplingParameters(const RTXDI_GISpatioTemporalResamplingParameters& spatioTemporalParams)
{
    m_spatioTemporalResamplingParams = spatioTemporalParams;
}

void ReSTIRGIContext::SetFinalShadingParameters(const RTXDI_GIFinalShadingParameters& finalShadingParams)
{
    m_finalShadingParams = finalShadingParams;
}

void ReSTIRGIContext::UpdateBufferIndices()
{
    switch (m_resamplingMode)
    {
    case rtxdi::ReSTIRGI_ResamplingMode::None:
        m_bufferIndices.secondarySurfaceReSTIRDIOutputBufferIndex = 0;
        m_bufferIndices.finalShadingInputBufferIndex = 0;
        break;
    case rtxdi::ReSTIRGI_ResamplingMode::Temporal:
        m_bufferIndices.secondarySurfaceReSTIRDIOutputBufferIndex = m_frameIndex & 1;
        m_bufferIndices.temporalResamplingInputBufferIndex = !m_bufferIndices.secondarySurfaceReSTIRDIOutputBufferIndex;
        m_bufferIndices.temporalResamplingOutputBufferIndex = m_bufferIndices.secondarySurfaceReSTIRDIOutputBufferIndex;
        m_bufferIndices.finalShadingInputBufferIndex = m_bufferIndices.temporalResamplingOutputBufferIndex;
        break;
    case rtxdi::ReSTIRGI_ResamplingMode::Spatial:
        m_bufferIndices.secondarySurfaceReSTIRDIOutputBufferIndex = 0;
        m_bufferIndices.spatialResamplingInputBufferIndex = 0;
        m_bufferIndices.spatialResamplingOutputBufferIndex = 1;
        m_bufferIndices.finalShadingInputBufferIndex = 1;
        break;
    case rtxdi::ReSTIRGI_ResamplingMode::TemporalAndSpatial:
        m_bufferIndices.secondarySurfaceReSTIRDIOutputBufferIndex = 0;
        m_bufferIndices.temporalResamplingInputBufferIndex = 1;
        m_bufferIndices.temporalResamplingOutputBufferIndex = 0;
        m_bufferIndices.spatialResamplingInputBufferIndex = 0;
        m_bufferIndices.spatialResamplingOutputBufferIndex = 1;
        m_bufferIndices.finalShadingInputBufferIndex = 1;
        break;
    case rtxdi::ReSTIRGI_ResamplingMode::FusedSpatiotemporal:
        m_bufferIndices.secondarySurfaceReSTIRDIOutputBufferIndex = m_frameIndex & 1;
        m_bufferIndices.temporalResamplingInputBufferIndex = !m_bufferIndices.secondarySurfaceReSTIRDIOutputBufferIndex;
        m_bufferIndices.spatialResamplingOutputBufferIndex = m_bufferIndices.secondarySurfaceReSTIRDIOutputBufferIndex;
        m_bufferIndices.finalShadingInputBufferIndex = m_bufferIndices.spatialResamplingOutputBufferIndex;
        break;
    }
}

}
