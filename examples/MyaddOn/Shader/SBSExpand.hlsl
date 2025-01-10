struct PSInput
{
    float4 m_v4Position : SV_POSITION;
    float4 m_v4Color : COLOR;
    float2 m_v2UV : TEXCOORD;
};

cbuffer MVOBuffer : register(b0)
{
    float4x4 m_MVO;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 v4Position : POSITION, float4 v4Color : COLOR, float2 v2UV : TEXCOORD)
{
    PSInput result;

    //result.m_v4Position = mul(v4Position, m_MVO);
	result.m_v4Position = v4Position;
    result.m_v4Color = v4Color;
    result.m_v2UV = v2UV;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float2 uv;
    uv = input.m_v2UV; // * float2(0.8, 0.8);
    
    if (uv.x < 0.5)
    {
        
        uv.x = uv.x  * 2;
        
        float4 color = g_texture.Sample(g_sampler, uv);
        color = color * float4(0.9, 0.4, 0.1,1);    //ºì×Ø
        return color;
    }
     
    else
    {
        uv.x = (uv.x - 0.5) * 2;
        float4 color = g_texture.Sample(g_sampler, uv);
        color = color * float4(0.1, 0.8, 0.2,1);    //Æ«ÂÌ
        return color;
    }     
    
    
}
