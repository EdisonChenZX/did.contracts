#include <amax.did/amax.did.hpp>
#include <did.ntoken/did.ntoken.hpp>
#include <aplink.farm/aplink.farm.hpp>

#include<math.hpp>

#include <utils.hpp>

static constexpr eosio::name active_permission{"active"_n};
static constexpr symbol   APL_SYMBOL          = symbol(symbol_code("APL"), 4);
static constexpr eosio::name MT_BANK{"amax.token"_n};

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

   void amax_did::init( const name& admin, const name& nft_contract, const name& fee_collector) {
      require_auth( _self );

      CHECKC( is_account( admin ), err::PARAM_ERROR, "admin account does not exist");
      CHECKC( is_account( fee_collector ), err::PARAM_ERROR, "fee_collector account does not exist");

      _gstate.nft_contract       = nft_contract;
      _gstate.admin              = admin;
      _gstate.fee_collector      = fee_collector;
   }

    void amax_did::ontransfer(const name& from, const name& to, const asset& quant, const string& memo) {

      if (from == get_self() || to != get_self()) return;

      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );
      if ( memo == "refuel" ) return;
      CHECKC( quant.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )

      auto parts                 = split( memo, ":" );
      CHECK( parts.size() == 3, "Expected format 'vendor_account:kyc_level:md5'" );
      auto vendor_account        = name( parts[0] );
      auto kyc_level             = to_uint64( parts[1], "key_level" );

      vendor_info_t::idx_t vendor_infos(_self, _self.value);
      auto vendor_info_idx      = vendor_infos.get_index<"vendoridx"_n>();
      auto vendor_info_ptr      = vendor_info_idx.find(((uint128_t) vendor_account.value << 64) + kyc_level);
      CHECKC( vendor_info_ptr != vendor_info_idx.end(), err::RECORD_NOT_FOUND, "vendor info does not exist. ");
      CHECKC( vendor_info_ptr->status == vendor_info_status::RUNNING, err::STATUS_ERROR, "vendor status is not runnig ");
      CHECKC( vendor_info_ptr->user_charge_amount == quant, err::PARAM_ERROR, "transfer amount error");

      order_t::order_idx orders(_self, _self.value);
      auto order_idx       = orders.get_index<"makeridx"_n>();
      auto order_ptr       = order_idx.find( from.value );
      CHECKC( order_ptr == order_idx.end(), err::RECORD_EXISTING, "order already exist. ");
      _gstate.last_order_idx ++;

      orders.emplace(_self, [&]( auto& row ) {
         row.id               =  _gstate.last_order_idx;
         row.maker            = from;
         row.vendor_account   = vendor_account;
         row.kyc_level        = kyc_level;
         row.secret_md5       = parts[2];
         row.created_at       = time_point_sec( current_time_point() );
      });
   }

   /**
    * @brief send nasset tokens into nftone marketplace
    *
    * @param order_id
    *
    */
   void amax_did::finishdid( const uint64_t& order_id, const string& msg ) {
      CHECKC( has_auth(_self) || has_auth(_gstate.admin), err::NO_AUTH, "no auth for operate" )
      order_t::order_idx orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not exist. ");

      vendor_info_t::idx_t vendor_infos(_self, _self.value);
      auto vendor_info_idx       = vendor_infos.get_index<"vendoridx"_n>();
      auto vendor_info_ptr      = vendor_info_idx.find(((uint128_t) order_ptr->vendor_account.value << 64) + order_ptr->kyc_level);

      CHECKC( vendor_info_ptr != vendor_info_idx.end(), err::RECORD_EXISTING, "vendor info already not exist. ");

      auto did_quantity = nasset(1, vendor_info_ptr->nft_id);
      vector<nasset> quants = { did_quantity };
      TRANSFER_D( _gstate.nft_contract, order_ptr->maker, quants, "send did: " + to_string(order_id) );

      // TRANSFER(MT_BANK, vendor_info_ptr->vendor_account, vendor_info_ptr->vendor_charge_quant, to_string(order_id));

      if( vendor_info_ptr->user_charge_amount.amount > 0  ) {
         TRANSFER(MT_BANK, _gstate.fee_collector, vendor_info_ptr->user_charge_amount, to_string(order_id));
         _reward_farmer(vendor_info_ptr->user_reward_quant, order_ptr->maker);
      }

      _on_audit_log(
                  order_ptr->id,
                  order_ptr->maker,
                  vendor_info_ptr->vendor_name,
                  order_ptr->vendor_account,
                  order_ptr->kyc_level,
                  vendor_info_ptr->user_charge_amount,
                  "successed"_n,
                  msg,
                  current_time_point()
                  );
      orders.erase(*order_ptr);
   }

    void amax_did::faildid( const uint64_t& order_id, const string& reason) {
      CHECKC( has_auth(_self) || has_auth(_gstate.admin), err::NO_AUTH, "no auth for operate" )
      order_t::order_idx orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not exist. ");

      vendor_info_t::idx_t vendor_infos(_self, _self.value);
      auto vendor_info_idx       = vendor_infos.get_index<"vendoridx"_n>();
      auto vendor_info_ptr      = vendor_info_idx.find(((uint128_t) order_ptr->vendor_account.value << 64) + order_ptr->kyc_level);

      CHECKC( vendor_info_ptr != vendor_info_idx.end(), err::RECORD_EXISTING, "vendor info already not exist. ");

      // TRANSFER(MT_BANK, vendor_info_ptr->vendor_account, vendor_info_ptr->vendor_charge_quant, to_string(order_id));
      if( vendor_info_ptr->user_charge_amount.amount > 0 ) {
         TRANSFER(MT_BANK, _gstate.fee_collector, vendor_info_ptr->user_charge_amount, to_string(order_id));
      }
      _on_audit_log(
                  order_ptr->id,
                  order_ptr->maker,
                  vendor_info_ptr->vendor_name,
                  order_ptr->vendor_account,
                  order_ptr->kyc_level,
                  vendor_info_ptr->user_charge_amount,
                  "failed"_n,
                  reason,
                  current_time_point()
                  );
      orders.erase(*order_ptr);
   }

   void amax_did::addvendor(const string& vendor_name, const name& vendor_account,
                        uint32_t& kyc_level,
                        const asset& user_reward_quant, 
                        const asset& user_charge_amount,
                        const nsymbol& nft_id ) {

      CHECKC( has_auth(_self) || has_auth(_gstate.admin), err::NO_AUTH, "no auth for operate" )
      CHECKC( user_reward_quant.amount > 0, err::PARAM_ERROR, "user_reward_quant amount inpostive");
      CHECKC( user_charge_amount.amount > 0, err::PARAM_ERROR, "user_charge_amount amount does not exist");
      

      vendor_info_t::idx_t vendor_infos(_self, _self.value);
      auto vendor_info_idx    = vendor_infos.get_index<"vendoridx"_n>();
      auto vendor_info_ptr    = vendor_info_idx.find((uint128_t) vendor_account.value << 64 | (uint128_t)kyc_level);
      CHECKC( vendor_info_ptr == vendor_info_idx.end(), err::RECORD_EXISTING, "vendor info already not exist. ");

      auto now                   = current_time_point();
      _gstate.last_vendor_id ++;
      auto last_vendor_id         = _gstate.last_vendor_id;
      vendor_infos.emplace( _self, [&]( auto& row ) {
         row.id 					   = last_vendor_id;
         row.vendor_name 		   = vendor_name;
         row.vendor_account      = vendor_account;
         row.kyc_level           = kyc_level;
         row.user_reward_quant 	= user_reward_quant;
         row.user_charge_amount  = user_charge_amount;
         row.nft_id              = nft_id;
         row.status			      = vendor_info_status::RUNNING;
         row.created_at          = now;
         row.updated_at          = now;
      });   
   }

   void amax_did::chgvendor(const uint64_t& vendor_id, const name& status) {

      CHECKC( has_auth(_self) || has_auth(_gstate.admin), err::NO_AUTH, "no auth for operate" )

      vendor_info_t::idx_t vendor_infos(_self, _self.value);
      auto vender_itr = vendor_infos.find( vendor_id );
      CHECKC( vender_itr != vendor_infos.end(), err::RECORD_NOT_FOUND, "vender not found: " + to_string(vendor_id) );
      CHECKC( vender_itr->status != status, err::STATUS_ERROR, "vender status already equal: " + to_string(vendor_id) );
      
      vendor_infos.modify( vender_itr, _self, [&]( auto& row ) {
         row.status           = status;
         row.updated_at       = time_point_sec( current_time_point() );
      });

   }

   void amax_did::_reward_farmer( const asset& reward_quant, const name& farmer ) {
      auto apples = asset(0, APLINK_SYMBOL);
      aplink::farm::available_apples( _gstate.apl_farm.contract, _gstate.apl_farm.lease_id, apples );
      if (apples.amount == 0) return;

      ALLOT_APPLE( _gstate.apl_farm.contract, _gstate.apl_farm.lease_id, farmer, reward_quant, "xin reward" )
   }

   void amax_did::auditlog( 
                     const uint64_t& order_id,
                     const name& taker,
                     const string& vendor_name,
                     const name& vendor_account,
                     uint32_t& kyc_level,
                     const asset& vendor_charge_quant,
                     const name& status,
                     const string& msg,
                     const time_point&   created_at) {
      require_auth(get_self());
      require_recipient(taker);

    }

    void amax_did::_on_audit_log(
                     const uint64_t& order_id,
                     const name& maker,
                     const string& vendor_name,
                     const name& vendor_account,
                     const uint32_t& kyc_level,
                     const asset& user_charge_amount,
                     const name& status,
                     const string& msg,
                     const time_point&   created_at
      ) {
            amax_did::auditlog_action act{ _self, { {_self, active_permission} } };
            act.send(order_id, maker, vendor_name, vendor_account, kyc_level, user_charge_amount, status, msg, created_at   );
      }

} //namespace amax