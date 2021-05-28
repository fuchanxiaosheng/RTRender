#pragma once

#include <DirectXMath.h>

struct PointLight
{
    PointLight(): PositionWS( 0.0f, 0.0f, 0.0f, 1.0f ), PositionVS( 0.0f, 0.0f, 0.0f, 1.0f ), Color( 1.0f, 1.0f, 1.0f, 1.0f ), Intensity( 1.0f ), Attenuation( 0.0f ) {}

    DirectX::XMFLOAT4 PositionWS; 
    DirectX::XMFLOAT4 PositionVS; 
    DirectX::XMFLOAT4 Color;
    float Intensity;
    float Attenuation;
    float Padding[2];            
};

struct SpotLight
{
    SpotLight() 
		: PositionWS( 0.0f, 0.0f, 0.0f, 1.0f ), PositionVS( 0.0f, 0.0f, 0.0f, 1.0f ), DirectionWS( 0.0f, 0.0f, 1.0f, 0.0f ), DirectionVS( 0.0f, 0.0f, 1.0f, 0.0f )
        , Color( 1.0f, 1.0f, 1.0f, 1.0f ), Intensity( 1.0f ), SpotAngle( DirectX::XM_PIDIV2 ), Attenuation( 0.0f ) {}

    DirectX::XMFLOAT4 PositionWS;
    DirectX::XMFLOAT4 PositionVS; 
    DirectX::XMFLOAT4 DirectionWS; 
    DirectX::XMFLOAT4 DirectionVS; 
    DirectX::XMFLOAT4 Color;
    float Intensity;
    float SpotAngle;
    float Attenuation;
    float Padding;               
};

struct DirectionalLight
{
    DirectionalLight() : DirectionWS( 0.0f, 0.0f, 1.0f, 0.0f ), DirectionVS( 0.0f, 0.0f, 1.0f, 0.0f ), Color( 1.0f, 1.0f, 1.0f, 1.0f ) {}
    DirectX::XMFLOAT4 DirectionWS; 
    DirectX::XMFLOAT4 DirectionVS; 
    DirectX::XMFLOAT4 Color;
};
