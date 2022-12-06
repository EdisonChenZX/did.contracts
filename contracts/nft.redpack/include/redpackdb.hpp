#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>
#include "amax.ntoken/amax.ntoken.db.hpp"
#include "amax.ntoken/amax.nsymbol.hpp"
#include "amax.ntoken/amax.nasset.hpp"
using namespace eosio;
using namespace std;
using namespace amax;
using std::string;

// using namespace wasm;
#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr eosio::name active_perm        {"active"_n};
static constexpr symbol SYS_SYMBOL              = SYMBOL("AMAX", 8);
static constexpr name SYS_BANK                  { "amax.token"_n };

#ifndef DAY_SECONDS_FOR_TEST
static constexpr uint64_t DAY_SECONDS           = 24 * 60 * 60;
#else
#warning "DAY_SECONDS_FOR_TEST should be used only for test!!!"
static constexpr uint64_t DAY_SECONDS           = DAY_SECONDS_FOR_TEST;
#endif//DAY_SECONDS_FOR_TEST

static constexpr uint32_t MAX_TITLE_SIZE        = 64;

namespace wasm { namespace db {

#define TG_TBL [[eosio::table, eosio::contract("nft.redpack")]]
#define TG_TBL_NAME(name) [[eosio::table(name), eosio::contract("nft.redpack")]]

struct TG_TBL_NAME("global") global_t {
    name tg_admin;
    uint16_t expire_hours;
    uint16_t data_failure_hours;
    bool     enable_did;
    EOSLIB_SERIALIZE( global_t, (tg_admin)(expire_hours)(data_failure_hours)(enable_did) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;


namespace redpack_status {
    static constexpr eosio::name INIT           = "init"_n;
    static constexpr eosio::name CREATED        = "created"_n;
    static constexpr eosio::name FINISHED       = "finished"_n;
    static constexpr eosio::name CANCELLED      = "cancelled"_n;

};

uint128_t get_unionid( const name& rec, uint64_t packid ) {
     return ( (uint128_t) rec.value << 64 ) | packid;
}

struct TG_TBL redpack_t {
    name            code;
    name            sender;
    string          pw_hash;
    nasset          total_quantity;
    nasset          remain_quantity;
    asset           fee;
    name            status;
    uint16_t        type;  //0 random,1 mean
    name            nft_contract;
    time_point      created_at;
    time_point      updated_at;

    uint64_t primary_key() const { return code.value; }

    uint64_t by_updatedid() const { return ((uint64_t)updated_at.sec_since_epoch() << 32) | (code.value & 0x00000000FFFFFFFF); }
    uint64_t by_sender() const { return sender.value; }

    redpack_t(){}
    redpack_t( const name& c ): code(c){}

    typedef eosio::multi_index<"redpacks"_n, redpack_t,
        indexed_by<"updatedid"_n,  const_mem_fun<redpack_t, uint64_t, &redpack_t::by_updatedid> >,
        indexed_by<"senderid"_n,  const_mem_fun<redpack_t, uint64_t, &redpack_t::by_sender> >
    > idx_t;

    EOSLIB_SERIALIZE( redpack_t, (code)(sender)(pw_hash)(total_quantity)(remain_quantity)
                                    (fee)(status)(type)(nft_contract)(created_at)(updated_at) )
};

struct TG_TBL claim_t {
    uint64_t        id;
    name            red_pack_code;
    name            sender;                     //plan owner
    name            receiver;                      //plan title: <=64 chars
    nasset          quantity;             //asset issuing contract (ARC20)
    time_point      claimed_at;                 //update time: last updated at
    uint64_t primary_key() const { return id; }
    uint128_t by_unionid() const { return get_unionid(receiver, red_pack_code.value); }
    uint64_t by_claimedid() const { return ((uint64_t)claimed_at.sec_since_epoch() << 32) | (id & 0x00000000FFFFFFFF); }
    uint64_t by_sender() const { return sender.value; }
    uint64_t by_receiver() const { return receiver.value; }
    uint64_t by_packid() const { return red_pack_code.value; }

    typedef eosio::multi_index<"claims"_n, claim_t,
        indexed_by<"unionid"_n,  const_mem_fun<claim_t, uint128_t, &claim_t::by_unionid> >,
        indexed_by<"claimedid"_n,  const_mem_fun<claim_t, uint64_t, &claim_t::by_claimedid> >,
        indexed_by<"packid"_n,  const_mem_fun<claim_t, uint64_t, &claim_t::by_packid> >,
        indexed_by<"senderid"_n,  const_mem_fun<claim_t, uint64_t, &claim_t::by_sender> >,
        indexed_by<"receiverid"_n,  const_mem_fun<claim_t, uint64_t, &claim_t::by_receiver> >
    > idx_t;

    EOSLIB_SERIALIZE( claim_t, (id)(red_pack_code)(sender)(receiver)(quantity)(claimed_at) )
};

struct TG_TBL fee_t {
    name            nft_contract;
    asset           fee;
    name            fee_contract;
    
    fee_t() {};
    fee_t( const name& contract_name ): nft_contract( contract_name ) {}

    uint64_t primary_key()const { return nft_contract.value; }

    typedef eosio::multi_index< "fees"_n,  fee_t > idx_t;

    EOSLIB_SERIALIZE( fee_t, (nft_contract)(fee)(fee_contract) );
};


} }