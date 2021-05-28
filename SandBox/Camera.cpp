#include "Camera.h"

using namespace DirectX;

Camera::Camera() : mViewDirty( true ), mInverseViewDirty( true ), mProjectionDirty( true ), mInverseProjectionDirty( true ), mvFoV( 45.0f ), mAspectRatio( 1.0f ), mzNear( 0.1f ), mzFar( 100.0f )
{
    pData = (AlignedData*)_aligned_malloc( sizeof(AlignedData), 16 );
    pData->mTranslation = XMVectorZero();
    pData->mRotation = XMQuaternionIdentity();
}

Camera::~Camera()
{
    _aligned_free(pData);
}

void XM_CALLCONV Camera::SetLookAt( FXMVECTOR eye, FXMVECTOR target, FXMVECTOR up )
{
    pData->mViewMatrix = XMMatrixLookAtLH( eye, target, up );

    pData->mTranslation = eye;
    pData->mRotation = XMQuaternionRotationMatrix( XMMatrixTranspose(pData->mViewMatrix) );

    mInverseViewDirty = true;
    mViewDirty = false;
}

XMMATRIX Camera::GetViewMatrix() const
{
    if ( mViewDirty )
    {
        UpdateViewMatrix();
    }
    return pData->mViewMatrix;
}

XMMATRIX Camera::GetInverseViewMatrix() const
{
    if ( mInverseViewDirty )
    {
        pData->mInverseViewMatrix = XMMatrixInverse( nullptr, pData->mViewMatrix );
        mInverseViewDirty = false;
    }

    return pData->mInverseViewMatrix;
}

void Camera::SetProjection( float fovy, float aspect, float zNear, float zFar )
{
    mvFoV = fovy;
    mAspectRatio = aspect;
    mzNear = zNear;
    mzFar = zFar;

    mProjectionDirty = true;
    mInverseProjectionDirty = true;
}

XMMATRIX Camera::GetProjectionMatrix() const
{
    if ( mProjectionDirty )
    {
        UpdateProjectionMatrix();
    }

    return pData->mProjectionMatrix;
}

XMMATRIX Camera::GetInverseProjectionMatrix() const
{
    if ( mInverseProjectionDirty )
    {
        UpdateInverseProjectionMatrix();
    }

    return pData->mInverseProjectionMatrix;
}

void Camera::SetFoV(float fovy)
{
    if (mvFoV != fovy)
    {
        mvFoV = fovy;
        mProjectionDirty = true;
        mInverseProjectionDirty = true;
    }
}

float Camera::GetFoV() const
{
    return mvFoV;
}


void XM_CALLCONV Camera::SetTranslation( FXMVECTOR translation )
{
    pData->mTranslation = translation;
    mViewDirty = true;
}

XMVECTOR Camera::GetTranslation() const
{
    return pData->mTranslation;
}

void Camera::SetRotation( FXMVECTOR rotation )
{
    pData->mRotation = rotation;
}

XMVECTOR Camera::GetRotation() const
{
    return pData->mRotation;
}

void XM_CALLCONV Camera::Translate( FXMVECTOR translation, Space space )
{
    switch ( space )
    {
    case Space::Local:
        {
            pData->mTranslation += XMVector3Rotate( translation, pData->mRotation );
        }
        break;
    case Space::World:
        {
            pData->mTranslation += translation;
        }
        break;
    }

    pData->mTranslation = XMVectorSetW( pData->mTranslation, 1.0f );

    mViewDirty = true;
    mInverseViewDirty = true;
}

void Camera::Rotate( FXMVECTOR quaternion )
{
    pData->mRotation = XMQuaternionMultiply( pData->mRotation, quaternion );

    mViewDirty = true;
    mInverseViewDirty = true;
}

void Camera::UpdateViewMatrix() const
{
    XMMATRIX rotationMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion( pData->mRotation ));
    XMMATRIX translationMatrix = XMMatrixTranslationFromVector( -(pData->mTranslation) );

    pData->mViewMatrix = translationMatrix * rotationMatrix;

    mInverseViewDirty = true;
    mViewDirty = false;
}

void Camera::UpdateInverseViewMatrix() const
{
    if ( mViewDirty )
    {
        UpdateViewMatrix();
    }

    pData->mInverseViewMatrix = XMMatrixInverse( nullptr, pData->mViewMatrix );
    mInverseViewDirty = false;
}

void Camera::UpdateProjectionMatrix() const
{
    pData->mProjectionMatrix = XMMatrixPerspectiveFovLH( XMConvertToRadians(mvFoV), mAspectRatio, mzNear, mzFar );

    mProjectionDirty = false;
    mInverseProjectionDirty = true;
}

void Camera::UpdateInverseProjectionMatrix() const
{
    if ( mProjectionDirty )
    {
        UpdateProjectionMatrix();
    }

    pData->mInverseProjectionMatrix = XMMatrixInverse( nullptr, pData->mProjectionMatrix );
    mInverseProjectionDirty = false;
}
