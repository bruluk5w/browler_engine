#include "PropagationShader.h"

#include "Propagation_cs_cs.h"

#include "ComputeBuffers.h"
#include "DxHelpers.h"
#include "Renderer/PAL/TextureResource.h"
#include "PitCollection.h"
#include "Renderer/TextureHandle.h"

#include "Core/BrowlerEngine.h"
#include "Core/Timer.h"


BRWL_RENDERER_NS


PropagationShader::PropagationShader(ID3D12Device* device) :
    rootSignature(nullptr),
    pipelineState(nullptr),
    slicesPerInvocation(10)
{
    destroy();
    // building pipeline state object and rootsignature

    {
        unsigned int uavRegisterIdx = 0;
        unsigned int srvRegisterIdx = 0;

        // pong buffers - write
        D3D12_DESCRIPTOR_RANGE pingPongRanges[2];
        D3D12_DESCRIPTOR_RANGE& uavRangePong = pingPongRanges[0];
        memset(&uavRangePong, 0, sizeof(uavRangePong));
        uavRangePong.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRangePong.NumDescriptors = ComputeBuffers::numBuffers;
        uavRangePong.BaseShaderRegister = uavRegisterIdx;
        uavRangePong.RegisterSpace = 0;
        uavRangePong.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        
        uavRegisterIdx += uavRangePong.NumDescriptors;

        // ping buffers - read
        D3D12_DESCRIPTOR_RANGE& uavRangePing = pingPongRanges[1];
        memset(&uavRangePing, 0, sizeof(uavRangePing));
        uavRangePing.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRangePing.NumDescriptors = ComputeBuffers::numBuffers;
        uavRangePing.BaseShaderRegister = uavRegisterIdx;
        uavRangePing.RegisterSpace = 0;
        uavRangePing.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        uavRegisterIdx += uavRangePing.NumDescriptors;

        D3D12_ROOT_PARAMETER param[8];
        memset(&param, 0, sizeof(param));
        param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param[0].Constants.Num32BitValues = PropagationShader::DrawData::constantCount;
        param[0].Constants.ShaderRegister = 0;
        param[0].Constants.RegisterSpace = 0;
        param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param[1].DescriptorTable.NumDescriptorRanges = 1;
        param[1].DescriptorTable.pDescriptorRanges = &uavRangePong;
        param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        param[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param[2].DescriptorTable.NumDescriptorRanges = 1;
        param[2].DescriptorTable.pDescriptorRanges = &uavRangePing;
        param[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // preintegration tables and volume texture
        D3D12_DESCRIPTOR_RANGE descriptorRanges[ENUM_CLASS_TO_NUM(PitTex::MAX) + 1];
        memset(&descriptorRanges, 0, sizeof(descriptorRanges));
        for (int i = 0; i < countof(descriptorRanges); ++i)
        {
            D3D12_DESCRIPTOR_RANGE& range = descriptorRanges[i];
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            range.NumDescriptors = 1;
            range.BaseShaderRegister = srvRegisterIdx;
            range.RegisterSpace = 0;
            range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            srvRegisterIdx += range.NumDescriptors;
        }

        for (int i = 0; i < countof(descriptorRanges); ++i)
        {
            param[3 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param[3 + i].DescriptorTable.NumDescriptorRanges = 1;
            param[3 + i].DescriptorTable.pDescriptorRanges = &descriptorRanges[i];
            param[3 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }

        // volume and preintergration sampler are static samplers since they never change
        D3D12_STATIC_SAMPLER_DESC staticSamplers[3];
        makeStaticSamplerDescription(staticSamplers[0], 0, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_SHADER_VISIBILITY_ALL);
        makeStaticSamplerDescription(staticSamplers[1], 1, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_SHADER_VISIBILITY_ALL);
        makeStaticSamplerDescription(staticSamplers[2], 2, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_SHADER_VISIBILITY_ALL);

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        memset(&rootSignatureDesc, 0, sizeof(rootSignatureDesc));
        rootSignatureDesc.NumParameters = countof(param);
        rootSignatureDesc.pParameters = param;
        rootSignatureDesc.NumStaticSamplers = countof(staticSamplers);
        rootSignatureDesc.pStaticSamplers = staticSamplers;
        rootSignatureDesc.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        

        ComPtr<ID3DBlob> blob = nullptr;
        if (!BRWL_VERIFY(SUCCEEDED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, NULL)), BRWL_CHAR_LITERAL("Failed to serialize root signature.")))
        {
            ::BRWL::PAL::ShowLastWindowsError();
            destroy();
            return;
        }

        if (!BRWL_VERIFY(SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature))), BRWL_CHAR_LITERAL("Failed to create root signature")))
        {
            destroy();
            return;
        }

        rootSignature->SetName(L"Propagation Root Signature");

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc;
        memset(&psoDesc, 0, sizeof(psoDesc));
        psoDesc.pRootSignature = rootSignature.Get();
        psoDesc.CS = { Propagation_cs_cs, sizeof(Propagation_cs_cs) };
        psoDesc.NodeMask = 0;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;


        if (!BRWL_VERIFY(SUCCEEDED(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState))), BRWL_CHAR_LITERAL("Failed to create pipeline state.")))
        {
            destroy();
            return;
        }
    }
}

void PropagationShader::destroy()
{
    rootSignature = nullptr;
    pipelineState = nullptr;
}

PropagationShader::~PropagationShader()
{
    destroy();
}

unsigned int PropagationShader::draw(ID3D12GraphicsCommandList* cmd, const PropagationShader::DrawData& data, ComputeBuffers* computeBuffers, 
    PitCollection& pitCollection, TextureHandle& volumeTexture, unsigned int remainingSlices)
{
    SCOPED_GPU_EVENT(cmd, 0, 255, 0, "Propagation Compute Shader");
    BRWL_EXCEPTION(computeBuffers, nullptr);
    BRWL_EXCEPTION(computeBuffers->isResident(), nullptr);

    if (remainingSlices > 0)
    {
        const float targetDt = 1.0f / 60.f;
        const float currentDt = engine->time->getDeltaTime();
        // adjust slices per invocation to framerate
        if (currentDt < targetDt - 0.02f * targetDt) {
            ++slicesPerInvocation;
        }
        else if (currentDt > targetDt + 0.02f * targetDt) {
            slicesPerInvocation = Utils::max(slicesPerInvocation - 10, 10);
        }

        cmd->SetPipelineState(pipelineState.Get());
        cmd->SetComputeRootSignature(rootSignature.Get());

        // constants
        cmd->SetComputeRoot32BitConstants(0, data.constantCount, &data, 0);

        // Set preintegration tables
        for (int i = 0; i < ENUM_CLASS_TO_NUM(PitTex::MAX); ++i)
        {
            cmd->SetComputeRootDescriptorTable(3 + i, pitCollection.array[i].asPlatformHandle()->getDescriptorHandle()->getResident().residentGpu);
        }

        // set volume texture
        cmd->SetComputeRootDescriptorTable(3 + ENUM_CLASS_TO_NUM(PitTex::MAX), volumeTexture.getDescriptorHandle()->getResident().residentGpu);

        // Only propagate without the buffered outer region. This only works for directional lights where the the out-of-bounds
        // default values are constant.
        // point lights with falloff would need different values for the skirt on each new slice.
        unsigned int budgetCounter = 0;
        do {
            computeBuffers->beforeComputeUse(cmd);
            // cmd->SetComputeRoot32BitConstants(0, ShaderConstants::num32BitValues, &constants, 0);
            cmd->SetComputeRootDescriptorTable(1, computeBuffers->getTargetResourceDescriptorHandle(0).residentGpu);
            cmd->SetComputeRootDescriptorTable(2, computeBuffers->getSourceResourceDescriptorHandle(0).residentGpu);

            cmd->Dispatch(
                (unsigned int)std::ceil(computeBuffers->getWidth() / (float)DrawData::threadGroupSizeX),
                (unsigned int)std::ceil(computeBuffers->getHeight() / (float)DrawData::threadGroupSizeY),
                1
            );

            computeBuffers->afterComputeUse(cmd);

            ++budgetCounter;
            --remainingSlices;
        } while (budgetCounter < slicesPerInvocation && remainingSlices > 0);

        // swap one last time to make the last written texture the new source resource and complete all writes
        if (remainingSlices <= 0)
        {
            computeBuffers->beforeComputeUse(cmd);
        }
    }

    return remainingSlices;
}

BRWL_RENDERER_NS_END