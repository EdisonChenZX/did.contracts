#include <did.admin/did.admin.hpp>
#include <did.ntoken/did.ntoken.hpp>
#include <amax.token/amax.token.hpp>
#include<math.hpp>

#include <utils.hpp>

static constexpr eosio::name active_permission{"active"_n};
static constexpr eosio::name TOKEN  { "amax.token"_n };
static constexpr eosio::name DTOKEN { "did.ntoken"_n };
static constexpr eosio::symbol AMAX { symbol(symbol_code("AMAX"), 8) };

#define REBIND(token_co, src, desc, quantity) \
    {	didtoken::rebind_action act{ token_co, { {_self, active_perm} } };\
			act.send( src, desc, quantity );}

namespace amax {

using namespace std;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ") + msg); }

   void did_admin::ontransfer(const name& from, const name& to, const asset& quant, const string& memo) {
      if (from == get_self() || to != get_self()) return;

      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );
      CHECKC( quant > asset(10000000, AMAX), err::PARAM_ERROR, "Please pay at least 0.1 AMAX in order to rebind your DID" )
      CHECKC( memo != "",        err::MEMO_FORMAT_ERROR, "empty memo!" )

      auto parts                 = split( memo, ":" );
      CHECK( parts.size() == 3,  "Expected format: 'rebind:$did_id:$account'" )
      CHECK( parts[0] == "rebind", "memo string must start with rebind" )
      auto did_id                = to_uint64( parts[1], "Not a DID ID" );
      auto dest                  = name( parts[2] );
      CHECK( is_account( dest ), "dest account does not exist" )

      auto burn_quant = asset(5000000, AMAX);   //0.05 AMAX
      TRANSFER( TOKEN, "oooo"_n, burn_quant, "did rebind" )

      auto did = nasset(did_id, 0, 1);
      REBIND( DTOKEN, from, dest, did )
   }

} //namespace amax