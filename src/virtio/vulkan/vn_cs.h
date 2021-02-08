/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VN_CS_H
#define VN_CS_H

#include "vn_common.h"

struct vn_cs_iovec {
   void *iov_base;
   size_t iov_len;
};

struct vn_cs_encoder {
   const VkAllocationCallbacks *allocator;
   VkSystemAllocationScope alloc_scope;
   size_t min_iov_size;

   bool error;

   struct vn_cs_iovec *iovs;
   uint32_t iov_max;
   uint32_t iov_count;
   size_t last_iov_size;
   size_t total_iov_len;

   void *cur;
   const void *end;
};

struct vn_cs_decoder {
   const void *cur;
   const void *end;
};

void
vn_cs_init(struct vn_cs_encoder *enc,
           const VkAllocationCallbacks *alloc,
           VkSystemAllocationScope alloc_scope,
           size_t min_size);

void
vn_cs_fini(struct vn_cs_encoder *enc);

void
vn_cs_reset(struct vn_cs_encoder *enc);

static inline void
vn_cs_set_error(struct vn_cs_encoder *enc)
{
   /* This is fatal and should be treated as VK_ERROR_DEVICE_LOST or even
    * abort().  Note that vn_cs_reset does not clear this.
    */
   enc->error = true;
}

static inline bool
vn_cs_has_error(const struct vn_cs_encoder *enc)
{
   return enc->error;
}

static inline bool
vn_cs_has_out(const struct vn_cs_encoder *enc)
{
   return enc->iov_count && enc->cur != enc->iovs[0].iov_base;
}

bool
vn_cs_reserve_out_internal(struct vn_cs_encoder *enc, size_t size);

/**
 * Reserve space for commands.
 */
static inline bool
vn_cs_reserve_out(struct vn_cs_encoder *enc, size_t size)
{
   if (unlikely(size > enc->end - enc->cur)) {
      if (!vn_cs_reserve_out_internal(enc, size)) {
         vn_cs_set_error(enc);
         return false;
      }
      assert(size <= enc->end - enc->cur);
   }

   return true;
}

static inline void
vn_cs_out(struct vn_cs_encoder *enc,
          size_t size,
          const void *val,
          size_t val_size)
{
   assert(val_size <= size);
   assert(size <= enc->end - enc->cur);

   /* we should not rely on the compiler to optimize away memcpy... */
   memcpy(enc->cur, val, val_size);
   enc->cur += size;
}

void
vn_cs_end_out(struct vn_cs_encoder *enc);

static inline size_t
vn_cs_get_out_len(const struct vn_cs_encoder *enc)
{
   if (unlikely(!enc->iov_count))
      return 0;

   size_t len = enc->total_iov_len;
   const struct vn_cs_iovec *iov = &enc->iovs[enc->iov_count - 1];
   if (!iov->iov_len)
      len += enc->cur - iov->iov_base;
   return len;
}

static inline void
vn_cs_decoder_init(struct vn_cs_decoder *dec, const void *data, size_t size)
{
   dec->cur = data;
   dec->end = data + size;
}

static inline void
vn_cs_decoder_set_fatal(struct vn_cs_decoder *dec)
{
   abort();
}

static inline void
vn_cs_decoder_read(struct vn_cs_decoder *dec,
                   size_t size,
                   void *val,
                   size_t val_size)
{
   assert(val_size <= size);

   if (unlikely(size > dec->end - dec->cur)) {
      vn_cs_decoder_set_fatal(dec);
      memset(val, 0, val_size);
      return;
   }

   /* we should not rely on the compiler to optimize away memcpy... */
   memcpy(val, dec->cur, val_size);
   dec->cur += size;
}

static inline void
vn_cs_decoder_peek(struct vn_cs_decoder *dec, void *val, size_t val_size)
{
   if (unlikely(val_size > dec->end - dec->cur)) {
      vn_cs_decoder_set_fatal(dec);
      memset(val, 0, val_size);
      return;
   }

   /* we should not rely on the compiler to optimize away memcpy... */
   memcpy(val, dec->cur, val_size);
}

static inline vn_object_id
vn_cs_object_load_id(const void *obj_handle)
{
   const struct vn_object *obj =
      vn_object_from_handle(*(const void **)obj_handle);
   return obj ? obj->id : 0;
}

static inline void
vn_cs_object_store_id(void *obj_handle, vn_object_id id)
{
   struct vn_object *obj = vn_object_from_handle(*(void **)obj_handle);
   assert(obj && (!obj->id || obj->id == id));
   obj->id = id;
}

static inline vn_object_id
vn_cs_device_load_id(const VkDevice *dev_handle)
{
   const struct vn_device_object *dev =
      vn_device_object_from_handle(*dev_handle);
   return dev ? dev->id : 0;
}

static inline void
vn_cs_device_store_id(VkDevice *dev_handle, vn_object_id id)
{
   struct vn_device_object *dev = vn_device_object_from_handle(*dev_handle);
   assert(dev && (!dev->id || dev->id == id));
   dev->id = id;
}

#endif /* VN_CS_H */
