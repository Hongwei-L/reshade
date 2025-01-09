#pragma once

#define ratio 0.75
#define WIDTH 640
#define HEIGHT int(WIDTH * ratio)
#define WIDTH_D WIDTH * 2

#define MAX_BACKBUF_COUNT	3

//Render target format, DX and reshade have its own definations
#define  RTVFormat DXGI_FORMAT_R8G8B8A8_UNORM


//更简洁的向上边界对齐算法 内存管理中常用 请记住
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))




