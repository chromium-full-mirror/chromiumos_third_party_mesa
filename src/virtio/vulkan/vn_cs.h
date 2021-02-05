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

struct vn_cs {
   const VkAllocationCallbacks *allocator;
   VkSystemAllocationScope alloc_scope;

   bool error;

   struct vn_cs_in {
      size_t reserved;

      const void *cur;
      const void *end;
   } in;

   struct vn_cs_out {
      size_t min_iov_size;

      struct vn_cs_iovec *iovs;
      uint32_t iov_max;
      uint32_t iov_count;
      size_t last_iov_size;
      size_t total_iov_len;

      void *cur;
      const void *end;
   } out;
};

void
vn_cs_init(struct vn_cs *cs,
           const VkAllocationCallbacks *alloc,
           VkSystemAllocationScope alloc_scope,
           size_t out_min_size);

void
vn_cs_fini(struct vn_cs *cs);

void
vn_cs_reset(struct vn_cs *cs);

static inline void
vn_cs_set_error(struct vn_cs *cs)
{
   /* This is fatal and should be treated as VK_ERROR_DEVICE_LOST or even
    * abort().  Note that vn_cs_reset does not clear this.
    */
   cs->error = true;
}

static inline bool
vn_cs_has_error(const struct vn_cs *cs)
{
   return cs->error;
}

static inline void
vn_cs_reserve_in(struct vn_cs *cs, size_t size)
{
   cs->in.reserved += size;
}

void
vn_cs_set_in_data(struct vn_cs *cs, const void *data, size_t size);

static inline void
vn_cs_in(struct vn_cs *cs, size_t size, void *val, size_t val_size)
{
   assert(val_size <= size);

   if (unlikely(size > cs->in.end - cs->in.cur)) {
      vn_cs_set_error(cs);
      memset(val, 0, val_size);
      return;
   }

   /* we should not rely on the compiler to optimize away memcpy... */
   memcpy(val, cs->in.cur, val_size);
   cs->in.cur += size;
}

static inline void
vn_cs_in_peek(struct vn_cs *cs, void *val, size_t val_size)
{
   if (unlikely(val_size > cs->in.end - cs->in.cur)) {
      vn_cs_set_error(cs);
      memset(val, 0, val_size);
      return;
   }

   /* we should not rely on the compiler to optimize away memcpy... */
   memcpy(val, cs->in.cur, val_size);
}

static inline bool
vn_cs_has_out(const struct vn_cs *cs)
{
   return cs->out.iov_count && cs->out.cur != cs->out.iovs[0].iov_base;
}

bool
vn_cs_reserve_out_internal(struct vn_cs *cs, size_t size);

/**
 * Reserve space for commands.
 */
static inline bool
vn_cs_reserve_out(struct vn_cs *cs, size_t size)
{
   if (unlikely(size > cs->out.end - cs->out.cur)) {
      if (!vn_cs_reserve_out_internal(cs, size)) {
         vn_cs_set_error(cs);
         return false;
      }
      assert(size <= cs->out.end - cs->out.cur);
   }

   return true;
}

static inline void
vn_cs_out(struct vn_cs *cs, size_t size, const void *val, size_t val_size)
{
   assert(val_size <= size);
   assert(size <= cs->out.end - cs->out.cur);

   /* we should not rely on the compiler to optimize away memcpy... */
   memcpy(cs->out.cur, val, val_size);
   cs->out.cur += size;
}

void
vn_cs_end_out(struct vn_cs *cs);

static inline size_t
vn_cs_get_out_len(const struct vn_cs *cs)
{
   if (unlikely(!cs->out.iov_count))
      return 0;

   size_t len = cs->out.total_iov_len;
   const struct vn_cs_iovec *iov = &cs->out.iovs[cs->out.iov_count - 1];
   if (!iov->iov_len)
      len += cs->out.cur - iov->iov_base;
   return len;
}

static inline vn_object_id
vn_cs_object_load_id(const void *obj_handle)
{
   const struct vn_object *obj = *(const struct vn_object **)obj_handle;
   assert(!obj || obj->base.type != VK_OBJECT_TYPE_DEVICE);

   return obj ? obj->id : 0;
}

static inline void
vn_cs_object_store_id(void *obj_handle, vn_object_id id)
{
   struct vn_object *obj = *(struct vn_object **)obj_handle;
   assert(obj && obj->base.type != VK_OBJECT_TYPE_DEVICE);
   assert(!obj->id || obj->id == id);

   obj->id = id;
}

static inline vn_object_id
vn_cs_device_load_id(const VkDevice *dev_handle)
{
   const struct vn_device_object *dev =
      *(const struct vn_device_object **)dev_handle;
   assert(!dev || dev->base.base.type == VK_OBJECT_TYPE_DEVICE);

   return dev ? dev->id : 0;
}

static inline void
vn_cs_device_store_id(VkDevice *dev_handle, vn_object_id id)
{
   struct vn_device_object *dev = *(struct vn_device_object **)dev_handle;
   assert(dev && dev->base.base.type == VK_OBJECT_TYPE_DEVICE);
   assert(!dev->id || dev->id == id);

   dev->id = id;
}

#endif /* VN_CS_H */
