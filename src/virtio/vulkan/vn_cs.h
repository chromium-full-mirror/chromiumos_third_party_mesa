/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VN_CS_H
#define VN_CS_H

#include "vn_common.h"

struct vn_cs_buffer {
   void *base;
   size_t size;
};

struct vn_cs_encoder {
   const VkAllocationCallbacks *allocator;
   VkSystemAllocationScope alloc_scope;
   size_t min_buffer_size;

   bool fatal_error;

   struct vn_cs_buffer *buffers;
   uint32_t buffer_count;
   uint32_t buffer_max;
   size_t total_buffer_size;

   /* the current buffer is buffers[buffer_count - 1].base */
   size_t current_buffer_size;

   /* cur is the write pointer.  When cur passes end, the slow path is
    * triggered.
    */
   void *cur;
   const void *end;
};

struct vn_cs_decoder {
   const void *cur;
   const void *end;
};

void
vn_cs_encoder_init(struct vn_cs_encoder *enc,
                   const VkAllocationCallbacks *alloc,
                   VkSystemAllocationScope alloc_scope,
                   size_t min_size);

void
vn_cs_encoder_fini(struct vn_cs_encoder *enc);

void
vn_cs_encoder_reset(struct vn_cs_encoder *enc);

static inline void
vn_cs_encoder_set_fatal(struct vn_cs_encoder *enc)
{
   /* This is fatal and should be treated as VK_ERROR_DEVICE_LOST or even
    * abort().  Note that vn_cs_encoder_reset does not clear this.
    */
   enc->fatal_error = true;
}

static inline bool
vn_cs_encoder_get_fatal(const struct vn_cs_encoder *enc)
{
   return enc->fatal_error;
}

static inline bool
vn_cs_encoder_is_empty(const struct vn_cs_encoder *enc)
{
   return !enc->buffer_count || enc->cur == enc->buffers[0].base;
}

static inline size_t
vn_cs_encoder_get_len(const struct vn_cs_encoder *enc)
{
   if (unlikely(!enc->buffer_count))
      return 0;

   size_t len = enc->total_buffer_size;
   const struct vn_cs_buffer *cur_buf = &enc->buffers[enc->buffer_count - 1];
   if (!cur_buf->size)
      len += enc->cur - cur_buf->base;
   return len;
}

bool
vn_cs_encoder_reserve_internal(struct vn_cs_encoder *enc, size_t size);

/**
 * Reserve space for commands.
 */
static inline bool
vn_cs_encoder_reserve(struct vn_cs_encoder *enc, size_t size)
{
   if (unlikely(size > enc->end - enc->cur)) {
      if (!vn_cs_encoder_reserve_internal(enc, size)) {
         vn_cs_encoder_set_fatal(enc);
         return false;
      }
      assert(size <= enc->end - enc->cur);
   }

   return true;
}

static inline void
vn_cs_encoder_write(struct vn_cs_encoder *enc,
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
vn_cs_encoder_end(struct vn_cs_encoder *enc);

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
