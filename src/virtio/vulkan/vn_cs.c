/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vn_cs.h"

static void
vn_cs_encoder_sanity_check(struct vn_cs_encoder *enc)
{
   assert(enc->buffer_count <= enc->buffer_max);

   size_t total_committed_size = 0;
   for (uint32_t i = 0; i < enc->buffer_count; i++)
      total_committed_size += enc->buffers[i].committed_size;
   assert(enc->total_committed_size == total_committed_size);

   if (enc->buffer_count) {
      const struct vn_cs_buffer *cur_buf =
         &enc->buffers[enc->buffer_count - 1];
      assert(cur_buf->base <= enc->cur && enc->cur <= enc->end &&
             enc->end <= cur_buf->base + enc->current_buffer_size);
      if (cur_buf->committed_size)
         assert(enc->cur == enc->end);
   } else {
      assert(!enc->current_buffer_size);
      assert(!enc->cur && !enc->end);
   }
}

static void
vn_cs_encoder_add_buffer(struct vn_cs_encoder *enc, void *base, size_t size)
{
   /* add a buffer and make it current */
   assert(enc->buffer_count < enc->buffer_max);
   struct vn_cs_buffer *cur_buf = &enc->buffers[enc->buffer_count++];
   cur_buf->base = base;
   cur_buf->committed_size = 0;
   enc->current_buffer_size = size;

   /* update the write pointer */
   enc->cur = base;
   enc->end = base + size;
}

static void
vn_cs_encoder_commit_buffer(struct vn_cs_encoder *enc)
{
   assert(enc->buffer_count);
   struct vn_cs_buffer *cur_buf = &enc->buffers[enc->buffer_count - 1];
   const size_t written_size = enc->cur - cur_buf->base;
   if (cur_buf->committed_size) {
      assert(cur_buf->committed_size == written_size);
   } else {
      cur_buf->committed_size = written_size;
      enc->total_committed_size += written_size;
   }
}

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

   /* free all but the current buffer */
   struct vn_cs_buffer *cur_buf = &enc->buffers[enc->buffer_count - 1];
   for (uint32_t i = 0; i < enc->buffer_count - 1; i++)
      vk_free(enc->allocator, enc->buffers[i].base);

   /* move the current buffer to the beginning */
   enc->buffer_count = 0;
   vn_cs_encoder_add_buffer(enc, cur_buf->base, enc->current_buffer_size);

   enc->total_committed_size = 0;
}

static uint32_t
next_array_size(uint32_t cur_size, uint32_t min_size)
{
   const uint32_t next_size = cur_size ? cur_size * 2 : min_size;
   return next_size > cur_size ? next_size : 0;
}

static size_t
next_buffer_size(size_t cur_size, size_t min_size, size_t need)
{
   size_t next_size = cur_size ? cur_size * 2 : min_size;
   while (next_size < need) {
      next_size *= 2;
      if (!next_size)
         return 0;
   }
   return next_size;
}

static bool
vn_cs_encoder_grow_buffer_array(struct vn_cs_encoder *enc)
{
   const uint32_t buf_max = next_array_size(enc->buffer_max, 4);
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

/**
 * Add a new vn_cs_buffer to a cs.
 */
bool
vn_cs_encoder_reserve_internal(struct vn_cs_encoder *enc, size_t size)
{
   if (enc->buffer_count >= enc->buffer_max) {
      if (!vn_cs_encoder_grow_buffer_array(enc))
         return false;
      assert(enc->buffer_count < enc->buffer_max);
   }

   const size_t buf_size =
      next_buffer_size(enc->current_buffer_size, enc->min_buffer_size, size);
   if (!buf_size)
      return false;

   void *base =
      vk_alloc(enc->allocator, buf_size, VN_DEFAULT_ALIGN, enc->alloc_scope);
   if (!base)
      return false;

   if (likely(enc->buffer_count))
      vn_cs_encoder_commit_buffer(enc);

   vn_cs_encoder_add_buffer(enc, base, buf_size);

   vn_cs_encoder_sanity_check(enc);

   return true;
}

/*
 * Commit written data.
 */
void
vn_cs_encoder_commit(struct vn_cs_encoder *enc)
{
   if (likely(enc->buffer_count)) {
      vn_cs_encoder_commit_buffer(enc);

      /* trigger the slow path on next vn_cs_encoder_reserve */
      enc->end = enc->cur;
   }

   vn_cs_encoder_sanity_check(enc);
}
