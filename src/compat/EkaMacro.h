/*
 * EkaMacro.h
 *     Contains various preprocessor utilities used in other
 * headers.
 */

#ifndef __EKA_MACRO_H__
#define __EKA_MACRO_H__

#define EKA__NUM_PARAMS(...)                               \
  _EKA__NUM_PARAMS(__VA_ARGS__ 9, 8, 7, 6, 5, 4, 3, 2, 1)

#define _EKA__NUM_PARAMS(_1, _2, _3, _4, _5, _6, _7, _8,   \
                         _9, ...)                          \
  _9

#define EKA__DELAYED_CAT(_0, _1) _EKA__DELAYED_CAT(_0, _1)
#define _EKA__DELAYED_CAT(_0, _1) _0##_1

#define EKA__DELAYED_CAT3(_0, _1, _2)                      \
  EKA__DELAYED_CAT(EKA__DELAYED_CAT(_0, _1), _2)

#define EKA__FIELD_DEF(_type, _name) _type _name;

/**
 * First parameter is a required key.
 * The second parameter is an optional value.
 */
#define EKA__ENUM_DEF(...)                                 \
  EKA__DELAYED_CAT(_EKA__ENUM_DEF_,                        \
                   EKA__NUM_PARAMS(__VA_ARGS__))           \
  (__VA_ARGS__)

#define _EKA__ENUM_DEF_1(_key) EKA__DELAYED_CAT(k, _key),
#define _EKA__ENUM_DEF_2(_key, _val)                       \
  EKA__DELAYED_CAT(k, _key) = _val,

#define EKA__VAL(str) #str
#define EKA__TOSTRING(str) EKA__VAL(str)

#endif
