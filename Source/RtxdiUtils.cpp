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

#include <Rtxdi/RtxdiUtils.h>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace rtxdi
{

namespace
{

constexpr uint32_t c_SpatialStratumCount = 4;
constexpr float c_R2Phi = 1.0f / 1.3247179572447f;
constexpr float c_HalfPi = 1.57079632679489661923f;
constexpr float c_Snorm8Scale = 127.0f;

bool IsNonzeroPowerOfTwo(uint32_t value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

void FillLowDiscrepancyNeighborOffsetBuffer(uint8_t* buffer, uint32_t neighborOffsetCount)
{
    // Create a sequence of low-discrepancy samples within a unit radius around the origin
    // for "randomly" sampling neighbors during spatial resampling.
    const int radiusScale = 250;
    uint32_t num = 0;
    float u = 0.5f;
    float v = 0.5f;
    while (num < neighborOffsetCount * 2)
    {
        u += c_R2Phi;
        v += c_R2Phi * c_R2Phi;
        if (u >= 1.0f)
        {
            u -= 1.0f;
        }
        if (v >= 1.0f)
        {
            v -= 1.0f;
        }

        const float radiusSquared = (u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f);
        if (radiusSquared > 0.25f)
        {
            continue;
        }

        const int quantizedU = int((u - 0.5f) * float(radiusScale));
        const int quantizedV = int((v - 0.5f) * float(radiusScale));
        buffer[num++] = static_cast<uint8_t>(quantizedU);
        buffer[num++] = static_cast<uint8_t>(quantizedV);
    }
}

float HashToUnitFloat(uint32_t value)
{
    constexpr uint32_t mantissaMask = (1u << 24) - 1u;
    constexpr float mantissaScale = 1.0f / float(1u << 24);
    return float(JenkinsHash(value) & mantissaMask) * mantissaScale;
}

void StoreSnorm8Offset(uint8_t* buffer, uint32_t offsetIndex, float x, float y)
{
    int quantizedX = std::max(-127, std::min(127, int(std::lround(x * c_Snorm8Scale))));
    int quantizedY = std::max(-127, std::min(127, int(std::lround(y * c_Snorm8Scale))));

    const int radiusSquared = quantizedX * quantizedX + quantizedY * quantizedY;
    if (radiusSquared > int(c_Snorm8Scale * c_Snorm8Scale))
    {
        const float inwardScale = c_Snorm8Scale / std::sqrt(float(radiusSquared));
        quantizedX = int(float(quantizedX) * inwardScale);
        quantizedY = int(float(quantizedY) * inwardScale);
    }

    buffer[offsetIndex * 2] = static_cast<uint8_t>(static_cast<int8_t>(quantizedX));
    buffer[offsetIndex * 2 + 1] = static_cast<uint8_t>(static_cast<int8_t>(quantizedY));
}

void FillStratifiedLowDiscrepancyNeighborOffsetBuffer(uint8_t* buffer, uint32_t neighborOffsetCount)
{
    const uint32_t groupCount = neighborOffsetCount / c_SpatialStratumCount;

    // Each quadrant receives a distinct fixed phase-shifted R2 sequence. The square-to-quarter-disk
    // transform is area preserving because the radius is the square root of a uniform value.
    for (uint32_t quadrant = 0; quadrant < c_SpatialStratumCount; ++quadrant)
    {
        const uint32_t shiftSeed = 0x243f6a88u + quadrant * 2u;
        float radiusSample = HashToUnitFloat(shiftSeed);
        float angleSample = HashToUnitFloat(shiftSeed + 1u);

        for (uint32_t group = 0; group < groupCount; ++group)
        {
            radiusSample += c_R2Phi;
            angleSample += c_R2Phi * c_R2Phi;
            if (radiusSample >= 1.0f)
            {
                radiusSample -= 1.0f;
            }
            if (angleSample >= 1.0f)
            {
                angleSample -= 1.0f;
            }

            const float radius = std::sqrt(radiusSample);
            const float angle = (float(quadrant) + angleSample) * c_HalfPi;
            const uint32_t offsetIndex = group * c_SpatialStratumCount + quadrant;
            StoreSnorm8Offset(buffer, offsetIndex, radius * std::cos(angle), radius * std::sin(angle));
        }
    }
}

}

RTXDI_ReservoirBufferParameters CalculateReservoirBufferParameters(uint32_t renderWidth, uint32_t renderHeight, CheckerboardMode checkerboardMode)
{
    renderWidth = (checkerboardMode == CheckerboardMode::Off)
        ? renderWidth
        : (renderWidth + 1) / 2;
    uint32_t renderWidthBlocks = (renderWidth + RTXDI_RESERVOIR_BLOCK_SIZE - 1) / RTXDI_RESERVOIR_BLOCK_SIZE;
    uint32_t renderHeightBlocks = (renderHeight + RTXDI_RESERVOIR_BLOCK_SIZE - 1) / RTXDI_RESERVOIR_BLOCK_SIZE;
    RTXDI_ReservoirBufferParameters params;
    params.reservoirBlockRowPitch = renderWidthBlocks * (RTXDI_RESERVOIR_BLOCK_SIZE * RTXDI_RESERVOIR_BLOCK_SIZE);
    params.reservoirArrayPitch = params.reservoirBlockRowPitch * renderHeightBlocks;
    return params;
}

void ComputePdfTextureSize(uint32_t maxItems, uint32_t& outWidth, uint32_t& outHeight, uint32_t& outMipLevels)
{
    // Compute the size of a power-of-2 rectangle that fits all items, 1 item per pixel
    double textureWidth = std::max(1.0, ceil(sqrt(double(maxItems))));
    textureWidth = exp2(ceil(log2(textureWidth)));
    double textureHeight = std::max(1.0, ceil(maxItems / textureWidth));
    textureHeight = exp2(ceil(log2(textureHeight)));
    double textureMips = std::max(1.0, log2(std::max(textureWidth, textureHeight)) + 1.0);

    outWidth = uint32_t(textureWidth);
    outHeight = uint32_t(textureHeight);
    outMipLevels = uint32_t(textureMips);
}

void FillNeighborOffsetBuffer(
    uint8_t* buffer,
    uint32_t neighborOffsetCount)
{
    FillNeighborOffsetBuffer(buffer, neighborOffsetCount, ReSTIRDI_SpatialSamplingMode::LowDiscrepancy);
}

void FillNeighborOffsetBuffer(
    uint8_t* buffer,
    uint32_t neighborOffsetCount,
    ReSTIRDI_SpatialSamplingMode samplingMode)
{
    assert(buffer != nullptr);
    assert(IsNonzeroPowerOfTwo(neighborOffsetCount));

    if (samplingMode == ReSTIRDI_SpatialSamplingMode::StratifiedLowDiscrepancy)
    {
        assert(neighborOffsetCount >= c_SpatialStratumCount);
        FillStratifiedLowDiscrepancyNeighborOffsetBuffer(buffer, neighborOffsetCount);
    }
    else
    {
        assert(samplingMode == ReSTIRDI_SpatialSamplingMode::LowDiscrepancy);
        FillLowDiscrepancyNeighborOffsetBuffer(buffer, neighborOffsetCount);
    }
}

uint32_t JenkinsHash(uint32_t a)
{
    // http://burtleburtle.net/bob/hash/integer.html
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

}
