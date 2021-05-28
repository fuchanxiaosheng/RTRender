#pragma once

#include "Camera.h"
#include "../Render/game.h""
#include "../Render/IndexBuffer.h"
#include "Light.h"
#include "../Render/window.h"
#include "../Render/Mesh.h"
#include "../Render/RenderTarget.h"
#include "../Render/RootSignature.h"
#include "../Render/Texture.h"
#include "../Render/VertexBuffer.h"

#include <DirectXMath.h>

class Renderer : public Game
{
public:
    using super = Game;

	Renderer(const std::wstring& name, int width, int height, bool vSync = false);
    virtual ~Renderer();
    virtual bool LoadContent() override;
    virtual void UnloadContent() override;
protected:

    virtual void OnUpdate(UpdateEventArgs& e) override;
    virtual void OnRender(RenderEventArgs& e) override;
    virtual void OnKeyPressed(KeyEventArgs& e) override;
    virtual void OnKeyReleased(KeyEventArgs& e);
    virtual void OnMouseMoved(MouseMotionEventArgs& e);
    virtual void OnMouseWheel(MouseWheelEventArgs& e) override;

    void RescaleHDRRenderTarget(float scale);
    virtual void OnResize(ResizeEventArgs& e) override; 


    void OnGUI();

private:
    std::unique_ptr<Mesh> mCubeMesh;
    std::unique_ptr<Mesh> mSphereMesh;
    std::unique_ptr<Mesh> mConeMesh;
    std::unique_ptr<Mesh> mTorusMesh;
    std::unique_ptr<Mesh> mPlaneMesh;

    std::unique_ptr<Mesh> mSkyboxMesh;

    Texture mDefaultTexture;
    Texture mDirectXTexture;
    Texture mSphereTexture;
    Texture mCubeTexture;
    Texture mSkyboxTexture;
    Texture mSkyboxCubemap;

    RenderTarget mHDRRenderTarget;
    RootSignature mSkyboxSignature;
    RootSignature mHDRRootSignature;
    RootSignature mSDRRootSignature;


	ComPtr<ID3D12PipelineState> mSkyboxPipelineState;
    ComPtr<ID3D12PipelineState> mHDRPipelineState;
    ComPtr<ID3D12PipelineState> mSDRPipelineState;

    D3D12_RECT mScissorRect;

    Camera mCamera;
    struct alignas( 16 ) CameraData
    {
        DirectX::XMVECTOR mInitialCamPos;
        DirectX::XMVECTOR mInitialCamRot;
        float mInitialFov;
    };
    CameraData* mpAlignedCameraData;

    float mForward;
    float mBackward;
    float mLeft;
    float mRight;
    float mUp;
    float mDown;

    float mPitch;
    float mYaw;

    bool mAnimateLights;
    bool mShift;

    int mWidth;
    int mHeight;

    float mRenderScale;

    std::vector<PointLight> mPointLights;
    std::vector<SpotLight> mSpotLights;
};