#pragma once


#include <DirectXMath.h>


enum class Space
{
    Local,
    World,
};

class Camera
{
public:

    Camera();
    virtual ~Camera();

    void XM_CALLCONV SetLookAt( DirectX::FXMVECTOR eye, DirectX::FXMVECTOR target, DirectX::FXMVECTOR up );
    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetInverseViewMatrix() const;
    void SetProjection( float fovy, float aspect, float zNear, float zFar );
    DirectX::XMMATRIX GetProjectionMatrix() const;
    DirectX::XMMATRIX GetInverseProjectionMatrix() const;
    void SetFoV(float fovy);
    float GetFoV() const;
    void XM_CALLCONV SetTranslation( DirectX::FXMVECTOR translation );
    DirectX::XMVECTOR GetTranslation() const;
    void XM_CALLCONV SetRotation( DirectX::FXMVECTOR rotation );
    DirectX::XMVECTOR GetRotation() const;

    void XM_CALLCONV Translate( DirectX::FXMVECTOR translation, Space space = Space::Local );
    void Rotate( DirectX::FXMVECTOR quaternion );

protected:
    virtual void UpdateViewMatrix() const;
    virtual void UpdateInverseViewMatrix() const;
    virtual void UpdateProjectionMatrix() const;
    virtual void UpdateInverseProjectionMatrix() const;

    __declspec(align(16)) struct AlignedData
    {
        DirectX::XMVECTOR mTranslation;
        DirectX::XMVECTOR mRotation;
        DirectX::XMMATRIX mViewMatrix, mInverseViewMatrix;
        DirectX::XMMATRIX mProjectionMatrix, mInverseProjectionMatrix;
    };
    AlignedData* pData;


    float mvFoV;   
    float mAspectRatio; 
    float mzNear;      
    float mzFar;       

    mutable bool mViewDirty, mInverseViewDirty;
    mutable bool mProjectionDirty, mInverseProjectionDirty;

};