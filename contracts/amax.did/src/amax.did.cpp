#include <amax.did/amax.did.hpp>
#include <did.ntoken/did.ntoken.hpp>
#include <aplink.farm/aplink.farm.hpp>

#include<math.hpp>

#include <utils.hpp>

static constexpr eosio::name active_permission{"active"_n};
static constexpr symbol   APL_SYMBOL          = symbol(symbol_code("APL"), 4);

#define ALLOT_APPLE(farm_contract, lease_id, to, quantity, memo) \
    {   aplink::farm::allot_action(farm_contract, { {_self, active_perm} }).send( \
            lease_id, to, quantity, memo );}

namespace amax {

using namespace std;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("$$$") + to_string((int)code) + string("$$$ ") + msg); }


   inline int64_t get_precision(const symbol &s) {
      int64_t digit = s.precision();
      CHECK(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
      return calc_precision(digit);
   }

   void amax_did::init( const name& admin, const name& nft_contract, const nsymbol& did_token) {
      require_auth( _self );

      _gstate.nft_contract       = nft_contract;
      _gstate.admin              = admin;
      _gstate.did_token          = did_token;
   }

    void amax_did::ontransfer(const name& from, const name& to, const asset& quant, const string& memo) {
      if (from == get_self() || to != get_self()) return;

      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );
      if ( memo == "refuel" ) return;
      CHECKC( quant.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )

      auto parts                 = split( memo, ":" );
      CHECK( parts.size() == 2, "Expected format 'vendor_account:kyc_level'" );
      auto vendor_account        = name( parts[1] );
      auto kyc_level             = to_uint64( parts[2], "key_level" );

      vender_info_t::idx_t vender_infos(_self, _self.value);
      auto vender_info_idx       = vender_infos.get_index<"venderidx"_n>();
      auto vender_info_ptr        = vender_info_idx.find((uint128_t) vendor_account.value << 64 | (uint128_t)kyc_level);
      CHECKC( vender_info_ptr != vender_info_idx.end(), err::RECORD_NOT_FOUND, "vender info does not exist. ");
      CHECKC( vender_info_ptr->status == vender_info_status::RUNNING, err::STATUS_ERROR, "vender status is not runnig ");

      order_t::order_idx orders(_self, _self.value);
      auto order_idx       = orders.get_index<"makeridx"_n>();
      auto order_ptr       = order_idx.find( from.value );
      CHECKC( order_ptr != order_idx.end(), err::RECORD_NOT_FOUND, "order already exist. ");
      _gstate.last_order_idx++;

      orders.emplace(_self, [&]( auto& row ) {
         row.id               =  _gstate.last_order_idx;
         row.maker            = from;
         row.vendor_account   = vendor_account;
         row.kyc_level        = kyc_level;
         row.created_at       = time_point_sec( current_time_point() );
      });
   }

   /**
    * @brief send nasset tokens into nftone marketplace
    *
    * @param order_id
    *
    */
   void amax_did::finishdid( const uint64_t& order_id ) {
      CHECKC( has_auth(_self) || has_auth(_gstate.admin), err::NO_AUTH, "no auth for operate" )
      order_t::order_idx orders(_self, _self.value);
      auto order_ptr       = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order already exist. ");
      orders.erase(*order_ptr);
      
      auto did_quantity = nasset(1, _gstate.did_token);
      vector<nasset> quants = { did_quantity };
      TRANSFER_D( _gstate.nft_contract, order_ptr->maker, quants, "send did: " + to_string(order_id) );
   }

} //namespace amax