#include <cstring>

#include "snackis/crypt/error.hpp"
#include "snackis/crypt/key.hpp"

namespace snackis {
namespace crypt {
  const Key null_key;
  
  Key::Key() {
    memset(data, 0, sizeof data);
  }

  Key::Key(PubKey &pub) {
    crypto_box_keypair(pub.data, data);
  }

  Key::Key(std::istream &in) {
    in.read((char *)data, sizeof data);
  }

  bool operator ==(const Key &x, const Key &y) {
    return x.data == y.data;
  }
  
  bool operator <(const Key &x, const Key &y) {
    return memcmp(x.data, y.data, crypto_box_SECRETKEYBYTES) < 0;
  }

  Data encrypt(const Key &key, const PubKey &pub_key,
	       const unsigned char *in,
	       size_t len) {
    Data out(crypto_box_NONCEBYTES+crypto_box_MACBYTES+len);
    randombytes_buf(&out[0], crypto_box_NONCEBYTES);
    
    if (crypto_box_easy(&out[crypto_box_NONCEBYTES],
			in, len,
			&out[0],
			pub_key.data, key.data) != 0) {
      ERROR(Crypt, "failed encrypting data");
    }

    return out;
  }

  Data decrypt(const Key &key, const PubKey &pub_key,
	       const unsigned char *in,
	       size_t len) {
    Data out(len-crypto_box_NONCEBYTES-crypto_box_MACBYTES);
    
    if (crypto_box_open_easy(&out[0],
			     &in[crypto_box_NONCEBYTES], len-crypto_box_NONCEBYTES,
			     &in[0],
			     pub_key.data, key.data) != 0) {
      ERROR(Crypt, "failed decrypting data");
    }

    return out;
  }

  void init_key(Key &key, PubKey &pub_key) {
    crypto_box_keypair(pub_key.data, key.data);
  }
}}
