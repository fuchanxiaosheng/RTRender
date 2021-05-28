#include "Renderer.h"

#include "../Render/application.h"
#include "../Render/commandQueue.h"
#include "../Render/CommandList.h"
#include "../Render/Helpers.h"
#include "Light.h"
#include "Material.h"
#include "../Render/window.h"

#include <wrl.h>
using namespace Microsoft::WRL;

#include "../Render/d3dx12.h"
#include <d3dcompiler.h>
#include <DirectXColors.h>
#include <DirectXMath.h>

using namespace DirectX;

#include <algorithm> 
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

struct Mat
{
    XMMATRIX ModelMatrix;
    XMMATRIX ModelViewMatrix;
    XMMATRIX InverseTransposeModelViewMatrix;
    XMMATRIX ModelViewProjectionMatrix;
};

struct LightProperties
{
    uint32_t NumPointLights;
    uint32_t NumSpotLights;
};

enum TonemapMethod : uint32_t
{
    TM_Linear,
    TM_Reinhard,
    TM_ReinhardSq,
    TM_ACESFilmic,
};

struct TonemapParameters
{
    TonemapParameters() 
		: TonemapMethod(TM_Reinhard), Exposure(0.0f), MaxLuminance(1.0f), K(1.0f), A(0.22f), B(0.3f) , C(0.1f), D(0.2f), E(0.01f), F(0.3f), LinearWhite(11.2f), Gamma(2.2f) {}

    TonemapMethod TonemapMethod;
    float Exposure;
    float MaxLuminance;
    float K;
    float A; 
    float B; 
    float C; 
    float D; 
    float E; 
    float F; 
    float LinearWhite;
    float Gamma;
};

TonemapParameters gTonemapParameters;

enum RootParameters
{
    MatricesCB,        
    MaterialCB,        
    LightPropertiesCB, 
    PointLights,       
    SpotLights,        
    Textures,          
    NumRootParameters
};

template<typename T>
constexpr const T& clamp(const T& val, const T& min = T(0), const T& max = T(1))
{
    return val < min ? min : val > max ? max : val;
}

XMMATRIX XM_CALLCONV LookAtMatrix(FXMVECTOR Position, FXMVECTOR Direction, FXMVECTOR Up)
{
    assert(!XMVector3Equal(Direction, XMVectorZero()));
    assert(!XMVector3IsInfinite(Direction));
    assert(!XMVector3Equal(Up, XMVectorZero()));
    assert(!XMVector3IsInfinite(Up));

    XMVECTOR R2 = XMVector3Normalize(Direction);

    XMVECTOR R0 = XMVector3Cross(Up, R2);
    R0 = XMVector3Normalize(R0);

    XMVECTOR R1 = XMVector3Cross(R2, R0);

    XMMATRIX M(R0, R1, R2, Position);

    return M;
}

Renderer::Renderer(const std::wstring& name, int width, int height, bool vSync)
    : super(name, width, height, vSync), mScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)), mForward(0), mBackward(0), mLeft(0), mRight(0), mUp(0), mDown(0), mPitch(0)
    , mYaw(0), mAnimateLights(false), mShift(false), mWidth(0), mHeight(0), mRenderScale(1.0f)
{

    XMVECTOR cameraPos = XMVectorSet(0, 5, -20, 1);
    XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
    XMVECTOR cameraUp = XMVectorSet(0, 1, 0, 0);

    mCamera.SetLookAt(cameraPos, cameraTarget, cameraUp);
    mCamera.SetProjection(45.0f, width / (float)height, 0.1f, 100.0f);

    mpAlignedCameraData = (CameraData*)_aligned_malloc(sizeof(CameraData), 16);

    mpAlignedCameraData->mInitialCamPos = mCamera.GetTranslation();
    mpAlignedCameraData->mInitialCamRot = mCamera.GetRotation();
    mpAlignedCameraData->mInitialFov = mCamera.GetFoV();
}

Renderer::~Renderer()
{
    _aligned_free(mpAlignedCameraData);
}

bool Renderer::LoadContent()
{
    auto device = Application::Get().GetDevice();
    auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
    auto commandList = commandQueue->GetCommandList();

    mCubeMesh = Mesh::CreateCube(*commandList);
    mSphereMesh = Mesh::CreateSphere(*commandList);
    mConeMesh = Mesh::CreateCone(*commandList);
    mTorusMesh = Mesh::CreateTorus(*commandList);
    mPlaneMesh = Mesh::CreatePlane(*commandList);
    mSkyboxMesh = Mesh::CreateCube(*commandList, 1.0f, true);

    commandList->LoadTextureFromFile(mDefaultTexture, L"D:\\Files\\Code\\C++\\RTRender\\Assets\\Textures\\DefaultWhite.bmp");
    commandList->LoadTextureFromFile(mDirectXTexture, L"D:\\Files\\Code\\C++\\RTRender\\Assets\\Textures\\Marble014_2K_Color.jpg");
    commandList->LoadTextureFromFile(mSphereTexture, L"D:\\Files\\Code\\C++\\RTRender\\Assets\\Textures\\grassCube1024.dds");
    commandList->LoadTextureFromFile(mCubeTexture, L"D:\\Files\\Code\\C++\\RTRender\\Assets\\Textures\\Cover.jpg");
    commandList->LoadTextureFromFile(mSkyboxTexture, L"D:\\Files\\Code\\C++\\RTRender\\Assets\\Textures\\kloppenheim_07.jpg");

    auto cubemapDesc = mSkyboxTexture.GetD3D12ResourceDesc();
    cubemapDesc.Width = cubemapDesc.Height = 1024;
    cubemapDesc.DepthOrArraySize = 6;
    cubemapDesc.MipLevels = 0;

    mSkyboxCubemap = Texture(cubemapDesc, nullptr, TextureUsage::Albedo, L"SkyboxCubemap");

    commandList->PanoToCubemap(mSkyboxCubemap, mSkyboxTexture);

    DXGI_FORMAT HDRFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;

    auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(HDRFormat, mWidth, mHeight);
    colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE colorClearValue;
    colorClearValue.Format = colorDesc.Format;
    colorClearValue.Color[0] = 0.4f;
    colorClearValue.Color[1] = 0.6f;
    colorClearValue.Color[2] = 0.9f;
    colorClearValue.Color[3] = 1.0f;

    Texture HDRTexture = Texture(colorDesc, &colorClearValue, TextureUsage::RenderTarget, L"HDR Texture");

    auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat, mWidth, mHeight);
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthClearValue;
    depthClearValue.Format = depthDesc.Format;
    depthClearValue.DepthStencil = { 1.0f, 0 };

    Texture depthTexture = Texture(depthDesc, &depthClearValue, TextureUsage::Depth, L"Depth Render Target");

    mHDRRenderTarget.AttachTexture(AttachmentPoint::Color0, HDRTexture);
    mHDRRenderTarget.AttachTexture(AttachmentPoint::DepthStencil, depthTexture);

    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    {
        ComPtr<ID3DBlob> vs = Utility::ShaderCompile(L"D:\\Files\\Code\\C++\\RTRender\\RTRender\\SandBox\\Shader\\Skybox_VS.hlsl", nullptr, "main", "vs_5_1");
        ComPtr<ID3DBlob> ps = Utility::ShaderCompile(L"D:\\Files\\Code\\C++\\RTRender\\RTRender\\SandBox\\Shader\\Skybox_PS.hlsl", nullptr, "main", "ps_5_1");

        D3D12_INPUT_ELEMENT_DESC inputLayout[1] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
														D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

        CD3DX12_ROOT_PARAMETER1 rootParameters[2];
        rootParameters[0].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[1].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 
													   D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(2, rootParameters, 1, &linearClampSampler, rootSignatureFlags);

        mSkyboxSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

        struct SkyboxPipelineState
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
            CD3DX12_PIPELINE_STATE_STREAM_VS VS;
            CD3DX12_PIPELINE_STATE_STREAM_PS PS;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        } skyboxPipelineStateStream;

        skyboxPipelineStateStream.pRootSignature = mSkyboxSignature.GetRootSignature().Get();
        skyboxPipelineStateStream.InputLayout = { inputLayout, 1 };
        skyboxPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        skyboxPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
        skyboxPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
        skyboxPipelineStateStream.RTVFormats = mHDRRenderTarget.GetRenderTargetFormats();

        D3D12_PIPELINE_STATE_STREAM_DESC skyboxPipelineStateStreamDesc = {
            sizeof(SkyboxPipelineState), &skyboxPipelineStateStream
        };
        ThrowIfFailed(device->CreatePipelineState(&skyboxPipelineStateStreamDesc, IID_PPV_ARGS(&mSkyboxPipelineState)));
    }

    {
        ComPtr<ID3DBlob> vs = Utility::ShaderCompile(L"D:\\Files\\Code\\C++\\RTRender\\RTRender\\SandBox\\Shader\\HDR_VS.hlsl", nullptr, "main", "vs_5_1");
        ComPtr<ID3DBlob> ps = Utility::ShaderCompile(L"D:\\Files\\Code\\C++\\RTRender\\RTRender\\SandBox\\Shader\\HDR_PS.hlsl", nullptr, "main", "ps_5_1");
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
														D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

        CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
        rootParameters[RootParameters::MatricesCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[RootParameters::MaterialCB].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[RootParameters::LightPropertiesCB].InitAsConstants(sizeof(LightProperties) / 4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[RootParameters::PointLights].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[RootParameters::SpotLights].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[RootParameters::Textures].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_STATIC_SAMPLER_DESC linearRepeatSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);
        CD3DX12_STATIC_SAMPLER_DESC anisotropicSampler(0, D3D12_FILTER_ANISOTROPIC);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(RootParameters::NumRootParameters, rootParameters, 1, &linearRepeatSampler, rootSignatureFlags);

        mHDRRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

        struct HDRPipelineStateStream
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
            CD3DX12_PIPELINE_STATE_STREAM_VS VS;
            CD3DX12_PIPELINE_STATE_STREAM_PS PS;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        } hdrPipelineStateStream;

        hdrPipelineStateStream.pRootSignature = mHDRRootSignature.GetRootSignature().Get();
        hdrPipelineStateStream.InputLayout = { VertexPositionNormalTexture::InputElements, VertexPositionNormalTexture::InputElementCount };
        hdrPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        hdrPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
        hdrPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
        hdrPipelineStateStream.DSVFormat = mHDRRenderTarget.GetDepthStencilFormat();
        hdrPipelineStateStream.RTVFormats = mHDRRenderTarget.GetRenderTargetFormats();

        D3D12_PIPELINE_STATE_STREAM_DESC hdrPipelineStateStreamDesc = {
            sizeof(HDRPipelineStateStream), &hdrPipelineStateStream
        };
        ThrowIfFailed(device->CreatePipelineState(&hdrPipelineStateStreamDesc, IID_PPV_ARGS(&mHDRPipelineState)));
    }
    {
        CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

        CD3DX12_ROOT_PARAMETER1 rootParameters[2];
        rootParameters[0].InitAsConstants(sizeof(TonemapParameters) / 4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[1].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_STATIC_SAMPLER_DESC linearClampsSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(2, rootParameters, 1, &linearClampsSampler );

        mSDRRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

        ComPtr<ID3DBlob> vs = Utility::ShaderCompile(L"D:/Files/Code/C++/RTRender/RTRender/SandBox/Shader/HDRtoSDR_VS.hlsl", nullptr, "main", "vs_5_1");
        ComPtr<ID3DBlob> ps = Utility::ShaderCompile(L"D:/Files/Code/C++/RTRender/RTRender/SandBox/Shader/HDRtoSDR_PS.hlsl", nullptr, "main", "ps_5_1");

        CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
        rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

        struct SDRPipelineStateStream
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
            CD3DX12_PIPELINE_STATE_STREAM_VS VS;
            CD3DX12_PIPELINE_STATE_STREAM_PS PS;
            CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        } sdrPipelineStateStream;

        sdrPipelineStateStream.pRootSignature = mSDRRootSignature.GetRootSignature().Get();
        sdrPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        sdrPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
        sdrPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
        sdrPipelineStateStream.Rasterizer = rasterizerDesc;
        sdrPipelineStateStream.RTVFormats = mpWindow->GetRenderTarget().GetRenderTargetFormats();

        D3D12_PIPELINE_STATE_STREAM_DESC sdrPipelineStateStreamDesc = {
            sizeof(SDRPipelineStateStream), &sdrPipelineStateStream
        };
        ThrowIfFailed(device->CreatePipelineState(&sdrPipelineStateStreamDesc, IID_PPV_ARGS(&mSDRPipelineState)));
    }

    auto fenceValue = commandQueue->ExecuteCommandList(commandList);
    commandQueue->WaitForFenceValue(fenceValue);

    return true;
}

void Renderer::RescaleHDRRenderTarget(float scale)
{
    uint32_t width = static_cast<uint32_t>(mWidth * scale);
    uint32_t height = static_cast<uint32_t>(mHeight * scale);
    
    width = clamp<uint32_t>(width, 1, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);
    height = clamp<uint32_t>(height, 1, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);

    mHDRRenderTarget.Resize(width, height);
}

void Renderer::OnResize(ResizeEventArgs& e)
{
    super::OnResize(e);

    if (mWidth != e.Width || mHeight != e.Height)
    {
        mWidth = std::max(1, e.Width);
        mHeight = std::max(1, e.Height);

        float fov = mCamera.GetFoV();
        float aspectRatio = mWidth / (float)mHeight;
        mCamera.SetProjection(fov, aspectRatio, 0.1f, 100.0f);

        RescaleHDRRenderTarget(mRenderScale);
    }
}

void Renderer::UnloadContent()
{
}

static double g_FPS = 0.0;

void Renderer::OnUpdate(UpdateEventArgs& e)
{
    static uint64_t frameCount = 0;
    static double totalTime = 0.0;

    super::OnUpdate(e);

    totalTime += e.ElapsedTime;
    frameCount++;

    if (totalTime > 1.0)
    {
        g_FPS = frameCount / totalTime;

        char buffer[512];
        sprintf_s(buffer, "FPS: %f\n", g_FPS);
        OutputDebugStringA(buffer);

        frameCount = 0;
        totalTime = 0.0;
    }

    float speedMultipler = (mShift ? 16.0f : 4.0f);

    XMVECTOR cameraTranslate = XMVectorSet(mRight - mLeft, 0.0f, mForward - mBackward, 1.0f) * speedMultipler * static_cast<float>(e.ElapsedTime);
    XMVECTOR cameraPan = XMVectorSet(0.0f, mUp - mDown, 0.0f, 1.0f) * speedMultipler * static_cast<float>(e.ElapsedTime);
    mCamera.Translate(cameraTranslate, Space::Local);
    mCamera.Translate(cameraPan, Space::Local);

    XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(mPitch), XMConvertToRadians(mYaw), 0.0f);
    mCamera.SetRotation(cameraRotation);

    XMMATRIX viewMatrix = mCamera.GetViewMatrix();

    const int numPointLights = 4;
    const int numSpotLights = 4;

    static const XMVECTORF32 LightColors[] =
    {
        Colors::White, Colors::Orange, Colors::Yellow, Colors::Green, Colors::Blue, Colors::Indigo, Colors::Violet, Colors::White
    };

    static float lightAnimTime = 0.0f;
    if (mAnimateLights)
    {
        lightAnimTime += static_cast<float>(e.ElapsedTime) * 0.5f * XM_PI;
    }

    const float radius = 8.0f;
    const float offset = 2.0f * XM_PI / numPointLights;
    const float offset2 = offset + (offset / 2.0f);

    mPointLights.resize(numPointLights);
    for (int i = 0; i < numPointLights; ++i)
    {
        PointLight& l = mPointLights[i];

        l.PositionWS = {
            static_cast<float>(std::sin(lightAnimTime + offset * i)) * radius,
            9.0f,
            static_cast<float>(std::cos(lightAnimTime + offset * i)) * radius,
            1.0f
        };
        XMVECTOR positionWS = XMLoadFloat4(&l.PositionWS);
        XMVECTOR positionVS = XMVector3TransformCoord(positionWS, viewMatrix);
        XMStoreFloat4(&l.PositionVS, positionVS);

        l.Color = XMFLOAT4(LightColors[i]);
        l.Intensity = 1.0f;
        l.Attenuation = 0.0f;
    }

    mSpotLights.resize(numSpotLights);
    for (int i = 0; i < numSpotLights; ++i)
    {
        SpotLight& l = mSpotLights[i];

        l.PositionWS = {
            static_cast<float>(std::sin(lightAnimTime + offset * i + offset2)) * radius,
            9.0f,
            static_cast<float>(std::cos(lightAnimTime + offset * i + offset2)) * radius,
            1.0f
        };
        XMVECTOR positionWS = XMLoadFloat4(&l.PositionWS);
        XMVECTOR positionVS = XMVector3TransformCoord(positionWS, viewMatrix);
        XMStoreFloat4(&l.PositionVS, positionVS);

        XMVECTOR directionWS = XMVector3Normalize(XMVectorSetW(XMVectorNegate(positionWS), 0));
        XMVECTOR directionVS = XMVector3Normalize(XMVector3TransformNormal(directionWS, viewMatrix));
        XMStoreFloat4(&l.DirectionWS, directionWS);
        XMStoreFloat4(&l.DirectionVS, directionVS);

        l.Color = XMFLOAT4(LightColors[numPointLights + i]);
        l.Intensity = 1.0f;
        l.SpotAngle = XMConvertToRadians(45.0f);
        l.Attenuation = 0.0f;
    }
}

static void ShowHelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static const int VALUES_COUNT = 256;
static const float HDR_MAX = 12.0f;

float LinearTonemapping(float HDR, float max)
{
    if (max > 0.0f)
    {
        return clamp(HDR / max);
    }
    return HDR;
}

float LinearTonemappingPlot(void*, int index)
{
    return LinearTonemapping(index / (float)VALUES_COUNT * HDR_MAX, gTonemapParameters.MaxLuminance);
}

float ReinhardTonemapping(float HDR, float k)
{
    return HDR / (HDR + k);
}

float ReinhardTonemappingPlot(void*, int index)
{
    return ReinhardTonemapping(index / (float)VALUES_COUNT * HDR_MAX, gTonemapParameters.K);
}

float ReinhardSqrTonemappingPlot(void*, int index)
{
    float reinhard = ReinhardTonemapping(index / (float)VALUES_COUNT * HDR_MAX, gTonemapParameters.K);
    return reinhard * reinhard;
}

float ACESFilmicTonemapping(float x, float A, float B, float C, float D, float E, float F)
{
    return (((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F));
}

float ACESFilmicTonemappingPlot(void*, int index)
{
    float HDR = index / (float)VALUES_COUNT * HDR_MAX;
    return ACESFilmicTonemapping(HDR, gTonemapParameters.A, gTonemapParameters.B, gTonemapParameters.C, gTonemapParameters.D, gTonemapParameters.E, gTonemapParameters.F) /
        ACESFilmicTonemapping(gTonemapParameters.LinearWhite, gTonemapParameters.A, gTonemapParameters.B, gTonemapParameters.C, gTonemapParameters.D, gTonemapParameters.E, gTonemapParameters.F);
}

void Renderer::OnGUI()
{
    static bool showDemoWindow = false;
    static bool showOptions = true;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit", "Esc"))
            {
                Application::Get().Quit();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("ImGui Demo", nullptr, &showDemoWindow);
            ImGui::MenuItem("Tonemapping", nullptr, &showOptions);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Options") )
        {
            bool vSync = mpWindow->IsVSync();
            if (ImGui::MenuItem("V-Sync", "V", &vSync))
            {
                mpWindow->SetVSync(vSync);
            }

            bool fullscreen = mpWindow->IsFullScreen();
            if (ImGui::MenuItem("Full screen", "Alt+Enter", &fullscreen) )
            {
                mpWindow->SetFullscreen(fullscreen);
            }

            ImGui::EndMenu();
        }

        char buffer[256];

        {
            float renderScale = mRenderScale;
            ImGui::PushItemWidth(300.0f);
            ImGui::SliderFloat("Resolution Scale", &renderScale, 0.1f, 2.0f);
            renderScale = clamp(renderScale, 0.0f, 2.0f);

            auto size = mHDRRenderTarget.GetSize();
            ImGui::SameLine();
            sprintf_s(buffer, _countof(buffer), "(%ux%u)", size.x, size.y);
            ImGui::Text(buffer);

            if (renderScale != mRenderScale)
            {
                mRenderScale = renderScale;
                RescaleHDRRenderTarget(mRenderScale);
            }
        }

        {
            sprintf_s(buffer, _countof(buffer), "FPS: %.2f (%.2f ms)  ", g_FPS, 1.0 / g_FPS * 1000.0);
            auto fpsTextSize = ImGui::CalcTextSize(buffer);
            ImGui::SameLine(ImGui::GetWindowWidth() - fpsTextSize.x);
            ImGui::Text(buffer);
        }

        ImGui::EndMainMenuBar();
    }

    if (showDemoWindow)
    {
        ImGui::ShowDemoWindow(&showDemoWindow);
    }

    if (showOptions)
    {
        ImGui::Begin("Tonemapping", &showOptions);
        {
            ImGui::TextWrapped("Use the Exposure slider to adjust the overall exposure of the HDR scene.");
            ImGui::SliderFloat("Exposure", &gTonemapParameters.Exposure, -10.0f, 10.0f);
            ImGui::SameLine(); ShowHelpMarker("Adjust the overall exposure of the HDR scene.");
            ImGui::SliderFloat("Gamma", &gTonemapParameters.Gamma, 0.01f, 5.0f);
            ImGui::SameLine(); ShowHelpMarker("Adjust the Gamma of the output image.");

            const char* toneMappingMethods[] = {
                "Linear",
                "Reinhard",
                "Reinhard Squared",
                "ACES Filmic"
            };

            ImGui::Combo("Tonemapping Methods", (int*)(&gTonemapParameters.TonemapMethod), toneMappingMethods, 4);

            switch (gTonemapParameters.TonemapMethod)
            {
            case TonemapMethod::TM_Linear:
                ImGui::PlotLines("Linear Tonemapping", &LinearTonemappingPlot, nullptr, VALUES_COUNT, 0, nullptr, 0.0f, 1.0f, ImVec2(0, 250));
                ImGui::SliderFloat("Max Brightness", &gTonemapParameters.MaxLuminance, 1.0f, 10.0f);
                ImGui::SameLine(); ShowHelpMarker("Linearly scale the HDR image by the maximum brightness.");
                break;
            case TonemapMethod::TM_Reinhard:
                ImGui::PlotLines("Reinhard Tonemapping", &ReinhardTonemappingPlot, nullptr, VALUES_COUNT, 0, nullptr, 0.0f, 1.0f, ImVec2(0, 250));
                ImGui::SliderFloat("Reinhard Constant", &gTonemapParameters.K, 0.01f, 10.0f);
                ImGui::SameLine(); ShowHelpMarker("The Reinhard constant is used in the denominator.");
                break;
            case TonemapMethod::TM_ReinhardSq:
                ImGui::PlotLines("Reinhard Squared Tonemapping", &ReinhardSqrTonemappingPlot, nullptr, VALUES_COUNT, 0, nullptr, 0.0f, 1.0f, ImVec2(0, 250));
                ImGui::SliderFloat("Reinhard Constant", &gTonemapParameters.K, 0.01f, 10.0f);
                ImGui::SameLine(); ShowHelpMarker("The Reinhard constant is used in the denominator.");
                break;
            case TonemapMethod::TM_ACESFilmic:
                ImGui::PlotLines("ACES Filmic Tonemapping", &ACESFilmicTonemappingPlot, nullptr, VALUES_COUNT, 0, nullptr, 0.0f, 1.0f, ImVec2(0, 250));
                ImGui::SliderFloat("Shoulder Strength", &gTonemapParameters.A, 0.01f, 5.0f);
                ImGui::SliderFloat("Linear Strength", &gTonemapParameters.B, 0.0f, 100.0f);
                ImGui::SliderFloat("Linear Angle", &gTonemapParameters.C, 0.0f, 1.0f);
                ImGui::SliderFloat("Toe Strength", &gTonemapParameters.D, 0.01f, 1.0f);
                ImGui::SliderFloat("Toe Numerator", &gTonemapParameters.E, 0.0f, 10.0f);
                ImGui::SliderFloat("Toe Denominator", &gTonemapParameters.F, 1.0f, 10.0f);
                ImGui::SliderFloat("Linear White", &gTonemapParameters.LinearWhite, 1.0f, 120.0f);
                break;
            default:
                break;
            }
        }

        if (ImGui::Button("Reset to Defaults"))
        {
            TonemapMethod method = gTonemapParameters.TonemapMethod;
            gTonemapParameters = TonemapParameters();
            gTonemapParameters.TonemapMethod = method;
        }

        ImGui::End();

    }
}

void XM_CALLCONV ComputeMatrices(FXMMATRIX model, CXMMATRIX view, CXMMATRIX viewProjection, Mat& mat)
{
    mat.ModelMatrix = model;
    mat.ModelViewMatrix = model * view;
    mat.InverseTransposeModelViewMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, mat.ModelViewMatrix));
    mat.ModelViewProjectionMatrix = model * viewProjection;
}

void Renderer::OnRender(RenderEventArgs& e)
{
    super::OnRender(e);

    auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto commandList = commandQueue->GetCommandList();

    {
        FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

        commandList->ClearTexture(mHDRRenderTarget.GetTexture(AttachmentPoint::Color0), clearColor);
        commandList->ClearDepthStencilTexture(mHDRRenderTarget.GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
    }

    commandList->SetRenderTarget(mHDRRenderTarget);
    commandList->SetViewport(mHDRRenderTarget.GetViewport());
    commandList->SetScissorRect(mScissorRect);
	{
    
        auto viewMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion(mCamera.GetRotation()));
        auto projMatrix = mCamera.GetProjectionMatrix();
        auto viewProjMatrix = viewMatrix * projMatrix;

        commandList->SetPipelineState(mSkyboxPipelineState);
        commandList->SetGraphicsRootSignature(mSkyboxSignature);

        commandList->SetGraphics32BitConstants(0, viewProjMatrix);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = mSkyboxCubemap.GetD3D12ResourceDesc().Format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = (UINT)-1; 

        commandList->SetShaderResourceView(1, 0, mSkyboxCubemap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &srvDesc);

        mSkyboxMesh->Draw(*commandList);
    }


    commandList->SetPipelineState(mHDRPipelineState);
    commandList->SetGraphicsRootSignature(mHDRRootSignature);

    LightProperties lightProps;
    lightProps.NumPointLights = static_cast<uint32_t>(mPointLights.size());
    lightProps.NumSpotLights = static_cast<uint32_t>(mSpotLights.size());

    commandList->SetGraphics32BitConstants(RootParameters::LightPropertiesCB, lightProps);
    commandList->SetGraphicsDynamicStructuredBuffer(RootParameters::PointLights, mPointLights);
    commandList->SetGraphicsDynamicStructuredBuffer(RootParameters::SpotLights, mSpotLights);

    XMMATRIX translationMatrix = XMMatrixTranslation(-4.0f, 2.0f, -4.0f);
    XMMATRIX rotationMatrix = XMMatrixIdentity();
    XMMATRIX scaleMatrix = XMMatrixScaling(4.0f, 4.0f, 4.0f);
    XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
    XMMATRIX viewMatrix = mCamera.GetViewMatrix();
    XMMATRIX viewProjectionMatrix = viewMatrix * mCamera.GetProjectionMatrix();

    Mat matrices;
    ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::White);
    commandList->SetShaderResourceView(RootParameters::Textures, 0, mSphereTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    mSphereMesh->Draw(*commandList);

    translationMatrix = XMMatrixTranslation(4.0f, 4.0f, 4.0f);
    rotationMatrix = XMMatrixRotationY(XMConvertToRadians(45.0f));
    scaleMatrix = XMMatrixScaling(4.0f, 8.0f, 4.0f);
    worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

    ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::White);
    commandList->SetShaderResourceView(RootParameters::Textures, 0, mCubeTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    mCubeMesh->Draw(*commandList);

    translationMatrix = XMMatrixTranslation(4.0f, 0.6f, -4.0f);
    rotationMatrix = XMMatrixRotationY(XMConvertToRadians(45.0f));
    scaleMatrix = XMMatrixScaling(4.0f, 4.0f, 4.0f);
    worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

    ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Ruby);
    commandList->SetShaderResourceView(RootParameters::Textures, 0, mDefaultTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    mTorusMesh->Draw(*commandList);

    float scalePlane = 20.0f;
    float translateOffset = scalePlane / 2.0f;

    translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
    rotationMatrix = XMMatrixIdentity();
    scaleMatrix = XMMatrixScaling(scalePlane, 1.0f, scalePlane);
    worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

    ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::White);
    commandList->SetShaderResourceView(RootParameters::Textures, 0, mDirectXTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    mPlaneMesh->Draw(*commandList);

    translationMatrix = XMMatrixTranslation(0, translateOffset, translateOffset);
    rotationMatrix = XMMatrixRotationX(XMConvertToRadians(-90));
    worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

    ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);

    mPlaneMesh->Draw(*commandList);

    translationMatrix = XMMatrixTranslation(0, translateOffset * 2.0f, 0);
    rotationMatrix = XMMatrixRotationX(XMConvertToRadians(180));
    worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

    ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);

    mPlaneMesh->Draw(*commandList);

    translationMatrix = XMMatrixTranslation(0, translateOffset, -translateOffset);
    rotationMatrix = XMMatrixRotationX(XMConvertToRadians(90));
    worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

    ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);

    mPlaneMesh->Draw(*commandList);

    translationMatrix = XMMatrixTranslation(-translateOffset, translateOffset, 0);
    rotationMatrix = XMMatrixRotationX(XMConvertToRadians(-90)) * XMMatrixRotationY(XMConvertToRadians(-90));
    worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

    ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Red);
    commandList->SetShaderResourceView(RootParameters::Textures, 0, mDefaultTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    mPlaneMesh->Draw(*commandList);

    translationMatrix = XMMatrixTranslation(translateOffset, translateOffset, 0);
    rotationMatrix = XMMatrixRotationX(XMConvertToRadians(-90)) * XMMatrixRotationY(XMConvertToRadians(90));
    worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

    ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Blue);
    mPlaneMesh->Draw(*commandList);

    Material lightMaterial;
    lightMaterial.Specular = { 0, 0, 0, 1 };
    for (const auto& l : mPointLights)
    {
        lightMaterial.Emissive = l.Color;
        XMVECTOR lightPos = XMLoadFloat4(&l.PositionWS);
        worldMatrix = XMMatrixTranslationFromVector(lightPos);
        ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

        commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
        commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, lightMaterial);

        mSphereMesh->Draw(*commandList);
    }

    for (const auto& l : mSpotLights)
    {
        lightMaterial.Emissive = l.Color;
        XMVECTOR lightPos = XMLoadFloat4(&l.PositionWS);
        XMVECTOR lightDir = XMLoadFloat4(&l.DirectionWS);
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);

        rotationMatrix = XMMatrixRotationX(XMConvertToRadians(-90.0f));
        worldMatrix = rotationMatrix * LookAtMatrix(lightPos, lightDir, up);

        ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

        commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
        commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, lightMaterial);

        mConeMesh->Draw(*commandList);
    }

    commandList->SetRenderTarget(mpWindow->GetRenderTarget());
    commandList->SetViewport(mpWindow->GetRenderTarget().GetViewport());
    commandList->SetPipelineState(mSDRPipelineState);
    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootSignature(mSDRRootSignature);
    commandList->SetGraphics32BitConstants(0, gTonemapParameters);
    commandList->SetShaderResourceView(1, 0, mHDRRenderTarget.GetTexture(Color0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    commandList->Draw(3);

    commandQueue->ExecuteCommandList(commandList);

    OnGUI();

    mpWindow->Present();
}

static bool g_AllowFullscreenToggle = true;

void Renderer::OnKeyPressed(KeyEventArgs& e)
{
    super::OnKeyPressed(e);

    if (!ImGui::GetIO().WantCaptureKeyboard)
    {
        switch (e.Key)
        {
        case KeyCode::Escape:
            Application::Get().Quit(0);
            break;
        case KeyCode::Enter:
            if (e.Alt)
            {
        case KeyCode::F11:
            if (g_AllowFullscreenToggle)
            {
                mpWindow->ToggleFullscreen();
                g_AllowFullscreenToggle = false;
            }
            break;
            }
        case KeyCode::V:
            mpWindow->ToggleVSync();
            break;
        case KeyCode::R:
            mCamera.SetTranslation(mpAlignedCameraData->mInitialCamPos);
            mCamera.SetRotation(mpAlignedCameraData->mInitialCamRot);
            mCamera.SetFoV(mpAlignedCameraData->mInitialFov);
            mPitch = 0.0f;
            mYaw = 0.0f;
            break;
        case KeyCode::Up:
        case KeyCode::W:
            mForward = 1.0f;
            break;
        case KeyCode::Left:
        case KeyCode::A:
            mLeft = 1.0f;
            break;
        case KeyCode::Down:
        case KeyCode::S:
            mBackward = 1.0f;
            break;
        case KeyCode::Right:
        case KeyCode::D:
            mRight = 1.0f;
            break;
        case KeyCode::Q:
            mDown = 1.0f;
            break;
        case KeyCode::E:
            mUp = 1.0f;
            break;
        case KeyCode::Space:
            mAnimateLights = !mAnimateLights;
            break;
        case KeyCode::ShiftKey:
            mShift = true;
            break;
        }
    }
}

void Renderer::OnKeyReleased(KeyEventArgs& e)
{
    super::OnKeyReleased(e);

    if (!ImGui::GetIO().WantCaptureKeyboard)
    {
        switch (e.Key)
        {
        case KeyCode::Enter:
            if (e.Alt)
            {
        case KeyCode::F11:
            g_AllowFullscreenToggle = true;
            }
            break;
        case KeyCode::Up:
        case KeyCode::W:
            mForward = 0.0f;
            break;
        case KeyCode::Left:
        case KeyCode::A:
            mLeft = 0.0f;
            break;
        case KeyCode::Down:
        case KeyCode::S:
            mBackward = 0.0f;
            break;
        case KeyCode::Right:
        case KeyCode::D:
            mRight = 0.0f;
            break;
        case KeyCode::Q:
            mDown = 0.0f;
            break;
        case KeyCode::E:
            mUp = 0.0f;
            break;
        case KeyCode::ShiftKey:
            mShift = false;
            break;
        }
    }
}

void Renderer::OnMouseMoved(MouseMotionEventArgs& e)
{
    super::OnMouseMoved(e);

    const float mouseSpeed = 0.1f;
    if (!ImGui::GetIO().WantCaptureMouse)
    {
        if (e.LeftButton)
        {
            mPitch -= e.RelY * mouseSpeed;

            mPitch = clamp(mPitch, -90.0f, 90.0f);

            mYaw -= e.RelX * mouseSpeed;
        }
    }
}


void Renderer::OnMouseWheel(MouseWheelEventArgs& e)
{
    if (!ImGui::GetIO().WantCaptureMouse)
    {
        auto fov = mCamera.GetFoV();

        fov -= e.WheelDelta;
        fov = clamp(fov, 12.0f, 90.0f);

        mCamera.SetFoV(fov);

        char buffer[256];
        sprintf_s(buffer, "FoV: %f\n", fov);
        OutputDebugStringA(buffer);
    }
}
