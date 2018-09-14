#include <gfd.h>
#include <gx2/draw.h>
#include <gx2/shaders.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2r/draw.h>
#include <gx2r/buffer.h>
#include <string.h>
#include <stdio.h>
#include <whb/file.h>
#include <whb/proc.h>
#include <whb/sdcard.h>
#include <whb/gfx.h>
#include <whb/log.h>
#include <whb/log_udp.h>

#include <coreinit/memdefaultheap.h>
#include <coreinit/systeminfo.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include <stdlib.h>

static const float position_vb[] =
{
   -1.0f, -1.0f,
    1.0f, -1.0f,
    1.0f,  1.0f,
   -1.0f,  1.0f,
};

static const float tex_coord_vb[] =
{
   0.0f, 1.0f,
   1.0f, 1.0f,
   1.0f, 0.0f,
   0.0f, 0.0f,
};

GX2SamplerVar psampler = { "s", GX2_SAMPLER_VAR_TYPE_SAMPLER_2D, 0 };
GX2Texture texture = {0};

uint32_t* create_mandelbrot(int width, int height)
{
   int i, x, y;

   uint16_t max = 0x20;
   float x0 = 0.36;
   float x1 = 0.60;
   float y0 = 0.36;
   float y1 = 0.80;

   int full_w = width / (x1 - x0);
   int full_h = height / (y1 - y0);

   int off_x = width * x0;
   int off_y = height * y0;

   uint32_t* data = (uint32_t*)malloc(width * height * sizeof(uint32_t));
   uint16_t* data_cnt = (uint16_t*)malloc(full_w * full_h * sizeof(uint16_t));
   uint32_t palette [max];
   memset(palette, 0, sizeof(palette));

   for (i = 0; i < full_w * full_h; i++)
   {
      float real  = ((i % full_w) - (full_w / 2.0)) * (4.0 / full_w);
      float im    = ((i / full_w) - (full_h / 2.0)) * (4.0 / full_w);
      float xx    = 0;
      float yy    = 0;
      int counter = 0;

      while (xx * xx + yy * yy <= 4 && counter < max)
      {
         float tmp = xx * xx - yy * yy + real;
         yy = 2 * xx * yy + im;
         xx = tmp;
         counter++;
      }

      data_cnt[i] = counter;
   }

   for (i = 0; i < full_w * full_h; i++)
      palette[data_cnt[i] - 1]++;

   for (i = 1; i < max; i++)
      palette[i] += palette[i - 1];

   for (y = 0; y < height; y++)
      for (x = 0; x < width; x++)
         data[x + y * width] = max * palette[data_cnt[x + off_x + (y + off_y) * full_w] - 1] | 0xff;


   free(data_cnt);
   return data;
}

int main(int argc, char **argv)
{
   GX2RBuffer position_buffer = { 0 };
   GX2RBuffer tex_coord_buffer = { 0 };
   WHBGfxShaderGroup group = { 0 };
   void *buffer = NULL;
   char *gshFileData = NULL;
   char *sdRootPath = NULL;
   char path[256];
   int result = 0;

   WHBLogUdpInit();
   WHBProcInit();
   WHBGfxInit();

   if (!WHBMountSdCard()) {
      result = -1;
      goto exit;
   }

   sdRootPath = WHBGetSdCardMountPath();
   sprintf(path, "%s/wut/content/texture_shader.gsh", sdRootPath);

   gshFileData = WHBReadWholeFile(path, NULL);
   if (!gshFileData) {
      result = -1;
      WHBLogPrintf("WHBReadWholeFile(%s) returned NULL", path);
      goto exit;
   }

   if (!WHBGfxLoadGFDShaderGroup(&group, 0, gshFileData)) {
      result = -1;
      WHBLogPrintf("WHBGfxLoadGFDShaderGroup returned FALSE");
      goto exit;
   }

   WHBGfxInitShaderAttribute(&group, "position", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
   WHBGfxInitShaderAttribute(&group, "tex_coord_in", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
   WHBGfxInitFetchShader(&group);

   WHBFreeWholeFile(gshFileData);
   gshFileData = NULL;

   /* set up Attribute Buffers */
   

   // Set vertex position
   position_buffer.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER |
                           GX2R_RESOURCE_USAGE_CPU_READ |
                           GX2R_RESOURCE_USAGE_CPU_WRITE |
                           GX2R_RESOURCE_USAGE_GPU_READ;
   position_buffer.elemSize = 2 * 4;
   position_buffer.elemCount = 4;
   GX2RCreateBuffer(&position_buffer);
   buffer = GX2RLockBufferEx(&position_buffer, 0);
   memcpy(buffer, position_vb, position_buffer.elemSize * position_buffer.elemCount);
   GX2RUnlockBufferEx(&position_buffer, 0);

   // Set vertex colour
   tex_coord_buffer.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER |
                            GX2R_RESOURCE_USAGE_CPU_READ |
                            GX2R_RESOURCE_USAGE_CPU_WRITE |
                            GX2R_RESOURCE_USAGE_GPU_READ;
   tex_coord_buffer.elemSize = 2 * 4;
   tex_coord_buffer.elemCount = 4;
   GX2RCreateBuffer(&tex_coord_buffer);
   buffer = GX2RLockBufferEx(&tex_coord_buffer, 0);
   memcpy(buffer, tex_coord_vb, tex_coord_buffer.elemSize * tex_coord_buffer.elemCount);
   GX2RUnlockBufferEx(&tex_coord_buffer, 0);

   /* create a texture */
   int width = 800;
   int height = 480;
   texture.surface.width    = width;
   texture.surface.height   = height;
   texture.surface.depth    = 1;
   texture.surface.dim      = GX2_SURFACE_DIM_TEXTURE_2D;
   texture.surface.format   = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
   texture.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
   texture.viewNumSlices    = 1;
   texture.compMap          = 0x00010203;
   GX2CalcSurfaceSizeAndAlignment(&texture.surface);
   GX2InitTextureRegs(&texture);

   texture.surface.image = MEMAllocFromDefaultHeapEx(texture.surface.imageSize, texture.surface.alignment);

   /* create an image so we have something to display */   
   uint32_t* data = create_mandelbrot(width, height);
   uint32_t* dst = (uint32_t*)texture.surface.image;
   uint32_t* src = (uint32_t*)data;

   for (int i = 0; i < texture.surface.height; i++)
   {
      memcpy(dst, src, width * sizeof(uint32_t));
      dst += texture.surface.pitch;
      src += width;
   }
   free(data);

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, texture.surface.image, texture.surface.imageSize);

   /* create a sampler */
   group.pixelShader->samplerVarCount = 1;
   group.pixelShader->samplerVars = &psampler;

   GX2Sampler sampler;
   GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
   

   WHBLogPrintf("Begin rendering...");
   while (WHBProcIsRunning()) {
      // Render!
      WHBGfxBeginRender();

      WHBGfxBeginRenderTV();
      WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      GX2SetFetchShader(&group.fetchShader);
      GX2SetVertexShader(group.vertexShader);
      GX2SetPixelShader(group.pixelShader);
      GX2RSetAttributeBuffer(&position_buffer, 0, position_buffer.elemSize, 0);
      GX2RSetAttributeBuffer(&tex_coord_buffer, 1, tex_coord_buffer.elemSize, 0);

      GX2SetPixelTexture(&texture, psampler.location);
      GX2SetPixelSampler(&sampler, psampler.location);

      GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
      WHBGfxFinishRenderTV();

      WHBGfxBeginRenderDRC();
      WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      GX2SetFetchShader(&group.fetchShader);
      GX2SetVertexShader(group.vertexShader);
      GX2SetPixelShader(group.pixelShader);
      GX2RSetAttributeBuffer(&position_buffer, 0, position_buffer.elemSize, 0);
      GX2RSetAttributeBuffer(&tex_coord_buffer, 1, tex_coord_buffer.elemSize, 0);

      GX2SetPixelTexture(&texture, psampler.location);
      GX2SetPixelSampler(&sampler, psampler.location);

      GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
      WHBGfxFinishRenderDRC();

      WHBGfxFinishRender();
   }

   MEMFreeToDefaultHeap(texture.surface.image);

exit:
   WHBLogPrintf("Exiting...");
   GX2RDestroyBufferEx(&position_buffer, 0);
   GX2RDestroyBufferEx(&tex_coord_buffer, 0);
   WHBUnmountSdCard();
   WHBGfxShutdown();
   WHBProcShutdown();
   WHBLogUdpDeinit();
   return result;
}
