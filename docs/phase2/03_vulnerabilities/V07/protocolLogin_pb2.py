# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: protocolLogin.proto

import sys
_b=sys.version_info[0]<3 and (lambda x:x) or (lambda x:x.encode('latin1'))
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='protocolLogin.proto',
  package='protocol_msg',
  syntax='proto3',
  serialized_options=None,
  serialized_pb=_b('\n\x13protocolLogin.proto\x12\x0cprotocol_msg\"-\n\x08LoginMsg\x12\x0f\n\x07user_id\x18\x01 \x01(\t\x12\x10\n\x08password\x18\x02 \x01(\tb\x06proto3')
)




_LOGINMSG = _descriptor.Descriptor(
  name='LoginMsg',
  full_name='protocol_msg.LoginMsg',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='user_id', full_name='protocol_msg.LoginMsg.user_id', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='password', full_name='protocol_msg.LoginMsg.password', index=1,
      number=2, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=37,
  serialized_end=82,
)

DESCRIPTOR.message_types_by_name['LoginMsg'] = _LOGINMSG
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

LoginMsg = _reflection.GeneratedProtocolMessageType('LoginMsg', (_message.Message,), dict(
  DESCRIPTOR = _LOGINMSG,
  __module__ = 'protocolLogin_pb2'
  # @@protoc_insertion_point(class_scope:protocol_msg.LoginMsg)
  ))
_sym_db.RegisterMessage(LoginMsg)


# @@protoc_insertion_point(module_scope)
