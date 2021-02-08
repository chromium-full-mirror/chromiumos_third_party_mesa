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
   enc->min_iov_size = min_size;
}

void
vn_cs_encoder_fini(struct vn_cs_encoder *enc)
{
   for (uint32_t i = 0; i < enc->iov_count; i++)
      vk_free(enc->allocator, enc->iovs[i].iov_base);
   if (enc->iovs)
      vk_free(enc->allocator, enc->iovs);
}

/**
 * Reset a cs for reuse.
 */
void
vn_cs_encoder_reset(struct vn_cs_encoder *enc)
{
   /* enc->error is sticky */

   if (unlikely(!enc->iov_count))
      return;

   /* free all but the last iov */
   for (uint32_t i = 0; i < enc->iov_count - 1; i++)
      vk_free(enc->allocator, enc->iovs[i].iov_base);

   /* move the last iov to the beginning */
   struct vn_cs_iovec *iov = &enc->iovs[0];
   iov->iov_base = enc->iovs[enc->iov_count - 1].iov_base;
   iov->iov_len = 0;
   enc->iov_count = 1;

   enc->total_iov_len = 0;

   enc->cur = iov->iov_base;
   enc->end = iov->iov_base + enc->last_iov_size;
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
encoder_grow_iovs(struct vn_cs_encoder *enc)
{
   const uint32_t iov_max =
      grow_array_size(enc->iov_max, enc->iov_count, 1, 4);
   if (!iov_max)
      return false;

   void *iovs =
      vk_realloc(enc->allocator, enc->iovs, sizeof(*enc->iovs) * iov_max,
                 VN_DEFAULT_ALIGN, enc->alloc_scope);
   if (!iovs)
      return false;

   enc->iovs = iovs;
   enc->iov_max = iov_max;

   return true;
}

static void
encoder_set_iov_len(struct vn_cs_encoder *enc)
{
   if (unlikely(!enc->iov_count))
      return;

   struct vn_cs_iovec *iov = &enc->iovs[enc->iov_count - 1];
   assert(!iov->iov_len && iov->iov_base <= enc->cur);
   iov->iov_len = enc->cur - iov->iov_base;
   assert(iov->iov_len <= enc->last_iov_size);
   enc->total_iov_len += iov->iov_len;

   enc->end = enc->cur;
}

/**
 * Add a new iovec to a cs.
 */
bool
vn_cs_encoder_reserve_internal(struct vn_cs_encoder *enc, size_t size)
{
   if (enc->iov_count >= enc->iov_max) {
      if (!encoder_grow_iovs(enc))
         return false;
      assert(enc->iov_count < enc->iov_max);
   }

   const size_t iov_size = grow_buffer_size(
      enc->last_iov_size, enc->last_iov_size, size, enc->min_iov_size);
   if (!iov_size)
      return false;

   void *base =
      vk_alloc(enc->allocator, iov_size, VN_DEFAULT_ALIGN, enc->alloc_scope);
   if (!base)
      return false;

   encoder_set_iov_len(enc);

   /* add a new iov */
   struct vn_cs_iovec *iov = &enc->iovs[enc->iov_count++];
   iov->iov_base = base;
   iov->iov_len = 0;
   enc->last_iov_size = iov_size;

   /* switch to the new iov */
   enc->cur = iov->iov_base;
   enc->end = iov->iov_base + enc->last_iov_size;

   return true;
}

/*
 * End command emission.
 */
void
vn_cs_encoder_end(struct vn_cs_encoder *enc)
{
   encoder_set_iov_len(enc);
}
