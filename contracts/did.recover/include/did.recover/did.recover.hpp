#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>

#include <string>
#include <did.recover/did.recover.db.hpp>
#include <wasm_db.hpp>
#include <utils.hpp>

namespace amax {

using std::string;

using namespace wasm::db;
using namespace eosio;

enum class err: uint8_t {
   NONE                 = 0,
   RECORD_NOT_FOUND     = 1,
   RECORD_EXISTING      = 2,
   SYMBOL_MISMATCH      = 4,
   PARAM_ERROR          = 5,
   MEMO_FORMAT_ERROR    = 6,
   PAUSED               = 7,
   NO_AUTH              = 8,
   NOT_POSITIVE         = 9,
   NOT_STARTED          = 10,
   OVERSIZED            = 11,
   TIME_EXPIRED         = 12,
   NOTIFY_UNRELATED     = 13,
   ACTION_REDUNDANT     = 14,
   ACCOUNT_INVALID      = 15,
   FEE_INSUFFICIENT     = 16,
   FIRST_CREATOR        = 17,
   STATUS_ERROR         = 18,
   SCORE_NOT_ENOUGH     = 19,
   NEED_MANUAL_CHECK    = 20
};

/**
 * The `did.recover` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `did.recover` contract instead of developing their own.
 *
 * The `did.recover` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
 *
 * The `did.recover` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
 *
 * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
 */
class [[eosio::contract("did.recover")]] did_recover : public contract {
   private:
      dbc                 _db;

   public:
      using contract::contract;
  
   did_recover(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
         _db(get_self()),
         _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }
    ~did_recover() { _global.set( _gstate, get_self() ); }

   ACTION init( const extended_asset& fee_info, const name& fee_collector );
   /**
    * @brief 
    * 
    * @param account 
    * @param actions 
    * @return ACTION 
    */
   ACTION setauth( const name& account, const set<name>& actions );

   ACTION delauth( const name& account ) ;

   // ACTION cancelorder( const name& account );

   ACTION verifydid( const name& submitter, const uint64_t& order_id, const bool& passed);

   // ACTION verifyvote( const name& submitter, const name& account, const bool& passed);

   ACTION assetrecast( const name& submitter, const uint64_t& order_id, const bool& passed);

   ACTION delorder(  const name& submitter, const uint64_t& order_id );

   [[eosio::on_notify("amax.token::transfer")]]
   void on_amax_transfer(const name& from, const name& to, const asset& quant, const string& memo);

   // ACTION deltable(){

   //    require_auth(_self);   

   //    recover_order_t::idx_t orders( _self, _self.value);

   //    auto itr = orders.begin();
   //    while ( itr != orders.end())
   //    {
   //       itr = orders.erase( itr );
   //    }
      

   // }
   
   ACTION auditlog(const uint64_t& order_id,
                  const name& submitter, 
                  const name& owner,
                  const name& recover_name,
                  const name& type, 
                  const bool& passed,
                  const time_point&  created_at);

   using auditlog_action = eosio::action_wrapper<"auditlog"_n, &did_recover::auditlog>;                  
   private:
        global_singleton    _global;
        global_t            _gstate;

   private:
      void _check_action_auth(const name& admin, const name& action_type);
      void _create_order(const name& owner, const name& account);
      void _on_audit_log(const uint64_t& order_id,const name& submitter, const name& owner,const name& recover_name, const name& type, const bool& passed);

};


} //namespace amax
