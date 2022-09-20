   #pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>

#include <string>

#include <amax.did/amax.did_db.hpp>
#include <amax.mtoken/amax.mtoken.hpp>

namespace amax {

using std::string;
using std::vector;


#define TRANSFER(bank, to, quantity, memo) \
    {	mtoken::transfer_action act{ bank, { {_self, active_perm} } };\
			act.send( _self, to, quantity , memo );}

using namespace eosio;

static constexpr name      NFT_BANK    = "did.ntoken"_n;
static constexpr name      CNYD_BANK   = "cnyd.token"_n;
static constexpr eosio::name active_perm{"active"_n};


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
   STATUS_ERROR         = 18

};
namespace vendor_info_status {
    static constexpr eosio::name RUNNING            = "running"_n;
    static constexpr eosio::name STOP               = "stop"_n;
};

/**
 * The `amax.did` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `amax.did` contract instead of developing their own.
 *
 * The `amax.did` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
 *
 * The `amax.did` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
 *
 * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
 */
class [[eosio::contract("amax.did")]] amax_did : public contract {
   public:
      using contract::contract;

   amax_did(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
        _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~amax_did() { _global.set( _gstate, get_self() ); }


   [[eosio::on_notify("amax.mtoken::transfer")]]
   void ontransfer(const name& from, const name& to, const asset& quant, const string& memo);

   ACTION init( const name& admin, const name& nft_contract, const name& fee_colletor);

   ACTION finishdid(const uint64_t& order_id);

   ACTION  addvendor(const string& vendor_name,
                     const name& vendor_account,
                     uint32_t& kyc_level,
                     const asset& vendor_charge_quant,
                     const asset& user_reward_quant, 
                     const asset& user_charge_amount,
                     const nsymbol& nft_id );
    ACTION chgvendor(const uint64_t& vendor_id, const name& status);

   // ACTION cancelorder( const uint64_t& order_id );
   private:
      global_singleton    _global;
      global_t            _gstate;

   private:

      void _reward_farmer( const asset& fee, const name& farmer );
};
} //namespace amax
