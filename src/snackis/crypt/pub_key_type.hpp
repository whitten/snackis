#ifndef SNACKIS_CRYPT_PUB_KEY_TYPE_HPP
#define SNACKIS_CRYPT_PUB_KEY_TYPE_HPP

#include <iostream>
#include "snackis/core/type.hpp"
#include "snackis/crypt/pub_key.hpp"

namespace snackis {
namespace crypt {
  struct PubKeyType: public Type<PubKey> {
    PubKeyType();
    PubKey read(std::istream &in) const override;
    void write(const PubKey &val, std::ostream &out) const override;
  };

  extern const PubKeyType pub_key_type;
}}

#endif