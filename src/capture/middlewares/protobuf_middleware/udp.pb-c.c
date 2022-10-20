/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: udp.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "udp.pb-c.h"
void   udp__udp_schema__init
                     (Udp__UdpSchema         *message)
{
  static const Udp__UdpSchema init_value = UDP__UDP_SCHEMA__INIT;
  *message = init_value;
}
size_t udp__udp_schema__get_packed_size
                     (const Udp__UdpSchema *message)
{
  assert(message->base.descriptor == &udp__udp_schema__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t udp__udp_schema__pack
                     (const Udp__UdpSchema *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &udp__udp_schema__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t udp__udp_schema__pack_to_buffer
                     (const Udp__UdpSchema *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &udp__udp_schema__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
Udp__UdpSchema *
       udp__udp_schema__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (Udp__UdpSchema *)
     protobuf_c_message_unpack (&udp__udp_schema__descriptor,
                                allocator, len, data);
}
void   udp__udp_schema__free_unpacked
                     (Udp__UdpSchema *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &udp__udp_schema__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor udp__udp_schema__field_descriptors[5] =
{
  {
    "id",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(Udp__UdpSchema, id),
    NULL,
    &protobuf_c_empty_string,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "source",
    2,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(Udp__UdpSchema, source),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "dest",
    3,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(Udp__UdpSchema, dest),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "len",
    4,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(Udp__UdpSchema, len),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "check_p",
    5,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(Udp__UdpSchema, check_p),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned udp__udp_schema__field_indices_by_name[] = {
  4,   /* field[4] = check_p */
  2,   /* field[2] = dest */
  0,   /* field[0] = id */
  3,   /* field[3] = len */
  1,   /* field[1] = source */
};
static const ProtobufCIntRange udp__udp_schema__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 5 }
};
const ProtobufCMessageDescriptor udp__udp_schema__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "udp.UdpSchema",
  "UdpSchema",
  "Udp__UdpSchema",
  "udp",
  sizeof(Udp__UdpSchema),
  5,
  udp__udp_schema__field_descriptors,
  udp__udp_schema__field_indices_by_name,
  1,  udp__udp_schema__number_ranges,
  (ProtobufCMessageInit) udp__udp_schema__init,
  NULL,NULL,NULL    /* reserved[123] */
};