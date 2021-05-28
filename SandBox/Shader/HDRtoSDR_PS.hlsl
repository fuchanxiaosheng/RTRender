#define TM_Linear     0
#define TM_Reinhard   1
#define TM_ReinhardSq 2
#define TM_ACESFilmic 3

struct TonemapParameters
{
    uint TonemapMethod;
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

ConstantBuffer<TonemapParameters> TonemapParametersCB : register( b0 );

float3 Linear( float3 HDR, float max )
{
    float3 SDR = HDR;
    if ( max > 0 )
    {
        SDR = saturate( HDR / max );
    }

    return SDR;
}
float3 Reinhard( float3 HDR, float k )
{
    return HDR / ( HDR + k );
}
float3 ReinhardSqr( float3 HDR, float k )
{
    return pow( Reinhard( HDR, k ), 2 );
}

float3 ACESFilmic( float3 x, float A, float B, float C, float D, float E, float F )
{
    return ( ( x * ( A * x + C * B ) + D * E ) / ( x * ( A * x + B ) + D * F ) ) - ( E / F );
}

Texture2D<float3> HDRTexture : register( t0 );
SamplerState LinearClampSampler : register(s0);

float4 main( float2 TexCoord : TEXCOORD ) : SV_Target0
{
    float3 HDR = HDRTexture.SampleLevel(LinearClampSampler, TexCoord, 0);

    HDR *= exp2( TonemapParametersCB.Exposure );
    float3 SDR = ( float3 )0;
    switch ( TonemapParametersCB.TonemapMethod )
    {
    case TM_Linear:
        SDR = Linear( HDR, TonemapParametersCB.MaxLuminance );
        break;
    case TM_Reinhard:
        SDR = Reinhard( HDR, TonemapParametersCB.K );
        break;
    case TM_ReinhardSq:
        SDR = ReinhardSqr( HDR, TonemapParametersCB.K );
        break;
    case TM_ACESFilmic:
        SDR = ACESFilmic( HDR, TonemapParametersCB.A, TonemapParametersCB.B, TonemapParametersCB.C, TonemapParametersCB.D, TonemapParametersCB.E, TonemapParametersCB.F ) /
              ACESFilmic(TonemapParametersCB.LinearWhite, TonemapParametersCB.A, TonemapParametersCB.B, TonemapParametersCB.C, TonemapParametersCB.D, TonemapParametersCB.E, TonemapParametersCB.F );
        break;
    }


    return float4( pow( abs(SDR), 1.0f / TonemapParametersCB.Gamma), 1 );
}