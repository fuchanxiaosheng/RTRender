/**
 * Compute shader to generate mipmaps for a given texture.
 * Source: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/GenerateMipsCS.hlsli
 */

#define BLOCK_SIZE 8

#define WIDTH_HEIGHT_EVEN 0     
#define WIDTH_ODD_HEIGHT_EVEN 1 
#define WIDTH_EVEN_HEIGHT_ODD 2 
#define WIDTH_HEIGHT_ODD 3      

struct ComputeShaderInput
{
    uint3 GroupID           : SV_GroupID;          
    uint3 GroupThreadID     : SV_GroupThreadID;    
    uint3 DispatchThreadID  : SV_DispatchThreadID; 
    uint  GroupIndex        : SV_GroupIndex;       
};

cbuffer GenerateMipsCB : register( b0 )
{
    uint SrcMipLevel;	
    uint NumMipLevels;	
    uint SrcDimension;  
    bool IsSRGB;        
    float2 TexelSize;	
}

Texture2D<float4> SrcMip : register( t0 );

RWTexture2D<float4> OutMip1 : register( u0 );
RWTexture2D<float4> OutMip2 : register( u1 );
RWTexture2D<float4> OutMip3 : register( u2 );
RWTexture2D<float4> OutMip4 : register( u3 );

SamplerState LinearClampSampler : register( s0 );

#define GenerateMips_RootSignature \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 6), " \
    "DescriptorTable( SRV(t0, numDescriptors = 1) )," \
    "DescriptorTable( UAV(u0, numDescriptors = 4) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"




groupshared float gs_R[64];
groupshared float gs_G[64];
groupshared float gs_B[64];
groupshared float gs_A[64];

void StoreColor( uint Index, float4 Color )
{
    gs_R[Index] = Color.r;
    gs_G[Index] = Color.g;
    gs_B[Index] = Color.b;
    gs_A[Index] = Color.a;
}

float4 LoadColor( uint Index )
{
    return float4( gs_R[Index], gs_G[Index], gs_B[Index], gs_A[Index] );
}

float3 ConvertToLinear(float3 x)
{
    return x < 0.04045f ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

float3 ConvertToSRGB( float3 x )
{
    return x < 0.0031308 ? 12.92 * x : 1.055 * pow(abs(x), 1.0 / 2.4) - 0.055;
}


float4 PackColor(float4 x)
{
    if (IsSRGB)
    {
        return float4(ConvertToSRGB(x.rgb), x.a);
    }
    else
    {
        return x;
    }
}

[RootSignature( GenerateMips_RootSignature )]
[numthreads( BLOCK_SIZE, BLOCK_SIZE, 1 )]
void main( ComputeShaderInput IN )
{
    float4 Src1 = (float4)0;

    switch ( SrcDimension )
    {
        case WIDTH_HEIGHT_EVEN:
        {
            float2 UV = TexelSize * ( IN.DispatchThreadID.xy + 0.5 );

            Src1 = SrcMip.SampleLevel( LinearClampSampler, UV, SrcMipLevel );
        }
        break;
        case WIDTH_ODD_HEIGHT_EVEN:
        {
            float2 UV1 = TexelSize * ( IN.DispatchThreadID.xy + float2( 0.25, 0.5 ) );
            float2 Off = TexelSize * float2( 0.5, 0.0 );

            Src1 = 0.5 * ( SrcMip.SampleLevel( LinearClampSampler, UV1, SrcMipLevel ) +
                           SrcMip.SampleLevel( LinearClampSampler, UV1 + Off, SrcMipLevel ) );
        }
        break;
        case WIDTH_EVEN_HEIGHT_ODD:
        {
            float2 UV1 = TexelSize * ( IN.DispatchThreadID.xy + float2( 0.5, 0.25 ) );
            float2 Off = TexelSize * float2( 0.0, 0.5 );

            Src1 = 0.5 * ( SrcMip.SampleLevel( LinearClampSampler, UV1, SrcMipLevel ) +
                           SrcMip.SampleLevel( LinearClampSampler, UV1 + Off, SrcMipLevel ) );
        }
        break;
        case WIDTH_HEIGHT_ODD:
        {
            float2 UV1 = TexelSize * ( IN.DispatchThreadID.xy + float2( 0.25, 0.25 ) );
            float2 Off = TexelSize * 0.5;

            Src1 =  SrcMip.SampleLevel( LinearClampSampler, UV1, SrcMipLevel );
            Src1 += SrcMip.SampleLevel( LinearClampSampler, UV1 + float2( Off.x, 0.0   ), SrcMipLevel );
            Src1 += SrcMip.SampleLevel( LinearClampSampler, UV1 + float2( 0.0,   Off.y ), SrcMipLevel );
            Src1 += SrcMip.SampleLevel( LinearClampSampler, UV1 + float2( Off.x, Off.y ), SrcMipLevel );
            Src1 *= 0.25;
        }
        break;
    }

    OutMip1[IN.DispatchThreadID.xy] = PackColor( Src1 );

    if ( NumMipLevels == 1 )
        return;

    StoreColor( IN.GroupIndex, Src1 );


    GroupMemoryBarrierWithGroupSync();

    if ( ( IN.GroupIndex & 0x9 ) == 0 )
    {
        float4 Src2 = LoadColor( IN.GroupIndex + 0x01 );
        float4 Src3 = LoadColor( IN.GroupIndex + 0x08 );
        float4 Src4 = LoadColor( IN.GroupIndex + 0x09 );
        Src1 = 0.25 * ( Src1 + Src2 + Src3 + Src4 );

        OutMip2[IN.DispatchThreadID.xy / 2] = PackColor( Src1 );
        StoreColor( IN.GroupIndex, Src1 );
    }

    if ( NumMipLevels == 2 )
        return;

    GroupMemoryBarrierWithGroupSync();
    if ( ( IN.GroupIndex & 0x1B ) == 0 )
    {
        float4 Src2 = LoadColor( IN.GroupIndex + 0x02 );
        float4 Src3 = LoadColor( IN.GroupIndex + 0x10 );
        float4 Src4 = LoadColor( IN.GroupIndex + 0x12 );
        Src1 = 0.25 * ( Src1 + Src2 + Src3 + Src4 );

        OutMip3[IN.DispatchThreadID.xy / 4] = PackColor( Src1 );
        StoreColor( IN.GroupIndex, Src1 );
    }

    if ( NumMipLevels == 3 )
        return;

    GroupMemoryBarrierWithGroupSync();
    if ( IN.GroupIndex == 0 )
    {
        float4 Src2 = LoadColor( IN.GroupIndex + 0x04 );
        float4 Src3 = LoadColor( IN.GroupIndex + 0x20 );
        float4 Src4 = LoadColor( IN.GroupIndex + 0x24 );
        Src1 = 0.25 * ( Src1 + Src2 + Src3 + Src4 );

        OutMip4[IN.DispatchThreadID.xy / 8] = PackColor( Src1 );
    }
}

