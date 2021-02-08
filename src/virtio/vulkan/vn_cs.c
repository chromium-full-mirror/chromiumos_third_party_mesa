/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vn_cs.h"

void
vn_cs_encoder_init(struct vn_cs_encoder *enc,
                   const VkAllocationCallbacks *alloc,
                   VkSystemAllocationScope alloc_scope,
                   size_t min_size)
{
   memset(enc, 0, sizeof(*enc));
   enc->allocator = alloc;
   enc->alloc_scope = alloc_scope;
   enc->min_buffer_size = min_size;
}

void
vn_cs_encoder_fini(struct vn_cs_encoder *enc)
{
   for (uint32_t i = 0; i < enc->buffer_count; i++)
      vk_free(enc->allocator, enc->buffers[i].base);
   if (enc->buffers)
      vk_free(enc->allocator, enc->buffers);
}

/**
 * Reset a cs for reuse.
 */
void
vn_cs_encoder_reset(struct vn_cs_encoder *enc)
{
   /* enc->error is sticky */

   if (unlikely(!enc->buffer_count))
      return;

   /* free all but the last buffer */
   for (uint32_t i = 0; i < enc->buffer_count - 1; i++)
      vk_free(enc->allocator, enc->buffers[i].base);

   /* move the last buffer to the beginning */
   struct vn_cs_buffer *buf = &enc->buffers[0];
   buf->base = enc->buffers[enc->buffer_count - 1].base;
   buf->size = 0;
   enc->buffer_count = 1;

   enc->total_buffer_size = 0;

   enc->cur = buf->base;
   enc->end = buf->base + enc->last_buffer_size;
}

static uint32_t
grow_array_size(uint32_t size,
                uint32_t used,
                uint32_t growth,
                uint32_t min_size)
{
   assert(size >= used && min_size);
   if (!size)
      size = min_size;

   uint32_t new_size = size;
   while (new_size - used < growth) {
      new_size *= 2;
      if (new_size < size)
         return 0;
   }
   return new_size;
}

static size_t
grow_buffer_size(size_t size, size_t used, size_t growth, size_t min_size)
{
   assert(size >= used && min_size);
   if (!size)
      size = min_size;

   size_t new_size = size;
   while (new_size - used < growth) {
      new_size *= 2;
      if (new_size < size)
         return 0;
   }
   return new_size;
}

static bool
encoder_grow_buffers(struct vn_cs_encoder *enc)
{
   const uint32_t buf_max =
      grow_array_size(enc->buffer_max, enc->buffer_count, 1, 4);
   if (!buf_max)
      return false;

   void *bufs = vk_realloc(enc->allocator, enc->buffers,
                           sizeof(*enc->buffers) * buf_max, VN_DEFAULT_ALIGN,
                           enc->alloc_scope);
   if (!bufs)
      return false;

   enc->buffers = bufs;
   enc->buffer_max = buf_max;

   return true;
}

static void
encoder_set_size(struct vn_cs_encoder *enc)
{
   if (unlikely(!enc->buffer_count))
      return;

   struct vn_cs_buffer *buf = &enc->buffers[enc->buffer_count - 1];
   assert(!buf->size && buf->base <= enc->cur);
   buf->size = enc->cur - buf->base;
   assert(buf->size <= enc->last_buffer_size);
   enc->total_buffer_size += buf->size;

   enc->end = enc->cur;
}

/**
 * Add a new vn_cs_buffer to a cs.
 */
bool
vn_cs_encoder_reserve_internal(struct vn_cs_encoder *enc, size_t size)
{
   if (enc->buffer_count >= enc->buffer_max) {
      if (!encoder_grow_buffers(enc))
         return false;
      assert(enc->buffer_count < enc->buffer_max);
   }

   const size_t buf_size =
      grow_buffer_size(enc->last_buffer_size, enc->last_buffer_size, size,
                       enc->min_buffer_size);
   if (!buf_size)
      return false;

   void *base =
      vk_alloc(enc->allocator, buf_size, VN_DEFAULT_ALIGN, enc->alloc_scope);
   if (!base)
      return false;

   encoder_set_size(enc);

   /* add a new buffer */
   struct vn_cs_buffer *buf = &enc->buffers[enc->buffer_count++];
   buf->base = base;
   buf->size = 0;
   enc->last_buffer_size = buf_size;

   /* switch to the new buffer */
   enc->cur = buf->base;
   enc->end = buf->base + enc->last_buffer_size;

   return true;
}

/*
 * End command emission.
 */
void
vn_cs_encoder_end(struct vn_cs_encoder *enc)
{
   encoder_set_size(enc);
}
