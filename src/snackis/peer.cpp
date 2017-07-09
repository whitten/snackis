#include "snackis/ctx.hpp"
#include "snackis/peer.hpp"

namespace snackis {
  Peer::Peer(Ctx &ctx): IdRec(ctx), created_at(now()), changed_at(created_at)
  { }

  Peer::Peer(Ctx &ctx, const db::Rec<Peer> &src): IdRec(ctx, null_uid) {
    copy(*this, src);
  }
  
  Peer::Peer(const Msg &msg):
    IdRec(msg.ctx, msg.from_id)
  {
    Ctx &ctx(msg.ctx);
    bool exists = load(ctx.db.peers, *this) ? true : false;    

    if (!exists) {
      created_at = now();
      changed_at = created_at;
      name = msg.peer_name;
    }
    
    email = msg.from;
    crypt_key = msg.crypt_key;
  }

  opt<Peer> find_peer_id(Ctx &ctx, const UId &id) {
    db::Rec<Peer> rec;
    set(rec, ctx.db.peer_id, id);
    if (!load(ctx.db.peers, rec)) { return nullopt; }
    return Peer(ctx, rec);
  }

  Peer get_peer_id(Ctx &ctx, const UId &id) {
    auto found(find_peer_id(ctx, id));
    CHECK(found, _);
    return *found;
  }

  void encrypt(const Peer &peer, const Path &in, const Path &out, bool encode) {
    Data in_buf(slurp_data(in));
    Data out_buf(crypt::encrypt(*get_val(peer.ctx.settings.crypt_key),
				peer.crypt_key,
				&in_buf[0], in_buf.size()));
    
    if (encode) {
      const str hex(bin_hex(&out_buf[0], out_buf.size()));
      out_buf.assign(hex.begin(), hex.end());
    }

    dump_data(out_buf, out);
  }

  void decrypt(const Peer &peer, const Path &in, const Path &out, bool decode) {
    Data in_buf(slurp_data(in));

    if (decode) {
      const str hex(in_buf.begin(), in_buf.end());
      in_buf = hex_bin(hex);
    }

    Data out_buf(crypt::decrypt(*get_val(peer.ctx.settings.crypt_key),
				peer.crypt_key,
				&in_buf[0], in_buf.size()));
    
    dump_data(out_buf, out);
  }
}
