#pragma once

#define ratio 0.75
#define WIDTH 640
#define HEIGHT int(WIDTH * ratio)
#define WIDTH_D WIDTH * 2

#define MAX_BACKBUF_COUNT	3

//Render target format, DX and reshade have its own definations
#define  RTVFormat DXGI_FORMAT_R8G8B8A8_UNORM


//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))




